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
    // Plataformas com pelo menos um depot ("linux", "windows", "mac") declarado.
    QStringList supportedPlatforms(const QString& appId) const;
    // Um registro por depot: {"depot": id, "oslist": ["linux",...]}. Vazio em erro.
    QList<QVariantMap> depotDetails(const QString& appId) const;
    // Baixa o ZIP de manifestos da Hubcap, extrai os .manifest para destDir e retorna depotId -> manifestId.
    QMap<QString, QString> fetchManifests(const QString& appId, const QString& destDir) const;
    // AppID/manifest de um item do Workshop, lidos dos cabeçalhos X-App-Id/X-Manifest-Id. Lança em erro.
    QVariantMap fetchWorkshopInfo(const QString& workshopId) const;
    // AppID dono de um item do Workshop pela página pública da Steam. Lança em erro.
    QString fetchSteamWorkshopAppId(const QString& workshopId) const;
    static QStringList parseWorkshopIds(const QString& text);
    static bool resolveGameName(Package& package,
                                QUrl base = QUrl("https://store.steampowered.com/api"));
    // Promote keyed IDs that steamcmd info says are apps. Fail closed: errors leave them as depots.
    static void promoteKeyedApps(Package& package, QUrl base = QUrl("https://api.steamcmd.net"));
    static QString contentLabel(const QString& content);
    static QString validateAppId(const QString& value);

private:
    // headers (chaves em minúsculas) é preenchido quando não-nulo.
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
