#pragma once
#include <QString>

// Registra um item do Workshop no appworkshop_<appid>.acf da Steam, mesclando
// com os itens que já estavam no arquivo. Devolve QString vazia em sucesso ou
// a mensagem de erro.
QString registerWorkshopItem(const QString& acfPath,
                             const QString& appId,
                             const QString& workshopId,
                             const QString& manifestId,
                             const QString& modDir);