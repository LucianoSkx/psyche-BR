#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
namespace Proton {
QVariantMap tiers(const QStringList& appIds, const QString& dataDir, int maxFetches = 30);

/**
 * Registers `appId` in Steam's `config.vdf` `CompatToolMapping` so it runs
 * through Proton. With an empty `tool` picks the most used one (fallback
 * `proton_experimental`) and does nothing when a mapping already exists; with
 * a tool set writes it, overwriting the current one. Will not write while
 * Steam is running: it rewrites the file on quit. Returns an error/warning
 * message, or empty when nothing was needed.
 */
QString forceCompatTool(const QString& appId, const QString& steamDir, bool steamRunning,
                        const QString& tool = {});

/**
 * Lists installed compat tools: the ones under `compatibilitytools.d` plus the
 * official Protons (`steamapps/common/Proton*`) from Steam libraries. Each
 * item carries `name`, `label` and `current` (the current mapping for
 * `appId`). Tools mapped but not installed locally come first in the list.
 */
QVariantList installedTools(const QString& steamDir, const QStringList& libraries,
                            const QString& appId = {});

/**
 * Picks the Steam root (native or Snap) that owns the library; Flatpak is
 * out of scope. Returns the first root with `config/config.vdf` when none
 * match.
 */
QString steamRootFor(const QVariantList& steams, const QString& steamApps);
}
