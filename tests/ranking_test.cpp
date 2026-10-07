#include "ranking.h"
#include <QCoreApplication>
#include <QDebug>
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QVariantList games;
    auto add = [&](QString name) {
        QVariantMap m;
        m["appId"] = name;
        m["name"] = name;
        games.append(m);
    };
    add("Lost in Tandem");
    add("Lost in Tandem - Digital Soundtrack");
    add("Lost Tandem");
    add("The Lost City");
    add("Lost in Tandem Demo");
    auto ranked = Ranking::rankGames(games, "lost in tandem");
    if (ranked.isEmpty() || ranked.first().toMap()["name"] != "Lost in Tandem") {
        qFatal("ranking: exact match must be first, got %s",
                qPrintable(ranked.isEmpty() ? QString() : ranked.first().toMap()["name"].toString()));
    }
    if (ranked.last().toMap()["name"] != "Lost in Tandem - Digital Soundtrack" &&
        ranked.last().toMap()["name"] != "Lost in Tandem Demo") {
        qFatal("ranking: blacklisted entries must sink");
    }
    QVariantList dupes;
    {
        QVariantMap a; a["name"] = "Hades"; dupes.append(a);
        QVariantMap b; b["name"] = "hades"; dupes.append(b);
    }
    if (Ranking::rankGames(dupes, "hades").size() != 1)
        qFatal("ranking: duplicates by normalized name must collapse");
    return 0;
}
