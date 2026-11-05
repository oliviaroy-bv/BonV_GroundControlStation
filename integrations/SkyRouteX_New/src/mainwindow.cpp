// // #include "mainwindow.h"

// // #include <QFileDialog>
// // #include <QDir>
// // #include <QNetworkRequest>
// // #include <QNetworkReply>
// // #include <QJsonDocument>
// // #include <QJsonArray>
// // #include <QJsonObject>
// // #include <QUrlQuery>
// // #include <cmath>
// // #include <QCoreApplication>

// // // ── Constructor ───────────────────────────────────────────────────────────────

// // MainWindow::MainWindow(QObject* parent)
// //     : QObject(parent)
// //     , m_geocodeNam(new QNetworkAccessManager(this))
// //     , m_tileStitcher(new TileStitcher(this))
// //     , m_droneManager(new DroneManager(this))
// //     , m_srtm(new SrtmReader(this))
// //     , m_drones(allDroneModels())
// // {
// //     connect(m_srtm, &SrtmReader::tileReady,
// //             this,   &MainWindow::onTileReady);

// //     // Wire TileStitcher signals through to QML
// //     connect(m_tileStitcher, &TileStitcher::textureReady,
// //             this, &MainWindow::tileTextureReady);
// //     connect(m_tileStitcher, &TileStitcher::progress,
// //             this, &MainWindow::tileProgress);
// //     connect(m_tileStitcher, &TileStitcher::statusChanged,
// //             this, &MainWindow::tileStatus);
// //     connect(m_srtm, &SrtmReader::downloadProgress,
// //             this,   &MainWindow::onDownloadProgress);
// //     connect(m_srtm, &SrtmReader::downloadError,
// //             this,   &MainWindow::onDownloadError);
// // }

// // // ── Q_INVOKABLE: generateMission ──────────────────────────────────────────────

// // void MainWindow::generateMission(
// //     const QString& takeoffLat,  const QString& takeoffLon,
// //     const QString& fromName,
// //     const QString& landLat,     const QString& landLon,
// //     const QString& toName,
// //     double takeoffAlt,
// //     double nearHomeAlt,
// //     double cruiseAlt,
// //     double landAlt,
// //     double speedMs,
// //     double waitS,
// //     int    missionTypeIdx,
// //     int    tripModeIdx,
// //     const QString& droneId,
// //     const QString& outputDir)
// // {
// //     // Build MissionParams from QML strings/values
// //     MissionParams p;
// //     p.takeoffLat    = takeoffLat.toDouble();
// //     p.takeoffLon    = takeoffLon.toDouble();
// //     p.landLat       = landLat.toDouble();
// //     p.landLon       = landLon.toDouble();
// //     p.fromName      = fromName.trimmed();
// //     p.toName        = toName.trimmed();
// //     p.takeoffAlt    = takeoffAlt;
// //     p.nearHomeAlt   = nearHomeAlt;
// //     p.cruiseAlt     = cruiseAlt;
// //     p.landAlt       = landAlt;
// //     p.cruiseSpeedMs = speedMs;
// //     p.waitDelayS    = waitS;

// //     TripMode    trip  = (tripModeIdx == 1) ? TripMode::Return : TripMode::OneWay;
// //     MissionType mtype = static_cast<MissionType>(
// //                             qBound(0, missionTypeIdx, 2));

// //     // Look up drone from DroneManager by ID
// //     QVariantList activeDrones = m_droneManager->activeDrones();
// //     DroneModel drone;
// //     bool found = false;
// //     for (const QVariant& v : activeDrones) {
// //         QVariantMap dm = v.toMap();
// //         if (dm["id"].toString() == droneId) {
// //             drone.id              = dm["id"].toString();
// //             drone.displayName     = dm["name"].toString();
// //             drone.maxRangeM           = dm["maxOneWayKm"].toDouble() * 1000.0;
// //             drone.defaultCruiseSpeed  = dm["maxSpeedMs"].toDouble();
// //             drone.defaultTakeoffAlt    = dm["takeoffAlt"].toDouble();
// //             drone.defaultNearHomeAlt   = dm["nearHomeAlt"].toDouble();
// //             drone.defaultCruiseAlt     = dm["cruiseAlt"].toDouble();
// //             drone.defaultLandAlt       = dm["landAlt"].toDouble();
// //             drone.rcSwitch             = dm["rcSwitch"].toInt();
// //             found = true;
// //             break;
// //         }
// //     }
// //     if (!found) {
// //         appendLog("ERROR", "Drone not found: " + droneId);
// //         return;
// //     }

// //     // Validate before doing any terrain work
// //     auto vr = m_validator.validate(p, drone, trip);
// //     if (!vr.ok) {
// //         for (const auto& e : vr.errors)
// //             appendLog("ERROR", e);
// //         setStatus("Validation failed.", false);
// //         emit progressChanged(0);
// //         return;
// //     }
// //     for (const auto& w : vr.warnings)
// //         appendLog("WARN", w);

// //     runGenerate(p, trip, mtype, drone, outputDir);
// // }

// // // ── Core generation ───────────────────────────────────────────────────────────

// // void MainWindow::runGenerate(const MissionParams& p,
// //                               TripMode             trip,
// //                               MissionType          mtype,
// //                               const DroneModel&    drone,
// //                               const QString&       outputDir)
// // {
// //     setStatus("Checking SRTM tile cache...", true);
// //     emit progressChanged(5);

// //     appendLog("INFO", QString("Mission: %1  |  %2 → %3  |  %4")
// //         .arg(missionLabel(mtype))
// //         .arg(p.fromName, p.toName)
// //         .arg(trip == TripMode::Return ? "Return" : "One-Way"));

// //     appendLog("INFO", QString("Takeoff: %1, %2   Landing: %3, %4")
// //         .arg(p.takeoffLat, 0, 'f', 7)
// //         .arg(p.takeoffLon, 0, 'f', 7)
// //         .arg(p.landLat,    0, 'f', 7)
// //         .arg(p.landLon,    0, 'f', 7));

// //     // Prefetch / check SRTM tiles
// //     m_srtm->prefetchTiles(p.takeoffLat, p.takeoffLon, p.landLat, p.landLon);

// //     if (!m_srtm->allTilesCached(p.takeoffLat, p.takeoffLon,
// //                                  p.landLat,    p.landLon)) {
// //         setStatus("Downloading SRTM tile(s)... please wait.", true);
// //         appendLog("INFO", "SRTM tiles not cached — downloading in background.");
// //         m_pendingGenerate  = true;
// //         m_pendingParams    = p;
// //         m_pendingTrip      = trip;
// //         m_pendingMtype     = mtype;
// //         m_pendingDrone     = drone;
// //         m_pendingOutputDir = outputDir;
// //         return;
// //     }

// //     // ── Plan outbound path ────────────────────────────────────────────────────
// //     emit progressChanged(30);
// //     setStatus("Planning outbound path...", true);

// //     auto elevFn = [this](double lat, double lon) {
// //         return m_srtm->elevation(lat, lon);
// //     };

// //     QVector<GeoPoint> outAnchors = m_planner.plan(p, elevFn);
// //     if (outAnchors.isEmpty()) {
// //         setStatus("Path planning failed — elevation data unavailable.", false);
// //         appendLog("ERROR", "Could not plan path. Retry after tile download.");
// //         emit progressChanged(0);
// //         return;
// //     }
// //     appendLog("INFO", QString("Outbound: %1 waypoints").arg(outAnchors.size()));

// //     emit progressChanged(60);

// //     // ── Plan return path (if needed) ──────────────────────────────────────────
// //     QVector<GeoPoint> retAnchors;
// //     if (trip == TripMode::Return) {
// //         setStatus("Planning return path...", true);
// //         MissionParams retP = p;
// //         std::swap(retP.takeoffLat, retP.landLat);
// //         std::swap(retP.takeoffLon, retP.landLon);
// //         retAnchors = m_planner.plan(retP, elevFn);
// //         if (retAnchors.isEmpty()) {
// //             setStatus("Return path planning failed.", false);
// //             appendLog("ERROR", "Return path failed.");
// //             emit progressChanged(0);
// //             return;
// //         }
// //         appendLog("INFO", QString("Return: %1 waypoints").arg(retAnchors.size()));
// //     }

// //     emit progressChanged(80);
// //     setStatus("Writing waypoint file...", true);

// //     // ── Write .waypoints file ─────────────────────────────────────────────────
// //     double distM = PathPlanner::haversineM(
// //         p.takeoffLat, p.takeoffLon, p.landLat, p.landLon);

// //     QString outPath = WaypointWriter::buildFilename(
// //         outputDir, p, drone, mtype, trip, distM);

// //     if (m_writer.write(outPath, p, outAnchors, retAnchors, trip, mtype, drone)) {
// //         emit progressChanged(100);
// //         setStatus("Done!", false);
// //         appendLog("OK", "Saved: " + outPath);

// //         // ── Build dense terrain profile (100m sample interval) ──────────────
// //         // The waypoint anchors are too sparse for a meaningful profile chart.
// //         // We interpolate at 100m intervals along each leg, query SRTM for
// //         // terrain height, and linearly interpolate drone altitude between anchors.
// //         QVariantList pts;
// //         QVariantList waypointMarkers;  // just the anchor points for the map
// //         const double SAMPLE_M = 100.0;
// //         double cumDistM = 0.0;

// //         auto elevFn = [this](double lat, double lon) {
// //             return m_srtm->elevation(lat, lon);
// //         };

// //         // Helper: emit one dense profile point
// //         auto addProfilePt = [&](double lat, double lon,
// //                                  double droneAlt, double distM) {
// //             double terrainH = elevFn(lat, lon);
// //             if (std::isnan(terrainH)) terrainH = 0.0;
// //             QVariantMap m;
// //             m["lat"]        = lat;
// //             m["lon"]        = lon;
// //             m["droneAlt"]   = droneAlt;
// //             m["terrainAlt"] = terrainH;
// //             m["distKm"]     = distM / 1000.0;
// //             pts.append(m);
// //         };

// //         // Helper: add map marker for an anchor
// //         auto addMapPt = [&](const GeoPoint& gp, double distM) {
// //             QVariantMap m;
// //             m["lat"]        = gp.lat;
// //             m["lon"]        = gp.lon;
// //             m["droneAlt"]   = gp.droneAlt;
// //             m["terrainAlt"] = gp.terrainH;
// //             m["distKm"]     = distM / 1000.0;
// //             waypointMarkers.append(m);
// //         };

