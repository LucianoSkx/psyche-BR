#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
namespace Proton {
QVariantMap tiers(const QStringList& appIds, const QString& dataDir, int maxFetches = 30);

/**
 * Registra `appId` em `CompatToolMapping` no `config.vdf` da Steam para rodar
 * via Proton. Não grava com a Steam aberta: ela reescreve o arquivo ao sair.
 * Devolve uma mensagem de erro/aviso, ou vazio quando nada precisou ser feito.
 */
QString forceCompatTool(const QString& appId, const QString& steamDir, bool steamRunning);

/**
 * Escolhe a raiz da Steam (nativa ou Snap) dona da biblioteca; Flatpak fica de
 * fora. Devolve a primeira com `config/config.vdf` quando nenhuma casa.
 */
QString steamRootFor(const QVariantList& steams, const QString& steamApps);
}
