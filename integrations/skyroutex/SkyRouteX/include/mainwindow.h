// // #pragma once

// // #include <QObject>
// // #include <QString>
// // #include <QVariantList>
// // #include <QNetworkAccessManager>
// // #include <QNetworkReply>
// // #include <QSettings>
// // #include <QCryptographicHash>

// // #include "drone_models.h"
// // #include "srtm_reader.h"
// // #include "path_planner.h"
// // #include "waypoint_writer.h"
// // #include "mission_validator.h"
// // #include "TileStitcher.h"
// // #include "DroneManager.h"

// // // MainWindow is no longer a QWidget/QMainWindow.
// // // It is a plain QObject that acts as the C++ backend for the QML UI.
// // // All UI state lives in QML; this class owns the business logic.
// // //
// // // Registered in main.cpp as:
// // //   engine.rootContext()->setContextProperty("backend", &mainWindow);

// // class MainWindow : public QObject
// // {
// //     Q_OBJECT

// // public:
// //     explicit MainWindow(QObject* parent = nullptr);

// //     // ── Signals — QML reads all state through these ───────────────────────────
// // signals:
// //     void logLine(const QString& level, const QString& message);
// //     // level: "INFO" | "OK" | "WARN" | "ERROR"

// //     void progressChanged(int percent);
// //     // 0 or 100 → QML hides progress bar
// //     // 1–99     → QML shows progress bar

// //     void statusChanged(const QString& text, bool busy);
// //     // busy=true  → animated status dot
// //     // busy=false → static green dot

// //     void srtmInfoChanged(const QString& text);
// //     // e.g. "N28 E077 cached"

// //     void outputDirChanged(const QString& dir);
// //     // emitted by browseOutputDir() after user picks a folder

// //     void routeReady(const QVariantList& points);
// //     // emitted after successful path planning

// //     void geocodeDone(const QVariantList& results);

// //     // 3D view signals — imagePath is a local file path to the stitched JPEG
// //     void tileTextureReady(const QString& imagePath, double minLat, double maxLat, double minLon, double maxLon);
// //     void tileProgress(int fetched, int total);
// //     void tileStatus(const QString& msg);
// //     // results: [{name, lat, lon}, ...]

// //     void geocodeError(const QString& message);

// //     // ── Invokables — QML calls these directly ─────────────────────────────────
// // public slots:
// //     Q_INVOKABLE void generateMission(
// //         const QString& takeoffLat,  const QString& takeoffLon,
// //         const QString& fromName,
// //         const QString& landLat,     const QString& landLon,
// //         const QString& toName,
// //         double takeoffAlt,
// //         double nearHomeAlt,
// //         double cruiseAlt,
// //         double landAlt,
// //         double speedMs,
// //         double waitS,
// //         int    missionTypeIdx,      // 0=Delivery, 1=Drop, 2=Paradrop
// //         int    tripModeIdx,         // 0=OneWay,   1=Return
// //         const QString& droneId,     // ID from DroneManager
// //         const QString& outputDir);

// //     Q_INVOKABLE void browseOutputDir();
// //     Q_INVOKABLE void browseSrtmDir();

// //     Q_INVOKABLE void geocodeSearch(const QString& query);

// //     // 3D view
// //     Q_INVOKABLE void fetch3DTiles(double minLat, double maxLat,
// //                                   double minLon, double maxLon,
// //                                   int tileSource = 0, int zoom = 14);

// //     // Returns the SrtmReader pointer so QML can pass it to GeoTerrainGeometry
// //     Q_INVOKABLE QObject* srtmReader()    const { return m_srtm; }
// //     Q_INVOKABLE QObject* droneManagerObj() const { return m_droneManager; }

// //     // PIN management — called from QML PinGate and Settings page
// //     Q_INVOKABLE bool verifyPin(const QString& section, const QString& pin);
// //     Q_INVOKABLE void setPin(const QString& section, const QString& pin);
// //     Q_INVOKABLE bool hasPin(const QString& section);
// //     // Queries Nominatim geocoder, emits geocodeDone or geocodeError