// //         // Sample outbound anchors densely
// //         for (int i = 0; i < outAnchors.size(); i++) {
// //             const GeoPoint& gp = outAnchors[i];

// //             if (i == 0) {
// //                 addProfilePt(gp.lat, gp.lon, gp.droneAlt, 0.0);
// //                 addMapPt(gp, 0.0);
// //                 continue;
// //             }

// //             const GeoPoint& prev = outAnchors[i - 1];
// //             double legM = PathPlanner::haversineM(prev.lat, prev.lon, gp.lat, gp.lon);
// //             int    steps = std::max(1, static_cast<int>(legM / SAMPLE_M));

// //             for (int s = 1; s <= steps; s++) {
// //                 double t        = static_cast<double>(s) / steps;
// //                 double lat      = prev.lat      + t * (gp.lat      - prev.lat);
// //                 double lon      = prev.lon      + t * (gp.lon      - prev.lon);
// //                 double droneAlt = prev.droneAlt + t * (gp.droneAlt - prev.droneAlt);
// //                 cumDistM       += legM / steps;
// //                 addProfilePt(lat, lon, droneAlt, cumDistM);
// //             }
// //             addMapPt(gp, cumDistM);
// //         }

// //         // Sample return anchors densely (if any)
// //         for (int i = 0; i < retAnchors.size(); i++) {
// //             const GeoPoint& gp = retAnchors[i];

// //             if (i == 0) {
// //                 // Don't duplicate the last outbound point
// //                 addMapPt(gp, cumDistM);
// //                 continue;
// //             }

// //             const GeoPoint& prev = retAnchors[i - 1];
// //             double legM = PathPlanner::haversineM(prev.lat, prev.lon, gp.lat, gp.lon);
// //             int    steps = std::max(1, static_cast<int>(legM / SAMPLE_M));

// //             for (int s = 1; s <= steps; s++) {
// //                 double t        = static_cast<double>(s) / steps;
// //                 double lat      = prev.lat      + t * (gp.lat      - prev.lat);
// //                 double lon      = prev.lon      + t * (gp.lon      - prev.lon);
// //                 double droneAlt = prev.droneAlt + t * (gp.droneAlt - prev.droneAlt);
// //                 cumDistM       += legM / steps;
// //                 addProfilePt(lat, lon, droneAlt, cumDistM);
// //             }
// //             addMapPt(gp, cumDistM);
// //         }

// //         appendLog("INFO", QString("Terrain profile: %1 sample points at 100m intervals")
// //                       .arg(pts.size()));

// //         // routeReady carries:
// //         //   pts             — dense profile points (for TerrainProfile chart)
// //         //   waypointMarkers — sparse anchor points (for map polyline)
// //         // QML separates them: map uses waypointMarkers, chart uses pts.
// //         // We pack both into a single signal using a wrapper map.
// //         QVariantMap payload;
// //         payload["profile"]   = pts;
// //         payload["waypoints"] = waypointMarkers;
// //         emit routeReady(QVariantList{ payload });

// //     } else {
// //         setStatus("Failed to write output file.", false);
// //         appendLog("ERROR", "Write failed: " + outPath);
// //     }

// //     emit progressChanged(0);
// // }

// // // ── SrtmReader callbacks ──────────────────────────────────────────────────────

// // void MainWindow::onTileReady(int lat, int lon)
// // {
// //     appendLog("OK", QString("SRTM tile ready: N%1 E%2").arg(lat).arg(lon));
// //     emit srtmInfoChanged(QString("N%1 E%2 cached").arg(lat).arg(lon));

// //     if (!m_pendingGenerate) return;

// //     const MissionParams& p = m_pendingParams;
// //     if (m_srtm->allTilesCached(p.takeoffLat, p.takeoffLon,
// //                                 p.landLat,    p.landLon)) {
// //         m_pendingGenerate = false;
// //         appendLog("INFO", "All tiles ready — resuming generation.");
// //         runGenerate(p, m_pendingTrip, m_pendingMtype,
// //                     m_pendingDrone, m_pendingOutputDir);
// //     }
// // }

// // void MainWindow::onDownloadProgress(int pct)
// // {
// //     // Map tile download progress into the 0–30 range on the progress bar
// //     emit progressChanged(pct / 5);
// //     setStatus(QString("Downloading SRTM tile... %1%").arg(pct), true);
// // }

// // void MainWindow::onDownloadError(const QString& msg)
// // {
// //     appendLog("ERROR", msg);
// //     setStatus("Download failed: " + msg, false);
// //     m_pendingGenerate = false;
// //     emit progressChanged(0);
// // }

// // // ── Q_INVOKABLE: browseOutputDir ──────────────────────────────────────────────

// // void MainWindow::browseOutputDir()
// // {
// //     QString dir = QFileDialog::getExistingDirectory(
// //         nullptr,
// //         "Select output folder",
// //         QDir::currentPath());

// //     if (!dir.isEmpty())
// //         emit outputDirChanged(dir);
// // }

// // // ── Private helpers ───────────────────────────────────────────────────────────

// // void MainWindow::appendLog(const QString& level, const QString& msg)
// // {
// //     emit logLine(level, msg);
// // }

// // void MainWindow::setStatus(const QString& msg, bool busy)
// // {
// //     emit statusChanged(msg, busy);
// // }

// // void MainWindow::browseSrtmDir()
// // {
// //     QString dir = QFileDialog::getExistingDirectory(
// //         nullptr,
// //         "Select SRTM1 terrain cache folder",
// //         QDir::currentPath());

// //     if (!dir.isEmpty()) {
// //         m_srtm->setCacheDir(dir);
// //         appendLog("INFO", "SRTM cache dir set to: " + dir);
// //         setStatus("SRTM cache updated.", false);
// //     }
// // }

// // // ── 3D Tile fetch ─────────────────────────────────────────────────────────────

// // void MainWindow::fetch3DTiles(double minLat, double maxLat,
// //                                double minLon, double maxLon,
// //                                int tileSource, int zoom)
// // {
// //     // Prefetch SRTM tiles for the full 3D area (including 5km padding)
// //     // so GeoTerrainGeometry.rebuild() can sample elevation synchronously
// //     m_srtm->prefetchTiles(minLat, minLon, maxLat, maxLon);

// //     // Use same cache dir as the 2D map widget (terrain/tiles beside exe)
// //     QString cacheDir = QCoreApplication::applicationDirPath() + "/terrain/tiles";
// //     m_tileStitcher->setCacheDir(cacheDir);
// //     m_tileStitcher->setTileSource(tileSource);
// //     m_tileStitcher->fetchForBounds(minLat, maxLat, minLon, maxLon, zoom);
// // }

// // // ── Geocoding (Nominatim) ─────────────────────────────────────────────────────

// // void MainWindow::geocodeSearch(const QString& query)
// // {
// //     // Nominatim OpenStreetMap geocoder — free, no API key required.
// //     // Terms: must set a descriptive User-Agent and not hammer the server.
// //     QUrl url("https://nominatim.openstreetmap.org/search");

// //     QUrlQuery params;
// //     params.addQueryItem("q",              query);
// //     params.addQueryItem("format",         "json");
// //     params.addQueryItem("limit",          "6");
// //     params.addQueryItem("addressdetails", "0");
// //     url.setQuery(params);

// //     QNetworkRequest req(url);
// //     req.setHeader(QNetworkRequest::UserAgentHeader,
// //                   "FlightPlanner/2.0 (logistics UAV mission planner)");
// //     req.setTransferTimeout(8000);

// //     QNetworkReply* reply = m_geocodeNam->get(req);

// //     connect(reply, &QNetworkReply::finished, this, [=]() {
// //         reply->deleteLater();

// //         if (reply->error() != QNetworkReply::NoError) {
// //             emit geocodeError("Network error: " + reply->errorString());
// //             return;
// //         }

// //         QByteArray    data = reply->readAll();
// //         QJsonDocument doc  = QJsonDocument::fromJson(data);

// //         if (!doc.isArray()) {
// //             emit geocodeError("Unexpected response from geocoder.");
// //             return;
// //         }

// //         QJsonArray    arr     = doc.array();
// //         QVariantList  results;

// //         for (const QJsonValue& v : arr) {
// //             QJsonObject obj = v.toObject();
// //             double lat = obj["lat"].toString().toDouble();
// //             double lon = obj["lon"].toString().toDouble();
// //             // displayName: use "display_name" but trim to first two comma-parts
// //             QString fullName = obj["display_name"].toString();
// //             QStringList parts = fullName.split(",");
// //             QString name = parts.size() >= 2
// //                            ? parts[0].trimmed() + ", " + parts[1].trimmed()
// //                            : parts[0].trimmed();

// //             QVariantMap r;
// //             r["name"] = name;
// //             r["lat"]  = lat;
// //             r["lon"]  = lon;
// //             results.append(r);
// //         }

// //         if (results.isEmpty()) {
// //             emit geocodeError("No results found for \"" + query + "\"");
// //         } else {
// //             emit geocodeDone(results);
// //         }
// //     });
// // }

// // // ── PIN management ────────────────────────────────────────────────────────────
// // // PINs are stored as SHA-256 hashes in QSettings under "pins/<section>".
// // // Default PIN for all sections is "1234" (set on first run if not present).

// // static QString hashPin(const QString& pin)
// // {
// //     return QString(QCryptographicHash::hash(
// //         pin.toUtf8(),
// //         QCryptographicHash::Sha256).toHex());
// // }

// // static const QString DEFAULT_PIN = "1234";

// // bool MainWindow::verifyPin(const QString& section, const QString& pin)
// // {
// //     QSettings s("BonV", "FlightPlanner");
// //     QString key      = "pins/" + section;
// //     QString stored   = s.value(key, hashPin(DEFAULT_PIN)).toString();
// //     return hashPin(pin) == stored;
// // }

// // void MainWindow::setPin(const QString& section, const QString& pin)
// // {
// //     QSettings s("BonV", "FlightPlanner");
// //     s.setValue("pins/" + section, hashPin(pin));
// //     s.sync();
// //     appendLog("INFO", "PIN updated for: " + section);
// // }

