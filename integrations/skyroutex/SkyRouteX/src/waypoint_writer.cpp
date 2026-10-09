// // // #include "waypoint_writer.h"
// // // #include <QFile>
// // // #include <QTextStream>
// // // #include <QDir>
// // // #include <QDebug>

// // // static const int CMD_HOME     = 16;
// // // static const int CMD_TAKEOFF  = 22;
// // // static const int CMD_WAYPOINT = 16;
// // // static const int CMD_LOITER   = 93;
// // // static const int CMD_LAND     = 21;
// // // static const int CMD_ARM      = 218;

// // // static void wpLine(QTextStream& s, int seq, int isCurrent, int frame,
// // //                    int cmd, double p1, double p2, double p3, double p4,
// // //                    double lat, double lon, double alt, int ac = 1)
// // // {
// // //     s << seq << "\t" << isCurrent << "\t" << frame << "\t" << cmd << "\t"
// // //       << QString::number(p1,'f',8) << "\t"
// // //       << QString::number(p2,'f',8) << "\t"
// // //       << QString::number(p3,'f',8) << "\t"
// // //       << QString::number(p4,'f',8) << "\t"
// // //       << QString::number(lat,'f',7) << "\t"
// // //       << QString::number(lon,'f',7) << "\t"
// // //       << QString::number(alt,'f',2) << "\t"
// // //       << ac << "\n";
// // // }

// // // static int writeOutbound(QTextStream& s, int seq,
// // //                           const QVector<GeoPoint>& anchors,
// // //                           const MissionParams& p)
// // // {
// // //     const GeoPoint& first = anchors.first();
// // //     const GeoPoint& last  = anchors.last();
// // //     double gnd = first.terrainH;

// // //     // HOME
// // //     wpLine(s, seq++, 1, 0, CMD_HOME,    0,0,0,0, first.lat, first.lon, gnd);
// // //     // TAKEOFF
// // //     wpLine(s, seq++, 0, 0, CMD_TAKEOFF, 0,0,0,0, first.lat, first.lon, gnd + p.takeoffAlt);
// // //     // Near-home climb
// // //     wpLine(s, seq++, 0, 0, CMD_WAYPOINT,0,0,0,0, first.lat, first.lon, gnd + p.nearHomeAlt);
// // //     // Cruise altitude at takeoff position
// // //     wpLine(s, seq++, 0, 0, CMD_WAYPOINT,0,0,0,0, first.lat, first.lon, gnd + p.cruiseAlt);

// // //     // Terrain barrier waypoints (skip index 0=takeoff, index last=landing)
// // //     for (int i = 1; i < anchors.size() - 1; ++i) {
// // //         const GeoPoint& gp = anchors[i];
// // //         wpLine(s, seq++, 0, 0, CMD_WAYPOINT, 0,0,0,0, gp.lat, gp.lon, gp.droneAlt);
// // //     }

// // //     // Approach: arrive at cruise height over landing point
// // //     wpLine(s, seq++, 0, 0, CMD_WAYPOINT, 0,0,0,0,
// // //            last.lat, last.lon, last.terrainH + p.cruiseAlt);
// // //     // LAND
// // //     wpLine(s, seq++, 0, 0, CMD_LAND, 0,0,0,0,
// // //            last.lat, last.lon, last.terrainH + p.landAlt);

// // //     return seq;
// // // }

// // // static int writeReturnLeg(QTextStream& s, int seq,
// // //                            const QVector<GeoPoint>& retAnchors,
// // //                            const MissionParams& p)
// // // {
// // //     // retAnchors[0] = the landing site (return takeoff point)
// // //     // retAnchors[last] = original takeoff site (return landing point)
// // //     const GeoPoint& retFirst = retAnchors.first();
// // //     const GeoPoint& retLast  = retAnchors.last();
// // //     double retGnd = retFirst.terrainH;

// // //     // Wait at destination
// // //     wpLine(s, seq++, 0, 0, CMD_LOITER, p.waitDelayS,0,0,0, retFirst.lat, retFirst.lon, 0);
// // //     // ARM
// // //     wpLine(s, seq++, 0, 0, CMD_ARM, 153,1,0,0, 0,0,0);
// // //     // Stabilise delay
// // //     wpLine(s, seq++, 0, 0, CMD_LOITER, 5,0,0,0, 0,0,0);

// // //     // Return TAKEOFF
// // //     wpLine(s, seq++, 0, 0, CMD_TAKEOFF, 0,0,0,0,
// // //            retFirst.lat, retFirst.lon, retGnd + p.takeoffAlt);
// // //     // Near-home
// // //     wpLine(s, seq++, 0, 0, CMD_WAYPOINT,0,0,0,0,
// // //            retFirst.lat, retFirst.lon, retGnd + p.nearHomeAlt);
// // //     // Cruise
// // //     wpLine(s, seq++, 0, 0, CMD_WAYPOINT,0,0,0,0,
// // //            retFirst.lat, retFirst.lon, retGnd + p.cruiseAlt);

// // //     // Barrier waypoints
// // //     for (int i = 1; i < retAnchors.size() - 1; ++i) {
// // //         const GeoPoint& gp = retAnchors[i];
// // //         wpLine(s, seq++, 0, 0, CMD_WAYPOINT, 0,0,0,0, gp.lat, gp.lon, gp.droneAlt);
// // //     }

// // //     // Approach home
// // //     wpLine(s, seq++, 0, 0, CMD_WAYPOINT, 0,0,0,0,
// // //            retLast.lat, retLast.lon, retLast.terrainH + p.cruiseAlt);
// // //     // LAND at home
// // //     wpLine(s, seq++, 0, 0, CMD_LAND, 0,0,0,0,
// // //            retLast.lat, retLast.lon, retLast.terrainH + p.landAlt);
// // //     // DISARM
// // //     wpLine(s, seq++, 0, 0, CMD_ARM, 153,0,0,0, 0,0,0);

// // //     return seq;
// // // }

