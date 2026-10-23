#include "permissions.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace {
void put(const QString& path, const QByteArray& data, bool executable = false) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size())
        qFatal("permissions: não gravou %s", qPrintable(path));
    file.close();
    if (executable)
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                        QFileDevice::ExeOwner);
}
bool executable(const QString& path) {
    return QFileInfo(path).permissions() & QFileDevice::ExeOwner;
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid())
        return 1;
    const auto root = temp.path() + "/jogo";
    QDir().mkpath(root + "/bin/sub");

    put(root + "/bin/elf", QByteArray("\x7f" "ELF", 4) + QByteArray(64, '\0'));
    put(root + "/bin/sub/script", QByteArray("#!/bin/sh\necho oi\n"));
    put(root + "/run.sh", QByteArray("#!/bin/sh\n"));
    put(root + "/readme.txt", QByteArray("texto"));
    put(root + "/blob", QByteArray(64, '\1'));
    put(root + "/bin/ja", QByteArray(64, '\0'), true);
    if (!QFile::link(root + "/bin/elf", root + "/bin/link"))
        qFatal("permissions: não criou o link simbólico");

    const auto changed = makeExecutable(root);
    if (changed != 3)
        qFatal("permissions: esperava 3 arquivos, veio %d", changed);
    for (const auto& path : {"bin/elf", "bin/sub/script", "run.sh"})
        if (!executable(root + "/" + path))
            qFatal("permissions: %s ficou sem +x", path);
    for (const auto& path : {"readme.txt", "blob"})
        if (executable(root + "/" + path))
            qFatal("permissions: %s não deveria ser executável", path);
    if (!QFileInfo(root + "/bin/link").isSymLink())
        qFatal("permissions: link virou arquivo comum");
    if (makeExecutable(root) != 0)
        qFatal("permissions: segunda passada deveria ser no-op");
    if (makeExecutable(temp.path() + "/nao-existe") != 0)
        qFatal("permissions: diretório inexistente deveria dar 0");
    return 0;
}
