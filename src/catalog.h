#pragma once
#include "package.h"
#include <QMap>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantList>
struct SearchPage {
    QVariantList games;
    bool hasMore = false;
};
class Catalog {
public:
    explicit Catalog(QString apiKey,
                     QUrl base = QUrl("https://hubcapmanifest.com/api/v1"),
                     QUrl appInfo = QUrl("https://api.steamcmd.net"));
    QVariantMap health() const;
    QVariantMap stats() const;
    SearchPage search(const QString& query, int offset = 0) const;
    Package fetch(const QString& appId, const QString& content = "full") const;
    QStringList supportedPlatforms(const QString& appId) const;
    QList<QVariantMap> depotDetails(const QString& appId) const;

    /**
     * Groups depotDetails for the card list (ASSella-style): Base, DLC,
     * Languages and Bonus. `system`/`language` set the pre-checked defaults;
     * 32-bit drops out when a 64-bit depot for the same system exists. Like
     * ASSella, rows are hidden: Bonus always, macOS/Android when they are not
     * the target system, and shared depots (no oslist) when the system has a
     * dedicated depot — those still download via sharedBaseDepots().
     */
    static QVariantList organizeDepots(const QVariantList& depots, const QString& system,
                                       const QString& language);

    /** Ids of hidden shared Base depots (required game content). */
    static QStringList sharedBaseDepots(const QVariantList& depots, const QString& system);
    // api.steamcmd.net /v1/info exposes no depot names.
    static QVariantMap dlcNames(const QStringList& appIds, QUrl store,
                                QUrl appInfo = QUrl("https://api.steamcmd.net"));
    // Fills name for depots: dlcappid via appdetails, depot id via depots.ini overlay.
    static QVariantList enrichDlcNames(QVariantList depots, QUrl store,
                                       QUrl appInfo = QUrl("https://api.steamcmd.net"),
                                       QHash<QString, QString> depotNames = {});
    // Parses ASSella's community depots.ini ("id = name" lines under [depots]).
    static QHash<QString, QString> parseDepotsIni(const QString& path);
    QMap<QString, QString> fetchManifests(const QString& appId, const QString& destDir) const;
    QVariantMap fetchWorkshopInfo(const QString& workshopId, const QString& destDir) const;
    QString fetchSteamWorkshopAppId(const QString& workshopId) const;
    static QStringList parseWorkshopIds(const QString& text);
    static bool resolveGameName(Package& package,
                                QUrl base = QUrl("https://store.steampowered.com/api"));
    static void promoteKeyedApps(Package& package, QUrl base = QUrl("https://api.steamcmd.net"));
    static QString contentLabel(const QString& content);
    static QString validateAppId(const QString& value);

private:
    QByteArray get(const QString& endpoint,
                   const QUrlQuery& query,
                   qint64 limit,
                   bool authenticated = true,
                   int timeout = 60000,
                   QMap<QString, QString>* headers = nullptr) const;
    QString m_key;
    QUrl m_base;
    QUrl m_appInfo;
};