// // // bool WaypointWriter::write(const QString&           outputPath,
// // //                             const MissionParams&     p,
// // //                             const QVector<GeoPoint>& anchors,
// // //                             const QVector<GeoPoint>& returnAnchors,
// // //                             TripMode                 trip,
// // //                             MissionType,
// // //                             const DroneModel&) const
// // // {
// // //     if (anchors.size() < 2) {
// // //         qWarning() << "WaypointWriter: not enough anchors";
// // //         return false;
// // //     }
// // //     QFile f(outputPath);
// // //     if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
// // //         qWarning() << "WaypointWriter: cannot open" << outputPath;
// // //         return false;
// // //     }
// // //     QTextStream s(&f);
// // //     s << "QGC WPL 110\n";

// // //     int seq = 0;
// // //     seq = writeOutbound(s, seq, anchors, p);
// // //     if (trip == TripMode::Return && returnAnchors.size() >= 2)
// // //         writeReturnLeg(s, seq, returnAnchors, p);

// // //     f.close();
// // //     qDebug() << "WaypointWriter: wrote" << outputPath;
// // //     return true;
// // // }

// // // QString WaypointWriter::buildFilename(const QString&       outputDir,
// // //                                        const MissionParams& p,
// // //                                        const DroneModel&    drone,
// // //                                        MissionType          mtype,
// // //                                        TripMode             trip,
// // //                                        double               distanceM)
// // // {
// // //     double distKm  = distanceM / 1000.0;
// // //     double waitMin = p.waitDelayS / 60.0;
// // //     QString tripTag = (trip == TripMode::Return) ? "Return" : "N-Return";
// // //     QString name = QString("M-SRTM1-P-ABS-S-%1-%2-%3_D-%4Kms_W-%5Min.waypoints")
// // //         .arg(drone.displayName)
// // //         .arg(missionTag(mtype))
// // //         .arg(tripTag)
// // //         .arg(distKm,  0, 'f', 1)
// // //         .arg(waitMin, 0, 'f', 0);

// // //     QString sub = (trip == TripMode::Return)
// // //                 ? "Delivery/Delivery_Return"
// // //                 : "Delivery/Delivery_Non-Return";
// // //     QDir().mkpath(outputDir + "/" + sub);
// // //     return outputDir + "/" + sub + "/" + name;
// // // }



// // #include "waypoint_writer.h"
// // #include <QFile>
// // #include <QTextStream>
// // #include <QDir>
// // #include <QDebug>

// // // ── MAVLink command IDs ───────────────────────────────────────────────────────
// // static const int CMD_HOME          = 16;   // NAV_WAYPOINT used as home
// // static const int CMD_TAKEOFF       = 22;   // NAV_TAKEOFF
// // static const int CMD_WAYPOINT      = 16;   // NAV_WAYPOINT
// // static const int CMD_LAND          = 21;   // NAV_LAND
// // static const int CMD_LOITER_TIME   = 19;   // NAV_LOITER_TIME  (p1 = seconds)
// // static const int CMD_ARM_DISARM    = 400;  // MAV_CMD_COMPONENT_ARM_DISARM
// // static const int CMD_DO_CHANGE_SPD = 178;  // DO_CHANGE_SPEED  (p1=0 airspeed, p2=m/s, p3=-1)
// // static const int CMD_DO_SET_SERVO  = 183;  // DO_SET_SERVO     (p1=channel, p2=PWM)

// // static const int PWM_LOW  = 1100;
// // static const int PWM_HIGH = 1900;

// // // ── Helper: emit one WPL line ─────────────────────────────────────────────────
// // static void wp(QTextStream& s, int& seq, int isCurrent, int frame,
// //                int cmd,
// //                double p1, double p2, double p3, double p4,
// //                double lat, double lon, double alt, int ac = 1)
// // {
// //     s << seq++ << "\t" << isCurrent << "\t" << frame << "\t" << cmd << "\t"
// //       << QString::number(p1, 'f', 6) << "\t"
// //       << QString::number(p2, 'f', 6) << "\t"
// //       << QString::number(p3, 'f', 6) << "\t"
// //       << QString::number(p4, 'f', 6) << "\t"
// //       << QString::number(lat, 'f', 7) << "\t"
// //       << QString::number(lon, 'f', 7) << "\t"
// //       << QString::number(alt, 'f', 2) << "\t"
// //       << ac << "\n";
// // }

// // // Convenience helpers
// // static void wpWaypoint(QTextStream& s, int& seq, double lat, double lon, double alt)
// // {
// //     wp(s, seq, 0, 3, CMD_WAYPOINT, 0,0,0,0, lat, lon, alt);
// // }

// // static void wpDelay(QTextStream& s, int& seq, double seconds)
// // {
// //     wp(s, seq, 0, 2, CMD_LOITER_TIME, seconds, 0, 0, 0, 0, 0, 0);
// // }

// // static void wpSpeed(QTextStream& s, int& seq, double speedMs)
// // {
// //     // p1=0(airspeed type), p2=speed m/s, p3=-1(no throttle change), p4=0
// //     wp(s, seq, 0, 2, CMD_DO_CHANGE_SPD, 0, speedMs, -1, 0, 0, 0, 0);
// // }

// // static void wpArm(QTextStream& s, int& seq)
// // {
// //     wp(s, seq, 0, 2, CMD_ARM_DISARM, 1, 0, 0, 0, 0, 0, 0);
// // }

// // static void wpDisarm(QTextStream& s, int& seq)
// // {
// //     wp(s, seq, 0, 2, CMD_ARM_DISARM, 0, 0, 0, 0, 0, 0, 0);
// // }

// // static void wpServoLow(QTextStream& s, int& seq, int channel)
// // {
// //     wp(s, seq, 0, 2, CMD_DO_SET_SERVO, channel, PWM_LOW, 0, 0, 0, 0, 0);
// // }

// // static void wpServoHigh(QTextStream& s, int& seq, int channel)
// // {
// //     wp(s, seq, 0, 2, CMD_DO_SET_SERVO, channel, PWM_HIGH, 0, 0, 0, 0, 0);
// // }

// // // ── RC trigger sequence for one channel ──────────────────────────────────────
// // // LOW → 2s delay → HIGH → 2s delay
// // static void rcTrigger(QTextStream& s, int& seq, int channel)
// // {
// //     wpServoLow (s, seq, channel);
// //     wpDelay    (s, seq, 2);
// //     wpServoHigh(s, seq, channel);
// //     wpDelay    (s, seq, 2);
// // }

