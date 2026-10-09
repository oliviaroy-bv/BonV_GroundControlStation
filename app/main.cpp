#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>

#include <QGuiApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QMutex>
#include <QStandardPaths>
#include <QTextStream>
#include <QWebEngineUrlScheme>
#include <QWindow>
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#include <QTimer>

#include "mission/MissionFileWriter.h"
#include "map/RasterOverlayManager.h"
#include "map/RasterOverlaySchemeHandler.h"
#include "map/TileCacheManager.h"
#include "map/TileCacheRequestInterceptor.h"
#include "map/TileCacheSchemeHandler.h"
#include "storage/AppDataPaths.h"
#include "video/VideoItem.h"
#include "vehicle/VehicleManager.h"
#include "AppBackend.h"
#include "imagerytileprovider.h"
#include "terrainmanager.h"
#include "terraintilegeometry.h"
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

namespace {

QFile *runtimeLogFile = nullptr;
QMutex runtimeLogMutex;

#ifdef _WIN32
void attachConsole()
{
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE *fp;

        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);

        std::ios::sync_with_stdio(true);
    }
}
#endif

void runtimeMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const char *level = "INFO";
    switch (type) {
    case QtDebugMsg:
        level = "DEBUG";
        break;
    case QtInfoMsg:
        level = "INFO";
        break;
    case QtWarningMsg:
        level = "WARN";
        break;
    case QtCriticalMsg:
        level = "CRITICAL";
        break;
    case QtFatalMsg:
        level = "FATAL";
        break;
    }

    const QString line = QStringLiteral("%1 [%2] %3 (%4:%5)\n")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                 QString::fromLatin1(level),
                 message,
                 QString::fromLatin1(context.file ? context.file : "unknown"))
            .arg(context.line);

    {
        QMutexLocker locker(&runtimeLogMutex);
        if (runtimeLogFile && runtimeLogFile->isOpen()) {
            runtimeLogFile->write(line.toUtf8());
            runtimeLogFile->flush();
        }
    }

    // QTextStream(stderr) << line;
    fprintf(stderr, "%s", line.toUtf8().constData());
    fflush(stderr);

    if (type == QtFatalMsg) {
        abort();
    }
}

void installRuntimeLogger()
{
    const QString logRoot = AppDataPaths::logsDirectory();
    AppDataPaths::ensureDirectory(logRoot);

    runtimeLogFile = new QFile(logRoot + QStringLiteral("/runtime.log"));
    if (runtimeLogFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        runtimeLogFile->write("\n--- BonV GCS session start ---\n");
        runtimeLogFile->flush();
        qInstallMessageHandler(runtimeMessageHandler);
    } else {
        delete runtimeLogFile;
        runtimeLogFile = nullptr;
    }
}

} // namespace

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QWebEngineUrlScheme qrcScheme(QByteArrayLiteral("qrc"));
    qrcScheme.setFlags(
        QWebEngineUrlScheme::SecureScheme
        | QWebEngineUrlScheme::LocalAccessAllowed
        | QWebEngineUrlScheme::CorsEnabled
        | QWebEngineUrlScheme::ViewSourceAllowed);

    QWebEngineUrlScheme::registerScheme(qrcScheme);

    QWebEngineUrlScheme bonvScheme(QByteArrayLiteral("bonv"));
    bonvScheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    bonvScheme.setFlags(QWebEngineUrlScheme::SecureScheme
        | QWebEngineUrlScheme::CorsEnabled
        | QWebEngineUrlScheme::FetchApiAllowed);
    QWebEngineUrlScheme::registerScheme(bonvScheme);

    QtWebEngineQuick::initialize();
    QCoreApplication::setOrganizationName(QStringLiteral("BonV"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("bonv.local"));
    QCoreApplication::setApplicationName(QStringLiteral("BonV GCS"));

    QGuiApplication app(argc, argv);

    #ifdef _WIN32
    attachConsole();
    #endif

    app.setWindowIcon(QIcon(QStringLiteral(":/logo/BonVGCS_Icon.png")));
    AppDataPaths::ensureAllDirectories();
    installRuntimeLogger();
    qDebug() << "========== BonV GCS Starting ==========";
    qDebug() << "Qt Version:" << QT_VERSION_STR;
    qDebug() << "Current directory:" << QDir::currentPath();

    TileCacheManager tileCacheManager;
    auto *webProfile = QQuickWebEngineProfile::defaultProfile();
    webProfile->installUrlSchemeHandler(
        QByteArrayLiteral("bonv"),
        new TileCacheSchemeHandler(&tileCacheManager, webProfile));
    webProfile->setUrlRequestInterceptor(new TileCacheRequestInterceptor(webProfile));

    QQmlApplicationEngine engine;

    VehicleManager manager;
    MissionFileWriter missionFileWriter;
    RasterOverlayManager rasterOverlayManager;
    AppBackend appBackend;

    qmlRegisterUncreatableType<Vehicle>("Vehicle", 1, 0, "Vehicle", "Not creatable");
    qmlRegisterType<VideoItem>("BonV.Video", 1, 0, "VideoItem");
    qmlRegisterType<ImageryTileProvider>("DroneTerrainSim", 1, 0, "ImageryTileProvider");
    qmlRegisterType<TerrainManager>("DroneTerrainSim", 1, 0, "TerrainManager");
    qmlRegisterType<TerrainTileGeometry>("DroneTerrainSim", 1, 0, "TerrainTileGeometry");
    engine.rootContext()->setContextProperty("vehicleManager", &manager);
    engine.rootContext()->setContextProperty("missionFileWriter", &missionFileWriter);
    engine.rootContext()->setContextProperty("rasterOverlayManager", &rasterOverlayManager);
    engine.rootContext()->setContextProperty("tileCacheManager", &tileCacheManager);
    engine.rootContext()->setContextProperty("backend", &appBackend);

    const QUrl url(QStringLiteral("qrc:/qml/core/App.qml"));

    engine.load(url);
    QObject *root = engine.rootObjects().isEmpty() ? nullptr : engine.rootObjects().first();

    if (root) {

        QObject::connect(root, &QObject::destroyed, []() {
            qDebug() << "***** ROOT WINDOW DESTROYED *****";
        });

        if (QWindow *window = qobject_cast<QWindow *>(root)) {

            QObject::connect(window, &QWindow::visibleChanged, [window]() {
                qDebug() << "VISIBLE =" << window->isVisible();
            });

            QObject::connect(window, &QWindow::visibilityChanged,
                            [](QWindow::Visibility v) {
                qDebug() << "VISIBILITY =" << v;
            });

            QObject::connect(window, &QWindow::activeChanged,
                            [window]() {
                qDebug() << "ACTIVE =" << window->isActive();
            });

            QObject::connect(window, &QObject::destroyed, []() {
                qDebug() << "WINDOW DESTROYED";
            });
        }
    }

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    const QIcon appIcon(QStringLiteral(":/logo/BonVGCS_Icon.png"));
    for (QObject *rootObject : engine.rootObjects()) {
        if (QWindow *window = qobject_cast<QWindow *>(rootObject)) {
            window->setIcon(appIcon);
        }
    }

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, []() {
        qDebug() << "GUI thread alive";
    });
    timer.start(1000);

    return app.exec();
}
