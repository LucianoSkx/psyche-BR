#include "permissions.h"
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace {
bool looksExecutable(const QString& path, const QString& name) {
    static const QStringList extensions{".sh", ".bash", ".x86",  ".x86_64",
                                        ".bin", ".run", ".elf", ".pck"};
    for (const auto& extension : extensions)
        if (name.endsWith(extension, Qt::CaseInsensitive))
            return true;
    if (name.contains('.'))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const auto header = file.read(4);
    return header.startsWith("\x7f" "ELF") || header.startsWith("#!");
}
}

int makeExecutable(const QString& dir) {
    if (dir.isEmpty() || !QFileInfo(dir).isDir())
        return 0;
    int changed = 0;
    QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const auto info = it.fileInfo();
        if (info.isSymLink() || (info.permissions() & QFileDevice::ExeOwner))
            continue;
        if (!looksExecutable(info.filePath(), info.fileName()))
            continue;
        auto permissions = info.permissions();
        permissions |= QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther;
        if (QFile::setPermissions(info.filePath(), permissions))
            ++changed;
    }
    return changed;
}
