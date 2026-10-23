#pragma once
#include <QList>
#include <QPair>
#include <QString>

QString registerWorkshopItem(const QString& acfPath,
                             const QString& appId,
                             const QString& workshopId,
                             const QString& manifestId,
                             const QString& modDir);

/**
 * Writes `steamapps/appmanifest_<appid>.acf` and copies the manifests used to
 * `steamapps/depotcache/`, so Steam lists the game as installed. An empty
 * `steamAppsDir` is an error: without a Steam library there is nowhere to
 * register. `windowsDepots` marks the ACF as a Linux install of Windows
 * depots (Proton).
 */
QString registerInstalledGame(const QString& steamAppsDir,
                              const QString& appId,
                              const QString& name,
                              const QString& installDir,
                              const QList<QPair<QString, QString>>& depots,
                              const QString& manifestDir,
                              bool windowsDepots);
