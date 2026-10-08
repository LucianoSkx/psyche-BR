#include "workshopacf.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVariantMap>
#include <QDebug>

namespace {
const char* kExistingAcf = R"("AppWorkshop"
{
	"appid"		"730"
	"WorkshopPreviewType"		"0"
	"NeedsUpdate"		"0"
	"NeedsDownload"		"0"
	"WorkshopItemsInstalled"
	{
		"111"
		{
			"size"		"2048"
			"timeupdated"		"1700000000"
			"manifest"		"999"
		}
	}
	"WorkshopItemDetails"
	{
		"111"
		{
			"manifest"		"999"
			"timeupdated"		"1700000000"
			"timetouched"		"1700000000"
			"subscribedby"		"777"
		}
	}
}
)";

bool contains(const QString& haystack, const QString& needle) {
    return haystack.contains(needle);
}

QString readAll(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const auto text = QString::fromUtf8(f.readAll());
    f.close();
    return text;
}

void writeAcf(const QString& path, const QString& content) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        qFatal("teste: não gravou %s", qPrintable(path));
    f.write(content.toUtf8());
    f.close();
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid())
        return 1;

    // Cria um diretório de mod com conteúdo para o cálculo de tamanho.
    const auto modDir = temp.path() + "/content/730/222";
    QDir().mkpath(modDir + "/sub");
    auto writeFile = [](const QString& path, int bytes) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("teste: não criou %s", qPrintable(path));
        f.resize(bytes);
        f.close();
    };
    writeFile(modDir + "/a.bin", 100);
    writeFile(modDir + "/sub/b.bin", 50);

    const auto acfPath = temp.path() + "/appworkshop_730.acf";

    // 1. Arquivo novo: cria do zero com o item registration.
    {
        const auto error = registerWorkshopItem(acfPath, "730", "222", "1234567", modDir);
        if (!error.isEmpty())
            qFatal("acf: criação falhou: %s", qPrintable(error));
        const auto text = readAll(acfPath);
        if (!contains(text, "\"AppWorkshop\"") || !contains(text, "\"appid\"\t\t\"730\""))
            qFatal("acf: cabeçalho AppWorkshop/appid ausente:\n%s", qPrintable(text));
        if (!contains(text, "\"222\""))
            qFatal("acf: item 222 não gravado:\n%s", qPrintable(text));
        if (!contains(text, "\"size\"\t\t\"150\""))
            qFatal("acf: tamanho (100+50) não somado:\n%s", qPrintable(text));
        if (!contains(text, "\"manifest\"\t\t\"1234567\""))
            qFatal("acf: manifest do Hubcap ausente:\n%s", qPrintable(text));
        if (!contains(text, "\"NeedsUpdate\"\t\t\"0\"") ||
            !contains(text, "\"NeedsDownload\"\t\t\"0\""))
            qFatal("acf: NeedsUpdate/NeedsDownload não zerados:\n%s", qPrintable(text));
    }

    // 2. Item existente no arquivo não pode sumir, e o novo deve ser mesclado.
    writeAcf(acfPath, kExistingAcf);
    {
        const auto error = registerWorkshopItem(acfPath, "730", "222", "5555", modDir);
        if (!error.isEmpty())
            qFatal("acf: mesclagem falhou: %s", qPrintable(error));
        const auto text = readAll(acfPath);
        if (!contains(text, "\"111\""))
            qFatal("acf: item preexistente 111 foi perdido:\n%s", qPrintable(text));
        if (!contains(text, "\"222\"") || !contains(text, "\"5555\""))
            qFatal("acf: item novo 222 não foi mesclado:\n%s", qPrintable(text));
        if (!contains(text, "\"manifest\"\t\t\"999\""))
            qFatal("acf: manifest do item 111 alterado:\n%s", qPrintable(text));
        if (!contains(text, "\"subscribedby\"\t\t\"777\""))
            qFatal("acf: subscribedby do item 111 alterado:\n%s", qPrintable(text));
        if (!contains(text, "\"WorkshopPreviewType\"\t\t\"0\""))
            qFatal("acf: metadado de raiz perdido:\n%s", qPrintable(text));
    }

    // 3. Sem manifest do Hubcap, preserva o que já estava gravado para o item.
    writeAcf(acfPath, kExistingAcf);
    {
        const auto error = registerWorkshopItem(acfPath, "730", "111", QString(), modDir);
        if (!error.isEmpty())
            qFatal("acf: registro sem manifest falhou: %s", qPrintable(error));
        const auto text = readAll(acfPath);
        if (!contains(text, "\"manifest\"\t\t\"999\""))
            qFatal("acf: manifest existente foi apagado sem o Hubcap:\n%s", qPrintable(text));
    }

    // 4. Arquivo vazio/corrompido não pode travar: recria do zero.
    writeAcf(acfPath, "isto não é um ACF {{{");
    {
        const auto error = registerWorkshopItem(acfPath, "730", "222", "1", modDir);
        if (!error.isEmpty())
            qFatal("acf: arquivo corrompido deveria ser tolerado: %s", qPrintable(error));
        if (!contains(readAll(acfPath), "\"222\""))
            qFatal("acf: não recuperou de arquivo corrompido");
    }

    // 5. Diretório inexistente: tamanho zero, sem crash.
    {
        const auto error = registerWorkshopItem(temp.path() + "/novo/appworkshop_1.acf",
                                               "1", "222", "42",
                                               temp.path() + "/nao-existe");
        if (!error.isEmpty())
            qFatal("acf: diretório inexistente deveria ser tolerado: %s", qPrintable(error));
        if (!contains(readAll(temp.path() + "/novo/appworkshop_1.acf"), "\"size\"\t\t\"0\""))
            qFatal("acf: diretório inexistente deveria dar size 0");
    }

    return 0;
}