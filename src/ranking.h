#pragma once
#include <QString>
#include <QVariantList>
namespace Ranking {
// Ordena por relevância com a consulta, rebaixa nomes na blacklist e remove duplicatas por nome normalizado.
QVariantList rankGames(const QVariantList& games, const QString& query);
}
