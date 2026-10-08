#include "backend.h"
#include "catalog.h"
#include "library.h"
#include "workshopacf.h"
#include "proton.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
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
    m_platforms.clear();
    m_proton.clear();
    m_depotDetails.clear();
    if (qEnvironmentVariableIsSet("PSYCHE_OFFLINE"))
        return;
    const auto pid = m_appId;
    const auto dataDir2 = m_settings->dataDirectory();
    auto w2 = new QFutureWatcher<QVariantMap>(this);
    connect(w2, &QFutureWatcherBase::finished, this, [this, w2, pid] {
        const auto tiers = w2->result();
        w2->deleteLater();
        m_proton = tiers.value(pid).toString();
        emit changed();
    });
    w2->setFuture(QtConcurrent::run([pid, dataDir2] { return Proton::tiers({pid}, dataDir2); }));
    auto w3 = new QFutureWatcher<QVariantList>(this);
    connect(w3, &QFutureWatcherBase::finished, this, [this, w3] {
        m_depotDetails = w3->result();
        w3->deleteLater();
        QSet<QString> platforms;
        for (const auto& entry : m_depotDetails)
            for (const auto& os : entry.toMap().value("oslist").toList())
                platforms.insert(os.toString());
        m_platforms = platforms.values();
        emit changed();
    });
    w3->setFuture(QtConcurrent::run([pid] {
        QVariantList mapped;
        for (const auto& entry : Catalog(QString()).depotDetails(pid))
            mapped.append(entry);
        return mapped;
    }));
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
void Backend::cancelDownload() {
    ++m_downloadToken;
    if (m_downloadProcess) {
        m_downloadProcess->kill();
        m_downloadProcess->deleteLater();
        m_downloadProcess = nullptr;
    }
    if (m_downloading) {
        m_downloading = false;
        m_downloadStatus = "Download cancelado.";
        m_downloadPercent = 0;
        emit changed();
    }
    ++m_workshopToken;
    if (m_workshopProcess) {
        m_workshopProcess->kill();
        m_workshopProcess->deleteLater();
        m_workshopProcess = nullptr;
    }
    if (m_workshopBusy) {
        m_workshopBusy = false;
        m_workshopStatus = "Download do Workshop cancelado.";
        m_workshopPercent = 0;
        emit changed();
    }
}

QString Backend::resolveDotnet() const {
    QStringList candidates;
    candidates << qEnvironmentVariable("PSYCHE_DOTNET") << QDir::homePath() + "/.dotnet/dotnet"
               << "/usr/share/dotnet/dotnet" << "/usr/lib/dotnet/dotnet";
    const auto pathEnv = QProcessEnvironment::systemEnvironment().value("PATH").split(':');
    for (const auto& dir : pathEnv)
        candidates << dir + "/dotnet";
    for (const auto& candidate : candidates)
        if (!candidate.isEmpty() && QFileInfo(candidate).isExecutable())
            return candidate;
    return {};
}

QString Backend::resolveDepotDownloader() const {
    const auto dataDir = m_settings->dataDirectory();
    QStringList candidates;
    candidates << qEnvironmentVariable("PSYCHE_DEPOTDOWNLOADER")
               << dataDir + "/runtime/depotdownloader/DepotDownloader.dll"
               << dataDir + "/depotdownloader/DepotDownloader.dll"
               << QCoreApplication::applicationDirPath() + "/../depotdownloader/DepotDownloader.dll";
    for (const auto& candidate : candidates)
        if (!candidate.isEmpty() && QFileInfo::exists(candidate))
            return QFileInfo(candidate).canonicalFilePath();
    return {};
}

