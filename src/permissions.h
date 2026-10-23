#pragma once
#include <QString>

/**
 * Marca como executáveis os binários Linux dentro de `dir` (ELF, shebang e
 * extensões conhecidas). O DepotDownloader grava tudo como 0644 e a Steam não
 * guarda permissão no manifesto, então o jogo não abre sem isso.
 * Devolve quantos arquivos mudaram.
 */
int makeExecutable(const QString& dir);
