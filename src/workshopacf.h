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
 * Grava `steamapps/appmanifest_<appid>.acf` e copia os manifestos usados para
 * `steamapps/depotcache/`, para a Steam listar o jogo como instalado.
 * `steamAppsDir` vazio devolve erro: sem biblioteca Steam não há onde registrar.
 * `windowsDepots` marca o ACF como instalação Linux de depôts Windows (Proton).
 */
QString registerInstalledGame(const QString& steamAppsDir,
                              const QString& appId,
                              const QString& name,
                              const QString& installDir,
                              const QList<QPair<QString, QString>>& depots,
                              const QString& manifestDir,
                              bool windowsDepots);
