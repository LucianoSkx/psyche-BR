#include "artwork.h"
#include "backend.h"
#include "cli.h"
#include "settings.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QtGlobal>
#include <QIcon>
#include <QLibrary>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <fcntl.h>
#include <csignal>
#include <execinfo.h>
#include <unistd.h>
namespace {
void dumpCrash(int signal) {
    const auto base = qEnvironmentVariableIsSet("PSYCHE_DATA_DIR")
        ? QString::fromLocal8Bit(qgetenv("PSYCHE_DATA_DIR"))
        : QDir::homePath() + "/.local/share/psyche";
    const int fd = ::open((base + "/crash.log").toLocal8Bit().constData(),
                          O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        void* frames[32];
        const auto count = backtrace(frames, 32);
        constexpr char header[] = "psyche crash\\n";
        (void)!::write(fd, header, sizeof(header) - 1);
        backtrace_symbols_fd(frames, count, fd);
        ::close(fd);
    }
    ::_exit(signal == SIGSEGV ? 11 : signal);
}
void redirectWarnings(QtMsgType type, const QMessageLogContext&, const QString& message) {
    if (type == QtWarningMsg || type == QtCriticalMsg) {
        const auto base = qEnvironmentVariableIsSet("PSYCHE_DATA_DIR")
            ? QString::fromLocal8Bit(qgetenv("PSYCHE_DATA_DIR"))
            : QDir::homePath() + "/.local/share/psyche";
        QFile file(base + "/crash.log");
        if (file.open(QIODevice::Append))
            file.write((QDateTime::currentDateTime().toString(Qt::ISODate) + " " + message + "\\n").toUtf8());
    }
}
}
static bool initializeBundledFonts() {
    const auto path = qgetenv("PSYCHE_FONTCONFIG_FILE");
    if (path.isEmpty())
        return true;
    QLibrary library("fontconfig", 1);
    library.setLoadHints(QLibrary::PreventUnloadHint);
    using Create = void* (*)();
    using Parse = int (*)(void*, const unsigned char*, int);
    using Configure = int (*)(void*);
    using Destroy = void (*)(void*);
    const auto create = reinterpret_cast<Create>(library.resolve("FcConfigCreate"));
    const auto parse = reinterpret_cast<Parse>(library.resolve("FcConfigParseAndLoad"));
    const auto build = reinterpret_cast<Configure>(library.resolve("FcConfigBuildFonts"));
    const auto current = reinterpret_cast<Configure>(library.resolve("FcConfigSetCurrent"));
    const auto destroy = reinterpret_cast<Destroy>(library.resolve("FcConfigDestroy"));
    if (!create || !parse || !build || !current || !destroy)
        return false;
    void* config = create();
    if (!config)
        return false;
    const bool success =
        parse(config, reinterpret_cast<const unsigned char*>(path.constData()), 1) &&
        build(config) && current(config);
    destroy(config);
    return success;
}
int main(int argc, char** argv) {
    if (argc > 1) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("psyche");
        app.setApplicationVersion(PSYCHE_VERSION);
        return runCli();
    }
    if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND"))
        qputenv("QT_QUICK_BACKEND", "software");
    if (!initializeBundledFonts()) {
        qCritical("Não foi possível iniciar as fontes embutidas.");
        return 1;
    }
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);
    app.setApplicationName("psyche");
    app.setApplicationVersion(PSYCHE_VERSION);
    app.setDesktopFileName("psyche");
    app.setWindowIcon(QIcon(":/qt/qml/Psyche/qml/icons/psyche.svg"));
    AppSettings settings;
    Backend backend(&settings);
    signal(SIGSEGV, dumpCrash);
    signal(SIGABRT, dumpCrash);
    qInstallMessageHandler(redirectWarnings);
    Artwork artwork(settings.dataDirectory());
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("preferences", &settings);
    engine.rootContext()->setContextProperty("artwork", &artwork);
    engine.loadFromModule("Psyche", "Main");
    if (engine.rootObjects().isEmpty()) {
        qWarning("Psyche: falha ao carregar a interface");
        return 1;
    }
    return app.exec();
}