// //     // ── Private slots — wired to SrtmReader signals ───────────────────────────
// // private slots:
// //     void onTileReady(int lat, int lon);
// //     void onDownloadProgress(int pct);
// //     void onDownloadError(const QString& msg);

// // private:
// //     // Core generation logic — called by generateMission() and onTileReady()
// //     void runGenerate(const MissionParams& p,
// //                      TripMode             trip,
// //                      MissionType          mtype,
// //                      const DroneModel&    drone,
// //                      const QString&       outputDir);

// //     // Convenience wrappers that emit signals
// //     void appendLog(const QString& level, const QString& msg);
// //     void setStatus(const QString& msg, bool busy = false);

// //     // ── Backend objects ───────────────────────────────────────────────────────
// //     QNetworkAccessManager* m_geocodeNam;
// //     TileStitcher*          m_tileStitcher;
// //     DroneManager*          m_droneManager;
// //     SrtmReader*      m_srtm;
// //     PathPlanner      m_planner;
// //     WaypointWriter   m_writer;
// //     MissionValidator m_validator;
// //     QVector<DroneModel> m_drones;

// //     // Pending generate — used when tile download is in progress
// //     bool          m_pendingGenerate  = false;
// //     MissionParams m_pendingParams;
// //     TripMode      m_pendingTrip      = TripMode::OneWay;
// //     MissionType   m_pendingMtype     = MissionType::Delivery;
// //     DroneModel    m_pendingDrone;
// //     QString       m_pendingOutputDir;
// // };


// #pragma once

// #include <QObject>
// #include <QString>
// #include <QVariantList>
// #include <QNetworkAccessManager>
// #include <QNetworkReply>
// #include <QSettings>
// #include <QCryptographicHash>

// #include "drone_models.h"
// #include "srtm_reader.h"
// #include "path_planner.h"
// #include "waypoint_writer.h"
// #include "mission_validator.h"
// #include "TileStitcher.h"
// #include "DroneManager.h"

// // MainWindow is no longer a QWidget/QMainWindow.
// // It is a plain QObject that acts as the C++ backend for the QML UI.
// // All UI state lives in QML; this class owns the business logic.
// //
// // Registered in main.cpp as:
// //   engine.rootContext()->setContextProperty("backend", &mainWindow);

// class MainWindow : public QObject
// {
//     Q_OBJECT

// public:
//     explicit MainWindow(QObject* parent = nullptr);

//     // ── Signals — QML reads all state through these ───────────────────────────
// signals:
//     void logLine(const QString& level, const QString& message);
//     // level: "INFO" | "OK" | "WARN" | "ERROR"

//     void progressChanged(int percent);
//     // 0 or 100 → QML hides progress bar
//     // 1–99     → QML shows progress bar

//     void statusChanged(const QString& text, bool busy);
//     // busy=true  → animated status dot
//     // busy=false → static green dot

//     void srtmInfoChanged(const QString& text);
//     // e.g. "N28 E077 cached"

//     void outputDirChanged(const QString& dir);
//     // emitted by browseOutputDir() after user picks a folder

//     void routeReady(const QVariantList& points);
//     // emitted after successful path planning

//     void geocodeDone(const QVariantList& results);

//     // 3D view signals — imagePath is a local file path to the stitched JPEG
//     void tileTextureReady(const QString& imagePath, double minLat, double maxLat, double minLon, double maxLon);
//     void tileProgress(int fetched, int total);
//     void tileStatus(const QString& msg);
//     // results: [{name, lat, lon}, ...]

//     void geocodeError(const QString& message);

