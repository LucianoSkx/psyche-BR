#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>
namespace Proton {
// appId -> tier em minúsculas (platinum, gold, silver, bronze, borked, native, pending).
// Falhas de rede omitem o appId. Cache em JSON no diretório de dados.
QVariantMap tiers(const QStringList& appIds, const QString& dataDir, int maxFetches = 30);
}
