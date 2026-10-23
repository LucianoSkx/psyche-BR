#include "proton.h"
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

    // 6. appmanifest + depotcache para um jogo baixado.
    {
        const auto steamApps = temp.path() + "/lib/steamapps";
        const auto gameDir = steamApps + "/common/Jogo Teste";
        QDir().mkpath(gameDir + "/bin");
        writeFile(gameDir + "/bin/game", 300);
        writeFile(gameDir + "/readme.txt", 100);

        const auto manifests = temp.path() + "/manifests";
        QDir().mkpath(manifests);
        writeFile(manifests + "/228990_777.manifest", 8);

        const auto error = registerInstalledGame(
            steamApps, "620", "Jogo Teste", "Jogo Teste",
            {{"228990", "777"}, {"620", "888"}}, manifests, false);
        if (!error.isEmpty())
            qFatal("appmanifest: falhou: %s", qPrintable(error));
        const auto text = readAll(steamApps + "/appmanifest_620.acf");
        if (!contains(text, "\"AppState\"") || !contains(text, "\"appid\"\t\t\"620\""))
            qFatal("appmanifest: cabeçalho ausente:\n%s", qPrintable(text));
        if (!contains(text, "\"StateFlags\"\t\t\"4\""))
            qFatal("appmanifest: StateFlags não é 4:\n%s", qPrintable(text));
        if (!contains(text, "\"installdir\"\t\t\"Jogo Teste\""))
            qFatal("appmanifest: installdir errado:\n%s", qPrintable(text));
        if (!contains(text, "\"SizeOnDisk\"\t\t\"400\""))
            qFatal("appmanifest: SizeOnDisk não somou 400:\n%s", qPrintable(text));
        if (!contains(text, "\"228990\"\n\t\t{\n\t\t\t\"manifest\"\t\t\"777\"") ||
            !contains(text, "\"620\"\n\t\t{\n\t\t\t\"manifest\"\t\t\"888\""))
            qFatal("appmanifest: InstalledDepots incompleto:\n%s", qPrintable(text));
        if (!QFile::exists(steamApps + "/depotcache/228990_777.manifest"))
            qFatal("appmanifest: manifesto não foi para depotcache");
        if (QFile::exists(steamApps + "/depotcache/620_888.manifest"))
            qFatal("appmanifest: copiou manifesto inexistente");
    }

    // 7. Nome com aspas e barra invertida não pode quebrar o VDF.
    {
        const auto steamApps = temp.path() + "/lib2/steamapps";
        QDir().mkpath(steamApps + "/common/X");
        const auto error = registerInstalledGame(
            steamApps, "10", "Jogo \"X\" \\ Y", "X", {{"1", "2"}}, temp.path(), false);
        if (!error.isEmpty())
            qFatal("appmanifest: nome com aspas falhou: %s", qPrintable(error));
        const auto text = readAll(steamApps + "/appmanifest_10.acf");
        if (!contains(text, "\"name\"\t\t\"Jogo \\\"X\\\" \\\\ Y\""))
            qFatal("appmanifest: nome não escapado:\n%s", qPrintable(text));
    }

    // 8. Sem biblioteca Steam e AppID inválido: erro, sem gravar fora do lugar.
    {
        if (registerInstalledGame(QString(), "10", "x", "x", {}, temp.path(), false).isEmpty())
            qFatal("appmanifest: biblioteca vazia deveria falhar");
        if (registerInstalledGame(temp.path() + "/lib3/steamapps", "../10", "x", "x", {},
                                  temp.path(), false)
                .isEmpty())
            qFatal("appmanifest: AppID inválido deveria falhar");
        if (QFile::exists(temp.path() + "/lib3/steamapps/appmanifest_../10.acf"))
            qFatal("appmanifest: gravou com AppID inválido");
    }

    // 9. Depôts Windows baixados no Linux marcam platform_override para o Proton.
    {
        const auto steamApps = temp.path() + "/lib4/steamapps";
        QDir().mkpath(steamApps + "/common/Y");
        const auto error =
            registerInstalledGame(steamApps, "20", "Y", "Y", {{"3", "4"}}, temp.path(), true);
        if (!error.isEmpty())
            qFatal("appmanifest: platform_override falhou: %s", qPrintable(error));
        const auto text = readAll(steamApps + "/appmanifest_20.acf");
        const auto marker = QString("\"platform_override_dest\"\t\t\"linux\"");
        if (text.count(marker) != 2 ||
            text.count("\"platform_override_source\"\t\t\"windows\"") != 2)
            qFatal("appmanifest: platform_override ausente em UserConfig/MountedConfig:\n%s",
                   qPrintable(text));
    }

    // 10. Steam aberta: não mexe no config.vdf; fechada: insere o mapping uma vez.
    {
        const auto steamDir = temp.path() + "/steam";
        QDir().mkpath(steamDir + "/config");
        const auto configPath = steamDir + "/config/config.vdf";
        writeAcf(configPath,
                 R"("InstallConfigStore"
{
	"Software"
	{
		"Valve"
		{
			"Steam"
			{
				"CompatToolMapping"
				{
					"111"
					{
						"name"		"proton_9"
						"config"		""
						"priority"		"250"
					}
				}
			}
		}
	}
}
)");
        const auto before = readAll(configPath);
        if (Proton::forceCompatTool("620", steamDir, true).isEmpty())
            qFatal("proton: Steam aberta deveria avisar");
        if (readAll(configPath) != before)
            qFatal("proton: gravou com a Steam aberta");

        if (!Proton::forceCompatTool("620", steamDir, false).isEmpty())
            qFatal("proton: falhou com a Steam fechada");
        const auto text = readAll(configPath);
        if (text.count("\"620\"") != 1)
            qFatal("proton: mapping não inserido uma vez:\n%s", qPrintable(text));
        if (!contains(text, "\"111\"") || !contains(text, "\"proton_9\""))
            qFatal("proton: mapping existente foi perdido:\n%s", qPrintable(text));
        if (text.indexOf("\"620\"") < text.indexOf("\"CompatToolMapping\""))
            qFatal("proton: mapping fora do CompatToolMapping:\n%s", qPrintable(text));
        if (!contains(text,
                      "\"620\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"name\"\t\t\"proton_9\""))
            qFatal("proton: não herdou a ferramenta já usada:\n%s", qPrintable(text));
        if (text.count('{') != text.count('}'))
            qFatal("proton: config.vdf desbalanceado:\n%s", qPrintable(text));

        if (!Proton::forceCompatTool("620", steamDir, false).isEmpty())
            qFatal("proton: repetição deveria ser no-op");
        if (readAll(configPath).count("\"620\"") != 1)
            qFatal("proton: duplicou o mapping");
        if (Proton::forceCompatTool("620", steamDir + "/nao-existe", false).isEmpty())
            qFatal("proton: config.vdf ausente deveria falhar");
    }

    // 11. Raiz da Steam: Flatpak ignorada, Snap e nativa aceitas.
    {
        const auto base = temp.path() + "/roots";
        const auto native = base + "/.local/share/Steam";
        const auto flatpak = base + "/.var/app/com.valvesoftware.Steam/data/Steam";
        const auto snap = base + "/snap/steam/common/.local/share/Steam";
        for (const auto& root : {native, flatpak, snap}) {
            QDir().mkpath(root + "/config");
            writeAcf(root + "/config/config.vdf", "\"a\"\n{\n}\n");
        }
        const QVariantList steams{{QVariantMap{{"path", flatpak}}},
                                  {QVariantMap{{"path", native}}},
                                  {QVariantMap{{"path", snap}}}};
        if (Proton::steamRootFor(steams, native + "/steamapps") != native)
            qFatal("proton: raiz da biblioteca não foi escolhida");
        if (Proton::steamRootFor(steams, snap + "/steamapps") != snap)
            qFatal("proton: raiz Snap não foi escolhida");
        if (Proton::steamRootFor(steams, base + "/outra") != native)
            qFatal("proton: fallback deveria pular o Flatpak");
        const QVariantList onlyFlatpak{{QVariantMap{{"path", flatpak}}}};
        if (!Proton::steamRootFor(onlyFlatpak, QString()).isEmpty())
            qFatal("proton: Flatpak deveria ser ignorado");
    }

    return 0;
}