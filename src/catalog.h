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
     * Agrupa os depotDetails para a lista do card (ASSella): Base, DLC,
     * Idiomas e Bônus (trilha/artbook/manuais). Cada item ganha `group` e
     * `checked`: marca o que baixaria por padrão — depôts compartilhados ou do
     * `system` (32-bit sai quando existe 64-bit do mesmo sistema), idioma
     * escolhido; bônus desmarcado. Ordem estável dentro de cada grupo.
     */
    static QVariantList organizeDepots(const QVariantList& depots, const QString& system,
                                       const QString& language);
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