//     // ── Invokables — QML calls these directly ─────────────────────────────────
// public slots:
//     Q_INVOKABLE void generateMission(
//         const QString& takeoffLat,  const QString& takeoffLon,
//         const QString& fromName,
//         const QString& landLat,     const QString& landLon,
//         const QString& toName,
//         double takeoffAlt,
//         double nearHomeAlt,
//         double cruiseAlt,
//         double dropAlt,
//         double landAlt,
//         double speedMs,
//         double waitS,
//         int    missionTypeIdx,      // 0=Delivery, 1=Drop, 2=Paradrop
//         int    tripModeIdx,         // 0=OneWay,   1=Return
//         const QString& droneId,     // ID from DroneManager
//         const QString& outputDir);

//     Q_INVOKABLE void browseOutputDir();
//     Q_INVOKABLE void browseSrtmDir();

//     Q_INVOKABLE void geocodeSearch(const QString& query);

//     // 3D view
//     Q_INVOKABLE void fetch3DTiles(double minLat, double maxLat,
//                                   double minLon, double maxLon,
//                                   int tileSource = 0, int zoom = 14);

//     // Returns the SrtmReader pointer so QML can pass it to GeoTerrainGeometry
//     Q_INVOKABLE QObject* srtmReader()    const { return m_srtm; }
//     Q_INVOKABLE QObject* droneManagerObj() const { return m_droneManager; }

//     // PIN management — called from QML PinGate and Settings page
//     Q_INVOKABLE bool verifyPin(const QString& section, const QString& pin);
//     Q_INVOKABLE void setPin(const QString& section, const QString& pin);
//     Q_INVOKABLE bool hasPin(const QString& section);
//     // Queries Nominatim geocoder, emits geocodeDone or geocodeError

//     // ── Private slots — wired to SrtmReader signals ───────────────────────────
// private slots:
//     void onTileReady(int lat, int lon);
//     void onDownloadProgress(int pct);
//     void onDownloadError(const QString& msg);

// private:
//     // Core generation logic — called by generateMission() and onTileReady()
//     void runGenerate(const MissionParams& p,
//                      TripMode             trip,
//                      MissionType          mtype,
//                      const DroneModel&    drone,
//                      const QString&       outputDir);

//     // Convenience wrappers that emit signals
//     void appendLog(const QString& level, const QString& msg);
//     void setStatus(const QString& msg, bool busy = false);

//     // ── Backend objects ───────────────────────────────────────────────────────
//     QNetworkAccessManager* m_geocodeNam;
//     TileStitcher*          m_tileStitcher;
//     DroneManager*          m_droneManager;
//     SrtmReader*      m_srtm;
//     PathPlanner      m_planner;
//     WaypointWriter   m_writer;
//     MissionValidator m_validator;
//     QVector<DroneModel> m_drones;

//     // Pending generate — used when tile download is in progress
//     bool          m_pendingGenerate  = false;
//     MissionParams m_pendingParams;
//     TripMode      m_pendingTrip      = TripMode::OneWay;
//     MissionType   m_pendingMtype     = MissionType::Delivery;
//     DroneModel    m_pendingDrone;
//     QString       m_pendingOutputDir;
// };



#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QCryptographicHash>

#include "drone_models.h"
#include "srtm_reader.h"
#include "path_planner.h"
#include "waypoint_writer.h"
#include "mission_validator.h"
#include "TileStitcher.h"
#include "DroneManager.h"
#include "KeyManager.h"

// MainWindow is no longer a QWidget/QMainWindow.
// It is a plain QObject that acts as the C++ backend for the QML UI.
// All UI state lives in QML; this class owns the business logic.
//
// Registered in main.cpp as:
//   engine.rootContext()->setContextProperty("backend", &mainWindow);

class MainWindow : public QObject
{
    Q_OBJECT

public:
    explicit MainWindow(QObject* parent = nullptr);

    // ── Signals — QML reads all state through these ───────────────────────────
signals:
    void logLine(const QString& level, const QString& message);
    // level: "INFO" | "OK" | "WARN" | "ERROR"

    void progressChanged(int percent);
    // 0 or 100 → QML hides progress bar
    // 1–99     → QML shows progress bar

    void statusChanged(const QString& text, bool busy);
    // busy=true  → animated status dot
    // busy=false → static green dot

    void srtmInfoChanged(const QString& text);
    // e.g. "N28 E077 cached"

