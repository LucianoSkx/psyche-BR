#include "proton.h"
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <algorithm>
namespace {
const qint64 SUCCESS_EXPIRY = 7 * 24 * 3600;
const qint64 FAILURE_EXPIRY = 24 * 3600;
QString fetchTier(const QString& appId) {
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl("https://www.protondb.com/api/v1/reports/summaries/" + appId + ".json"));
    request.setRawHeader("User-Agent", "psyche/" PSYCHE_VERSION);
    auto reply = manager.get(request);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
        timedOut = true;
        reply->abort();
    });
    timer.start(10000);
    loop.exec();
    if (timedOut || reply->error() != QNetworkReply::NoError)
        return QString();
    auto doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject())
        return QString();
    auto tier = doc.object().value("tier").toString().toLower();
    return tier.isEmpty() ? QString() : tier;
}

int blockEnd(const QString& text, int open) {
    int depth = 0;
    bool quoted = false;
    for (int i = open; i < text.size(); ++i) {
        const auto c = text.at(i);
        if (quoted) {
            if (c == QLatin1Char('\\'))
                ++i;
            else if (c == QLatin1Char('"'))
                quoted = false;
        } else if (c == QLatin1Char('"')) {
            quoted = true;
        } else if (c == QLatin1Char('{')) {
            ++depth;
        } else if (c == QLatin1Char('}') && --depth == 0) {
            return i;
        }
    }
    return -1;
}

// VDF accepts keys without quotes; config.vdf uses them. Spaces/tabs separate.
QString mappedTool(const QString& block, const QString& appId) {
    const auto at = block.indexOf(QLatin1Char('"') + appId + QLatin1Char('"'));
    if (at < 0)
        return {};
    const auto open = block.indexOf(QLatin1Char('{'), at + appId.size() + 2);
    if (open < 0)
        return {};
    const auto end = blockEnd(block, open);
    if (end < 0)
        return {};
    auto match = QRegularExpression(R"re("name"\s*"([^"]+)")re").match(block.mid(open, end - open));
    return match.hasMatch() ? match.captured(1) : QString();
}

QString preferredTool(const QString& block) {
    QMap<QString, int> counts;
    auto matches = QRegularExpression("\"name\"\\s*\"([^\"]+)\"").globalMatch(block);
    while (matches.hasNext()) {
        const auto name = matches.next().captured(1);
        if (name != "Steam-Play-None" && !name.contains('"') && !name.contains('\\'))
            ++counts[name];
    }
    int best = 0;
    QString chosen;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        if (it.value() > best)
            best = it.value(), chosen = it.key();
    return chosen.isEmpty() ? QStringLiteral("proton_experimental") : chosen;
}
}
QVariantMap Proton::tiers(const QStringList& appIds, const QString& dataDir, int maxFetches) {
    const auto cachePath = dataDir + "/protondb.json";
    QJsonObject cache;
    if (QFile file(cachePath); file.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject())
            cache = doc.object();
    }
    QVariantMap result;
    QStringList stale;
    const auto now = QDateTime::currentSecsSinceEpoch();
    for (const auto& appId : appIds) {
        auto entry = cache.value(appId).toObject();
        const auto ts = qint64(entry.value("ts").toDouble());
        const bool ok = entry.value("ok").toBool(true);
        const auto age = now - ts;
        if (!entry.isEmpty() && (ok ? age < SUCCESS_EXPIRY : age < FAILURE_EXPIRY)) {
            if (ok) {
                auto tier = entry.value("tier").toString();
                if (!tier.isEmpty())
                    result[appId] = tier;
            }
            continue;
        }
        stale.append(appId);
    }
    int fetched = 0;
    for (const auto& appId : stale) {
        if (fetched >= maxFetches)
            break;
        ++fetched;
        auto tier = fetchTier(appId);
        QJsonObject entry;
        entry["ts"] = double(now);
        if (tier.isEmpty()) {
            entry["ok"] = false;
        } else {
            entry["ok"] = true;
            entry["tier"] = tier;
            result[appId] = tier;
        }
        cache[appId] = entry;
    }
    QDir().mkpath(dataDir);
    QSaveFile file(cachePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(cache).toJson());
        file.commit();
    }
    return result;
}

