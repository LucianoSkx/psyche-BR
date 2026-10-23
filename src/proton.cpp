#include "proton.h"
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
namespace {
const qint64 SUCCESS_EXPIRY = 7 * 24 * 3600;
const qint64 FAILURE_EXPIRY = 24 * 3600;
QString fetchTier(const QString& appId) {
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl("https://www.protondb.com/api/v1/reports/summaries/" + appId + ".json"));
    request.setRawHeader("User-Agent", "psyche/" PSYCHE_VERSION);
    auto reply = manager.get(request);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
        timedOut = true;
        reply->abort();
    });
    timer.start(10000);
    loop.exec();
    if (timedOut || reply->error() != QNetworkReply::NoError)
        return QString();
    auto doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject())
        return QString();
    auto tier = doc.object().value("tier").toString().toLower();
    return tier.isEmpty() ? QString() : tier;
}

int blockEnd(const QString& text, int open) {
    int depth = 0;
    bool quoted = false;
    for (int i = open; i < text.size(); ++i) {
        const auto c = text.at(i);
        if (quoted) {
            if (c == QLatin1Char('\\'))
                ++i;
            else if (c == QLatin1Char('"'))
                quoted = false;
        } else if (c == QLatin1Char('"')) {
            quoted = true;
        } else if (c == QLatin1Char('{')) {
            ++depth;
        } else if (c == QLatin1Char('}') && --depth == 0) {
            return i;
        }
    }
    return -1;
}

QString preferredTool(const QString& block) {
    QMap<QString, int> counts;
    auto matches = QRegularExpression("\"name\"\\s*\"([^\"]+)\"").globalMatch(block);
    while (matches.hasNext()) {
        const auto name = matches.next().captured(1);
        if (name != "Steam-Play-None" && !name.contains('"') && !name.contains('\\'))
            ++counts[name];
    }
    int best = 0;
    QString chosen;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        if (it.value() > best)
            best = it.value(), chosen = it.key();
    return chosen.isEmpty() ? QStringLiteral("proton_experimental") : chosen;
}
}
QVariantMap Proton::tiers(const QStringList& appIds, const QString& dataDir, int maxFetches) {
    const auto cachePath = dataDir + "/protondb.json";
    QJsonObject cache;
    if (QFile file(cachePath); file.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject())
            cache = doc.object();
    }
    QVariantMap result;
    QStringList stale;
    const auto now = QDateTime::currentSecsSinceEpoch();
    for (const auto& appId : appIds) {
        auto entry = cache.value(appId).toObject();
        const auto ts = qint64(entry.value("ts").toDouble());
        const bool ok = entry.value("ok").toBool(true);
        const auto age = now - ts;
        if (!entry.isEmpty() && (ok ? age < SUCCESS_EXPIRY : age < FAILURE_EXPIRY)) {
            if (ok) {
                auto tier = entry.value("tier").toString();
                if (!tier.isEmpty())
                    result[appId] = tier;
            }
            continue;
        }
        stale.append(appId);
    }
    int fetched = 0;
    for (const auto& appId : stale) {
        if (fetched >= maxFetches)
            break;
        ++fetched;
        auto tier = fetchTier(appId);
        QJsonObject entry;
        entry["ts"] = double(now);
        if (tier.isEmpty()) {
            entry["ok"] = false;
        } else {
            entry["ok"] = true;
            entry["tier"] = tier;
            result[appId] = tier;
        }
        cache[appId] = entry;
    }
    QDir().mkpath(dataDir);
    QSaveFile file(cachePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(cache).toJson());
        file.commit();
    }
    return result;
}

QString Proton::forceCompatTool(const QString& appId, const QString& steamDir, bool steamRunning) {
    for (const auto& digit : appId)
        if (!digit.isDigit())
            return "AppID inválido para definir o Proton.";
    if (steamDir.isEmpty())
        return "Steam não encontrada para definir o Proton.";
    if (steamRunning)
        return "Feche a Steam e clique em Definir Proton.";

    const auto path = steamDir + "/config/config.vdf";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024)
        return "Não foi possível ler " + path;
    const auto text = QString::fromUtf8(file.readAll());
    file.close();

    const auto marker = QStringLiteral("\"CompatToolMapping\"");
    const auto at = text.indexOf(marker);
    const auto open = at < 0 ? -1 : text.indexOf(QLatin1Char('{'), at + marker.size());
    const auto close = open < 0 ? -1 : blockEnd(text, open);
    if (close < 0)
        return "CompatToolMapping não encontrado em " + path;
    const auto block = text.mid(open, close - open);
    if (block.contains(QLatin1Char('"') + appId + QLatin1Char('"')))
        return {};

    auto updated = text;
    updated.insert(open + 1,
                   "\n\t\t\t\t\t\"" + appId + "\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"name\"\t\t\"" +
                       preferredTool(block) +
                       "\"\n\t\t\t\t\t\t\"config\"\t\t\"\"\n\t\t\t\t\t\t\"priority\"\t\t\"250\"\n"
                       "\t\t\t\t\t}");

    const auto backup = path + ".bak";
    if (!QFile::exists(backup) && !QFile::copy(path, backup))
        return "Não foi possível fazer backup de " + path;
    const auto payload = updated.toUtf8();
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(payload) != payload.size() || !out.commit())
        return "Não foi possível gravar " + path;
    return {};
}

QString Proton::steamRootFor(const QVariantList& steams, const QString& steamApps) {
    QString fallback;
    for (const auto& item : steams) {
        const auto path = item.toMap().value("path").toString();
        if (path.isEmpty() || path.contains("/.var/app/") ||
            !QFileInfo(path + "/config/config.vdf").isFile())
            continue;
        if (!steamApps.isEmpty() && steamApps.startsWith(path + "/"))
            return path;
        if (fallback.isEmpty())
            fallback = path;
    }
    return fallback;
}
