#include "proton.h"
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
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
} // namespace
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
