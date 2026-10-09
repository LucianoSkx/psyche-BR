#include "backend.h"
#include <QCoreApplication>
#include <QDebug>

static int failures = 0;

static void check(bool ok, const char* label) {
    if (!ok) {
        ++failures;
        qWarning("download_queue: FALHOU: %s", label);
    }
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const QMap<QString, QString> keys{{"10", "aa"}, {"20", "bb"}, {"30", "cc"}};
    const QMap<QString, QString> manifests{{"10", "m10"}, {"30", "m30"}};

    // wanted vazio = todos; 20 sem manifesto vai para missing.
    auto all = Backend::splitDownloadQueue(keys, {}, manifests);
    check(all.first.size() == 2, "todos: fila com 2");
    check(all.second == QStringList{"20"}, "todos: missing com 20");

    // Filtro por seleção: 30 fora da seleção nem aparece.
    auto sel = Backend::splitDownloadQueue(keys, {"10", "20"}, manifests);
    check(sel.first.size() == 1 && sel.first.first().first == "10", "selecao: fila so com 10");
    check(sel.second == QStringList{"20"}, "selecao: missing so com 20");

    // Tudo com manifesto: sem aviso.
    auto full = Backend::splitDownloadQueue(keys, {}, {{"10", "m10"}, {"20", "m20"}, {"30", "m30"}});
    check(full.first.size() == 3, "completo: fila com 3");
    check(full.second.isEmpty(), "completo: sem missing");

    // Nada com manifesto: fila vazia, todos em missing.
    auto none = Backend::splitDownloadQueue(keys, {}, {});
    check(none.first.isEmpty(), "vazio: fila vazia");
    check(none.second.size() == 3, "vazio: 3 em missing");

    if (failures)
        qFatal("download_queue: %d falhas", failures);
    return 0;
}