// // // ── Write outbound waypoints (terrain-following anchors) ──────────────────────
// // // Returns terrain altitude at takeoff (used as ASL reference base).
// // static void writeOutboundWaypoints(QTextStream& s, int& seq,
// //                                     const QVector<GeoPoint>& anchors,
// //                                     const MissionParams& p)
// // {
// //     // All anchors already have correct droneAlt (terrain + clearance)
// //     // Skip first (takeoff position, already emitted as takeoff cmd)
// //     // and last (landing position, handled separately)
// //     for (int i = 1; i < anchors.size() - 1; ++i) {
// //         const GeoPoint& gp = anchors[i];
// //         wpWaypoint(s, seq, gp.lat, gp.lon, gp.droneAlt);
// //     }
// // }

// // // ── DELIVERY ONE-WAY ──────────────────────────────────────────────────────────
// // static void writeDeliveryOneWay(QTextStream& s, int& seq,
// //                                  const QVector<GeoPoint>& anchors,
// //                                  const MissionParams& p)
// // {
// //     const GeoPoint& first = anchors.first();
// //     const GeoPoint& last  = anchors.last();
// //     double homeAsl = first.terrainH;   // terrain elevation at takeoff (ASL)

// //     // HOME
// //     wp(s, seq, 1, 0, CMD_HOME, 0,0,0,0, first.lat, first.lon, homeAsl);

// //     // TakeOff
// //     wp(s, seq, 0, 0, CMD_TAKEOFF, 0,0,0,0, first.lat, first.lon, homeAsl + p.takeoffAlt);

// //     // Near-home alt
// //     wpWaypoint(s, seq, first.lat, first.lon, homeAsl + p.nearHomeAlt);

// //     // Terrain clearance alt at takeoff pos
// //     wpWaypoint(s, seq, first.lat, first.lon, homeAsl + p.cruiseAlt);

// //     // Change cruise speed
// //     wpSpeed(s, seq, p.cruiseSpeedMs);

// //     // Route waypoints
// //     writeOutboundWaypoints(s, seq, anchors, p);

// //     // Descend to land alt over landing point
// //     wpWaypoint(s, seq, last.lat, last.lon, last.terrainH + p.landAlt);

// //     // LAND
// //     wp(s, seq, 0, 0, CMD_LAND, 0,0,0,0, last.lat, last.lon, last.terrainH);

// //     // DISARM
// //     wpDisarm(s, seq);
// // }

// // // ── DELIVERY RETURN ───────────────────────────────────────────────────────────
// // static void writeDeliveryReturn(QTextStream& s, int& seq,
// //                                  const QVector<GeoPoint>& outAnchors,
// //                                  const QVector<GeoPoint>& retAnchors,
// //                                  const MissionParams& p)
// // {
// //     const GeoPoint& homeFirst = outAnchors.first();
// //     const GeoPoint& destFirst = outAnchors.last();   // destination = return start
// //     const GeoPoint& homeLast  = retAnchors.last();   // return landing = home
// //     double homeAsl = homeFirst.terrainH;
// //     double destAsl = destFirst.terrainH;

// //     // ── Outbound leg ──────────────────────────────────────────────────────────
// //     wp(s, seq, 1, 0, CMD_HOME, 0,0,0,0, homeFirst.lat, homeFirst.lon, homeAsl);
// //     wp(s, seq, 0, 0, CMD_TAKEOFF, 0,0,0,0, homeFirst.lat, homeFirst.lon, homeAsl + p.takeoffAlt);
// //     wpWaypoint(s, seq, homeFirst.lat, homeFirst.lon, homeAsl + p.nearHomeAlt);
// //     wpWaypoint(s, seq, homeFirst.lat, homeFirst.lon, homeAsl + p.cruiseAlt);
// //     wpSpeed(s, seq, p.cruiseSpeedMs);
// //     writeOutboundWaypoints(s, seq, outAnchors, p);
// //     wpWaypoint(s, seq, destFirst.lat, destFirst.lon, destAsl + p.landAlt);
// //     wp(s, seq, 0, 0, CMD_LAND, 0,0,0,0, destFirst.lat, destFirst.lon, destAsl);
// //     wpDisarm(s, seq);

// //     // ── At destination: user delay ────────────────────────────────────────────
// //     wpDelay(s, seq, p.waitDelayS);        // user-defined seconds

// //     // ── Return leg ────────────────────────────────────────────────────────────
// //     wpArm(s, seq);
// //     wpDelay(s, seq, 5);                    // constant 5-second stabilise delay

// //     wp(s, seq, 0, 0, CMD_TAKEOFF, 0,0,0,0, destFirst.lat, destFirst.lon, destAsl + p.takeoffAlt);
// //     wpWaypoint(s, seq, destFirst.lat, destFirst.lon, destAsl + p.nearHomeAlt);
// //     wpWaypoint(s, seq, destFirst.lat, destFirst.lon, destAsl + p.cruiseAlt);
// //     wpSpeed(s, seq, p.cruiseSpeedMs);

// //     // Return waypoints (retAnchors: destination → home)
// //     for (int i = 1; i < retAnchors.size() - 1; ++i) {
// //         const GeoPoint& gp = retAnchors[i];
// //         wpWaypoint(s, seq, gp.lat, gp.lon, gp.droneAlt);
// //     }

// //     double retLandAsl = homeLast.terrainH;
// //     wpWaypoint(s, seq, homeLast.lat, homeLast.lon, retLandAsl + p.landAlt);
// //     wp(s, seq, 0, 0, CMD_LAND, 0,0,0,0, homeLast.lat, homeLast.lon, retLandAsl);
// //     wpDisarm(s, seq);
// // }

// // // ── DROP / PARADROP RETURN ────────────────────────────────────────────────────
// // // Drop and Paradrop have identical waypoint sequences — only mission tag differs.
// // static void writeDropReturn(QTextStream& s, int& seq,
// //                              const QVector<GeoPoint>& outAnchors,
// //                              const QVector<GeoPoint>& retAnchors,
// //                              const MissionParams& p,
// //                              int rcChannel,      // primary RC channel from drone config
// //                              int rcTriggerCount) // number of RC trigger pairs
// // {
// //     const GeoPoint& homeFirst = outAnchors.first();
// //     const GeoPoint& destFirst = outAnchors.last();
// //     const GeoPoint& homeLast  = retAnchors.last();
// //     double homeAsl = homeFirst.terrainH;
// //     double destAsl = destFirst.terrainH;

