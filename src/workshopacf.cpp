#include "workshopacf.h"
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QVariantMap>

namespace {

// Um ACF é texto VDF: pares "chave" "valor" e blocos "chave" { ... }.
// Blocos viram QVariantMap; valores escalares viram QString.
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

} // namespace

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
    // Sem manifest do Hubcap, preserva o que já estava gravado para o item.
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