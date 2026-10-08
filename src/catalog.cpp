#include "catalog.h"
#include "ranking.h"
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryFile>
#include <QTimer>
#include <archive.h>
#include <archive_entry.h>
#include <memory>
#include <stdexcept>
namespace {
void error(const QString& text) {
    throw std::runtime_error(text.toStdString());
}
QString field(const QJsonObject& object, const QStringList& names) {
    for (const auto& name : names) {
        auto value = object.value(name);
        auto text = value.isString()   ? value.toString().trimmed()
                    : value.isDouble() ? QString::number(value.toDouble(), 'f', 0)
                                       : QString();
        if (!text.isEmpty())
            return text;
    }
    return {};
}
} // namespace
Catalog::Catalog(QString apiKey, QUrl base, QUrl appInfo)
    : m_key(apiKey.trimmed()), m_base(base), m_appInfo(appInfo) {}
QString Catalog::validateAppId(const QString& value) {
    if (!QRegularExpression("^[1-9][0-9]{0,9}$").match(value).hasMatch() ||
        value.toULongLong() > 4294967295ULL)
        error("AppID inválido: " + value);
    return value;
}
QByteArray Catalog::get(const QString& endpoint,
                        const QUrlQuery& query,
                        qint64 limit,
                        bool authenticated,
                        int timeout,
                        QMap<QString, QString>* headers) const {
    if (authenticated && m_key.isEmpty())
        error("Defina sua chave Hubcap no app ou PSYCHE_HUBCAP_API_KEY.");
    if (authenticated && (m_key.contains('\r') || m_key.contains('\n')))
        error("Chave Hubcap inválida.");
    auto url = m_base;
    url.setPath(url.path() + endpoint);
    url.setQuery(query);
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    if (authenticated)
        request.setRawHeader("Authorization", "Bearer " + m_key.toUtf8());
    request.setRawHeader("User-Agent", "psyche/" PSYCHE_VERSION);
    // Don't follow redirects with the Hubcap bearer token. TLS stays on.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(qMin(15000, timeout));
    auto reply = manager.get(request);
    reply->setReadBufferSize(64 * 1024);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QByteArray data;
    bool tooLarge = false, timedOut = false;
    auto read = [&] {
        data += reply->readAll();
        if (data.size() > limit) {
            tooLarge = true;
            reply->abort();
        }
    };
    QObject::connect(reply, &QIODevice::readyRead, &loop, read);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
        timedOut = true;
        reply->abort();
    });
    timer.start(timeout);
    loop.exec();
    read();
    timer.stop();
    if (tooLarge)
        error("Resposta da Hubcap excede o limite de tamanho.");
    if (timedOut)
        error("Requisição à Hubcap expirou.");
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 401)
        error("Chave Hubcap ausente, inválida ou expirada (401). Renove se expirou.");
    if (status == 403)
        error("Acesso negado pela Hubcap (403).");
    if (status == 404)
        error(endpoint.startsWith("/generate/workshopmanifest")
                  ? "Item de Workshop não encontrado na Hubcap (404). Ele pode ter sido removido do Steam."
                  : "AppID/pacote não encontrado na Hubcap (404).");
    if (status == 429)
        error("Limite da Hubcap atingido (429).");
    if (status == 503 && endpoint == "/health")
        return data;
    if (status >= 300)
        error(QString("Erro da API Hubcap (HTTP %1).").arg(status));
    if (reply->error() != QNetworkReply::NoError)
        error("Erro de conexão com a Hubcap: " + reply->errorString());
    if (status != 200)
        error("Resposta inesperada da Hubcap.");
    if (headers)
        for (const auto& name : reply->rawHeaderList())
            headers->insert(QString::fromLatin1(name).toLower(),
                            QString::fromUtf8(reply->rawHeader(name)));
    return data;
}
namespace {
QVariantMap responseObject(const QByteArray& bytes) {
    QJsonParseError parse;
    auto doc = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject())
        error("Resposta de status Hubcap inválida.");
    return doc.object().toVariantMap();
}
} // namespace
QVariantMap Catalog::health() const {
    auto result = responseObject(get("/health", {}, 256 * 1024, false));
    auto status = result.value("status").toString();
    if (status != "healthy" && status != "degraded")
        error("Estado de saúde Hubcap desconhecido.");
    return {{"status", status}};
}
QVariantMap Catalog::stats() const {
    auto result = responseObject(get("/user/stats", {}, 256 * 1024));
    if (result.contains("error"))
        error("A Hubcap não retornou estatísticas da conta.");
    QVariantMap safe;
    for (const auto& field : {"username",
                              "daily_usage",
                              "daily_limit",
                              "api_key_usage_count",
                              "api_key_expires_at",
                              "can_make_requests"})
        if (result.contains(field))
            safe[field] = result[field];
    if (safe.isEmpty())
        error("Estatísticas da conta Hubcap ausentes.");
    return safe;
}
SearchPage Catalog::search(const QString& query, int offset) const {
    if (query.trimmed().isEmpty())
        error("Digite o nome do jogo.");
    if (offset < 0)
        error("Deslocamento inválido.");
    QUrlQuery params;
    params.addQueryItem("search", query.trimmed());
    params.addQueryItem("limit", "100");
    params.addQueryItem("offset", QString::number(offset));
    params.addQueryItem("sort_by", "name");
    auto bytes = get("/library", params, 4 * 1024 * 1024);
    QJsonParseError parseError;
    auto doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        error("Resposta JSON inválida da Hubcap.");
    QJsonArray rows;
    auto root = doc.object();
    if (doc.isArray())
        rows = doc.array();
    else if (root.value("games").isArray())
        rows = root.value("games").toArray();
    else if (root.value("results").isArray())
        rows = root.value("results").toArray();
    else
        error("Formato de busca Hubcap inválido.");
    SearchPage result;
    QSet<QString> seen;
    for (auto row : rows) {
        if (!row.isObject())
            error("Resultado Hubcap inválido.");
        auto object = row.toObject();
        auto appId = field(object, {"game_id", "app_id", "appid", "appId", "id"});
        try {
            validateAppId(appId);
        } catch (const std::exception&) {
            continue;
        }
        if (seen.contains(appId))
            continue;
        seen.insert(appId);
        auto name = field(object, {"name", "game_name", "title"});
        result.games.append(
            QVariantMap{{"appId", appId}, {"name", name.isEmpty() ? "Sem nome" : name}});
    }
    result.games = Ranking::rankGames(result.games, query);
    auto total = root.value("total_count");
    bool valid = false;
    auto count = total.toVariant().toLongLong(&valid);
    result.hasMore = !rows.isEmpty() && (valid ? offset + rows.size() < count : rows.size() >= 100);
    return result;
}
QString Catalog::contentLabel(const QString& content) {
    if (content == "full")
        return "Lua completo";
    if (content == "basegame")
        return "Lua • jogo base";
    if (content == "dlc")
        return "Lua • DLCs";
    if (content == "zip")
        return "ZIP completo";
    error("Conteúdo inválido. Use full, basegame, dlc ou zip.");
    return {};
}
Package Catalog::fetch(const QString& appId, const QString& content) const {
    contentLabel(content);
    validateAppId(appId);
    Package package;
    if (content != "zip") {
        auto endpoint = content == "full" ? "/lua/" + appId : "/lua/" + content + "/" + appId;
        package = readLuaPackage(get(endpoint, {}, 8 * 1024 * 1024));
    } else {
        auto bytes = get("/manifest/" + appId, {}, 32 * 1024 * 1024);
        QTemporaryFile zip;
        if (!zip.open() || zip.write(bytes) != bytes.size() || !zip.flush())
            error("Falha ao preparar ZIP temporário.");
        package = readPackage(zip.fileName());
    }
    package.mainAppId = appId;
    promoteKeyedApps(package, m_appInfo);
    package.setGameName("AppID " + appId);
    return package;
}