void Backend::downloadGame(QVariantList selectedDepots) {
    if (m_busy || m_downloading || !m_ready || m_package.keys.isEmpty()) {
        if (m_ready && m_package.keys.isEmpty()) {
            m_downloadStatus = "Este pacote não tem chaves de depot.";
            emit changed();
        }
        return;
    }
    const auto appId = m_package.mainAppId.isEmpty() ? m_appId : m_package.mainAppId;
    if (appId.isEmpty()) {
        m_downloadStatus = "AppID desconhecido para este pacote.";
        emit changed();
        return;
    }
    const auto dataDir = m_settings->dataDirectory();
    const auto name = (gameName().isEmpty() ? "AppID " + appId : gameName()).replace('/', '-');
    auto library = m_settings->downloadLibrary();
    if (library.isEmpty()) {
        const auto libs = m_settings->libraries();
        if (!libs.isEmpty())
            library = libs.first().toMap().value("path").toString();
    }
    if (!library.isEmpty()) {
        QDir().mkpath(library + "/steamapps/common");
        m_downloadDir = library + "/steamapps/common/" + name;
    } else {
        m_downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/psyche/" + name;
    }
    m_downloading = true;
    ++m_downloadToken;
    const int token = m_downloadToken;
    m_eosWarning = false;
    m_downloadPercent = 0;
    m_downloadStatus = "Procurando manifestos em cache…";
    emit changed();
    const auto apiKey = m_settings->effectiveApiKey();
    const auto manifestsDir = dataDir + "/manifests/" + appId;
    auto proceed = [this, appId, dataDir, manifestsDir, token, selectedDepots](const QMap<QString, QString>& manifests) {
        if (token != m_downloadToken)
            return;
        m_keysFile = dataDir + "/depot-keys-" + appId + ".vdf";
        QFile keysFile(m_keysFile);
        if (!keysFile.open(QIODevice::WriteOnly)) {
            m_downloading = false;
            m_downloadStatus = "Não foi possível gravar o arquivo de chaves.";
            emit changed();
            return;
        }
        m_downloadQueue.clear();
        QSet<QString> wanted;
        for (const auto& depot : selectedDepots)
            wanted.insert(depot.toString());
        for (auto it = m_package.keys.constBegin(); it != m_package.keys.constEnd(); ++it) {
            if (!wanted.isEmpty() && !wanted.contains(it.key()))
                continue;
            keysFile.write((it.key() + ";" + it.value() + "\n").toUtf8());
            const auto manifest = manifests.value(it.key());
            if (!manifest.isEmpty())
                m_downloadQueue.append({it.key(), manifest});
        }
        keysFile.close();
        if (m_downloadQueue.isEmpty()) {
            m_downloading = false;
            m_downloadStatus = "Nenhum manifesto compatível com as chaves do pacote.";
            emit changed();
            return;
        }
        const auto dotnet = resolveDotnet();
        const auto dll = resolveDepotDownloader();
        if (dotnet.isEmpty() || dll.isEmpty()) {
            m_downloading = false;
            m_downloadStatus = dotnet.isEmpty()
                ? "Runtime .NET 9 não encontrado. Instale com o script dotnet-install."
                : "DepotDownloader não encontrado junto ao psyche.";
            emit changed();
            return;
        }
        m_manifestsDir = manifestsDir;
        m_downloadIndex = 0;
        m_downloadStatus = "Baixando jogos e conteúdos…";
        emit changed();
        runNextDepot(dotnet, dll, appId);
    };
    QMap<QString, QString> cached;
    QSet<QString> wantedCache;
    for (const auto& depot : selectedDepots)
        wantedCache.insert(depot.toString());
    for (auto it = m_package.keys.constBegin(); it != m_package.keys.constEnd(); ++it) {
        if (!wantedCache.isEmpty() && !wantedCache.contains(it.key()))
            continue;
        const auto files =
            QDir(manifestsDir).entryList({it.key() + "_*.manifest"}, QDir::Files);
        if (files.isEmpty())
            continue;
        const auto match = QRegularExpression("^(\\d+)_(\\d+)\\.manifest$").match(files.first());
        if (match.hasMatch())
            cached[it.key()] = match.captured(2);
    }
    const int wantedCount = selectedDepots.isEmpty() ? m_package.keys.size() : selectedDepots.size();
    if (cached.size() == wantedCount) {
        m_downloadStatus = "Usando manifestos em cache…";
        proceed(cached);
        return;
    }
    m_downloadStatus = "Baixando manifestos da Hubcap…";
    emit changed();
    auto watcher = new QFutureWatcher<QPair<QMap<QString, QString>, QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, proceed] {
        auto result = watcher->result();
        watcher->deleteLater();
        if (!result.second.isEmpty()) {
            m_downloading = false;
            m_downloadStatus = "Falha ao obter manifestos: " + result.second;
            emit changed();
            return;
        }
        proceed(result.first);
    });
    const auto apiK = m_settings->effectiveApiKey();
    watcher->setFuture(QtConcurrent::run([apiK, appId, manifestsDir] {
        try {
            return qMakePair(Catalog(apiK).fetchManifests(appId, manifestsDir), QString());
        } catch (const std::exception& e) {
            return qMakePair(QMap<QString, QString>{}, QString::fromUtf8(e.what()));
        }
    }));
}