// // bool MainWindow::hasPin(const QString& section)
// // {
// //     QSettings s("BonV", "FlightPlanner");
// //     return s.contains("pins/" + section);
// // }



// #include "mainwindow.h"

// #include <QFileDialog>
// #include <QDir>
// #include <QNetworkRequest>
// #include <QNetworkReply>
// #include <QJsonDocument>
// #include <QJsonArray>
// #include <QJsonObject>
// #include <QUrlQuery>
// #include <cmath>
// #include <QCoreApplication>


// // ── Constructor ───────────────────────────────────────────────────────────────

// MainWindow::MainWindow(QObject* parent)
//     : QObject(parent)
//     , m_geocodeNam(new QNetworkAccessManager(this))
//     , m_tileStitcher(new TileStitcher(this))
//     , m_droneManager(new DroneManager(this))
//     , m_srtm(new SrtmReader(this))
//     , m_drones(allDroneModels())
// {
//     connect(m_srtm, &SrtmReader::tileReady,
//             this,   &MainWindow::onTileReady);

//     // Wire TileStitcher signals through to QML
//     connect(m_tileStitcher, &TileStitcher::textureReady,
//             this, &MainWindow::tileTextureReady);
//     connect(m_tileStitcher, &TileStitcher::progress,
//             this, &MainWindow::tileProgress);
//     connect(m_tileStitcher, &TileStitcher::statusChanged,
//             this, &MainWindow::tileStatus);
//     connect(m_srtm, &SrtmReader::downloadProgress,
//             this,   &MainWindow::onDownloadProgress);
//     connect(m_srtm, &SrtmReader::downloadError,
//             this,   &MainWindow::onDownloadError);
// }

// // ── Q_INVOKABLE: generateMission ──────────────────────────────────────────────

// void MainWindow::generateMission(
//     const QString& takeoffLat,  const QString& takeoffLon,
//     const QString& fromName,
//     const QString& landLat,     const QString& landLon,
//     const QString& toName,
//     double takeoffAlt,
//     double nearHomeAlt,
//     double cruiseAlt,
//     double dropAlt,
//     double landAlt,
//     double speedMs,
//     double waitS,
//     int    missionTypeIdx,
//     int    tripModeIdx,
//     const QString& droneId,
//     const QString& outputDir)
// {
//     // Build MissionParams from QML strings/values
//     MissionParams p;
//     p.takeoffLat    = takeoffLat.toDouble();
//     p.takeoffLon    = takeoffLon.toDouble();
//     p.landLat       = landLat.toDouble();
//     p.landLon       = landLon.toDouble();
//     p.fromName      = fromName.trimmed();
//     p.toName        = toName.trimmed();
//     p.takeoffAlt    = takeoffAlt;
//     p.nearHomeAlt   = nearHomeAlt;
//     p.cruiseAlt     = cruiseAlt;
//     p.dropAlt       = dropAlt;
//     p.landAlt       = landAlt;
//     p.cruiseSpeedMs = speedMs;
//     p.waitDelayS    = waitS;

//     TripMode    trip  = (tripModeIdx == 1) ? TripMode::Return : TripMode::OneWay;
//     MissionType mtype = static_cast<MissionType>(
//                             qBound(0, missionTypeIdx, 2));

//     // Look up drone from DroneManager by ID
//     QVariantList activeDrones = m_droneManager->activeDrones();
//     DroneModel drone;
//     bool found = false;
//     for (const QVariant& v : activeDrones) {
//         QVariantMap dm = v.toMap();
//         if (dm["id"].toString() == droneId) {
//             drone.id              = dm["id"].toString();
//             drone.displayName     = dm["name"].toString();
//             drone.maxRangeM           = dm["maxOneWayKm"].toDouble() * 1000.0;
//             drone.defaultCruiseSpeed  = dm["maxSpeedMs"].toDouble();
//             drone.defaultTakeoffAlt    = dm["takeoffAlt"].toDouble();
//             drone.defaultNearHomeAlt   = dm["nearHomeAlt"].toDouble();
//             drone.defaultCruiseAlt     = dm["cruiseAlt"].toDouble();
//             drone.defaultLandAlt       = dm["landAlt"].toDouble();
//             drone.rcSwitch             = dm["rcSwitch"].toInt();
//             found = true;
//             break;
//         }
//     }
//     if (!found) {
//         appendLog("ERROR", "Drone not found: " + droneId);
//         return;
//     }

//     // Validate before doing any terrain work
//     auto vr = m_validator.validate(p, drone, trip);
//     if (!vr.ok) {
//         for (const auto& e : vr.errors)
//             appendLog("ERROR", e);
//         setStatus("Validation failed.", false);
//         emit progressChanged(0);
//         return;
//     }
//     for (const auto& w : vr.warnings)
//         appendLog("WARN", w);

//     runGenerate(p, trip, mtype, drone, outputDir);
// }

// // ── Core generation ───────────────────────────────────────────────────────────

// void MainWindow::runGenerate(const MissionParams& p,
//                               TripMode             trip,
//                               MissionType          mtype,
//                               const DroneModel&    drone,
//                               const QString&       outputDir)
// {
//     setStatus("Checking SRTM tile cache...", true);
//     emit progressChanged(5);

//     appendLog("INFO", QString("Mission: %1  |  %2 → %3  |  %4")
//         .arg(missionLabel(mtype))
//         .arg(p.fromName, p.toName)
//         .arg(trip == TripMode::Return ? "Return" : "One-Way"));

//     appendLog("INFO", QString("Takeoff: %1, %2   Landing: %3, %4")
//         .arg(p.takeoffLat, 0, 'f', 7)
//         .arg(p.takeoffLon, 0, 'f', 7)
//         .arg(p.landLat,    0, 'f', 7)
//         .arg(p.landLon,    0, 'f', 7));

//     // Prefetch / check SRTM tiles
//     m_srtm->prefetchTiles(p.takeoffLat, p.takeoffLon, p.landLat, p.landLon);

//     if (!m_srtm->allTilesCached(p.takeoffLat, p.takeoffLon,
//                                  p.landLat,    p.landLon)) {
//         setStatus("Downloading SRTM tile(s)... please wait.", true);
//         appendLog("INFO", "SRTM tiles not cached — downloading in background.");
//         m_pendingGenerate  = true;
//         m_pendingParams    = p;
//         m_pendingTrip      = trip;
//         m_pendingMtype     = mtype;
//         m_pendingDrone     = drone;
//         m_pendingOutputDir = outputDir;
//         return;
//     }

//     // ── Plan outbound path ────────────────────────────────────────────────────
//     emit progressChanged(30);
//     setStatus("Planning outbound path...", true);

//     auto elevFn = [this](double lat, double lon) {
//         return m_srtm->elevation(lat, lon);
//     };

//     QVector<GeoPoint> outAnchors = m_planner.plan(p, elevFn);
//     if (outAnchors.isEmpty()) {
//         setStatus("Path planning failed — elevation data unavailable.", false);
//         appendLog("ERROR", "Could not plan path. Retry after tile download.");
//         emit progressChanged(0);
//         return;
//     }
//     appendLog("INFO", QString("Outbound: %1 waypoints").arg(outAnchors.size()));

//     emit progressChanged(60);

//     // ── Plan return path (if needed) ──────────────────────────────────────────
//     QVector<GeoPoint> retAnchors;
//     if (trip == TripMode::Return) {
//         setStatus("Planning return path...", true);
//         MissionParams retP = p;
//         std::swap(retP.takeoffLat, retP.landLat);
//         std::swap(retP.takeoffLon, retP.landLon);
//         retAnchors = m_planner.plan(retP, elevFn);
//         if (retAnchors.isEmpty()) {
//             setStatus("Return path planning failed.", false);
//             appendLog("ERROR", "Return path failed.");
//             emit progressChanged(0);
//             return;
//         }
//         appendLog("INFO", QString("Return: %1 waypoints").arg(retAnchors.size()));
//     }

//     emit progressChanged(80);
//     setStatus("Writing waypoint file...", true);

//     // ── Write .waypoints file ─────────────────────────────────────────────────
//     double distM = PathPlanner::haversineM(
//         p.takeoffLat, p.takeoffLon, p.landLat, p.landLon);

//     QString outPath = WaypointWriter::buildFilename(
//         outputDir, p, drone, mtype, trip, distM);

//     if (m_writer.write(outPath, p, outAnchors, retAnchors, trip, mtype, drone)) {
//         emit progressChanged(100);
//         setStatus("Done!", false);
//         appendLog("OK", "Saved: " + outPath);

//         // ── Build dense terrain profile (100m sample interval) ──────────────
//         // The waypoint anchors are too sparse for a meaningful profile chart.
//         // We interpolate at 100m intervals along each leg, query SRTM for
//         // terrain height, and linearly interpolate drone altitude between anchors.
//         QVariantList pts;
//         QVariantList waypointMarkers;  // just the anchor points for the map
//         const double SAMPLE_M = 100.0;
//         double cumDistM = 0.0;

//         auto elevFn = [this](double lat, double lon) {
//             return m_srtm->elevation(lat, lon);
//         };

//         // Helper: emit one dense profile point
//         auto addProfilePt = [&](double lat, double lon,
//                                  double droneAlt, double distM) {
//             double terrainH = elevFn(lat, lon);
//             if (std::isnan(terrainH)) terrainH = 0.0;
//             QVariantMap m;
//             m["lat"]        = lat;
//             m["lon"]        = lon;
//             m["droneAlt"]   = droneAlt;
//             m["terrainAlt"] = terrainH;
//             m["distKm"]     = distM / 1000.0;
//             pts.append(m);
//         };

//         // Helper: add map marker for an anchor
//         auto addMapPt = [&](const GeoPoint& gp, double distM) {
//             QVariantMap m;
//             m["lat"]        = gp.lat;
//             m["lon"]        = gp.lon;
//             m["droneAlt"]   = gp.droneAlt;
//             m["terrainAlt"] = gp.terrainH;
//             m["distKm"]     = distM / 1000.0;
//             waypointMarkers.append(m);
//         };

//         // Sample outbound anchors densely
//         for (int i = 0; i < outAnchors.size(); i++) {
//             const GeoPoint& gp = outAnchors[i];

//             if (i == 0) {
//                 addProfilePt(gp.lat, gp.lon, gp.droneAlt, 0.0);
//                 addMapPt(gp, 0.0);
//                 continue;
//             }

