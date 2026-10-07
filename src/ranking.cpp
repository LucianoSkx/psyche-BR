#include "ranking.h"
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
namespace {
QString normalize(const QString& value) {
    auto lowered = value.trimmed().toLower();
    lowered.replace(QRegularExpression("[^a-z0-9]+"), " ");
    return lowered.simplified();
}
QRegularExpression blacklist() {
    static const QStringList patterns = {
        "soundtracks?", "sound tracks?", "ost", "original soundtrack", "piano collections?",
        "orchestras?", "orchestral", "world tour", "concerts?", "videos?", "artbooks?",
        "graphic novels?", "dlcs?", "demos?", "dedicated server", "servers?", "tools?", "sdks?",
        "wallpapers?", "digital contents?", "mod organizer", "ultimate collections?",
        "season pass(?:es)?", "content packs?", "free editions?", "upgrades?",
        "trailers?", "shorts?", "teasers?", "benchmarks?", "trial versions?", "beta(?:\\s+test)?",
    };
    return QRegularExpression("\\b(?:" + patterns.join('|') + ")\\b",
                              QRegularExpression::CaseInsensitiveOption);
}
int score(const QString& name, const QString& query) {
    auto normalizedName = normalize(name);
    auto normalizedQuery = normalize(query);
    if (normalizedName.isEmpty() || normalizedQuery.isEmpty())
        return 0;
    int result = 0;
    if (normalizedName == normalizedQuery)
        result += 5000;
    if (normalizedName.startsWith(normalizedQuery))
        result += 2500;
    if ((" " + normalizedName + " ").contains(" " + normalizedQuery + " "))
        result += 1500;
    const auto nameTokens = normalizedName.split(' ');
    for (const auto& token : normalizedQuery.split(' ')) {
        if (nameTokens.contains(token))
            result += 350;
        else {
            for (const auto& part : nameTokens)
                if (part.startsWith(token)) {
                    result += 120;
                    break;
                }
        }
    }
    result -= qMin(700, qMax(0, (int)(normalizedName.size() - normalizedQuery.size())) * 5);
    if (blacklist().match(name).hasMatch())
        result -= 10000;
    return result;
}
} // namespace
QVariantList Ranking::rankGames(const QVariantList& games, const QString& query) {
    QList<QPair<int, int>> indexScore;
    for (int i = 0; i < games.size(); ++i)
        indexScore.append({score(games[i].toMap().value("name").toString(), query), i});
    std::sort(indexScore.begin(), indexScore.end(), [&games](const auto& a, const auto& b) {
        if (a.first != b.first)
            return a.first > b.first;
        const auto na = games[a.second].toMap().value("name").toString().toLower();
        const auto nb = games[b.second].toMap().value("name").toString().toLower();
        return na < nb;
    });
    QVariantList ordered;
    QSet<QString> seen;
    for (const auto& entry : indexScore) {
        auto name = normalize(games[entry.second].toMap().value("name").toString());
        if (name.isEmpty() || seen.contains(name))
            continue;
        seen.insert(name);
        ordered.append(games[entry.second]);
    }
    return ordered;
}