void Backend::downloadWorkshop() {
    if (m_busy || m_downloading || m_workshopBusy)
        return;
    if (m_workshopIds.isEmpty()) {
        m_workshopStatus = "Cole IDs ou URLs do Workshop para baixar.";
        emit changed();
        return;
    }
    const auto dotnet = resolveDotnet();
    const auto dll = resolveDepotDownloader();
    if (dotnet.isEmpty()) {
        m_workshopStatus = "Runtime .NET 9 não encontrado. Instale com o script dotnet-install.";
        emit changed();
        return;
    }
    if (dll.isEmpty()) {
        m_workshopStatus = "DepotDownloader não encontrado junto ao psyche.";
        emit changed();
        return;
    }
    if (m_settings->effectiveApiKey().isEmpty()) {
        m_workshopStatus = "Defina sua chave Hubcap para obter os manifestos do Workshop.";
        emit changed();
        return;
    }
    m_workshopQueue.clear();
    for (const auto& id : m_workshopIds)
        m_workshopQueue.append(QVariantMap{{"id", id}, {"state", "pendente"}, {"error", QString()}});
    m_workshopBusy = true;
    ++m_workshopToken;
    m_workshopIndex = 0;
    m_workshopPercent = 0;
    m_workshopStatus = "Preparando downloads do Workshop…";
    emit changed();
    runNextWorkshop(dotnet, dll);
}

