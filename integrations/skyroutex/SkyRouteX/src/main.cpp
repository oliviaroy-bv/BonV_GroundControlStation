#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QLocale>
#include <QDir>
#include <QDebug>
#include <QIcon>
#include <QSslSocket>

#include "mainwindow.h"
#include "mapwidget.h"
#include "TerrainMeshGeometry.h"
#include "RouteLineGeometry.h"
#include "GeoTerrainGeometry.h"
#include "GeoRouteGeometry.h"

int main(int argc, char* argv[])
{
    QLocale::setDefault(QLocale::C);

    // Force Basic style so QML control customization (background, contentItem)
    // works correctly. Native style on Windows blocks custom backgrounds.
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QApplication app(argc, argv);
    app.setApplicationName("SkyRouteX");
    app.setApplicationVersion("2.0");
    app.setOrganizationName("BonV");

    qDebug() << "Qt SSL supported:" << QSslSocket::supportsSsl()
             << "active backend:" << QSslSocket::activeBackend()
             << "available backends:" << QSslSocket::availableBackends()
             << "build:" << QSslSocket::sslLibraryBuildVersionString()
             << "runtime:" << QSslSocket::sslLibraryVersionString();
    // app.setWindowIcon(QIcon(QDir::currentPath() + "/images/bonv_logo.ico"));
    app.setWindowIcon(QIcon(":/images/bonv_logo.ico"));
    // Register MapWidget so QML can use it as "MapWidget { }"
    qmlRegisterType<MapWidget>("FlightPlanner", 1, 0, "MapWidget");
    qmlRegisterType<TerrainMeshGeometry>("FlightPlanner", 1, 0, "TerrainMeshGeometry");
    qmlRegisterType<RouteLineGeometry>("FlightPlanner", 1, 0, "RouteLineGeometry");
    qmlRegisterType<GeoTerrainGeometry>("FlightPlanner", 1, 0, "GeoTerrainGeometry");
    qmlRegisterType<GeoRouteGeometry>("FlightPlanner", 1, 0, "GeoRouteGeometry");

    // C++ backend
    MainWindow backend;

    QQmlApplicationEngine engine;

    // Expose backend and drone manager to all QML files
    engine.rootContext()->setContextProperty("backend",      &backend);
    engine.rootContext()->setContextProperty("droneManager", backend.droneManagerObj());

    // QML_SOURCE_DIR is defined by the .pro as the absolute path to qml/
    // e.g. "C:/Users/.../FlightPlanner/qml/"
    // This lets us load QML directly from source — no .qrc embedding needed.
    // For a release build you would switch to qrc: resources instead.
            // QString qmlDir = QString(QML_SOURCE_DIR);

            // qDebug() << "QML source dir:" << qmlDir;

            // // Add qml/ as an import path so qmldir is found and
            // // all custom types (SectionBlock, FormRow, etc.) resolve correctly
            // engine.addImportPath(qmlDir);

            // // Load the root QML file
            // QString mainQml = qmlDir + "main.qml";
            // qDebug() << "Loading:" << mainQml;

             // engine.load(QUrl::fromLocalFile(mainQml));
             engine.load(QUrl("qrc:/qml/main.qml"));

    if (engine.rootObjects().isEmpty()) {
        qCritical("Failed to load QML root. Check QML_SOURCE_DIR and qmldir.");
        return -1;
    }

    return app.exec();
}
