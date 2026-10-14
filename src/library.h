#pragma once
#include "package.h"
#include <QVariantList>
QVariantList installedGames(const QString& directory);
QString removeInstalledGame(const QString& directory, const QString& gameId);
QString applyConfiguration(const Package& package, const QString& directory);

void restoreBackupContents(const QString& directory,
                           const QString& backup,
                           bool recoverLegacy = false);
