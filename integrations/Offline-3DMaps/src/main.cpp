#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    if (qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND"))
        qputenv("QSG_RHI_BACKEND", "opengl");
#endif
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("OpenFlightTools"));
    QCoreApplication::setApplicationName(QStringLiteral("DroneTerrainSim"));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("DroneTerrainSim"), QStringLiteral("Main"));

    if (app.arguments().contains(QStringLiteral("--screenshot"))) {
        if (engine.rootObjects().isEmpty())
            return -1;
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        if (!window)
            return -2;
        QTimer::singleShot(8000, window, [window] {
            window->grabWindow().save(QStringLiteral("verification.png"));
            QCoreApplication::quit();
        });
    }
    return app.exec();
}
