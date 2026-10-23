#include "workshopacf.h"
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QVariantMap>

namespace {

struct Parser {
    const QString& text;
    int pos = 0;

    void skip() {
        while (pos < text.size()) {
            const auto c = text.at(pos);
            if (c.isSpace()) {
                ++pos;
            } else if (c == QLatin1Char('/') && pos + 1 < text.size() &&
                       text.at(pos + 1) == QLatin1Char('/')) {
                while (pos < text.size() && text.at(pos) != QLatin1Char('\n'))
                    ++pos;
            } else if (c == QLatin1Char('/') && pos + 1 < text.size() &&
                       text.at(pos + 1) == QLatin1Char('*')) {
                pos += 2;
                while (pos + 1 < text.size() &&
                       !(text.at(pos) == QLatin1Char('*') && text.at(pos + 1) == QLatin1Char('/')))
                    ++pos;
                pos = qMin(pos + 2, text.size());
            } else {
                break;
            }
        }
    }

    bool atEnd() {
        skip();
        return pos >= text.size();
    }

    QString readToken() {
        skip();
        if (pos >= text.size())
            return {};
        if (text.at(pos) == QLatin1Char('"')) {
            const auto end = text.indexOf(QLatin1Char('"'), pos + 1);
            if (end < 0) {
                const auto rest = text.mid(pos + 1);
                pos = text.size();
                return rest;
            }
            const auto value = text.mid(pos + 1, end - pos - 1);
            pos = end + 1;
            return value;
        }
        auto end = pos;
        while (end < text.size() && !text.at(end).isSpace() && text.at(end) != QLatin1Char('{'))
            ++end;
        const auto value = text.mid(pos, end - pos);
        pos = end;
        return value;
    }