    void outputDirChanged(const QString& dir);
    // emitted by browseOutputDir() after user picks a folder

    void routeReady(const QVariantList& points);
    // emitted after successful path planning

    void geocodeDone(const QVariantList& results);

    // 3D view signals — imagePath is a local file path to the stitched JPEG
    void tileTextureReady(const QString& imagePath, double minLat, double maxLat, double minLon, double maxLon);
    void tileProgress(int fetched, int total);
    void tileStatus(const QString& msg);
    // results: [{name, lat, lon}, ...]

    void geocodeError(const QString& message);

    void activationSucceeded();
    void activationFailed(QString message);

    // ── Invokables — QML calls these directly ─────────────────────────────────
public slots:
    Q_INVOKABLE void generateMission(
        const QString& takeoffLat,  const QString& takeoffLon,
        const QString& fromName,
        const QString& landLat,     const QString& landLon,
        const QString& toName,
        double takeoffAlt,
        double nearHomeAlt,
        double cruiseAlt,
        double dropAlt,
        double landAlt,
        double speedMs,
        double waitS,
        int    missionTypeIdx,      // 0=Delivery, 1=Drop, 2=Paradrop
        int    tripModeIdx,         // 0=OneWay,   1=Return
        const QString& droneId,     // ID from DroneManager
        const QString& outputDir);

    Q_INVOKABLE void browseOutputDir();
    Q_INVOKABLE void browseSrtmDir();
    Q_INVOKABLE QString defaultOutputDir() const;
    Q_INVOKABLE QString srtmCacheDir() const;
    Q_INVOKABLE QString mapTilesCacheDir() const;

    Q_INVOKABLE void onValidateKey(QString key);
    Q_INVOKABLE bool isActivated();

    Q_INVOKABLE void geocodeSearch(const QString& query);

    // 3D view
    Q_INVOKABLE void fetch3DTiles(double minLat, double maxLat,
                                  double minLon, double maxLon,
                                  int tileSource = 0, int zoom = 14);

    // Returns the SrtmReader pointer so QML can pass it to GeoTerrainGeometry
    Q_INVOKABLE QObject* srtmReader()    const { return m_srtm; }
    Q_INVOKABLE QObject* droneManagerObj() const { return m_droneManager; }

    // PIN management — called from QML PinGate and Settings page
    Q_INVOKABLE bool verifyPin(const QString& section, const QString& pin);
    Q_INVOKABLE void setPin(const QString& section, const QString& pin);
    Q_INVOKABLE bool hasPin(const QString& section);
    // Queries Nominatim geocoder, emits geocodeDone or geocodeError

    // ── Private slots — wired to SrtmReader signals ───────────────────────────
private slots:
    void onTileReady(int lat, int lon);
    void onDownloadProgress(int pct);
    void onDownloadError(const QString& msg);

private:
    // Core generation logic — called by generateMission() and onTileReady()
    void runGenerate(const MissionParams& p,
                     TripMode             trip,
                     MissionType          mtype,
                     const DroneModel&    drone,
                     const QString&       outputDir);

    // Convenience wrappers that emit signals
    void appendLog(const QString& level, const QString& msg);
    void setStatus(const QString& msg, bool busy = false);

    // ── Backend objects ───────────────────────────────────────────────────────
    QNetworkAccessManager* m_geocodeNam;
    TileStitcher*          m_tileStitcher;
    DroneManager*          m_droneManager;
    SrtmReader*      m_srtm;
    PathPlanner      m_planner;
    WaypointWriter   m_writer;
    MissionValidator m_validator;
    QVector<DroneModel> m_drones;

    // Pending generate — used when tile download is in progress
    bool          m_pendingGenerate  = false;
    MissionParams m_pendingParams;
    TripMode      m_pendingTrip      = TripMode::OneWay;
    MissionType   m_pendingMtype     = MissionType::Delivery;
    DroneModel    m_pendingDrone;
    QString       m_pendingOutputDir;
};
