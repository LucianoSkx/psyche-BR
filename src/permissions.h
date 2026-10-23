#pragma once
#include <QString>

/**
 * Marks the Linux binaries inside `dir` as executable (ELF, shebang and known
 * extensions). DepotDownloader writes everything as 0644 and Steam does not
 * keep permissions in the manifest, so the game does not start without this.
 * Returns how many files changed.
 */
int makeExecutable(const QString& dir);
