#pragma once
#include "package.h"
#include "settings.h"
#include <QObject>
#include <QProcess>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList installed READ installed NOTIFY changed)
    Q_PROPERTY(QString libraryError READ libraryError NOTIFY changed)
    Q_PROPERTY(QVariantMap hubcapInfo READ hubcapInfo NOTIFY changed)
    Q_PROPERTY(bool checkingHubcap READ checkingHubcap NOTIFY changed)
    Q_PROPERTY(QVariantList games READ games NOTIFY changed)
    Q_PROPERTY(QString networkIssue READ networkIssue NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(int searchOffset READ searchOffset NOTIFY changed)
    Q_PROPERTY(QString preview READ preview NOTIFY changed)
    Q_PROPERTY(QVariantMap counts READ counts NOTIFY changed)
    Q_PROPERTY(QString appId READ appId NOTIFY changed)
    Q_PROPERTY(QString source READ source NOTIFY changed)
    Q_PROPERTY(QString gameName READ gameName WRITE setGameName NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString statusKind READ statusKind NOTIFY changed)
    Q_PROPERTY(QString activity READ activity NOTIFY changed)
    Q_PROPERTY(bool applied READ applied NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool downloading READ downloading NOTIFY changed)
    Q_PROPERTY(int downloadPercent READ downloadPercent NOTIFY changed)
    Q_PROPERTY(QString downloadStatus READ downloadStatus NOTIFY changed)
    Q_PROPERTY(QString downloadDir READ downloadDir NOTIFY changed)
    Q_PROPERTY(bool eosWarning READ eosWarning NOTIFY changed)
    Q_PROPERTY(QStringList platforms READ platforms NOTIFY changed)
    Q_PROPERTY(QString proton READ proton NOTIFY changed)
    Q_PROPERTY(QVariantList depotDetails READ depotDetails NOTIFY changed)
public:
    explicit Backend(AppSettings* settings,
                     QObject* parent = nullptr,
                     QUrl nameApi = QUrl("https://store.steampowered.com/api"))
        : QObject(parent), m_nameApi(nameApi), m_settings(settings) {
        connect(settings, &AppSettings::changed, this, [this] {
            if (m_hubcapKey != m_settings->effectiveApiKey() && !m_hubcapInfo.isEmpty()) {
                m_hubcapInfo.clear();
                emit changed();
            }
        });
    }
    QVariantList installed() const { return m_installed; }
    QString libraryError() const { return m_libraryError; }
    Q_INVOKABLE void refreshLibrary();
    Q_INVOKABLE void removeGame(QString gameId);
    QVariantMap hubcapInfo() const { return m_hubcapInfo; }
    bool checkingHubcap() const { return m_checkingHubcap; }
    Q_INVOKABLE void checkHubcap();
    QString preview() const { return m_preview; }
    QString status() const { return m_status; }
    QString gameName() const {
        return m_package.labels.isEmpty() ? QString() : m_package.labels.first();
    }
    void setGameName(const QString& name) {
        if (!m_busy) {
            m_package.setGameName(name);
            emit changed();
        }
    }
    QString appId() const { return m_appId; }
    QString source() const { return m_source; }
    QString statusKind() const { return m_kind; }
    QString activity() const { return m_activity; }
    QVariantMap counts() const {
        return {{"apps", m_package.apps.size()},
                {"depots", m_package.depots.size()},
                {"keys", m_package.keys.size()}};
    }
    bool applied() const { return m_applied; }
    bool ready() const { return m_ready; }
    bool busy() const { return m_busy; }
    bool downloading() const { return m_downloading; }
    int downloadPercent() const { return m_downloadPercent; }
    QString downloadStatus() const { return m_downloadStatus; }
    QString downloadDir() const { return m_downloadDir; }
    bool eosWarning() const { return m_eosWarning; }
    QStringList platforms() const { return m_platforms; }
    QString proton() const { return m_proton; }
    QVariantList depotDetails() const { return m_depotDetails; }
    QVariantList games() const { return m_games; }
    QString networkIssue() const { return m_networkIssue; }
    bool hasMore() const { return m_hasMore; }
    int searchOffset() const { return m_searchOffset; }
    Q_INVOKABLE void search(QString query, int offset = 0);
    Q_INVOKABLE void fetch(QString appId, QString name = QString());
    Q_INVOKABLE void inspect(QUrl file);
    Q_INVOKABLE void apply();
    Q_INVOKABLE void restore(int historyIndex);
    Q_INVOKABLE void downloadGame(QVariantList selectedDepots = QVariantList());
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE void openFolder(QString path);
signals:
    void changed();
    void packageLoaded();

private:
    QUrl m_nameApi;
    void begin(QString activity, QString status);
    void finish(QString status, QString kind = "info");
    void acceptPackage(const Package& package, const QString& error);
    void refreshHubcapOnAuthError(const QString& error);
    void runNextDepot(const QString& dotnet, const QString& dll, const QString& appId);
    AppSettings* m_settings;
    QVariantList m_games;
    QString m_networkIssue;
    bool m_hasMore = false;
    int m_searchOffset = 0;
    QVariantList m_installed;
    QString m_libraryError;
    QString m_hubcapKey;
    QVariantMap m_hubcapInfo;
    bool m_checkingHubcap = false;
    Package m_package;
    QString m_preview, m_source, m_appId, m_status = "Ready.", m_kind = "info", m_activity;
    bool m_ready = false, m_busy = false, m_applied = false;
    bool m_downloading = false;
    bool m_eosWarning = false;
    QStringList m_platforms;
    QString m_proton;
    QVariantList m_depotDetails;
    int m_downloadPercent = 0;
    int m_downloadToken = 0;
    QString m_downloadStatus, m_downloadDir;
    QProcess* m_downloadProcess = nullptr;
    QList<QPair<QString, QString>> m_downloadQueue;
    int m_downloadIndex = 0;
    QString m_keysFile, m_manifestsDir;
};