// //     // ── Outbound leg ──────────────────────────────────────────────────────────
// //     wp(s, seq, 1, 0, CMD_HOME, 0,0,0,0, homeFirst.lat, homeFirst.lon, homeAsl);
// //     wp(s, seq, 0, 0, CMD_TAKEOFF, 0,0,0,0, homeFirst.lat, homeFirst.lon, homeAsl + p.takeoffAlt);
// //     wpWaypoint(s, seq, homeFirst.lat, homeFirst.lon, homeAsl + p.nearHomeAlt);
// //     wpWaypoint(s, seq, homeFirst.lat, homeFirst.lon, homeAsl + p.cruiseAlt);
// //     wpSpeed(s, seq, p.cruiseSpeedMs);
// //     writeOutboundWaypoints(s, seq, outAnchors, p);

// //     // ── At destination: descend to dropping alt ───────────────────────────────
// //     double dropAsl = destAsl + p.dropAlt;
// //     wpWaypoint(s, seq, destFirst.lat, destFirst.lon, dropAsl);

// //     // ── Hover time (user-defined) ─────────────────────────────────────────────
// //     wpDelay(s, seq, p.waitDelayS);

// //     // ── RC trigger sequence: constant 2s + LOW + 2s + HIGH + 2s per channel ──
// //     // Repeat for each configured trigger count (each count = one channel pulse pair)
// //     // Channels start at rcChannel and increment
// //     for (int t = 0; t < rcTriggerCount; ++t) {
// //         int ch = rcChannel + t;    // first trigger = rcChannel, next = rcChannel+1, etc.
// //         wpDelay(s, seq, 2);        // constant 2s pre-trigger delay
// //         rcTrigger(s, seq, ch);     // LOW 2s HIGH 2s
// //         wpDelay(s, seq, 2);        // constant 2s post-trigger delay
// //     }

// //     // ── Climb back to cruise alt and return ───────────────────────────────────
// //     wpWaypoint(s, seq, destFirst.lat, destFirst.lon, destAsl + p.cruiseAlt);
// //     wpSpeed(s, seq, p.cruiseSpeedMs);

// //     // Return waypoints
// //     for (int i = 1; i < retAnchors.size() - 1; ++i) {
// //         const GeoPoint& gp = retAnchors[i];
// //         wpWaypoint(s, seq, gp.lat, gp.lon, gp.droneAlt);
// //     }

// //     double retLandAsl = homeLast.terrainH;
// //     wpWaypoint(s, seq, homeLast.lat, homeLast.lon, retLandAsl + p.landAlt);
// //     wp(s, seq, 0, 0, CMD_LAND, 0,0,0,0, homeLast.lat, homeLast.lon, retLandAsl);
// //     wpDisarm(s, seq);
// // }

// // // ── Public write ──────────────────────────────────────────────────────────────

// // bool WaypointWriter::write(const QString&           outputPath,
// //                             const MissionParams&     p,
// //                             const QVector<GeoPoint>& anchors,
// //                             const QVector<GeoPoint>& returnAnchors,
// //                             TripMode                 trip,
// //                             MissionType              mtype,
// //                             const DroneModel&        drone) const
// // {
// //     if (anchors.size() < 2) {
// //         qWarning() << "WaypointWriter: not enough anchors";
// //         return false;
// //     }

// //     QFile f(outputPath);
// //     if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
// //         qWarning() << "WaypointWriter: cannot open" << outputPath;
// //         return false;
// //     }

// //     QTextStream s(&f);
// //     s << "QGC WPL 110\n";

// //     int seq = 0;

// //     switch (mtype) {
// //     case MissionType::Delivery:
// //         if (trip == TripMode::Return && returnAnchors.size() >= 2)
// //             writeDeliveryReturn(s, seq, anchors, returnAnchors, p);
// //         else
// //             writeDeliveryOneWay(s, seq, anchors, p);
// //         break;

// //     case MissionType::Drop:
// //     case MissionType::Paradrop:
// //         // Drop and Paradrop always return
// //         if (returnAnchors.size() >= 2) {
// //             writeDropReturn(s, seq, anchors, returnAnchors, p,
// //                             drone.rcSwitch, p.rcTriggerCount);
// //         } else {
// //             qWarning() << "WaypointWriter: Drop/Paradrop requires return anchors";
// //         }
// //         break;
// //     }

// //     f.close();
// //     qDebug() << "WaypointWriter: wrote" << seq << "waypoints to" << outputPath;
// //     return true;
// // }

// // QString WaypointWriter::buildFilename(const QString&       outputDir,
// //                                        const MissionParams& p,
// //                                        const DroneModel&    drone,
// //                                        MissionType          mtype,
// //                                        TripMode             trip,
// //                                        double               distanceM)
// // {
// //     QString tripTag  = (trip == TripMode::Return) ? "Return" : "N-Return";
// //     QString typeTag  = missionTag(mtype);
// //     double  distKm   = distanceM / 1000.0;
// //     double  waitSec  = p.waitDelayS;

// //     QString name = QString("M-SRTM1-P-ABS-S-%1-%2-%3_D-%4Kms_W-%5Sec.waypoints")
// //         .arg(drone.displayName)
// //         .arg(typeTag)
// //         .arg(tripTag)
// //         .arg(distKm,  0, 'f', 1)
// //         .arg(waitSec, 0, 'f', 0);

// //     QString sub;
// //     switch (mtype) {
// //     case MissionType::Delivery:
// //         sub = (trip == TripMode::Return) ? "Delivery/Return" : "Delivery/OneWay";
// //         break;
// //     case MissionType::Drop:
// //         sub = "Drop/Return";
// //         break;
// //     case MissionType::Paradrop:
// //         sub = "Paradrop/Return";
// //         break;
// //     }

// //     QDir().mkpath(outputDir + "/" + sub);
// //     return outputDir + "/" + sub + "/" + name;
// // }


// #include "waypoint_writer.h"
// #include <QFile>
// #include <QTextStream>
// #include <QDir>
// #include <QDebug>