QMap<QString, QString> Catalog::fetchManifests(const QString& appId, const QString& destDir) const {
    validateAppId(appId);
    auto bytes = get("/manifest/" + appId, {}, 32 * 1024 * 1024);
    QTemporaryFile zip;
    if (!zip.open() || zip.write(bytes) != bytes.size() || !zip.flush())
        error("Falha ao preparar ZIP temporário.");
    QDir().mkpath(destDir);
    archive* ar = archive_read_new();
    archive_read_support_format_zip(ar);
    archive_read_support_filter_all(ar);
    auto deleter = [](archive* a) { archive_read_free(a); };
    std::unique_ptr<archive, decltype(deleter)> holder(ar, deleter);
    if (archive_read_open_filename(ar, zip.fileName().toLocal8Bit(), 16384) != ARCHIVE_OK)
        error("ZIP de manifestos inválido.");
    QMap<QString, QString> manifests;
    archive_entry* entry;
    while (archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
        const auto base = QFileInfo(QString::fromUtf8(archive_entry_pathname(entry))).fileName();
        if (!base.endsWith(".manifest")) {
            archive_read_data_skip(ar);
            continue;
        }
        QByteArray data;
        char buffer[16384];
        la_ssize_t n;
        while ((n = archive_read_data(ar, buffer, sizeof(buffer))) > 0)
            data.append(buffer, n);
        if (n < 0)
            error("ZIP de manifestos danificado.");
        if (data.size() < 16)
            error("Manifesto suspeito (tamanho inválido): " + base);
        QSaveFile out(destDir + "/" + base);
        if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() || !out.commit())
            error("Falha ao extrair " + base);
        const auto match = QRegularExpression("^(\\d+)_(\\d+)\\.manifest$").match(base);
        if (match.hasMatch())
            manifests[match.captured(1)] = match.captured(2);
    }
    if (manifests.isEmpty())
        error("Nenhum manifesto encontrado no pacote da Hubcap.");
    return manifests;
}

