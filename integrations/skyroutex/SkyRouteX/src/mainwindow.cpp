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
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFileInfo>

namespace {
QString writableSkyRouteXRoot()
{
#ifdef Q_OS_ANDROID
    QString baseDir = "/storage/emulated/0/Documents";
#else
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    if (baseDir.isEmpty())
        baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty())
        baseDir = QDir::homePath();

    QString root = baseDir + "/SkyRouteX";
    if (!QDir().mkpath(root)) {
        QString fallback = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (fallback.isEmpty())
            fallback = QDir::homePath();
        root = fallback + "/SkyRouteX";
        QDir().mkpath(root);
    }
    return root;
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

    TripMode    trip  = (tripModeIdx == 1) ? TripMode::Return : TripMode::OneWay;
    MissionType mtype = static_cast<MissionType>(
                            qBound(0, missionTypeIdx, 2));

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

    runGenerate(p, trip, mtype, drone, outputDir);
}

// ── Core generation ───────────────────────────────────────────────────────────

void MainWindow::runGenerate(const MissionParams& p,
                              TripMode             trip,
                              MissionType          mtype,
                              const DroneModel&    drone,
                              const QString&       outputDir)
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

    // Prefetch / check SRTM tiles
    m_srtm->prefetchTiles(p.takeoffLat, p.takeoffLon, p.landLat, p.landLon);

    if (!m_srtm->allTilesCached(p.takeoffLat, p.takeoffLon,
                                 p.landLat,    p.landLon)) {
        setStatus("Downloading SRTM tile(s)... please wait.", true);
        appendLog("INFO", "SRTM tiles not cached — downloading in background.");
        m_pendingGenerate  = true;
        m_pendingParams    = p;
        m_pendingTrip      = trip;
        m_pendingMtype     = mtype;
        m_pendingDrone     = drone;
        m_pendingOutputDir = effectiveOutputDir;
        return;
    }

    // ── Plan outbound path ────────────────────────────────────────────────────
    emit progressChanged(30);
    setStatus("Planning outbound path...", true);

    auto elevFn = [this](double lat, double lon) {
        return m_srtm->elevation(lat, lon);
    };

    QVector<GeoPoint> outAnchors = m_planner.plan(p, elevFn);
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
        setStatus("Planning return path...", true);
        MissionParams retP = p;
        std::swap(retP.takeoffLat, retP.landLat);
        std::swap(retP.takeoffLon, retP.landLon);
        retAnchors = m_planner.plan(retP, elevFn);
        if (retAnchors.isEmpty()) {
            setStatus("Return path planning failed.", false);
            appendLog("ERROR", "Return path failed.");
            emit progressChanged(0);
            return;
        }
        appendLog("INFO", QString("Return: %1 waypoints").arg(retAnchors.size()));
    }

    emit progressChanged(80);
    setStatus("Writing waypoint file...", true);

    // ── Write .waypoints file ─────────────────────────────────────────────────
    double distM = PathPlanner::haversineM(
        p.takeoffLat, p.takeoffLon, p.landLat, p.landLon);

    QString outPath = WaypointWriter::buildFilename(
        effectiveOutputDir, p, drone, mtype, trip, distM);

    if (m_writer.write(outPath, p, outAnchors, retAnchors, trip, mtype, drone)) {
        emit progressChanged(100);
        setStatus("Done!", false);
        appendLog("OK", "Saved: " + outPath);

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
                                 double droneAlt, double distM) {
            double terrainH = elevFn(lat, lon);
            if (std::isnan(terrainH)) terrainH = 0.0;
            QVariantMap m;
            m["lat"]        = lat;
            m["lon"]        = lon;
            m["droneAlt"]   = droneAlt;
            m["terrainAlt"] = terrainH;
            m["distKm"]     = distM / 1000.0;
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

        // Sample outbound anchors densely
        for (int i = 0; i < outAnchors.size(); i++) {
            const GeoPoint& gp = outAnchors[i];

            if (i == 0) {
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
    appendLog("OK", QString("SRTM tile ready: N%1 E%2").arg(lat).arg(lon));
    emit srtmInfoChanged(QString("N%1 E%2 cached").arg(lat).arg(lon));

    if (!m_pendingGenerate) return;

    const MissionParams& p = m_pendingParams;
    if (m_srtm->allTilesCached(p.takeoffLat, p.takeoffLon,
                                p.landLat,    p.landLon)) {
        m_pendingGenerate = false;
        appendLog("INFO", "All tiles ready — resuming generation.");
        runGenerate(p, m_pendingTrip, m_pendingMtype,
                    m_pendingDrone, m_pendingOutputDir);
    }
}

void MainWindow::onDownloadProgress(int pct)
{
    // Map tile download progress into the 0–30 range on the progress bar
    emit progressChanged(pct / 5);
    setStatus(QString("Downloading SRTM tile... %1%").arg(pct), true);
}

void MainWindow::onDownloadError(const QString& msg)
{
    appendLog("ERROR", msg);
    setStatus("Download failed: " + msg, false);
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
    emit logLine(level, msg);
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
        appendLog("INFO", "SRTM cache dir set to: " + dir);
        setStatus("SRTM cache updated.", false);
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

bool MainWindow::isActivated()
{
    return KeyManager::isActivated();
}

void MainWindow::onValidateKey(QString key)
{
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
