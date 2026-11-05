# QML Integration Notes

## File structure
Place all .qml files in a folder, e.g. `qml/`.
Add to your .pro:
```
QML_IMPORT_PATH += $$PWD/qml
RESOURCES += qml.qrc        # or use Qt Quick Compiler
```

## Registering the backend (main.cpp)
```cpp
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "mainwindow.h"   // your existing C++ class

int main(int argc, char* argv[]) {
    QLocale::setDefault(QLocale::C);
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));

    QQmlApplicationEngine engine;

    // Register Theme as singleton (auto via qmldir — nothing extra needed)

    // Expose C++ backend
    MainWindow backend;
    engine.rootContext()->setContextProperty("backend", &backend);

    engine.load(QUrl("qrc:/qml/main.qml"));
    return app.exec();
}
```

## Signals your MainWindow must emit (QML reads these)
```cpp
signals:
    void logLine(const QString& level, const QString& message);
    // level: "INFO" | "OK" | "WARN" | "ERROR"

    void progressChanged(int percent);          // 0–100; 0 or 100 hides bar
    void statusChanged(const QString& text, bool busy);
    void srtmInfoChanged(const QString& text);  // e.g. "N23 E077 cached"
    void outputDirChanged(const QString& dir);
```

## Slots QML calls on the backend
```cpp
Q_INVOKABLE void generateMission(
    const QString& takeoffLat, const QString& takeoffLon, const QString& fromName,
    const QString& landLat,    const QString& landLon,    const QString& toName,
    double takeoffAlt, double nearHomeAlt, double cruiseAlt, double landAlt,
    double speedMs, double waitS,
    int missionTypeIdx, int tripModeIdx, int droneIdx,
    const QString& outputDir);

Q_INVOKABLE void browseOutputDir();   // opens QFileDialog, emits outputDirChanged
```

## Notes
- Theme.qml is a pragma Singleton — import it in each file with:
    `import FlightPlanner 1.0`  (after adding qmldir to import path)
- The haversine distance check runs entirely in QML JS — no backend call needed.
- All validation logic stays in MissionValidator.cpp; call it inside generateMission()
  and emit appropriate logLine/statusChanged signals for errors.
