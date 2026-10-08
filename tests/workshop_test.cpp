#include "catalog.h"
#include <QCoreApplication>
#include <QDebug>

static void check(const QString& label, const QString& input, const QStringList& expected) {
    const auto got = Catalog::parseWorkshopIds(input);
    if (got != expected)
        qFatal("workshop: %s -> %s, esperado %s", qPrintable(label),
               qPrintable(got.join(',')),
               qPrintable(expected.join(',')));
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    check("URL", "https://steamcommunity.com/sharedfiles/filedetails/?id=123456", {"123456"});
    check("URL com query", "https://ex.org/x?id=42&a=1", {"42"});
    check("ID solto", "123456", {"123456"});
    check("vários", "111 222\n333", {"111", "222", "333"});
    check("separadores", "111, 222;333", {"111", "222", "333"});
    check("duplicatas", "111 222 111", {"111", "222"});
    check("lixo", "abc 111 def", {"111"});
    check("vazio", "  \n ", {});
    bool threw = false;
    try {
        Catalog("").fetchWorkshopAppId("0");
    } catch (const std::exception&) {
        threw = true;
    }
    if (!threw)
        qFatal("workshop: id invalido deve lancar");
    return 0;
}