void Backend::runNextWorkshop(const QString& dotnet, const QString& dll) {
    if (m_workshopIndex >= m_workshopQueue.size()) {
        m_workshopBusy = false;
        m_workshopPercent = 100;
        m_workshopStatus = "Downloads do Workshop concluídos.";
        emit changed();
        return;
    }
    const auto token = m_workshopToken;
    const auto workshopId = m_workshopQueue[m_workshopIndex].toMap().value("id").toString();
    m_workshopStatus = "Buscando manifesto do item " + workshopId + "…";
    emit changed();
    const auto apiKey = m_settings->effectiveApiKey();
    auto watcher = new QFutureWatcher<QPair<QVariantMap, QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, dotnet, dll, token, workshopId] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (token != m_workshopToken)
            return; // cancelado
        if (!result.second.isEmpty()) {
            auto failed = m_workshopQueue[m_workshopIndex].toMap();
            failed["state"] = "erro";
            failed["error"] = result.second;
            m_workshopQueue[m_workshopIndex] = failed;
            m_workshopStatus = "Item " + workshopId + ": " + result.second;
            ++m_workshopIndex;
            emit changed();
            runNextWorkshop(dotnet, dll);
            return;
        }
        const auto appId = result.first.value("appId").toString();
        const auto manifestId = result.first.value("manifestId").toString();
        auto library = m_settings->downloadLibrary();
        if (library.isEmpty()) {
            const auto libs = m_settings->libraries();
            if (!libs.isEmpty())
                library = libs.first().toMap().value("path").toString();
        }
        const auto root = library.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/psyche/workshop"
            : library + "/steamapps/workshop/content";
        const auto dir = root + "/" + appId + "/" + workshopId;
        QDir().mkpath(dir);
        auto process = new QProcess(this);
        m_workshopProcess = process;
        QStringList args;
        // O ID colado vem da URL do Workshop, que é um PublishedFileId: -pubfile.
        // -ugc espera o UGC id interno e devolve 404 com um PublishedFileId.
        args << dll << "-app" << appId << "-pubfile" << workshopId << "-dir" << dir << "-validate"
             << "-max-downloads" << "4";
        connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
            const auto data = process->readAllStandardOutput();
            QRegularExpression re("(\\d{1,3}(?:\\.\\d+)?)%");
            auto it = re.globalMatch(QString::fromUtf8(data));
            double percent = -1;
            while (it.hasNext())
                percent = it.next().captured(1).toDouble();
            if (percent >= 0 && m_workshopIndex < m_workshopQueue.size()) {
                const double overall =
                    (m_workshopIndex + percent / 100.0) / m_workshopQueue.size();
                m_workshopPercent = static_cast<int>(overall * 100);
                m_workshopStatus = QString("Baixando item %1… %2% total")
                                       .arg(m_workshopQueue[m_workshopIndex].toMap().value("id").toString())
                                       .arg(m_workshopPercent);
                emit changed();
            }
        });
        connect(process, &QProcess::readyReadStandardError, this, [this, process] {
            const auto data = process->readAllStandardError();
            if (!data.isEmpty()) {
                m_workshopStatus += " " + QString::fromUtf8(data).trimmed().left(80);
                emit changed();
            }
        });
        connect(process,
                qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this,
                [this, dotnet, dll, token, process, appId, manifestId, workshopId, dir,
                      library](int code, QProcess::ExitStatus) {
            if (m_workshopProcess == process)
                m_workshopProcess = nullptr;
            process->deleteLater();
            if (token != m_workshopToken)
                return; // cancelado
            auto done = m_workshopQueue[m_workshopIndex].toMap();
            if (code == 0) {
                done["state"] = "ok";
                // Só dentro de uma biblioteca Steam o ACF faz sentido: sem ela a
                // Steam nunca vai ler o arquivo.
                if (!library.isEmpty()) {
                    const auto error = registerWorkshopItem(
                        library + "/steamapps/workshop/appworkshop_" + appId + ".acf",
                        appId, workshopId, manifestId, dir);
                    if (!error.isEmpty())
                        done["note"] = error;
                }
            } else {
                done["state"] = "erro";
                done["error"] = QString("DepotDownloader saiu com código %1.").arg(code);
            }
            m_workshopQueue[m_workshopIndex] = done;
            ++m_workshopIndex;
            emit changed();
            runNextWorkshop(dotnet, dll);
        });
        process->start(dotnet, args);
    });
    watcher->setFuture(QtConcurrent::run([apiKey, workshopId] {
        try {
            return qMakePair(Catalog(apiKey).fetchWorkshopInfo(workshopId), QString());
        } catch (const std::exception&) {
            // A Hubcap cobre só parte do catálogo; sem o item nela, o AppID sai
            // da página pública da Steam e o manifesto fica vazio no ACF.
            try {
                const Catalog catalog(apiKey);
                return qMakePair(QVariantMap{{"appId", catalog.fetchSteamWorkshopAppId(workshopId)},
                                             {"manifestId", QString()}},
                                 QString());
            } catch (const std::exception& e) {
                return qMakePair(QVariantMap(), QString::fromUtf8(e.what()));
            }
        }
    }));
}