QStringList Catalog::parseWorkshopIds(const QString& text) {
    QStringList ids;
    QSet<QString> seen;
    const auto tokens = text.split(QRegularExpression("[\\s,;]+"), Qt::SkipEmptyParts);
    for (const auto& token : tokens) {
        const auto trimmed = token.trimmed();
        if (trimmed.isEmpty())
            continue;
        auto match = QRegularExpression("[?&]id=([0-9]+)").match(trimmed);
        if (!match.hasMatch())
            match = QRegularExpression("^([0-9]+)$").match(trimmed);
        if (!match.hasMatch())
            continue;
        const auto id = match.captured(1);
        if (!seen.contains(id))
            seen.insert(id), ids.append(id);
    }
    return ids;
}

QVariantMap Catalog::fetchWorkshopInfo(const QString& workshopId) const {
    if (!QRegularExpression("^[1-9][0-9]{0,19}$").match(workshopId).hasMatch())
        error("ID de Workshop inválido: " + workshopId);
    QMap<QString, QString> headers;
    // O corpo é o .manifest binário; só os cabeçalhos interessam aqui.
    get("/generate/workshopmanifest/" + workshopId, {}, 1024 * 1024, true, 60000, &headers);
    const auto appId = headers.value("x-app-id").trimmed();
    if (appId.isEmpty())
        error("Resposta sem X-App-Id para o item de Workshop " + workshopId + ".");
    return {{"appId", validateAppId(appId)}, {"manifestId", headers.value("x-manifest-id").trimmed()}};
}

QString Catalog::fetchSteamWorkshopAppId(const QString& workshopId) const {
    if (!QRegularExpression("^[1-9][0-9]{0,19}$").match(workshopId).hasMatch())
        error("ID de Workshop inválido: " + workshopId);
    // A Hubcap só tem parte do catálogo; a página pública da Steam resolve o resto.
    // Só o AppID importa aqui: o DepotDownloader oficial busca a chave de depôt na
    // Steam, então a chave que a Hubcap devolve não seria usada.
    auto url = QUrl("https://steamcommunity.com/sharedfiles/filedetails/");
    url.setQuery(QUrlQuery{{"id", workshopId}});
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "psyche/" PSYCHE_VERSION);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);
    auto reply = manager.get(request);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QByteArray data;
    QObject::connect(reply, &QIODevice::readyRead, &loop, [&] { data += reply->readAll(); });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] { reply->abort(); });
    timer.start(40000);
    loop.exec();
    data += reply->readAll();
    timer.stop();
    if (reply->error() != QNetworkReply::NoError && data.isEmpty())
        error("Não foi possível abrir a página do item na Steam: " + reply->errorString());
    // Links de browse do jogo dono aparecem como appid=<id>.
    auto match = QRegularExpression("appid=([1-9][0-9]{0,9})\\b").match(QString::fromUtf8(data));
    if (!match.hasMatch())
        error("A Steam não expôs o AppID do item de Workshop " + workshopId + ".");
    return validateAppId(match.captured(1));
}