//             const GeoPoint& prev = outAnchors[i - 1];
//             double legM = PathPlanner::haversineM(prev.lat, prev.lon, gp.lat, gp.lon);
//             int    steps = std::max(1, static_cast<int>(legM / SAMPLE_M));

//             for (int s = 1; s <= steps; s++) {
//                 double t        = static_cast<double>(s) / steps;
//                 double lat      = prev.lat      + t * (gp.lat      - prev.lat);
//                 double lon      = prev.lon      + t * (gp.lon      - prev.lon);
//                 double droneAlt = prev.droneAlt + t * (gp.droneAlt - prev.droneAlt);
//                 cumDistM       += legM / steps;
//                 addProfilePt(lat, lon, droneAlt, cumDistM);
//             }
//             addMapPt(gp, cumDistM);
//         }

//         // Sample return anchors densely (if any)
//         for (int i = 0; i < retAnchors.size(); i++) {
//             const GeoPoint& gp = retAnchors[i];

//             if (i == 0) {
//                 // Don't duplicate the last outbound point
//                 addMapPt(gp, cumDistM);
//                 continue;
//             }

//             const GeoPoint& prev = retAnchors[i - 1];
//             double legM = PathPlanner::haversineM(prev.lat, prev.lon, gp.lat, gp.lon);
//             int    steps = std::max(1, static_cast<int>(legM / SAMPLE_M));

//             for (int s = 1; s <= steps; s++) {
//                 double t        = static_cast<double>(s) / steps;
//                 double lat      = prev.lat      + t * (gp.lat      - prev.lat);
//                 double lon      = prev.lon      + t * (gp.lon      - prev.lon);
//                 double droneAlt = prev.droneAlt + t * (gp.droneAlt - prev.droneAlt);
//                 cumDistM       += legM / steps;
//                 addProfilePt(lat, lon, droneAlt, cumDistM);
//             }
//             addMapPt(gp, cumDistM);
//         }

//         appendLog("INFO", QString("Terrain profile: %1 sample points at 100m intervals")
//                       .arg(pts.size()));

//         // routeReady carries:
//         //   pts             — dense profile points (for TerrainProfile chart)
//         //   waypointMarkers — sparse anchor points (for map polyline)
//         // QML separates them: map uses waypointMarkers, chart uses pts.
//         // We pack both into a single signal using a wrapper map.
//         QVariantMap payload;
//         payload["profile"]   = pts;
//         payload["waypoints"] = waypointMarkers;
//         emit routeReady(QVariantList{ payload });

//     } else {
//         setStatus("Failed to write output file.", false);
//         appendLog("ERROR", "Write failed: " + outPath);
//     }

//     emit progressChanged(0);
// }

// // ── SrtmReader callbacks ──────────────────────────────────────────────────────

// void MainWindow::onTileReady(int lat, int lon)
// {
//     appendLog("OK", QString("SRTM tile ready: N%1 E%2").arg(lat).arg(lon));
//     emit srtmInfoChanged(QString("N%1 E%2 cached").arg(lat).arg(lon));

//     if (!m_pendingGenerate) return;

//     const MissionParams& p = m_pendingParams;
//     if (m_srtm->allTilesCached(p.takeoffLat, p.takeoffLon,
//                                 p.landLat,    p.landLon)) {
//         m_pendingGenerate = false;
//         appendLog("INFO", "All tiles ready — resuming generation.");
//         runGenerate(p, m_pendingTrip, m_pendingMtype,
//                     m_pendingDrone, m_pendingOutputDir);
//     }
// }

// void MainWindow::onDownloadProgress(int pct)
// {
//     // Map tile download progress into the 0–30 range on the progress bar
//     emit progressChanged(pct / 5);
//     setStatus(QString("Downloading SRTM tile... %1%").arg(pct), true);
// }

// void MainWindow::onDownloadError(const QString& msg)
// {
//     appendLog("ERROR", msg);
//     setStatus("Download failed: " + msg, false);
//     m_pendingGenerate = false;
//     emit progressChanged(0);
// }

// // ── Q_INVOKABLE: browseOutputDir ──────────────────────────────────────────────

// void MainWindow::browseOutputDir()
// {
//     QString dir = QFileDialog::getExistingDirectory(
//         nullptr,
//         "Select output folder",
//         QDir::currentPath());

//     if (!dir.isEmpty())
//         emit outputDirChanged(dir);
// }

// // ── Private helpers ───────────────────────────────────────────────────────────

// void MainWindow::appendLog(const QString& level, const QString& msg)
// {
//     emit logLine(level, msg);
// }

// void MainWindow::setStatus(const QString& msg, bool busy)
// {
//     emit statusChanged(msg, busy);
// }

// void MainWindow::browseSrtmDir()
// {
//     QString dir = QFileDialog::getExistingDirectory(
//         nullptr,
//         "Select SRTM1 terrain cache folder",
//         QDir::currentPath());

//     if (!dir.isEmpty()) {
//         m_srtm->setCacheDir(dir);
//         appendLog("INFO", "SRTM cache dir set to: " + dir);
//         setStatus("SRTM cache updated.", false);
//     }
// }

// // ── 3D Tile fetch ─────────────────────────────────────────────────────────────

// void MainWindow::fetch3DTiles(double minLat, double maxLat,
//                                double minLon, double maxLon,
//                                int tileSource, int zoom)
// {
//     // Prefetch SRTM tiles for the full 3D area (including 5km padding)
//     // so GeoTerrainGeometry.rebuild() can sample elevation synchronously
//     m_srtm->prefetchTiles(minLat, minLon, maxLat, maxLon);

//     // Use same cache dir as the 2D map widget (terrain/tiles beside exe)
//     QString cacheDir = QCoreApplication::applicationDirPath() + "/terrain/tiles";
//     m_tileStitcher->setCacheDir(cacheDir);
//     m_tileStitcher->setTileSource(tileSource);
//     m_tileStitcher->fetchForBounds(minLat, maxLat, minLon, maxLon, zoom);
// }

// // ── Geocoding (Nominatim) ─────────────────────────────────────────────────────

// void MainWindow::geocodeSearch(const QString& query)
// {
//     // Nominatim OpenStreetMap geocoder — free, no API key required.
//     // Terms: must set a descriptive User-Agent and not hammer the server.
//     QUrl url("https://nominatim.openstreetmap.org/search");

//     QUrlQuery params;
//     params.addQueryItem("q",              query);
//     params.addQueryItem("format",         "json");
//     params.addQueryItem("limit",          "6");
//     params.addQueryItem("addressdetails", "0");
//     url.setQuery(params);

//     QNetworkRequest req(url);
//     req.setHeader(QNetworkRequest::UserAgentHeader,
//                   "FlightPlanner/2.0 (logistics UAV mission planner)");
//     req.setTransferTimeout(8000);

//     QNetworkReply* reply = m_geocodeNam->get(req);

//     connect(reply, &QNetworkReply::finished, this, [=]() {
//         reply->deleteLater();

//         if (reply->error() != QNetworkReply::NoError) {
//             emit geocodeError("Network error: " + reply->errorString());
//             return;
//         }

//         QByteArray    data = reply->readAll();
//         QJsonDocument doc  = QJsonDocument::fromJson(data);

//         if (!doc.isArray()) {
//             emit geocodeError("Unexpected response from geocoder.");
//             return;
//         }

//         QJsonArray    arr     = doc.array();
//         QVariantList  results;

//         for (const QJsonValue& v : arr) {
//             QJsonObject obj = v.toObject();
//             double lat = obj["lat"].toString().toDouble();
//             double lon = obj["lon"].toString().toDouble();
//             // displayName: use "display_name" but trim to first two comma-parts
//             QString fullName = obj["display_name"].toString();
//             QStringList parts = fullName.split(",");
//             QString name = parts.size() >= 2
//                            ? parts[0].trimmed() + ", " + parts[1].trimmed()
//                            : parts[0].trimmed();

//             QVariantMap r;
//             r["name"] = name;
//             r["lat"]  = lat;
//             r["lon"]  = lon;
//             results.append(r);
//         }

//         if (results.isEmpty()) {
//             emit geocodeError("No results found for \"" + query + "\"");
//         } else {
//             emit geocodeDone(results);
//         }
//     });
// }

// // ── PIN management ────────────────────────────────────────────────────────────
// // PINs are stored as SHA-256 hashes in QSettings under "pins/<section>".
// // Default PIN for all sections is "1234" (set on first run if not present).

// static QString hashPin(const QString& pin)
// {
//     return QString(QCryptographicHash::hash(
//         pin.toUtf8(),
//         QCryptographicHash::Sha256).toHex());
// }

// static const QString DEFAULT_PIN = "1234";

// bool MainWindow::verifyPin(const QString& section, const QString& pin)
// {
//     QSettings s("BonV", "FlightPlanner");
//     QString key      = "pins/" + section;
//     QString stored   = s.value(key, hashPin(DEFAULT_PIN)).toString();
//     return hashPin(pin) == stored;
// }

// void MainWindow::setPin(const QString& section, const QString& pin)
// {
//     QSettings s("BonV", "FlightPlanner");
//     s.setValue("pins/" + section, hashPin(pin));
//     s.sync();
//     appendLog("INFO", "PIN updated for: " + section);
// }

// bool MainWindow::hasPin(const QString& section)
// {
//     QSettings s("BonV", "FlightPlanner");
//     return s.contains("pins/" + section);
// }



#include "mainwindow.h"

#include <QFileDialog>
#include <QDir>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <cmath>
#include <algorithm>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