void Backend::runNextDepot(const QString& dotnet, const QString& dll, const QString& appId) {
    if (m_downloadIndex >= m_downloadQueue.size()) {
        m_downloading = false;
        m_downloadStatus = "Download concluído.";
        m_downloadPercent = 100;
        QDirIterator it(m_downloadDir, {"EOSSDK*.dll"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (it.next().toLower().contains("eossdk-win64-shipping")) {
                m_eosWarning = true;
                break;
            }
        }
        emit changed();
        return;
    }
    const auto entry = m_downloadQueue[m_downloadIndex];
    const auto manifestFile = m_manifestsDir + "/" + entry.first + "_" + entry.second + ".manifest";
    auto process = new QProcess(this);
    m_downloadProcess = process;
    QStringList args;
    const auto osArg = m_settings->downloadOS() == "mac" ? "osx" : m_settings->downloadOS();
    args << dll << "-app" << appId << "-depot" << entry.first << "-manifest" << entry.second
         << "-manifestfile" << manifestFile << "-depotkeys" << m_keysFile << "-max-downloads" << "4"
         << "-dir" << m_downloadDir << "-validate" << "-os" << osArg;
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        const auto data = process->readAllStandardOutput();
        QRegularExpression re("(\\d{1,3}(?:\\.\\d+)?)%");
        auto it = re.globalMatch(QString::fromUtf8(data));
        double percent = -1;
        while (it.hasNext())
            percent = it.next().captured(1).toDouble();
        if (percent >= 0 && m_downloadIndex < m_downloadQueue.size()) {
            const double overall = (m_downloadIndex + percent / 100.0) / m_downloadQueue.size();
            m_downloadPercent = static_cast<int>(overall * 100);
            m_downloadStatus = QString("Baixando %1… %2% total")
                                   .arg(m_downloadQueue[m_downloadIndex].first)
                                   .arg(m_downloadPercent);
            emit changed();
        }
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process] {
        const auto data = process->readAllStandardError();
        m_downloadStatus += data.isEmpty() ? QString() : " " + QString::fromUtf8(data).trimmed().left(80);
        emit changed();
    });
    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this, dotnet, dll, appId, process](int code, QProcess::ExitStatus) {
        if (m_downloadProcess == process)
            m_downloadProcess = nullptr;
        process->deleteLater();
        if (!m_downloading)
            return; // cancelado: não encadeia nem sobrescreve o status
        if (code != 0) {
            m_downloading = false;
            m_downloadStatus = QString("DepotDownloader saiu com código %1.").arg(code);
            emit changed();
            return;
        }
        ++m_downloadIndex;
        runNextDepot(dotnet, dll, appId);
    });
    process->start(dotnet, args);
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
            if (result.second.contains("conexão") || result.second.contains("expirou") ||
                result.second.contains("Route") || result.second.contains("Network"))
                m_networkIssue = "Sem conexão com a Hubcap. Verifique sua internet.";
            finish(result.second, "error");
            refreshHubcapOnAuthError(result.second);
            return;
        }
        m_games = result.first.games;
        m_hasMore = result.first.hasMore;
        m_networkIssue.clear();
        finish(m_games.isEmpty() ? "Nenhum jogo encontrado." : "Escolha um jogo.");
        auto games = m_games;
        auto dataDir = m_settings->dataDirectory();
        auto ratings = new QFutureWatcher<QVariantMap>(this);
        connect(ratings, &QFutureWatcherBase::finished, this, [this, ratings] {
            auto tiers = ratings->result();
            ratings->deleteLater();
            for (auto& game : m_games) {
                auto map = game.toMap();
                auto tier = tiers.value(map.value("appId").toString()).toString();
                if (!tier.isEmpty()) {
                    map["proton"] = tier;
                    game = map;
                }
            }
            emit changed();
        });
        QStringList ids;
        for (const auto& game : games)
            ids.append(game.toMap().value("appId").toString());
        ratings->setFuture(QtConcurrent::run([ids, dataDir] { return Proton::tiers(ids, dataDir); }));
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