// static const int CMD_WAYPOINT    = 16;
// static const int CMD_TAKEOFF     = 22;
// static const int CMD_LAND        = 21;
// static const int CMD_LOITER_TIME = 93;
// static const int CMD_ARM_DISARM  = 218;   // ArduPilot: p1=153, p2=1 arm / p2=0 disarm
// static const int CMD_CHANGE_SPD  = 178;
// static const int CMD_SET_SERVO   = 183;
// static const int FRAME_GLOBAL    = 0;
// static const int FRAME_MISSION   = 2;
// static const int PWM_LOW         = 1100;
// static const int PWM_HIGH        = 1900;

// static void wpLine(QTextStream& s, int& seq,
//                    int isCurrent, int frame, int cmd,
//                    double p1, double p2, double p3, double p4,
//                    double lat, double lon, double alt, int ac = 1)
// {
//     s << seq++ << "\t" << isCurrent << "\t" << frame << "\t" << cmd << "\t"
//       << QString::number(p1,  'f', 8) << "\t"
//       << QString::number(p2,  'f', 8) << "\t"
//       << QString::number(p3,  'f', 8) << "\t"
//       << QString::number(p4,  'f', 8) << "\t"
//       << QString::number(lat, 'f', 7) << "\t"
//       << QString::number(lon, 'f', 7) << "\t"
//       << QString::number(alt, 'f', 2) << "\t"
//       << ac << "\n";
// }

// static void wpWP(QTextStream& s, int& seq, double lat, double lon, double alt)
// { wpLine(s, seq, 0, FRAME_GLOBAL, CMD_WAYPOINT, 0,0,0,0, lat, lon, alt); }

// static void wpDelay(QTextStream& s, int& seq, double sec)
// { wpLine(s, seq, 0, FRAME_MISSION, CMD_LOITER_TIME, sec, 0,0,0, 0,0,0); }

// static void wpSpeed(QTextStream& s, int& seq, double mps)
// { wpLine(s, seq, 0, FRAME_MISSION, CMD_CHANGE_SPD, 0, mps, -1, 0, 0,0,0); }

// static void wpArm(QTextStream& s, int& seq)
// { wpLine(s, seq, 0, FRAME_MISSION, CMD_ARM_DISARM, 153, 1, 0,0, 0,0,0); }

// static void wpDisarm(QTextStream& s, int& seq)
// { wpLine(s, seq, 0, FRAME_MISSION, CMD_ARM_DISARM, 153, 0, 0,0, 0,0,0); }

// static void wpServo(QTextStream& s, int& seq, int ch, int pwm)
// { wpLine(s, seq, 0, FRAME_MISSION, CMD_SET_SERVO, ch, pwm, 0,0, 0,0,0); }

// // One RC trigger cycle: 2s → LOW → 2s → HIGH → 2s
// static void rcTrigger(QTextStream& s, int& seq, int ch)
// {
//     wpDelay(s, seq, 2);
//     wpServo(s, seq, ch, PWM_LOW);
//     wpDelay(s, seq, 2);
//     wpServo(s, seq, ch, PWM_HIGH);
//     wpDelay(s, seq, 2);
// }

// // Skip first (takeoff) and last (landing) anchors — write only intermediate
// static void writeRouteWPs(QTextStream& s, int& seq, const QVector<GeoPoint>& anchors)
// {
//     for (int i = 1; i < anchors.size() - 1; ++i)
//         wpWP(s, seq, anchors[i].lat, anchors[i].lon, anchors[i].droneAlt);
// }

// // ── DELIVERY ONE-WAY ──────────────────────────────────────────────────────────
// static void deliveryOneWay(QTextStream& s, int& seq,
//                             const QVector<GeoPoint>& out, const MissionParams& p)
// {
//     const GeoPoint& A = out.first();
//     const GeoPoint& B = out.last();
//     double asl = A.terrainH;

//     wpLine(s, seq, 1, FRAME_GLOBAL, CMD_WAYPOINT, 0,0,0,0, A.lat, A.lon, asl);
//     wpLine(s, seq, 0, FRAME_GLOBAL, CMD_TAKEOFF,  0,0,0,0, A.lat, A.lon, asl + p.takeoffAlt);
//     wpWP(s, seq, A.lat, A.lon, asl + p.nearHomeAlt);
//     wpWP(s, seq, A.lat, A.lon, asl + p.cruiseAlt);
//     wpSpeed(s, seq, p.cruiseSpeedMs);
//     writeRouteWPs(s, seq, out);
//     wpWP(s, seq, B.lat, B.lon, B.terrainH + p.landAlt);
//     wpLine(s, seq, 0, FRAME_GLOBAL, CMD_LAND, 0,0,0,0, B.lat, B.lon, B.terrainH);
//     wpDisarm(s, seq);
// }

// // ── DELIVERY RETURN ───────────────────────────────────────────────────────────
// static void deliveryReturn(QTextStream& s, int& seq,
//                             const QVector<GeoPoint>& out,
//                             const QVector<GeoPoint>& ret,
//                             const MissionParams& p)
// {
//     const GeoPoint& A = out.first();   // home takeoff
//     const GeoPoint& B = out.last();    // destination
//     const GeoPoint& C = ret.last();    // home landing (same as A usually)

//     // Outbound
//     wpLine(s, seq, 1, FRAME_GLOBAL, CMD_WAYPOINT, 0,0,0,0, A.lat, A.lon, A.terrainH);
//     wpLine(s, seq, 0, FRAME_GLOBAL, CMD_TAKEOFF,  0,0,0,0, A.lat, A.lon, A.terrainH + p.takeoffAlt);
//     wpWP(s, seq, A.lat, A.lon, A.terrainH + p.nearHomeAlt);
//     wpWP(s, seq, A.lat, A.lon, A.terrainH + p.cruiseAlt);
//     wpSpeed(s, seq, p.cruiseSpeedMs);
//     writeRouteWPs(s, seq, out);
//     wpWP(s, seq, B.lat, B.lon, B.terrainH + p.landAlt);
//     wpLine(s, seq, 0, FRAME_GLOBAL, CMD_LAND, 0,0,0,0, B.lat, B.lon, B.terrainH);
//     wpDisarm(s, seq);