namespace {
bool canWriteDirectory(const QString& dir)
{
    QDir().mkpath(dir);

    const QString probePath = QDir(dir).filePath(".write_test");
    QFile probe(probePath);
    if (!probe.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    probe.write("ok");
    probe.close();
    QFile::remove(probePath);
    return true;
}

QString writableSkyRouteXRoot()
{
#ifdef Q_OS_ANDROID
    return "/storage/emulated/0/Documents/SkyRouteX";
#else
    QStringList candidates;
    candidates << QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
               << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    candidates << QDir::homePath();

    for (const QString& baseDir : candidates) {
        if (baseDir.isEmpty())
            continue;

        const QString root = QDir(baseDir).filePath("SkyRouteX");
        if (canWriteDirectory(root))
            return root;
    }

    return QDir::temp().filePath("SkyRouteX");
#endif
}

QString srtmTileFileName(int lat, int lon)
{
    const QString ns = (lat >= 0) ? "N" : "S";
    const QString ew = (lon >= 0) ? "E" : "W";
    return QString("%1%2%3%4.hgt")
        .arg(ns).arg(std::abs(lat), 2, 10, QChar('0'))
        .arg(ew).arg(std::abs(lon), 3, 10, QChar('0'));
}
}


// ── Constructor ───────────────────────────────────────────────────────────────

MainWindow::MainWindow(QObject* parent)
    : QObject(parent)
    , m_geocodeNam(new QNetworkAccessManager(this))
    , m_tileStitcher(new TileStitcher(this))
    , m_droneManager(new DroneManager(this))
    , m_srtm(new SrtmReader(this))
    , m_drones(allDroneModels())
{
    connect(m_srtm, &SrtmReader::tileReady,
            this,   &MainWindow::onTileReady);

    // Wire TileStitcher signals through to QML
    connect(m_tileStitcher, &TileStitcher::textureReady,
            this, &MainWindow::tileTextureReady);
    connect(m_tileStitcher, &TileStitcher::progress,
            this, &MainWindow::tileProgress);
    connect(m_tileStitcher, &TileStitcher::statusChanged,
            this, &MainWindow::tileStatus);
    connect(m_srtm, &SrtmReader::downloadProgress,
            this,   &MainWindow::onDownloadProgress);
    connect(m_srtm, &SrtmReader::downloadError,
            this,   &MainWindow::onDownloadError);

    QDir().mkpath(defaultOutputDir());
    QDir().mkpath(srtmCacheDir());
    QDir().mkpath(mapTilesCacheDir());
    m_srtm->setCacheDir(srtmCacheDir());
    m_tileStitcher->setAllowOnlineDownload(onlineTileDownloadAllowed());
}

QString MainWindow::defaultOutputDir() const
{
    return writableSkyRouteXRoot() + "/Waypoints";
}

QString MainWindow::srtmCacheDir() const
{
    return writableSkyRouteXRoot() + "/SRTM";
}

QString MainWindow::mapTilesCacheDir() const
{
    return writableSkyRouteXRoot() + "/MapTiles";
}

bool MainWindow::onlineTileDownloadAllowed() const
{
    return QSettings().value("offline/onlineTileDownloadAllowed", true).toBool();
}

void MainWindow::setOnlineTileDownloadAllowed(bool allowed)
{
    QSettings().setValue("offline/onlineTileDownloadAllowed", allowed);
    m_tileStitcher->setAllowOnlineDownload(allowed);
    appendLog("INFO", allowed
              ? "Online map tile download enabled."
              : "Offline map mode enabled. Cached map tiles only.");
}

// ── Q_INVOKABLE: generateMission ──────────────────────────────────────────────

void MainWindow::generateMission(
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
    int    missionTypeIdx,
    int    tripModeIdx,
    int    planModeIdx,
    const QVariantList& waypointMergerRoute,
    const QString& droneId,
    const QString& outputDir)
{
    // Build MissionParams from QML strings/values
    MissionParams p;
    p.takeoffLat    = takeoffLat.toDouble();
    p.takeoffLon    = takeoffLon.toDouble();
    p.landLat       = landLat.toDouble();
    p.landLon       = landLon.toDouble();
    p.fromName      = fromName.trimmed();
    p.toName        = toName.trimmed();
    p.takeoffAlt    = takeoffAlt;
    p.nearHomeAlt   = nearHomeAlt;
    p.cruiseAlt     = cruiseAlt;
    p.dropAlt       = dropAlt;
    p.landAlt       = landAlt;
    p.cruiseSpeedMs = speedMs;
    p.waitDelayS    = waitS;
    p.planMode      = qBound(0, planModeIdx, 4);

    TripMode    trip  = (tripModeIdx == 1) ? TripMode::Return : TripMode::OneWay;
    MissionType mtype = static_cast<MissionType>(
                            qBound(0, missionTypeIdx, 3));
    if (mtype == MissionType::Drop || mtype == MissionType::Paradrop || mtype == MissionType::LiDarDrop)
        trip = TripMode::Return;

    // Look up drone from DroneManager by ID
    QVariantList activeDrones = m_droneManager->activeDrones();
    DroneModel drone;
    bool found = false;
    for (const QVariant& v : activeDrones) {
        QVariantMap dm = v.toMap();
        if (dm["id"].toString() == droneId) {
            drone.id              = dm["id"].toString();
            drone.displayName     = dm["name"].toString();
            drone.maxRangeM           = dm["maxOneWayKm"].toDouble() * 1000.0;
            drone.defaultCruiseSpeed  = dm["maxSpeedMs"].toDouble();
            drone.defaultTakeoffAlt    = dm["takeoffAlt"].toDouble();
            drone.defaultNearHomeAlt   = dm["nearHomeAlt"].toDouble();
            drone.defaultCruiseAlt     = dm["cruiseAlt"].toDouble();
            drone.defaultLandAlt       = dm["landAlt"].toDouble();
            drone.minTakeoffAlt        = dm.value("minTakeoffAlt", 10.0).toDouble();
            drone.maxTakeoffAlt        = dm.value("maxTakeoffAlt", 500.0).toDouble();
            drone.minNearHomeAlt       = dm.value("minNearHomeAlt", 20.0).toDouble();
            drone.maxNearHomeAlt       = dm.value("maxNearHomeAlt", 500.0).toDouble();
            drone.minCruiseAlt         = dm.value("minCruiseAlt", 80.0).toDouble();
            drone.maxCruiseAlt         = dm.value("maxCruiseAlt", 800.0).toDouble();
            drone.minLandAlt           = dm.value("minLandAlt", 80.0).toDouble();
            drone.maxLandAlt           = dm.value("maxLandAlt", 150.0).toDouble();
            drone.minCruiseSpeed       = dm.value("minSpeedMs", 1.0).toDouble();
            drone.maxCruiseSpeed       = dm.value("maxSpeedMs", 15.0).toDouble();
            drone.defaultAscentSpeed    = dm.value("ascentSpeedMs", 1.5).toDouble();
            drone.defaultDescentSpeed   = dm.value("descentSpeedMs", 0.5).toDouble();
            drone.maxElevationDifferenceM = dm.value("maxElevationDifferenceM", 9999.0).toDouble();
            drone.allowNormalPlan       = dm.value("planNormal", true).toBool();
            drone.allowPeakXPlan        = dm.value("planPeakX", true).toBool();
            drone.allowRouteXPlan       = dm.value("planRouteX", true).toBool();
            drone.allowNexusXPlan      = dm.value("planNexusX", true).toBool();
            drone.allowFusionXPlan     = dm.value("planFusionX", true).toBool();
            p.ascentSpeedMs             = drone.defaultAscentSpeed > 0.0 ? drone.defaultAscentSpeed : 1.5;
            p.descentSpeedMs            = drone.defaultDescentSpeed > 0.0 ? drone.defaultDescentSpeed : 0.5;
            p.verticalUpSpeedMs          = dm.value("verticalUpSpeedMs", 1.0).toDouble() > 0.0
                                               ? dm.value("verticalUpSpeedMs", 1.0).toDouble()
                                               : 1.0;
            p.verticalDownSpeedMs        = dm.value("verticalDownSpeedMs", 0.5).toDouble() > 0.0
                                               ? dm.value("verticalDownSpeedMs", 0.5).toDouble()
                                               : 0.5;
            drone.rcSwitch             = dm["rcSwitch"].toInt();
            // rcCount stored in DroneManager but not in DroneModel struct —
            // pass via MissionParams instead
            // p.rcTriggerCount           = dm["rcCount"].toInt(1);
            p.rcTriggerCount = dm.contains("rcCount")
                                   ? dm["rcCount"].toInt()
                                   : 1;
            found = true;
            break;
        }
    }
    if (!found) {
        appendLog("ERROR", "Drone not found: " + droneId);
        return;
    }

    const bool planAllowed = (p.planMode == 0 && drone.allowNormalPlan)
                          || (p.planMode == 1 && drone.allowPeakXPlan)
                          || (p.planMode == 2 && drone.allowRouteXPlan)
                          || (p.planMode == 3 && drone.allowNexusXPlan)
                          || (p.planMode == 4 && drone.allowFusionXPlan);
    if (!planAllowed) {
        appendLog("ERROR", "Selected plan type is disabled for this drone.");
        setStatus("Plan type disabled for selected drone.", false);
        emit progressChanged(0);
        return;
    }

    // Validate before doing any terrain work
    auto vr = m_validator.validate(p, drone, trip);
    if (!vr.ok) {
        for (const auto& e : vr.errors)
            appendLog("ERROR", e);
        setStatus("Validation failed.", false);
        emit progressChanged(0);
        return;
    }
    for (const auto& w : vr.warnings)
        appendLog("WARN", w);

    runGenerate(p, trip, mtype, drone, outputDir, waypointMergerRoute);
}

struct TimePoint {
    double lat = 0.0;
    double lon = 0.0;
    double alt = 0.0;
};

static double estimateLegTimeS(const TimePoint& a,
                               const TimePoint& b,
                               const MissionParams& p)
{
    const double horizontalM = PathPlanner::haversineM(a.lat, a.lon, b.lat, b.lon);
    const double horizontalS = p.cruiseSpeedMs > 0.0 ? horizontalM / p.cruiseSpeedMs : 0.0;
    const double verticalM = b.alt - a.alt;
    const bool pureVertical = horizontalM < 1.0;
    const double upSpeed = pureVertical ? p.verticalUpSpeedMs : p.ascentSpeedMs;
    const double downSpeed = pureVertical ? p.verticalDownSpeedMs : p.descentSpeedMs;
    const double verticalS = verticalM > 0.0
        ? (upSpeed > 0.0 ? verticalM / upSpeed : 0.0)
        : (downSpeed > 0.0 ? std::abs(verticalM) / downSpeed : 0.0);

    return std::max(horizontalS, verticalS);
}

static void appendTimedLeg(QVector<TimePoint>& route, double lat, double lon, double alt)
{
    route.append({lat, lon, alt});
}

static QVector<TimePoint> deliveryLegPoints(const QVector<GeoPoint>& anchors,
                                            const MissionParams& p,
                                            bool includeTakeoff,
                                            bool includeLanding)
{
    QVector<TimePoint> route;
    if (anchors.isEmpty())
        return route;

    const GeoPoint& start = anchors.first();
    const GeoPoint& end = anchors.last();

    if (includeTakeoff) {
        appendTimedLeg(route, start.lat, start.lon, start.terrainH);
        appendTimedLeg(route, start.lat, start.lon, start.terrainH + p.takeoffAlt);
        appendTimedLeg(route, start.lat, start.lon, start.terrainH + p.nearHomeAlt);
    } else {
        appendTimedLeg(route, start.lat, start.lon, start.droneAlt);
    }

    appendTimedLeg(route, start.lat, start.lon, start.droneAlt);
    for (int i = 1; i < anchors.size() - 1; ++i)
        appendTimedLeg(route, anchors[i].lat, anchors[i].lon, anchors[i].droneAlt);
    appendTimedLeg(route, end.lat, end.lon, end.droneAlt);

    if (includeLanding)
        appendTimedLeg(route, end.lat, end.lon, end.terrainH);

    return route;
}

static double estimatePointsTimeS(const QVector<TimePoint>& route,
                                  const MissionParams& p)
{
    double totalS = 0.0;
    for (int i = 1; i < route.size(); ++i)
        totalS += estimateLegTimeS(route[i - 1], route[i], p);
    return totalS;
}

static double estimateFlightTimeS(const QVector<GeoPoint>& outAnchors,
                                  const QVector<GeoPoint>& retAnchors,
                                  const MissionParams& p,
                                  TripMode trip,
                                  MissionType mtype)
{
    double totalS = estimatePointsTimeS(
        deliveryLegPoints(outAnchors, p, true, mtype == MissionType::Delivery),
        p);

    if (mtype == MissionType::Delivery) {
        if (trip == TripMode::Return && !retAnchors.isEmpty()) {
            totalS += std::max(0.0, p.waitDelayS);
            totalS += 5.0;
            totalS += estimatePointsTimeS(deliveryLegPoints(retAnchors, p, true, true), p);
        }
        return totalS;
    }

    if (!outAnchors.isEmpty() && !retAnchors.isEmpty()) {
        const GeoPoint& drop = outAnchors.last();
        const double dropAlt = drop.terrainH + p.dropAlt;
        totalS += estimateLegTimeS({drop.lat, drop.lon, drop.droneAlt},
                                   {drop.lat, drop.lon, dropAlt}, p);

        if (mtype == MissionType::LiDarDrop) {
            totalS += p.waitDelayS > 0.0 ? p.waitDelayS : 5.0;
            totalS += estimateLegTimeS({drop.lat, drop.lon, dropAlt},
                                       {drop.lat, drop.lon, drop.terrainH}, p);
            totalS += estimateLegTimeS({drop.lat, drop.lon, drop.terrainH},
                                       {drop.lat, drop.lon, retAnchors.first().droneAlt}, p);
        } else {
            totalS += std::max(0.0, p.waitDelayS);
            totalS += std::max(1, p.rcTriggerCount) * 6.0;
            totalS += estimateLegTimeS({drop.lat, drop.lon, dropAlt},
                                       {drop.lat, drop.lon, retAnchors.first().droneAlt}, p);
        }

        QVector<TimePoint> returnRoute;
        appendTimedLeg(returnRoute, retAnchors.first().lat, retAnchors.first().lon,
                       retAnchors.first().droneAlt);
        for (int i = 1; i < retAnchors.size() - 1; ++i)
            appendTimedLeg(returnRoute, retAnchors[i].lat, retAnchors[i].lon,
                           retAnchors[i].droneAlt);
        appendTimedLeg(returnRoute, retAnchors.last().lat, retAnchors.last().lon,
                       retAnchors.last().droneAlt);
        appendTimedLeg(returnRoute, retAnchors.last().lat, retAnchors.last().lon,
                       retAnchors.last().terrainH);
        totalS += estimatePointsTimeS(returnRoute, p);
    }

    return totalS;
}

static QString formatFlightTime(double seconds)
{
    const int total = std::max(0, static_cast<int>(std::round(seconds)));
    const int h = total / 3600;
    const int m = (total % 3600) / 60;
    const int s = total % 60;

    if (h > 0)
        return QString("%1 h %2 min %3 sec").arg(h).arg(m).arg(s);
    if (m > 0)
        return QString("%1 min %2 sec").arg(m).arg(s);
    return QString("%1 sec").arg(s);
}

static double routeElevationDifferenceM(const QVector<GeoPoint>& anchors,
                                        const PathPlanner::ElevFn& elevFn)
{
    if (anchors.isEmpty())
        return 0.0;

    const double takeoffH = anchors.first().terrainH;
    const double landingH = anchors.last().terrainH;
    double highestH = std::max(takeoffH, landingH);
    const double sampleM = 50.0;

    for (int i = 0; i < anchors.size(); ++i) {
        highestH = std::max(highestH, anchors[i].terrainH);
        if (i == 0)
            continue;

        const GeoPoint& a = anchors[i - 1];
        const GeoPoint& b = anchors[i];
        const double legM = PathPlanner::haversineM(a.lat, a.lon, b.lat, b.lon);
        const int steps = std::max(1, static_cast<int>(std::ceil(legM / sampleM)));

        for (int s = 1; s < steps; ++s) {
            const double t = static_cast<double>(s) / steps;
            const double lat = a.lat + t * (b.lat - a.lat);
            const double lon = a.lon + t * (b.lon - a.lon);
            const double terrainH = elevFn(lat, lon);
            if (!std::isnan(terrainH))
                highestH = std::max(highestH, terrainH);
        }
    }

    return std::max(0.0, highestH - takeoffH)
         + std::max(0.0, highestH - landingH);
}

// ── Core generation ───────────────────────────────────────────────────────────

void MainWindow::runGenerate(const MissionParams& p,
                              TripMode             trip,
                              MissionType          mtype,
                              const DroneModel&    drone,
                              const QString&       outputDir,
                              const QVariantList&  waypointMergerRoute)
{
    QString effectiveOutputDir = outputDir.trimmed();
    if (effectiveOutputDir.isEmpty())
        effectiveOutputDir = defaultOutputDir();

    if (!QDir().mkpath(effectiveOutputDir)) {
        appendLog("ERROR", "Could not create output folder: " + effectiveOutputDir);
        setStatus("Failed to create output folder.", false);
        emit progressChanged(0);
        return;
    }

    QFileInfo outInfo(effectiveOutputDir);
    if (!outInfo.exists() || !outInfo.isDir() || !outInfo.isWritable()) {
        appendLog("ERROR", "Output folder is not writable: " + effectiveOutputDir);
        setStatus("Output folder is not writable.", false);
        emit progressChanged(0);
        return;
    }

    setStatus("Checking SRTM tile cache...", true);
    emit progressChanged(5);

    appendLog("INFO", QString("Mission: %1  |  %2 → %3  |  %4")
        .arg(missionLabel(mtype))
        .arg(p.fromName, p.toName)
        .arg(trip == TripMode::Return ? "Return" : "One-Way"));

    appendLog("INFO", QString("Takeoff: %1, %2   Landing: %3, %4")
        .arg(p.takeoffLat, 0, 'f', 7)
        .arg(p.takeoffLon, 0, 'f', 7)
        .arg(p.landLat,    0, 'f', 7)
        .arg(p.landLon,    0, 'f', 7));
    const auto planLabel = [](int mode) {
        if (mode == 1) return QStringLiteral("PeakX");
        if (mode == 2) return QStringLiteral("RouteX");
        if (mode == 3) return QStringLiteral("NexusX");
        if (mode == 4) return QStringLiteral("FusionX");
        return QStringLiteral("Normal");
    };
    appendLog("INFO", QString("Plan type: %1").arg(planLabel(p.planMode)));

    QVector<QPair<double, double>> mergerCoords;
    if (p.planMode == 2) {
        mergerCoords.reserve(waypointMergerRoute.size());
        for (const QVariant& v : waypointMergerRoute) {
            const QVariantMap m = v.toMap();
            bool latOk = false;
            bool lonOk = false;
            const double lat = m.value("lat").toDouble(&latOk);
            const double lon = m.value("lon").toDouble(&lonOk);
            if (latOk && lonOk && lat >= -90.0 && lat <= 90.0
                    && lon >= -180.0 && lon <= 180.0) {
                mergerCoords.append({lat, lon});
            }
        }

        if (mergerCoords.size() < 2) {
            appendLog("ERROR", "RouteX needs at least Takeoff and final point.");
            setStatus("RouteX route is incomplete.", false);
            emit progressChanged(0);
            return;
        }
        appendLog("INFO", QString("RouteX route: %1 user points")
                  .arg(mergerCoords.size()));
    }

    // Prefetch / check SRTM tiles
    double cacheMinLat = std::min(p.takeoffLat, p.landLat);
    double cacheMaxLat = std::max(p.takeoffLat, p.landLat);
    double cacheMinLon = std::min(p.takeoffLon, p.landLon);
    double cacheMaxLon = std::max(p.takeoffLon, p.landLon);
    for (const auto& pt : mergerCoords) {
        cacheMinLat = std::min(cacheMinLat, pt.first);
        cacheMaxLat = std::max(cacheMaxLat, pt.first);
        cacheMinLon = std::min(cacheMinLon, pt.second);
        cacheMaxLon = std::max(cacheMaxLon, pt.second);
    }

    if (!m_srtm->allTilesCached(cacheMinLat, cacheMinLon,
                                cacheMaxLat, cacheMaxLon)) {
        setStatus("Downloading terrain data... please wait.", true);
        appendLog("INFO", "SRTM tile(s) missing in local cache — downloading from internet.");
        m_pendingGenerate  = true;
        m_pendingParams    = p;
        m_pendingTrip      = trip;
        m_pendingMtype     = mtype;
        m_pendingDrone     = drone;
        m_pendingOutputDir = effectiveOutputDir;
        m_pendingWaypointMergerRoute = waypointMergerRoute;
        m_pendingCacheMinLat = cacheMinLat;
        m_pendingCacheMaxLat = cacheMaxLat;
        m_pendingCacheMinLon = cacheMinLon;
        m_pendingCacheMaxLon = cacheMaxLon;
        m_srtm->prefetchTiles(cacheMinLat, cacheMinLon, cacheMaxLat, cacheMaxLon);
        return;
    }
    appendLog("INFO", "Terrain data ready.");
    m_srtm->prefetchTiles(cacheMinLat, cacheMinLon, cacheMaxLat, cacheMaxLon);

    // ── Plan outbound path ────────────────────────────────────────────────────
    emit progressChanged(30);
    setStatus(p.planMode == 1 ? "Planning outbound low-climb path..."
                               : p.planMode == 2 ? "Using RouteX route..."
                                                  : "Planning outbound path...", true);

    auto elevFn = [this](double lat, double lon) {
        return m_srtm->elevation(lat, lon);
    };

    QVector<GeoPoint> outAnchors;
    if (p.planMode == 2) {
        outAnchors = m_planner.planWaypointMerger(p, mergerCoords, elevFn);
    } else {
        outAnchors = m_planner.plan(p, elevFn);
    }
    if (outAnchors.isEmpty()) {
        setStatus("Path planning failed — elevation data unavailable.", false);
        appendLog("ERROR", "Could not plan path. Retry after tile download.");
        emit progressChanged(0);
        return;
    }
    appendLog("INFO", QString("Outbound: %1 waypoints").arg(outAnchors.size()));

    emit progressChanged(60);

    // ── Plan return path (if needed) ──────────────────────────────────────────
    QVector<GeoPoint> retAnchors;
    if (trip == TripMode::Return) {
        setStatus(p.planMode == 1 ? "Planning return low-climb path..."
                                   : p.planMode == 2 ? "Using reversed RouteX route..."
                                                      : "Planning return path...", true);
        if (p.planMode == 2) {
            retAnchors = outAnchors;
            std::reverse(retAnchors.begin(), retAnchors.end());
        } else {
            MissionParams retP = p;
            std::swap(retP.takeoffLat, retP.landLat);
            std::swap(retP.takeoffLon, retP.landLon);
            retAnchors = m_planner.plan(retP, elevFn);
        }
        if (retAnchors.isEmpty()) {
            setStatus("Return path planning failed.", false);
            appendLog("ERROR", "Return path failed.");
            emit progressChanged(0);
            return;
        }
        appendLog("INFO", QString("Return: %1 waypoints").arg(retAnchors.size()));
    }

    const double elevationDifferenceM = routeElevationDifferenceM(outAnchors, elevFn);
    appendLog("INFO", QString("Elevation Difference: %1 m / limit %2 m")
              .arg(elevationDifferenceM, 0, 'f', 0)
              .arg(drone.maxElevationDifferenceM, 0, 'f', 0));
    if (drone.maxElevationDifferenceM > 0.0
            && elevationDifferenceM > drone.maxElevationDifferenceM) {
        appendLog("ERROR", QString("Mission blocked: Elevation Difference %1 m exceeds drone limit %2 m.")
                  .arg(elevationDifferenceM, 0, 'f', 0)
                  .arg(drone.maxElevationDifferenceM, 0, 'f', 0));
        setStatus("Elevation Difference limit exceeded.", false);
        emit progressChanged(0);
        return;
    }

    emit progressChanged(80);
    setStatus("Writing waypoint file...", true);

    // ── Write .waypoints file ─────────────────────────────────────────────────
    double distM = PathPlanner::haversineM(
        p.takeoffLat, p.takeoffLon, p.landLat, p.landLon);

    QString outPath = WaypointWriter::buildFilename(
        effectiveOutputDir, p, drone, mtype, trip, distM);

    if (m_writer.write(outPath, p, outAnchors, retAnchors, trip, mtype, drone)) {
        const double flightTimeS = estimateFlightTimeS(outAnchors, retAnchors, p, trip, mtype);
        const QString flightTimeText = formatFlightTime(flightTimeS);
        emit progressChanged(100);
        setStatus("Done! Est. flight time: " + flightTimeText, false);
        appendLog("OK", "Saved: " + outPath);
        appendLog("INFO", "Estimated flight time: " + flightTimeText);

        // ── Build dense terrain profile (100m sample interval) ──────────────
        // The waypoint anchors are too sparse for a meaningful profile chart.
        // We interpolate at 100m intervals along each leg, query SRTM for
        // terrain height, and linearly interpolate drone altitude between anchors.
        QVariantList pts;
        QVariantList waypointMarkers;  // just the anchor points for the map
        const double SAMPLE_M = 100.0;
        double cumDistM = 0.0;

        auto elevFn = [this](double lat, double lon) {
            return m_srtm->elevation(lat, lon);
        };

        // Helper: emit one dense profile point
        auto addProfilePt = [&](double lat, double lon,
                                 double droneAlt, double distM,
                                 bool ignoreClearance = false) {
            double terrainH = elevFn(lat, lon);
            if (std::isnan(terrainH)) terrainH = 0.0;
            QVariantMap m;
            m["lat"]        = lat;
            m["lon"]        = lon;
            m["droneAlt"]   = droneAlt;
            m["terrainAlt"] = terrainH;
            m["distKm"]     = distM / 1000.0;
            if (ignoreClearance)
                m["ignoreClearance"] = true;
            pts.append(m);
        };

        // Helper: add map marker for an anchor
        auto addMapPt = [&](const GeoPoint& gp, double distM) {
            QVariantMap m;
            m["lat"]        = gp.lat;
            m["lon"]        = gp.lon;
            m["droneAlt"]   = gp.droneAlt;
            m["terrainAlt"] = gp.terrainH;
            m["distKm"]     = distM / 1000.0;
            waypointMarkers.append(m);
        };

        const bool hasDropVertical =
            (mtype == MissionType::Drop
             || mtype == MissionType::Paradrop
             || mtype == MissionType::LiDarDrop);

        // Sample outbound anchors densely
        for (int i = 0; i < outAnchors.size(); i++) {
            const GeoPoint& gp = outAnchors[i];

            if (i == 0) {
                addProfilePt(gp.lat, gp.lon, gp.terrainH, 0.0, true);
                addProfilePt(gp.lat, gp.lon, gp.droneAlt, 0.0);
                addMapPt(gp, 0.0);
                continue;
            }

            const GeoPoint& prev = outAnchors[i - 1];
            double legM = PathPlanner::haversineM(prev.lat, prev.lon, gp.lat, gp.lon);
            int    steps = std::max(1, static_cast<int>(legM / SAMPLE_M));

            for (int s = 1; s <= steps; s++) {
                double t        = static_cast<double>(s) / steps;
                double lat      = prev.lat      + t * (gp.lat      - prev.lat);
                double lon      = prev.lon      + t * (gp.lon      - prev.lon);
                double droneAlt = prev.droneAlt + t * (gp.droneAlt - prev.droneAlt);
                cumDistM       += legM / steps;
                addProfilePt(lat, lon, droneAlt, cumDistM);
            }
            addMapPt(gp, cumDistM);
        }

        if (hasDropVertical && !outAnchors.isEmpty() && !retAnchors.isEmpty()) {
            const GeoPoint& drop = outAnchors.last();
            const double dropAltitude = drop.terrainH + p.dropAlt;
            addProfilePt(drop.lat, drop.lon, dropAltitude, cumDistM);

            if (mtype == MissionType::LiDarDrop)
                addProfilePt(drop.lat, drop.lon, drop.terrainH, cumDistM, true);

            addProfilePt(drop.lat, drop.lon, retAnchors.first().droneAlt, cumDistM);
        }

        if (!outAnchors.isEmpty() && retAnchors.isEmpty()) {
            const GeoPoint& gp = outAnchors.last();
            addProfilePt(gp.lat, gp.lon, gp.terrainH, cumDistM, true);
        }

        // Sample return anchors densely (if any)
        for (int i = 0; i < retAnchors.size(); i++) {
            const GeoPoint& gp = retAnchors[i];

            if (i == 0) {
                // Don't duplicate the last outbound point
                addMapPt(gp, cumDistM);
                continue;
            }

            const GeoPoint& prev = retAnchors[i - 1];
            double legM = PathPlanner::haversineM(prev.lat, prev.lon, gp.lat, gp.lon);
            int    steps = std::max(1, static_cast<int>(legM / SAMPLE_M));

            for (int s = 1; s <= steps; s++) {
                double t        = static_cast<double>(s) / steps;
                double lat      = prev.lat      + t * (gp.lat      - prev.lat);
                double lon      = prev.lon      + t * (gp.lon      - prev.lon);
                double droneAlt = prev.droneAlt + t * (gp.droneAlt - prev.droneAlt);
                cumDistM       += legM / steps;
                addProfilePt(lat, lon, droneAlt, cumDistM);
            }
            addMapPt(gp, cumDistM);
        }

        if (!retAnchors.isEmpty()) {
            const GeoPoint& gp = retAnchors.last();
            addProfilePt(gp.lat, gp.lon, gp.terrainH, cumDistM, true);
        }
        auto missionFileWaypoints = [&]() {
            QVariantList missionPts;
            QFile f(outPath);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                return missionPts;

            const double relativeHomeAlt = outAnchors.isEmpty() ? 0.0 : outAnchors.first().terrainH;
            double prevLat = 0.0;
            double prevLon = 0.0;
            double fileDistM = 0.0;
            bool hasPrev = false;

            QTextStream in(&f);
            while (!in.atEnd()) {
                const QString line = in.readLine().trimmed();
                if (line.isEmpty() || line.startsWith("QGC"))
                    continue;

                const QStringList c = line.split('\t');
                if (c.size() < 11)
                    continue;

                bool okFrame = false, okCmd = false, okLat = false, okLon = false, okAlt = false;
                const int frame = c[2].toInt(&okFrame);
                const int cmd = c[3].toInt(&okCmd);
                const double lat = c[8].toDouble(&okLat);
                const double lon = c[9].toDouble(&okLon);
                const double fileAlt = c[10].toDouble(&okAlt);
                if (!okFrame || !okCmd || !okLat || !okLon || !okAlt)
                    continue;
                if (std::abs(lat) < 0.0000001 && std::abs(lon) < 0.0000001)
                    continue;
                if (cmd != 16 && cmd != 21 && cmd != 22)
                    continue;

                if (hasPrev)
                    fileDistM += PathPlanner::haversineM(prevLat, prevLon, lat, lon);

                double terrainH = elevFn(lat, lon);
                if (std::isnan(terrainH))
                    terrainH = 0.0;

                QVariantMap m;
                m["lat"] = lat;
                m["lon"] = lon;
                m["droneAlt"] = frame == 3 ? fileAlt + relativeHomeAlt : fileAlt;
                m["terrainAlt"] = terrainH;
                m["distKm"] = fileDistM / 1000.0;
                missionPts.append(m);

                prevLat = lat;
                prevLon = lon;
                hasPrev = true;
            }
            return missionPts;
        }();

        if (missionFileWaypoints.size() >= 2)
            waypointMarkers = missionFileWaypoints;


        appendLog("INFO", QString("Terrain profile: %1 sample points at 100m intervals")
                      .arg(pts.size()));

        // routeReady carries:
        //   pts             — dense profile points (for TerrainProfile chart)
        //   waypointMarkers — sparse anchor points (for map polyline)
        // QML separates them: map uses waypointMarkers, chart uses pts.
        // We pack both into a single signal using a wrapper map.
        QVariantMap payload;
        payload["profile"]   = pts;
        payload["waypoints"] = waypointMarkers;
        payload["flightTimeSec"] = flightTimeS;
        payload["flightTimeText"] = flightTimeText;
        emit routeReady(QVariantList{ payload });

    } else {
        setStatus("Failed to write output file.", false);
        appendLog("ERROR", "Write failed: " + outPath);
    }

    emit progressChanged(0);
}

// ── SrtmReader callbacks ──────────────────────────────────────────────────────

void MainWindow::onTileReady(int lat, int lon)
{
    const QString tileName = srtmTileFileName(lat, lon);
    appendLog("OK", "Terrain tile ready: " + tileName);
    emit srtmInfoChanged(tileName + " cached");

    if (!m_pendingGenerate) return;

    if (m_srtm->allTilesCached(m_pendingCacheMinLat, m_pendingCacheMinLon,
                                m_pendingCacheMaxLat, m_pendingCacheMaxLon)) {
        m_pendingGenerate = false;
        appendLog("INFO", "All tiles ready — resuming generation.");
        runGenerate(m_pendingParams, m_pendingTrip, m_pendingMtype,
                    m_pendingDrone, m_pendingOutputDir,
                    m_pendingWaypointMergerRoute);
    }
}

void MainWindow::onDownloadProgress(int pct)
{
    // Map tile download progress into the 0–30 range on the progress bar
    emit progressChanged(pct / 5);
    setStatus(QString("Downloading terrain data... %1%").arg(pct), true);
}

void MainWindow::onDownloadError(const QString& msg)
{
    appendLog("ERROR", msg);
    setStatus("Terrain data download failed.", false);
    m_pendingGenerate = false;
    emit progressChanged(0);
}

// ── Q_INVOKABLE: browseOutputDir ──────────────────────────────────────────────

void MainWindow::browseOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(
        nullptr,
        "Select output folder",
        QDir::currentPath());

