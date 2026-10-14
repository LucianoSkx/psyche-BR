#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>
namespace Proton {
QVariantMap tiers(const QStringList& appIds, const QString& dataDir, int maxFetches = 30);
}