QStringList Catalog::supportedPlatforms(const QString& appId) const {
    QSet<QString> platforms;
    for (const auto& entry : depotDetails(appId))
        for (const auto& os : entry.value("oslist").toList())
            platforms.insert(os.toString());
    return platforms.values();
}
QList<QVariantMap> Catalog::depotDetails(const QString& appId) const {
    try {
        validateAppId(appId);
        auto bytes = Catalog({}, m_appInfo).get("/v1/info/" + appId, {}, 4 * 1024 * 1024, false, 15000);
        QJsonParseError parse;
        auto doc = QJsonDocument::fromJson(bytes, &parse);
        if (parse.error != QJsonParseError::NoError || !doc.isObject())
            return {};
        auto info = doc.object()["data"].toObject().value(appId).toObject();
        QList<QVariantMap> result;
        const auto depotsObj = info.value("depots").toObject();
        for (auto it = depotsObj.constBegin(); it != depotsObj.constEnd(); ++it) {
            bool ok;
            it.key().toUInt(&ok);
            if (!ok || it.key() == appId)
                continue;
            const auto depot = it.value().toObject();
            const auto oslist = depot.value("config").toObject().value("oslist").toString();
            QStringList normalized;
            for (const auto& raw : oslist.split(',', Qt::SkipEmptyParts)) {
                auto value = raw.trimmed().toLower();
                if (value == QStringLiteral("macos") || value == QStringLiteral("osx"))
                    value = QStringLiteral("mac");
                if (QStringList{"linux", "windows", "mac"}.contains(value))
                    normalized.append(value);
            }
            if (oslist.isEmpty() || !normalized.isEmpty())
                result.append({{"depot", it.key()},
                               {"oslist", normalized},
                               {"name", depot.value("name").toString()},
                               {"dlc", depot.contains("dlcappid") && !depot.value("dlcappid").isNull()}});
        }
        return result;
    } catch (const std::exception&) {
        return {};
    }
}
void Catalog::promoteKeyedApps(Package& package, QUrl base) {
    if (!base.isValid() || base.isEmpty())
        return;
    auto pending = (package.depots - package.apps).values();
    if (pending.isEmpty())
        return;
    pending.sort();
    QSet<QString> confirmed;
    for (const auto& id : pending) {
        try {
            auto bytes = Catalog({}, base).get("/v1/info/" + id, {}, 1024 * 1024, false, 8000);
            QJsonParseError parse;
            auto doc = QJsonDocument::fromJson(bytes, &parse);
            if (parse.error != QJsonParseError::NoError || !doc.isObject())
                continue;
            auto root = doc.object();
            auto status = root["status"].toString();
            if (!status.isEmpty() && status != "success")
                continue;
            auto info = root["data"].toObject().value(id);
            if (!info.isObject())
                continue;
            auto common = info.toObject()["common"];
            // Apps return PICS `common`; depots come back as {}.
            if (!common.isObject() || common.toObject().isEmpty())
                continue;
            confirmed.insert(id);
        } catch (const std::exception&) {
            continue;
        }
    }
    if (confirmed.isEmpty())
        return;
    package.apps.unite(confirmed);
    auto name = !package.games.isEmpty()
                    ? package.games.first().toObject()["name"].toString()
                    : (!package.mainAppId.isEmpty() ? "AppID " + package.mainAppId : QString());
    if (!name.isEmpty())
        package.setGameName(name);
}

// Steam store lookup uses an unauthenticated Catalog; no Hubcap token.
bool Catalog::resolveGameName(Package& package, QUrl base) {
    auto appId = package.mainAppId;
    if (appId.isEmpty() && package.apps.size() == 1)
        appId = *package.apps.begin();
    if (appId.isEmpty())
        return false;
    try {
        validateAppId(appId);
        QUrlQuery query;
        query.addQueryItem("appids", appId);
        query.addQueryItem("l", "english");
        query.addQueryItem("filters", "basic");
        auto bytes = Catalog({}, base).get("/appdetails", query, 1024 * 1024, false, 8000);
        auto result = QJsonDocument::fromJson(bytes).object()[appId].toObject();
        auto data = result["data"].toObject();
        auto name = data["name"].toString().trimmed();
        if (!result["success"].toBool() || data["steam_appid"].toVariant().toString() != appId ||
            name.isEmpty() || name.size() > 512)
            return false;
        name.replace(QRegularExpression("[\\r\\n\\t]+"), " ");
        package.mainAppId = appId;
        package.setGameName(name);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