    if (!dir.isEmpty())
        emit outputDirChanged(dir);
}

// ── Private helpers ───────────────────────────────────────────────────────────

void MainWindow::appendLog(const QString& level, const QString& msg)
{
    QString userMsg = msg;
    if (userMsg.contains("terrain.ardupilot.org") || userMsg.contains("TLS supported")) {
        userMsg = "Terrain data is not available. Check internet or preload the required terrain tile.";
    } else if (userMsg.contains("SRTM tile(s) missing")) {
        userMsg = "Terrain data missing locally. Downloading required tile(s)...";
    } else if (userMsg.contains("SRTM cache dir")) {
        userMsg = "Terrain cache folder updated.";
    }

    emit logLine(level, userMsg);
}

void MainWindow::setStatus(const QString& msg, bool busy)
{
    emit statusChanged(msg, busy);
}

void MainWindow::browseSrtmDir()
{
    QString dir = QFileDialog::getExistingDirectory(
        nullptr,
        "Select SRTM1 terrain cache folder",
        QDir::currentPath());

    if (!dir.isEmpty()) {
        m_srtm->setCacheDir(dir);
        appendLog("INFO", "Terrain cache folder updated.");
        setStatus("Terrain cache updated.", false);
    }
}

// ── 3D Tile fetch ─────────────────────────────────────────────────────────────