//     // At destination: user delay
//     wpDelay(s, seq, p.waitDelayS);

//     // Return: ARM → 5s → takeoff → route → land
//     wpArm(s, seq);
//     wpDelay(s, seq, 5);
//     wpLine(s, seq, 0, FRAME_GLOBAL, CMD_TAKEOFF, 0,0,0,0, B.lat, B.lon, B.terrainH + p.takeoffAlt);
//     wpWP(s, seq, B.lat, B.lon, B.terrainH + p.nearHomeAlt);
//     wpWP(s, seq, B.lat, B.lon, B.terrainH + p.cruiseAlt);
//     wpSpeed(s, seq, p.cruiseSpeedMs);
//     writeRouteWPs(s, seq, ret);
//     wpWP(s, seq, C.lat, C.lon, C.terrainH + p.landAlt);
//     wpLine(s, seq, 0, FRAME_GLOBAL, CMD_LAND, 0,0,0,0, C.lat, C.lon, C.terrainH);
//     wpDisarm(s, seq);
// }

// // ── DROP / PARADROP RETURN ────────────────────────────────────────────────────
// static void dropReturn(QTextStream& s, int& seq,
//                         const QVector<GeoPoint>& out,
//                         const QVector<GeoPoint>& ret,
//                         const MissionParams& p, int rcCh, int rcCount)
// {
//     const GeoPoint& A = out.first();
//     const GeoPoint& B = out.last();
//     const GeoPoint& C = ret.last();

//     // Outbound
//     wpLine(s, seq, 1, FRAME_GLOBAL, CMD_WAYPOINT, 0,0,0,0, A.lat, A.lon, A.terrainH);
//     wpLine(s, seq, 0, FRAME_GLOBAL, CMD_TAKEOFF,  0,0,0,0, A.lat, A.lon, A.terrainH + p.takeoffAlt);
//     wpWP(s, seq, A.lat, A.lon, A.terrainH + p.nearHomeAlt);
//     wpWP(s, seq, A.lat, A.lon, A.terrainH + p.cruiseAlt);
//     wpSpeed(s, seq, p.cruiseSpeedMs);
//     writeRouteWPs(s, seq, out);

//     // Arrive at destination at clearance altitude first
//     wpWP(s, seq, B.lat, B.lon, B.terrainH + p.cruiseAlt);

//     // Descend to dropping altitude
//     wpWP(s, seq, B.lat, B.lon, B.terrainH + p.dropAlt);

//     // Hover (user seconds)
//     wpDelay(s, seq, p.waitDelayS);

//     // RC trigger sequence for each configured trigger count
//     for (int t = 0; t < rcCount; ++t)
//         rcTrigger(s, seq, rcCh + t);

//     // Climb back to clearance altitude
//     wpWP(s, seq, B.lat, B.lon, B.terrainH + p.cruiseAlt);

//     // Return
//     wpSpeed(s, seq, p.cruiseSpeedMs);
//     writeRouteWPs(s, seq, ret);
//     wpWP(s, seq, C.lat, C.lon, C.terrainH + p.landAlt);
//     wpLine(s, seq, 0, FRAME_GLOBAL, CMD_LAND, 0,0,0,0, C.lat, C.lon, C.terrainH);
//     wpDisarm(s, seq);
// }

// // ── Public API ────────────────────────────────────────────────────────────────

// bool WaypointWriter::write(const QString& outputPath, const MissionParams& p,
//                             const QVector<GeoPoint>& anchors,
//                             const QVector<GeoPoint>& returnAnchors,
//                             TripMode trip, MissionType mtype,
//                             const DroneModel& drone) const
// {
//     if (anchors.size() < 2) { qWarning() << "WaypointWriter: not enough anchors"; return false; }
//     QFile f(outputPath);
//     if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
//         qWarning() << "WaypointWriter: cannot open" << outputPath; return false;
//     }
//     QTextStream s(&f);
//     s << "QGC WPL 110\n";
//     int seq = 0;

//     qDebug() << "WaypointWriter params:"
//              << "takeoffAlt=" << p.takeoffAlt
//              << "nearHomeAlt=" << p.nearHomeAlt
//              << "cruiseAlt=" << p.cruiseAlt
//              << "dropAlt=" << p.dropAlt
//              << "landAlt=" << p.landAlt
//              << "speed=" << p.cruiseSpeedMs
//              << "waitS=" << p.waitDelayS
//              << "rcCh=" << drone.rcSwitch
//              << "rcCount=" << p.rcTriggerCount;

//     switch (mtype) {
//     case MissionType::Delivery:
//         if (trip == TripMode::Return && returnAnchors.size() >= 2)
//             deliveryReturn(s, seq, anchors, returnAnchors, p);
//         else
//             deliveryOneWay(s, seq, anchors, p);
//         break;
//     case MissionType::Drop:
//     case MissionType::Paradrop: {
//         int ch = drone.rcSwitch > 0 ? drone.rcSwitch : 1;
//         int cnt = p.rcTriggerCount > 0 ? p.rcTriggerCount : 1;
//         if (returnAnchors.size() >= 2)
//             dropReturn(s, seq, anchors, returnAnchors, p, ch, cnt);
//         else
//             qWarning() << "WaypointWriter: Drop/Paradrop needs return anchors";
//         break;
//     }
//     }

//     f.close();
//     qDebug() << "WaypointWriter: wrote" << seq << "waypoints to" << outputPath;
//     return true;
// }

// QString WaypointWriter::buildFilename(const QString& outputDir, const MissionParams& p,
//                                        const DroneModel& drone, MissionType mtype,
//                                        TripMode trip, double distanceM)
// {
//     QString sub;
//     switch (mtype) {
//     case MissionType::Delivery: sub = (trip==TripMode::Return) ? "Delivery/Return" : "Delivery/OneWay"; break;
//     case MissionType::Drop:     sub = "Drop/Return";     break;
//     case MissionType::Paradrop: sub = "Paradrop/Return"; break;
//     }
//     QDir().mkpath(outputDir + "/" + sub);
//     return outputDir + "/" + sub + "/" +
//            QString("M-SRTM1-P-ABS-S-%1-%2-%3_D-%4Kms_W-%5Sec.waypoints")
//                .arg(drone.displayName)
//                .arg(missionTag(mtype))
//                .arg(trip==TripMode::Return ? "Return" : "OneWay")
//                .arg(distanceM/1000.0, 0, 'f', 1)
//                .arg(p.waitDelayS, 0, 'f', 0);
// }


