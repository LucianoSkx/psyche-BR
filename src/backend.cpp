#include "backend.h"
#include "catalog.h"
#include "library.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QStringList>
#include <QtConcurrent>
void Backend::begin(QString activity, QString status) {
    m_busy = true;
    m_activity = activity;
    m_status = status;
    m_kind = "info";
    emit changed();
}
void Backend::finish(QString status, QString kind) {
    m_busy = false;
    m_activity.clear();
    m_status = status;
    m_kind = kind;
    emit changed();
}
void Backend::acceptPackage(const Package& package, const QString& error) {
    if (!error.isEmpty()) {
        finish(error, "error");
        refreshHubcapOnAuthError(error);
        return;
    }
    m_package = package;
    m_preview = package.summary();
    m_ready = true;
    m_appId = package.mainAppId;
    if (m_appId.isEmpty() && package.apps.size() == 1)
        m_appId = *package.apps.begin();
    if (!gameName().isEmpty())
        m_source = gameName();
    finish("Pronto para adicionar.", "success");
    emit packageLoaded();
}
void Backend::inspect(QUrl file) {
    if (m_busy)
        return;
    if (!file.isLocalFile()) {
        finish("Selecione um ZIP local.", "error");
        return;
    }
    m_ready = false;
    m_applied = false;
    m_package = {};
    m_appId.clear();
    m_preview.clear();
    m_source = QFileInfo(file.toLocalFile()).fileName();
    m_settings->rememberImport(file);
    begin("import", "Lendo ZIP e buscando o jogo…");
    auto watcher = new QFutureWatcher<QPair<Package, QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        auto result = watcher->result();
        watcher->deleteLater();
        acceptPackage(result.first, result.second);
    });
    watcher->setFuture(QtConcurrent::run([file, api = m_nameApi] {
        try {
            auto package = readPackage(file.toLocalFile());
            Catalog::promoteKeyedApps(package);
            Catalog::resolveGameName(package, api);
            return qMakePair(package, QString());
        } catch (const std::exception& e) {
            return qMakePair(Package{}, QString::fromUtf8(e.what()));
        }
    }));
}
void Backend::apply() {
    if (!m_ready || m_busy || m_applied)
        return;
    if (!m_settings->destinationValid()) {
        finish("Escolha um diretório de destino em Configurações.", "error");
        return;
    }
    auto path = m_settings->destination();
    auto package = m_package;
    auto source = m_source;
    auto appId = m_appId;
    begin("apply", "Adicionando ao config.yaml…");
    auto watcher = new QFutureWatcher<QPair<QString, QString>>(this);
    connect(watcher,
            &QFutureWatcherBase::finished,
            this,
            [this, watcher, path, source, package, appId] {
                auto result = watcher->result();
                watcher->deleteLater();
                if (!result.second.isEmpty()) {
                    finish(result.second, "error");
                    return;
                }
                m_applied = true;
                bool saved = m_settings->recordApplication(
                    source, path, result.first, package.summary(), appId);
                finish(saved ? "Adicionado ao config.yaml."
                             : "Aplicado. Backup: " + result.first + ". Não foi possível salvar o histórico.",
                       saved ? "success" : "error");
                refreshLibrary();
            });
    watcher->setFuture(QtConcurrent::run([package, path] {
        try {
            return qMakePair(applyPackage(package, path), QString());
        } catch (const std::exception& e) {
            return qMakePair(QString(), QString::fromUtf8(e.what()));
        }
    }));
}
void Backend::restore(int historyIndex) {
    if (m_busy)
        return;
    auto history = m_settings->history();
    if (historyIndex < 0 || historyIndex >= history.size()) {
        finish("Backup não encontrado no histórico.", "error");
        return;
    }
    auto record = history[historyIndex].toMap();
    auto path = record["destination"].toString(), backup = record["backup"].toString();
    begin("restore", "Restaurando backup…");
    auto watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, backup] {
        auto error = watcher->result();
        watcher->deleteLater();
        if (!error.isEmpty()) {
            finish(error, "error");
            return;
        }
        m_applied = false;
        bool saved = m_settings->markRestored(backup);
        finish(saved ? "Backup restaurado com sucesso."
                     : "Backup restaurado, mas o histórico não pôde ser atualizado.",
               saved ? "success" : "error");
        refreshLibrary();
    });
    watcher->setFuture(QtConcurrent::run([path, backup] {
        try {
            restoreBackup(path, backup);
            return QString();
        } catch (const std::exception& e) {
            return QString::fromUtf8(e.what());
        }
    }));
}
void Backend::search(QString query, int offset) {
    if (m_busy)
        return;
    m_games.clear();
    m_hasMore = false;
    m_searchOffset = offset;
    m_settings->saveNavigation(m_settings->lastTab(), query);
    auto apiKey = m_settings->effectiveApiKey();
    begin("search", "Buscando na Hubcap…");
    auto watcher = new QFutureWatcher<QPair<SearchPage, QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        auto result = watcher->result();
        watcher->deleteLater();
        if (!result.second.isEmpty()) {
            finish(result.second, "error");
            refreshHubcapOnAuthError(result.second);
            return;
        }
        m_games = result.first.games;
        m_hasMore = result.first.hasMore;
        finish(m_games.isEmpty() ? "Nenhum jogo encontrado." : "Escolha um jogo.");
    });
    watcher->setFuture(QtConcurrent::run([query, apiKey, offset] {
        try {
            return qMakePair(Catalog(apiKey).search(query, offset), QString());
        } catch (const std::exception& e) {
            return qMakePair(SearchPage{}, QString::fromUtf8(e.what()));
        }
    }));
}
void Backend::fetch(QString appId, QString name) {
    if (m_busy)
        return;
    m_ready = false;
    m_applied = false;
    m_package = {};
    m_appId = appId;
    m_preview.clear();
    m_source = name.isEmpty() ? "AppID " + appId : name + " • " + appId;
    auto content = m_settings->downloadContent();
    m_source += " • " + Catalog::contentLabel(content);
    auto apiKey = m_settings->effectiveApiKey();
    begin("import", "Baixando " + Catalog::contentLabel(content) + " para o AppID " + appId + "…");
    auto watcher = new QFutureWatcher<QPair<Package, QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        auto result = watcher->result();
        watcher->deleteLater();
        acceptPackage(result.first, result.second);
    });
    watcher->setFuture(QtConcurrent::run([appId, apiKey, content, name, api = m_nameApi] {
        try {
            auto package = Catalog(apiKey).fetch(appId, content);
            if (!name.isEmpty())
                package.setGameName(name);
            else
                Catalog::resolveGameName(package, api);
            return qMakePair(package, QString());
        } catch (const std::exception& e) {
            return qMakePair(Package{}, QString::fromUtf8(e.what()));
        }
    }));
}
void Backend::openFolder(QString path) {
    if (!QFileInfo(path).isDir() || !QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        m_status = "Não foi possível abrir o diretório.";
        m_kind = "error";
        emit changed();
    }
}