    QVariantMap readBlock() {
        QVariantMap block;
        while (!atEnd()) {
            if (text.at(pos) == QLatin1Char('}')) {
                ++pos;
                return block;
            }
            if (text.at(pos) == QLatin1Char('{')) {
                ++pos;
                continue;
            }
            const auto key = readToken();
            if (key.isEmpty()) {
                ++pos;
                continue;
            }
            skip();
            if (pos < text.size() && text.at(pos) == QLatin1Char('{')) {
                ++pos;
                block.insert(key, readBlock());
            } else {
                block.insert(key, readToken());
            }
        }
        return block;
    }
};

QString quote(const QString& value) {
    auto escaped = value;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    escaped.replace(QLatin1Char('"'), QLatin1String("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

qint64 dirSize(const QString& path) {
    qint64 total = 0;
    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

}

QString registerWorkshopItem(const QString& acfPath,
                             const QString& appId,
                             const QString& workshopId,
                             const QString& manifestId,
                             const QString& modDir) {
    QVariantMap root;
    QFile acf(acfPath);
    if (acf.open(QIODevice::ReadOnly)) {
        const auto text = QString::fromUtf8(acf.readAll());
        acf.close();
        if (!text.trimmed().isEmpty())
            root = Parser{text}.readBlock();
    }

    const auto previous = root.value("AppWorkshop").toMap();
    auto installed = previous.value("WorkshopItemsInstalled").toMap();
    auto details = previous.value("WorkshopItemDetails").toMap();

    const auto now = QString::number(QDateTime::currentSecsSinceEpoch());
    const auto size = QString::number(dirSize(modDir));

    const auto oldInstalled = installed.value(workshopId).toMap();
    const auto oldDetail = details.value(workshopId).toMap();
    const auto manifest = !manifestId.isEmpty()
                              ? manifestId
                              : oldInstalled.value("manifest").toString();

    installed.insert(workshopId,
                     QVariantMap{{"size", size},
                                 {"timeupdated", now},
                                 {"manifest", manifest}});

    auto detail = oldDetail;
    detail.insert("manifest", manifest);
    detail.insert("timeupdated", now);
    if (detail.value("timetouched").toString().isEmpty())
        detail.insert("timetouched", now);
    if (detail.value("subscribedby").toString().isEmpty())
        detail.insert("subscribedby", now);
    detail.insert("latest_timeupdated", now);
    detail.insert("latest_manifest", manifest);
    details.insert(workshopId, detail);

    static const QStringList rootManaged{"appid", "NeedsUpdate", "NeedsDownload",
                                         "WorkshopItemsInstalled", "WorkshopItemDetails"};
    static const QStringList installedKeys{"size", "timeupdated", "manifest"};
    static const QStringList detailKeys{"manifest", "timeupdated", "timetouched", "subscribedby",
                                        "latest_timeupdated", "latest_manifest"};

    QString out;
    out += "\"AppWorkshop\"\n{\n";
    out += "\t\"appid\"\t\t" + quote(appId) + "\n";
    for (auto it = previous.constBegin(); it != previous.constEnd(); ++it) {
        if (rootManaged.contains(it.key()) || it.value().typeId() != QMetaType::QString)
            continue;
        out += "\t" + quote(it.key()) + "\t\t" + quote(it.value().toString()) + "\n";
    }
    out += "\t\"NeedsUpdate\"\t\t\"0\"\n";
    out += "\t\"NeedsDownload\"\t\t\"0\"\n";

    const auto writeBlock = [&out](const QString& name,
                                   const QVariantMap& block,
                                   const QStringList& allowed) {
        out += "\t" + quote(name) + "\n\t{\n";
        for (auto it = block.constBegin(); it != block.constEnd(); ++it) {
            out += "\t\t" + quote(it.key()) + "\n\t\t{\n";
            const auto entry = it.value().toMap();
            for (const auto& key : allowed)
                if (entry.contains(key))
                    out += "\t\t\t" + quote(key) + "\t\t" + quote(entry.value(key).toString()) + "\n";
            out += "\t\t}\n";
        }
        out += "\t}\n";
    };
    writeBlock("WorkshopItemsInstalled", installed, installedKeys);
    writeBlock("WorkshopItemDetails", details, detailKeys);
    out += "}\n";

    const auto payload = out.toUtf8();
    QDir().mkpath(QFileInfo(acfPath).absolutePath());
    QSaveFile file(acfPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size())
        return "Não foi possível gravar " + acfPath;
    if (!file.commit())
        return "Falha ao salvar " + acfPath;
    return {};
}

QString registerInstalledGame(const QString& steamAppsDir,
                              const QString& appId,
                              const QString& name,
                              const QString& installDir,
                              const QList<QPair<QString, QString>>& depots,
                              const QString& manifestDir,
                              bool windowsDepots) {
    if (steamAppsDir.isEmpty())
        return "Selecione uma biblioteca Steam para registrar o jogo.";
    for (const auto& c : appId)
        if (!c.isDigit())
            return "AppID inválido.";
    if (appId.isEmpty() || installDir.isEmpty() || installDir.contains('/') || installDir == "..")
        return "Nome de instalação inválido.";

    const auto root = QDir::cleanPath(steamAppsDir);
    const auto gameDir = root + "/common/" + installDir;
    const auto now = QString::number(QDateTime::currentSecsSinceEpoch());

    // Steam only recognizes a full install with StateFlags 4 and the manifest
    // gids in InstalledDepots; without them the game stays invisible in the library.
    QString out;
    out += "\"AppState\"\n{\n";
    out += "\t\"appid\"\t\t" + quote(appId) + "\n";
    out += "\t\"Universe\"\t\t\"1\"\n";
    out += "\t\"name\"\t\t" + quote(name) + "\n";
    out += "\t\"StateFlags\"\t\t\"4\"\n";
    out += "\t\"installdir\"\t\t" + quote(installDir) + "\n";
    out += "\t\"LastUpdated\"\t\t" + quote(now) + "\n";
    out += "\t\"LastPlayed\"\t\t\"0\"\n";
    out += "\t\"SizeOnDisk\"\t\t\"" + QString::number(dirSize(gameDir)) + "\"\n";
    out += "\t\"StagingSize\"\t\t\"0\"\n";
    // shortcut: buildid 0 and per-depot size 0 (Hubcap does not provide them);
    // Steam only offers an update when the manifest gids diverge.
    out += "\t\"buildid\"\t\t\"0\"\n";
    out += "\t\"LastOwner\"\t\t\"0\"\n";
    out += "\t\"DownloadType\"\t\t\"1\"\n";
    out += "\t\"UpdateResult\"\t\t\"0\"\n";
    out += "\t\"BytesToDownload\"\t\t\"0\"\n";
    out += "\t\"BytesDownloaded\"\t\t\"0\"\n";
    out += "\t\"BytesToStage\"\t\t\"0\"\n";
    out += "\t\"BytesStaged\"\t\t\"0\"\n";
    out += "\t\"TargetBuildID\"\t\t\"0\"\n";
    out += "\t\"AutoUpdateBehavior\"\t\t\"0\"\n";
    out += "\t\"AllowOtherDownloadsWhileRunning\"\t\t\"0\"\n";
    out += "\t\"ScheduledAutoUpdate\"\t\t\"0\"\n";
    out += "\t\"InstalledDepots\"\n\t{\n";
    for (const auto& depot : depots) {
        out += "\t\t" + quote(depot.first) + "\n\t\t{\n";
        out += "\t\t\t\"manifest\"\t\t" + quote(depot.second) + "\n";
        out += "\t\t\t\"size\"\t\t\"0\"\n";
        out += "\t\t}\n";
    }
    out += "\t}\n";
    out += "\t\"UserConfig\"\n\t{\n";
    if (windowsDepots) {
        out += "\t\t\"platform_override_dest\"\t\t\"linux\"\n";
        out += "\t\t\"platform_override_source\"\t\t\"windows\"\n";
    }
    out += "\t}\n";
    out += "\t\"MountedConfig\"\n\t{\n";
    if (windowsDepots) {
        out += "\t\t\"platform_override_dest\"\t\t\"linux\"\n";
        out += "\t\t\"platform_override_source\"\t\t\"windows\"\n";
    }
    out += "\t}\n";
    out += "}\n";

    const auto acfPath = root + "/appmanifest_" + appId + ".acf";
    const auto payload = out.toUtf8();
    QDir().mkpath(root);
    QSaveFile file(acfPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size() || !file.commit())
        return "Não foi possível gravar " + acfPath;

    const auto cacheDir = root + "/depotcache";
    QDir().mkpath(cacheDir);
    QStringList uncopied;
    for (const auto& depot : depots) {
        const auto fileName = depot.first + "_" + depot.second + ".manifest";
        if (depot.second.isEmpty() || QFile::exists(cacheDir + "/" + fileName))
            continue;
        if (!QFile::copy(manifestDir + "/" + fileName, cacheDir + "/" + fileName) &&
            QFile::exists(manifestDir + "/" + fileName))
            uncopied.append(fileName);
    }
    return uncopied.isEmpty() ? QString()
                              : "Manifestos não copiados para depotcache: " + uncopied.join(", ");
}
