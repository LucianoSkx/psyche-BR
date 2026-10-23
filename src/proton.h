#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
namespace Proton {
QVariantMap tiers(const QStringList& appIds, const QString& dataDir, int maxFetches = 30);

/**
 * Registra `appId` em `CompatToolMapping` no `config.vdf` da Steam para rodar
 * via Proton. Com `tool` vazio escolhe a mais usada (fallback
 * `proton_experimental`) e não mexe se já houver mapeamento; com `tool`
 * definido grava essa ferramenta, mesmo sobrescrevendo. Não grava com a Steam
 * aberta: ela reescreve o arquivo ao sair. Devolve uma mensagem de erro/aviso,
 * ou vazio quando nada precisou ser feito.
 */
QString forceCompatTool(const QString& appId, const QString& steamDir, bool steamRunning,
                        const QString& tool = {});

/**
 * Lista as compat tools instaladas: as de `compatibilitytools.d` e os Protons
 * oficiais em `steamapps/common/Proton*` das bibliotecas. Cada item traz
 * `name`, `label` e `current` (mapeamento atual do `appId`). Ferramentas
 * mapeadas mas sem instalação local aparecem no começo da lista.
 */
QVariantList installedTools(const QString& steamDir, const QStringList& libraries,
                            const QString& appId = {});

/**
 * Escolhe a raiz da Steam (nativa ou Snap) dona da biblioteca; Flatpak fica de
 * fora. Devolve a primeira com `config/config.vdf` quando nenhuma casa.
 */
QString steamRootFor(const QVariantList& steams, const QString& steamApps);
}