void Backend::refreshHubcapOnAuthError(const QString& error) {
    if (error.contains("(401)") || error.contains("(403)") || error.contains("(429)"))
        checkHubcap();
}

void Backend::checkHubcap() {
    if (m_checkingHubcap)
        return;
    auto key = m_settings->effectiveApiKey();
    m_hubcapKey = key;
    m_checkingHubcap = true;
    m_hubcapInfo.clear();
    emit changed();
    auto watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, key] {
        auto result = watcher->result();
        watcher->deleteLater();
        m_checkingHubcap = false;
        if (key != m_settings->effectiveApiKey()) {
            emit changed();
            return;
        }
        m_hubcapInfo = result;
        if (m_kind == "error" && (m_status.contains("(401)") || m_status.contains("(403)") ||
                                  m_status.contains("(429)"))) {
            const auto account = result.value("account").toMap();
            QStringList parts;
            const auto expires = account.value("api_key_expires_at").toString();
            if (!expires.isEmpty())
                parts << "Expira: " + expires;
            if (account.contains("daily_usage") || account.contains("daily_limit"))
                parts << "Hoje: " + account.value("daily_usage").toString() + " / " +
                             account.value("daily_limit").toString();
            if (account.value("can_make_requests") == false)
                parts << "Bloqueada";
            if (!parts.isEmpty() && !m_status.contains("Expira:") && !m_status.contains("Hoje:"))
                m_status += " " + parts.join(" · ") + ".";
        }
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([key] {
        QVariantMap result;
        Catalog catalog(key);
        try {
            result["health"] = catalog.health().value("status");
        } catch (const std::exception& e) {
            result["healthError"] = QString::fromUtf8(e.what());
        }
        if (!key.isEmpty())
            try {
                result["account"] = catalog.stats();
            } catch (const std::exception& e) {
                result["accountError"] = QString::fromUtf8(e.what());
            }
        return result;
    }));
}

void Backend::refreshLibrary() {
    if (m_busy)
        return;
    try {
        m_installed = installedGames(m_settings->destination());
        m_libraryError.clear();
    } catch (const std::exception& e) {
        m_installed.clear();
        m_libraryError = QString::fromUtf8(e.what());
    }
    emit changed();
}
void Backend::removeGame(QString gameId) {
    if (m_busy || !m_settings->destinationValid())
        return;
    auto path = m_settings->destination();
    QString name, appId;
    for (const auto& value : m_installed) {
        auto game = value.toMap();
        if (game["id"].toString() == gameId) {
            name = game["name"].toString();
            appId = game["appId"].toString();
            break;
        }
    }
    if (name.isEmpty())
        return;
    m_settings->refreshSteamRunning();
    const bool steamOpen = m_settings->steamRunning();
    begin("remove", "Removendo entradas do jogo…");
    auto watcher = new QFutureWatcher<QPair<QString, QString>>(this);
    connect(watcher,
            &QFutureWatcherBase::finished,
            this,
            [this, watcher, path, name, appId, steamOpen] {
                auto result = watcher->result();
                watcher->deleteLater();
                if (!result.second.isEmpty()) {
                    finish(result.second, "error");
                    refreshLibrary();
                    return;
                }
                m_applied = false;
                auto saved = m_settings->recordApplication(
                    "Removido: " + name, path, result.first, "Entradas do jogo removidas", appId);
                auto status =
                    saved ? QString("Entradas do jogo removidas. Backup salvo no Histórico.")
                          : QString("Removido. Não foi possível salvar o histórico. Backup: " + result.first);
                if (steamOpen)
                    status += " Reinicie a Steam para atualizar a biblioteca.";
                finish(status, saved ? "success" : "error");
                refreshLibrary();
            });
    watcher->setFuture(QtConcurrent::run([path, gameId] {
        try {
            return qMakePair(removeInstalledGame(path, gameId), QString());
        } catch (const std::exception& e) {
            return qMakePair(QString(), QString::fromUtf8(e.what()));
        }
    }));
}