void MainWindow::fetch3DTiles(double minLat, double maxLat,
                               double minLon, double maxLon,
                               int tileSource, int zoom)
{
    // Prefetch SRTM tiles for the full 3D area (including 5km padding)
    // so GeoTerrainGeometry.rebuild() can sample elevation synchronously
    m_srtm->prefetchTiles(minLat, minLon, maxLat, maxLon);

    m_tileStitcher->setCacheDir(mapTilesCacheDir());
    m_tileStitcher->setTileSource(tileSource);
    m_tileStitcher->setAllowOnlineDownload(onlineTileDownloadAllowed());
    m_tileStitcher->fetchForBounds(minLat, maxLat, minLon, maxLon, zoom);
}

// ── Geocoding (Nominatim) ─────────────────────────────────────────────────────

void MainWindow::geocodeSearch(const QString& query)
{
    // Nominatim OpenStreetMap geocoder — free, no API key required.
    // Terms: must set a descriptive User-Agent and not hammer the server.
    QUrl url("https://nominatim.openstreetmap.org/search");

    QUrlQuery params;
    params.addQueryItem("q",              query);
    params.addQueryItem("format",         "json");
    params.addQueryItem("limit",          "6");
    params.addQueryItem("addressdetails", "0");
    url.setQuery(params);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  "FlightPlanner/2.0 (logistics UAV mission planner)");
    req.setTransferTimeout(8000);

    QNetworkReply* reply = m_geocodeNam->get(req);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit geocodeError("Network error: " + reply->errorString());
            return;
        }

        QByteArray    data = reply->readAll();
        QJsonDocument doc  = QJsonDocument::fromJson(data);

        if (!doc.isArray()) {
            emit geocodeError("Unexpected response from geocoder.");
            return;
        }

        QJsonArray    arr     = doc.array();
        QVariantList  results;

        for (const QJsonValue& v : arr) {
            QJsonObject obj = v.toObject();
            double lat = obj["lat"].toString().toDouble();
            double lon = obj["lon"].toString().toDouble();
            // displayName: use "display_name" but trim to first two comma-parts
            QString fullName = obj["display_name"].toString();
            QStringList parts = fullName.split(",");
            QString name = parts.size() >= 2
                           ? parts[0].trimmed() + ", " + parts[1].trimmed()
                           : parts[0].trimmed();

            QVariantMap r;
            r["name"] = name;
            r["lat"]  = lat;
            r["lon"]  = lon;
            results.append(r);
        }

        if (results.isEmpty()) {
            emit geocodeError("No results found for \"" + query + "\"");
        } else {
            emit geocodeDone(results);
        }
    });
}