#include "waypoint_writer.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>

static const int CMD_WAYPOINT    = 16;
static const int CMD_TAKEOFF     = 22;
static const int CMD_LAND        = 21;
static const int CMD_LOITER_TIME = 93;
static const int CMD_ARM_DISARM  = 218;   // ArduPilot: p1=153, p2=1 arm / p2=0 disarm
static const int CMD_CHANGE_SPD  = 178;
static const int CMD_SET_SERVO   = 183;
static const int FRAME_GLOBAL    = 0;
static const int FRAME_MISSION   = 2;
static const int PWM_LOW         = 1100;
static const int PWM_HIGH        = 1900;

static void wpLine(QTextStream& s, int& seq,
                   int isCurrent, int frame, int cmd,
                   double p1, double p2, double p3, double p4,
                   double lat, double lon, double alt, int ac = 1)
{
    s << seq++ << "\t" << isCurrent << "\t" << frame << "\t" << cmd << "\t"
      << QString::number(p1,  'f', 8) << "\t"
      << QString::number(p2,  'f', 8) << "\t"
      << QString::number(p3,  'f', 8) << "\t"
      << QString::number(p4,  'f', 8) << "\t"
      << QString::number(lat, 'f', 7) << "\t"
      << QString::number(lon, 'f', 7) << "\t"
      << QString::number(alt, 'f', 2) << "\t"
      << ac << "\n";
}

static void wpWP(QTextStream& s, int& seq, double lat, double lon, double alt)
{ wpLine(s, seq, 0, FRAME_GLOBAL, CMD_WAYPOINT, 0,0,0,0, lat, lon, alt); }

static void wpDelay(QTextStream& s, int& seq, double sec)
{ wpLine(s, seq, 0, FRAME_MISSION, CMD_LOITER_TIME, sec, 0,0,0, 0,0,0); }

static void wpSpeed(QTextStream& s, int& seq, double mps)
{ wpLine(s, seq, 0, FRAME_MISSION, CMD_CHANGE_SPD, 0, mps, -1, 0, 0,0,0); }

static void wpArm(QTextStream& s, int& seq)
{ wpLine(s, seq, 0, FRAME_MISSION, CMD_ARM_DISARM, 153, 2, 0,0, 0,0,0); }

static void wpDisarm(QTextStream& s, int& seq)
{ wpLine(s, seq, 0, FRAME_MISSION, CMD_ARM_DISARM, 153, 0, 0,0, 0,0,0); }

static void wpServo(QTextStream& s, int& seq, int ch, int pwm)
{ wpLine(s, seq, 0, FRAME_MISSION, CMD_SET_SERVO, ch, pwm, 0,0, 0,0,0); }

// One RC trigger cycle: 2s → LOW → 2s → HIGH → 2s
static void rcTrigger(QTextStream& s, int& seq, int ch)
{
    wpDelay(s, seq, 2);
    wpServo(s, seq, ch, PWM_LOW);
    wpDelay(s, seq, 2);
    wpServo(s, seq, ch, PWM_HIGH);
    wpDelay(s, seq, 2);
}

// Skip first (takeoff) and last (landing) anchors — write only intermediate
static void writeRouteWPs(QTextStream& s, int& seq, const QVector<GeoPoint>& anchors)
{
    for (int i = 1; i < anchors.size() - 1; ++i)
        wpWP(s, seq, anchors[i].lat, anchors[i].lon, anchors[i].droneAlt);
}

// ── DELIVERY ONE-WAY ──────────────────────────────────────────────────────────
static void deliveryOneWay(QTextStream& s, int& seq,
                            const QVector<GeoPoint>& out, const MissionParams& p)
{
    const GeoPoint& A = out.first();
    const GeoPoint& B = out.last();
    double asl = A.terrainH;

    wpLine(s, seq, 1, FRAME_GLOBAL, CMD_WAYPOINT, 0,0,0,0, A.lat, A.lon, asl);
    wpLine(s, seq, 0, FRAME_GLOBAL, CMD_TAKEOFF,  0,0,0,0, A.lat, A.lon, asl + p.takeoffAlt);
    wpWP(s, seq, A.lat, A.lon, asl + p.nearHomeAlt);
    wpWP(s, seq, A.lat, A.lon, asl + p.cruiseAlt);
    wpSpeed(s, seq, p.cruiseSpeedMs);
    writeRouteWPs(s, seq, out);
    wpWP(s, seq, B.lat, B.lon, B.terrainH + p.landAlt);
    wpLine(s, seq, 0, FRAME_GLOBAL, CMD_LAND, 0,0,0,0, B.lat, B.lon, B.terrainH);
    wpDisarm(s, seq);
}

// ── DELIVERY RETURN ───────────────────────────────────────────────────────────
static void deliveryReturn(QTextStream& s, int& seq,
                            const QVector<GeoPoint>& out,
                            const QVector<GeoPoint>& ret,
                            const MissionParams& p)
{
    const GeoPoint& A = out.first();   // home takeoff
    const GeoPoint& B = out.last();    // destination
    const GeoPoint& C = ret.last();    // home landing (same as A usually)

    // Outbound
    wpLine(s, seq, 1, FRAME_GLOBAL, CMD_WAYPOINT, 0,0,0,0, A.lat, A.lon, A.terrainH);
    wpLine(s, seq, 0, FRAME_GLOBAL, CMD_TAKEOFF,  0,0,0,0, A.lat, A.lon, A.terrainH + p.takeoffAlt);
    wpWP(s, seq, A.lat, A.lon, A.terrainH + p.nearHomeAlt);
    wpWP(s, seq, A.lat, A.lon, A.terrainH + p.cruiseAlt);
    wpSpeed(s, seq, p.cruiseSpeedMs);
    writeRouteWPs(s, seq, out);
    wpWP(s, seq, B.lat, B.lon, B.terrainH + p.landAlt);
    wpLine(s, seq, 0, FRAME_GLOBAL, CMD_LAND, 0,0,0,0, B.lat, B.lon, B.terrainH);
    wpDisarm(s, seq);

    // At destination: user delay
    wpDelay(s, seq, p.waitDelayS);

    // Return: ARM → 5s → takeoff → route → land
    wpArm(s, seq);
    wpDelay(s, seq, 5);
    wpLine(s, seq, 0, FRAME_GLOBAL, CMD_TAKEOFF, 0,0,0,0, B.lat, B.lon, B.terrainH + p.takeoffAlt);
    wpWP(s, seq, B.lat, B.lon, B.terrainH + p.nearHomeAlt);
    wpWP(s, seq, B.lat, B.lon, B.terrainH + p.cruiseAlt);
    wpSpeed(s, seq, p.cruiseSpeedMs);
    writeRouteWPs(s, seq, ret);
    wpWP(s, seq, C.lat, C.lon, C.terrainH + p.landAlt);
    wpLine(s, seq, 0, FRAME_GLOBAL, CMD_LAND, 0,0,0,0, C.lat, C.lon, C.terrainH);
    wpDisarm(s, seq);
}