QString Proton::forceCompatTool(const QString& appId, const QString& steamDir, bool steamRunning,
                                const QString& tool) {
    for (const auto& digit : appId)
        if (!digit.isDigit())
            return "AppID inválido para definir o Proton.";
    static const QRegularExpression kToolName(R"(^[A-Za-z0-9][A-Za-z0-9 ._-]{0,127}$)");
    if (!tool.isEmpty() && !kToolName.match(tool).hasMatch())
        return "Nome de Proton inválido.";
    if (steamDir.isEmpty())
        return "Steam não encontrada para definir o Proton.";
    if (steamRunning)
        return "Feche a Steam e clique em Definir Proton.";

    const auto path = steamDir + "/config/config.vdf";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024)
        return "Não foi possível ler " + path;
    const auto text = QString::fromUtf8(file.readAll());
    file.close();

    const auto marker = QStringLiteral("\"CompatToolMapping\"");
    const auto at = text.indexOf(marker);
    const auto open = at < 0 ? -1 : text.indexOf(QLatin1Char('{'), at + marker.size());
    const auto close = open < 0 ? -1 : blockEnd(text, open);
    if (close < 0)
        return "CompatToolMapping não encontrado em " + path;
    const auto block = text.mid(open, close - open);
    const auto idKey = QLatin1Char('"') + appId + QLatin1Char('"');
    const auto existing = mappedTool(block, appId);
    if ((tool.isEmpty() && block.contains(idKey)) ||
        (!tool.isEmpty() && existing == tool))
        return {};
    const auto chosen = tool.isEmpty() ? preferredTool(block) : tool;

    auto updated = text;
    if (existing.isEmpty()) {
        updated.insert(open + 1,
                       "\n\t\t\t\t\t\"" + appId + "\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"name\"\t\t\"" +
                           chosen +
                           "\"\n\t\t\t\t\t\t\"config\"\t\t\"\"\n\t\t\t\t\t\t\"priority\"\t\t\"250\"\n"
                           "\t\t\t\t\t}");
    } else {
        const auto inner = text.mid(open, close - open);
        const auto idAt = inner.indexOf(idKey);
        const auto bOpen = inner.indexOf(QLatin1Char('{'), idAt + idKey.size());
        const auto bEnd = blockEnd(inner, bOpen);
        auto entry = inner.mid(bOpen, bEnd - bOpen);
        static const QRegularExpression kName(R"re("name"\s*"([^"]+)")re");
        if (kName.match(entry).hasMatch())
            entry.replace(kName, QStringLiteral("\"name\"\t\t\"") + chosen + "\"");
        else
            entry.insert(1, "\n\t\t\t\t\t\t\"name\"\t\t\"" + chosen + "\"\n");
        updated.replace(open + bOpen, bEnd - bOpen, entry);
    }

    const auto backup = path + ".bak";
    if (!QFile::exists(backup) && !QFile::copy(path, backup))
        return "Não foi possível fazer backup de " + path;
    const auto payload = updated.toUtf8();
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(payload) != payload.size() || !out.commit())
        return "Não foi possível gravar " + path;
    return {};
}

QVariantList Proton::installedTools(const QString& steamDir, const QStringList& libraries,
                                   const QString& appId) {
    // name -> label; the first source wins so names are not duplicated.
    QHash<QString, QString> labels;

    const auto dot = steamDir + "/compatibilitytools.d";
    for (const auto& dir :
         QDir(dot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QFile vdf(dot + "/" + dir + "/compatibilitytool.vdf");
        if (!vdf.open(QIODevice::ReadOnly))
            continue;
        const auto text = QString::fromUtf8(vdf.readAll());
        const auto marker = QStringLiteral("\"compat_tools\"");
        const auto at = text.indexOf(marker);
        const auto open = at < 0 ? -1 : text.indexOf(QLatin1Char('{'), at + marker.size());
        const auto end = open < 0 ? -1 : blockEnd(text, open);
        if (end < 0)
            continue;
        const auto block = text.mid(open + 1, end - open - 1);
        auto name = QRegularExpression(R"re("([^"]+)")re").match(block);
        if (!name.hasMatch())
            continue;
        auto display = QRegularExpression(R"re("display_name"\s*"([^"]+)")re").match(block);
        labels[name.captured(1)] = display.hasMatch() ? display.captured(1) : name.captured(1);
    }

    for (const auto& library : libraries) {
        const auto common = library + "/steamapps/common";
        for (const auto& dir :
             QDir(common).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            if (!dir.startsWith(QLatin1String("Proton ")))
                continue;
            QString name;
            if (dir == QLatin1String("Proton - Experimental"))
                name = "proton_experimental";
            else if (dir == QLatin1String("Proton Hotfix"))
                name = "proton_hotfix";
            else if (dir == QLatin1String("Proton Next"))
                name = "proton_next";
            else {
                auto version = QRegularExpression(R"(^Proton (\d+))").match(dir);
                if (version.hasMatch())
                    name = "proton_" + version.captured(1);
            }
            if (name.isEmpty() || labels.contains(name))
                continue;
            labels[name] = dir;
        }
    }

    QString current;
    if (!steamDir.isEmpty() && !appId.isEmpty()) {
        QFile cfg(steamDir + "/config/config.vdf");
        if (cfg.open(QIODevice::ReadOnly)) {
            const auto text = QString::fromUtf8(cfg.readAll());
            const auto marker = QStringLiteral("\"CompatToolMapping\"");
            const auto at = text.indexOf(marker);
            const auto open = at < 0 ? -1 : text.indexOf(QLatin1Char('{'), at + marker.size());
            const auto end = open < 0 ? -1 : blockEnd(text, open);
            if (end >= 0)
                current = mappedTool(text.mid(open, end - open), appId);
        }
    }

    QList<QPair<QString, QString>> sorted;
    sorted.reserve(labels.size());
    for (auto it = labels.constBegin(); it != labels.constEnd(); ++it)
        sorted.append({it.value(), it.key()});
    std::sort(sorted.begin(), sorted.end());
    QVariantList items;
    if (!current.isEmpty() && !labels.contains(current))
        items.append(QVariantMap{{"name", current}, {"label", current}, {"current", true}});
    for (const auto& [label, name] : sorted)
        items.append(QVariantMap{{"name", name}, {"label", label}, {"current", name == current}});
    return items;
}

QString Proton::steamRootFor(const QVariantList& steams, const QString& steamApps) {
    QString fallback;
    for (const auto& item : steams) {
        const auto path = item.toMap().value("path").toString();
        if (path.isEmpty() || path.contains("/.var/app/") ||
            !QFileInfo(path + "/config/config.vdf").isFile())
            continue;
        if (!steamApps.isEmpty() && steamApps.startsWith(path + "/"))
            return path;
        if (fallback.isEmpty())
            fallback = path;
    }
    return fallback;
}