// ── PIN management ────────────────────────────────────────────────────────────
// PINs are stored as SHA-256 hashes in QSettings under "pins/<section>".
// Default PIN for all sections is "1234" (set on first run if not present).

static QString hashPin(const QString& pin)
{
    return QString(QCryptographicHash::hash(
        pin.toUtf8(),
        QCryptographicHash::Sha256).toHex());
}

static const QString DEFAULT_PIN = "1234";

bool MainWindow::verifyPin(const QString& section, const QString& pin)
{
    QSettings s("BonV", "FlightPlanner");
    QString key      = "pins/" + section;
    QString stored   = s.value(key, hashPin(DEFAULT_PIN)).toString();
    return hashPin(pin) == stored;
}

void MainWindow::setPin(const QString& section, const QString& pin)
{
    QSettings s("BonV", "FlightPlanner");
    s.setValue("pins/" + section, hashPin(pin));
    s.sync();
    appendLog("INFO", "PIN updated for: " + section);
}

bool MainWindow::hasPin(const QString& section)
{
    QSettings s("BonV", "FlightPlanner");
    return s.contains("pins/" + section);
}

// bool MainWindow::isActivated()
// {
//     return KeyManager::isActivated();
// }

bool MainWindow::isActivated()
{
    QString key = KeyManager::getActivatedKey();

    if (key.isEmpty())
        return false;

    auto result =
        KeyManager::validateKeyDetailed(key);

    if (result == KeyManager::Expired)
    {
        m_lastActivationExpired = true;

        KeyManager::clearLocalActivation();

        return false;
    }

    return result == KeyManager::Valid;
}

// void MainWindow::onValidateKey(QString key)
// {
//     auto result =
//         KeyManager::validateKeyDetailed(key);

//     switch(result)
//     {
//     case KeyManager::Valid:

//         if (KeyManager::activateKey(key))
//             emit activationSucceeded();
//         else
//             emit activationFailed("Activation failed.");

//         break;

//     case KeyManager::Expired:

//         emit activationFailed(
//             "This license key has expired.");
//         break;

//     case KeyManager::InvalidSignature:

//         emit activationFailed(
//             "This key does not belong to this machine.");
//         break;

//     default:

//         emit activationFailed(
//             "Invalid activation key.");
//         break;
//     }
// }

void MainWindow::onValidateKey(
    QString key,
    QString activationType)
{
    if (activationType == "Drone Based")
    {
        emit activationFailed(
            "Drone-based activation is not supported yet. Please select Device Based activation.");

        return;
    }

    auto result =
        KeyManager::validateKeyDetailed(key);

    switch(result)
    {
    case KeyManager::Valid:

        if (KeyManager::activateKey(key))
        {
            emit activationSucceeded();
        }
        else
        {
            emit activationFailed(
                "Activation failed.");
        }

        break;

    case KeyManager::Expired:

        emit activationFailed(
            "This license key has expired. Please enter a valid key.");

        break;

    case KeyManager::InvalidSignature:

        emit activationFailed(
            "This key does not belong to this machine.");

        break;

    default:

        emit activationFailed(
            "Invalid activation key.");

        break;
    }
}

QString MainWindow::activationStatus()
{
    if (m_lastActivationExpired)
    {
        m_lastActivationExpired = false;
        return "EXPIRED";
    }

    return "NO_KEY";
}

// QString MainWindow::activationId()
// {
//     return KeyManager::getMachineId();
// }

QString MainWindow::activationId()
{
    QString id = KeyManager::getMachineId();

    qDebug() << "Activation ID sent to QML:" << id;

    return id;
}