// ── DROP / PARADROP RETURN ────────────────────────────────────────────────────
static void dropReturn(QTextStream& s, int& seq,
                        const QVector<GeoPoint>& out,
                        const QVector<GeoPoint>& ret,
                        const MissionParams& p, int rcCh, int rcCount)
{
    const GeoPoint& A = out.first();
    const GeoPoint& B = out.last();
    const GeoPoint& C = ret.last();

    // Outbound
    wpLine(s, seq, 1, FRAME_GLOBAL, CMD_WAYPOINT, 0,0,0,0, A.lat, A.lon, A.terrainH);
    wpLine(s, seq, 0, FRAME_GLOBAL, CMD_TAKEOFF,  0,0,0,0, A.lat, A.lon, A.terrainH + p.takeoffAlt);
    wpWP(s, seq, A.lat, A.lon, A.terrainH + p.nearHomeAlt);
    wpWP(s, seq, A.lat, A.lon, A.terrainH + p.cruiseAlt);
    wpSpeed(s, seq, p.cruiseSpeedMs);
    writeRouteWPs(s, seq, out);

    // Arrive at destination at clearance altitude first
    wpWP(s, seq, B.lat, B.lon, B.terrainH + p.cruiseAlt);

    // Descend to dropping altitude
    wpWP(s, seq, B.lat, B.lon, B.terrainH + p.dropAlt);

    // Hover (user seconds)
    wpDelay(s, seq, p.waitDelayS);

    // RC trigger sequence for each configured trigger count
    for (int t = 0; t < rcCount; ++t)
        rcTrigger(s, seq, rcCh + t);

    // Climb back to clearance altitude
    wpWP(s, seq, B.lat, B.lon, B.terrainH + p.cruiseAlt);

    // Return
    wpSpeed(s, seq, p.cruiseSpeedMs);
    writeRouteWPs(s, seq, ret);
    wpWP(s, seq, C.lat, C.lon, C.terrainH + p.landAlt);
    wpLine(s, seq, 0, FRAME_GLOBAL, CMD_LAND, 0,0,0,0, C.lat, C.lon, C.terrainH);
    wpDisarm(s, seq);
}

// ── Public API ────────────────────────────────────────────────────────────────

bool WaypointWriter::write(const QString& outputPath, const MissionParams& p,
                            const QVector<GeoPoint>& anchors,
                            const QVector<GeoPoint>& returnAnchors,
                            TripMode trip, MissionType mtype,
                            const DroneModel& drone) const
{
    if (anchors.size() < 2) { qWarning() << "WaypointWriter: not enough anchors"; return false; }
    QFile f(outputPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "WaypointWriter: cannot open" << outputPath; return false;
    }
    QTextStream s(&f);
    s << "QGC WPL 110\n";
    int seq = 0;

    qDebug() << "WaypointWriter params:"
             << "takeoffAlt=" << p.takeoffAlt
             << "nearHomeAlt=" << p.nearHomeAlt
             << "cruiseAlt=" << p.cruiseAlt
             << "dropAlt=" << p.dropAlt
             << "landAlt=" << p.landAlt
             << "speed=" << p.cruiseSpeedMs
             << "waitS=" << p.waitDelayS
             << "rcCh=" << drone.rcSwitch
             << "rcCount=" << p.rcTriggerCount;

    switch (mtype) {
    case MissionType::Delivery:
        if (trip == TripMode::Return && returnAnchors.size() >= 2)
            deliveryReturn(s, seq, anchors, returnAnchors, p);
        else
            deliveryOneWay(s, seq, anchors, p);
        break;
    case MissionType::Drop:
    case MissionType::Paradrop: {
        int ch = drone.rcSwitch > 0 ? drone.rcSwitch : 1;
        int cnt = p.rcTriggerCount > 0 ? p.rcTriggerCount : 1;
        if (returnAnchors.size() >= 2)
            dropReturn(s, seq, anchors, returnAnchors, p, ch, cnt);
        else
            qWarning() << "WaypointWriter: Drop/Paradrop needs return anchors";
        break;
    }
    }

    f.close();
    qDebug() << "WaypointWriter: wrote" << seq << "waypoints to" << outputPath;
    return true;
}

QString WaypointWriter::buildFilename(const QString& outputDir, const MissionParams& p,
                                       const DroneModel& drone, MissionType mtype,
                                       TripMode trip, double distanceM)
{
    QString sub;
    switch (mtype) {
    case MissionType::Delivery: sub = (trip==TripMode::Return) ? "Delivery/Return" : "Delivery/OneWay"; break;
    case MissionType::Drop:     sub = "Drop/Return";     break;
    case MissionType::Paradrop: sub = "Paradrop/Return"; break;
    }
    QDir().mkpath(outputDir + "/" + sub);
    return outputDir + "/" + sub + "/" +
           QString("M-SRTM1-P-ABS-S-%1-%2-%3_D-%4Kms_W-%5Sec.waypoints")
               .arg(drone.displayName)
               .arg(missionTag(mtype))
               .arg(trip==TripMode::Return ? "Return" : "OneWay")
               .arg(distanceM/1000.0, 0, 'f', 1)
               .arg(p.waitDelayS, 0, 'f', 0);
}
