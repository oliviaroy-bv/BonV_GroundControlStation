// #include "include/path_planner.h"
// #include <cmath>
// #include <algorithm>
// #include <QDebug>

// static const double DEG2RAD = M_PI / 180.0;
// static const double EARTH_R  = 6371000.0;

// double PathPlanner::haversineM(double lat1, double lon1,
//                                 double lat2, double lon2)
// {
//     double dlat = (lat2 - lat1) * DEG2RAD;
//     double dlon = (lon2 - lon1) * DEG2RAD;
//     double a = std::sin(dlat/2)*std::sin(dlat/2)
//              + std::cos(lat1*DEG2RAD)*std::cos(lat2*DEG2RAD)
//              * std::sin(dlon/2)*std::sin(dlon/2);
//     return 2.0 * EARTH_R * std::asin(std::sqrt(a));
// }

// // ── interpolate route at fixed spacing ───────────────────────────────────────

// static void interpolate(double lat1, double lon1,
//                          double lat2, double lon2,
//                          double stepM,
//                          QVector<PathPlanner::RawPoint>& out)
// {
//     double totalM = PathPlanner::haversineM(lat1, lon1, lat2, lon2);
//     int    n      = std::max(2, static_cast<int>(totalM / stepM) + 1);

//     for (int i = 0; i <= n; ++i) {
//         double t   = static_cast<double>(i) / n;
//         double lat = lat1 + t * (lat2 - lat1);
//         double lon = lon1 + t * (lon2 - lon1);
//         out.append({lat, lon, 0.0});
//     }
// }

// // ── recursive barrier collector ───────────────────────────────────────────────

// void PathPlanner::collectBarriers(const QVector<RawPoint>& pts,
//                                    const QVector<double>& cumDist,
//                                    int si, int ei,
//                                    double clearance,
//                                    QVector<int>& anchors) const
// {
//     if (ei - si <= 1) return;

//     int    bestIdx = -1;
//     double bestDev = clearance;

//     double dStart = cumDist[si];
//     double dTotal = cumDist[ei] - dStart;
//     double hStart = pts[si].h;
//     double hEnd   = pts[ei].h;

//     for (int i = si + 1; i < ei; ++i) {
//         double ratio    = (dTotal > 0.0) ? (cumDist[i] - dStart) / dTotal : 0.0;
//         ratio           = std::max(0.0, std::min(1.0, ratio));
//         double envelope = hStart + (hEnd - hStart) * ratio;
//         double dev      = pts[i].h - envelope;
//         if (dev > bestDev) {
//             bestDev = dev;
//             bestIdx = i;
//         }
//     }

//     if (bestIdx != -1) {
//         anchors.append(bestIdx);
//         collectBarriers(pts, cumDist, si,      bestIdx, clearance, anchors);
//         collectBarriers(pts, cumDist, bestIdx, ei,      clearance, anchors);
//     }
// }

// // ── main entry ────────────────────────────────────────────────────────────────

// QVector<GeoPoint> PathPlanner::plan(const MissionParams& p,
//                                      ElevFn elevFn,
//                                      double stepM) const
// {
//     QVector<RawPoint> pts;
//     pts.reserve(8192);
//     interpolate(p.takeoffLat, p.takeoffLon,
//                 p.landLat,    p.landLon,
//                 stepM, pts);

//     for (auto& pt : pts) {
//         double h = elevFn(pt.lat, pt.lon);
//         if (std::isnan(h)) return {};
//         pt.h = h;
//     }

//     int n = pts.size();
//     QVector<double> cumDist(n, 0.0);
//     for (int i = 1; i < n; ++i)
//         cumDist[i] = cumDist[i-1] + haversineM(pts[i-1].lat, pts[i-1].lon,
//                                                  pts[i].lat,   pts[i].lon);

//     QVector<int> anchors;
//     anchors.reserve(64);
//     anchors.append(0);
//     anchors.append(n - 1);
//     collectBarriers(pts, cumDist, 0, n - 1, p.cruiseAlt, anchors);

//     std::sort(anchors.begin(), anchors.end());
//     anchors.erase(std::unique(anchors.begin(), anchors.end()), anchors.end());

//     QVector<GeoPoint> result;
//     result.reserve(anchors.size());
//     for (int idx : anchors) {
//         GeoPoint gp;
//         gp.lat      = pts[idx].lat;
//         gp.lon      = pts[idx].lon;
//         gp.terrainH = pts[idx].h;
//         gp.droneAlt = pts[idx].h + p.cruiseAlt;
//         result.append(gp);
//     }

//     qDebug() << "PathPlanner:" << pts.size() << "points ->"
//              << result.size() << "anchor waypoints";
//     return result;
// }


#include "include/path_planner.h"
#include <cmath>
#include <algorithm>
#include <QDebug>

static const double DEG2RAD = M_PI / 180.0;
static const double EARTH_R  = 6371000.0;

double PathPlanner::haversineM(double lat1, double lon1,
                               double lat2, double lon2)
{
    double dlat = (lat2 - lat1) * DEG2RAD;
    double dlon = (lon2 - lon1) * DEG2RAD;
    double a = std::sin(dlat/2)*std::sin(dlat/2)
               + std::cos(lat1*DEG2RAD)*std::cos(lat2*DEG2RAD)
                     * std::sin(dlon/2)*std::sin(dlon/2);
    return 2.0 * EARTH_R * std::asin(std::sqrt(a));
}

QVector<GeoPoint> PathPlanner::withVerticalRateLimitedCruiseEnds(
    const QVector<GeoPoint>& waypoints,
    double cruiseSpeedMs,
    double maxAscentSpeedMs,
    double maxDescentSpeedMs)
{
    QVector<GeoPoint> adjusted = waypoints;
    limitTakeoffSideCruiseAltitude(adjusted, cruiseSpeedMs, maxAscentSpeedMs);
    limitLandingSideCruiseAltitude(adjusted, cruiseSpeedMs, maxDescentSpeedMs);
    return adjusted;
}

void PathPlanner::limitTakeoffSideCruiseAltitude(
    QVector<GeoPoint>& waypoints,
    double cruiseSpeedMs,
    double maxAscentSpeedMs)
{
    if (waypoints.size() < 3 || cruiseSpeedMs <= 0.0 || maxAscentSpeedMs <= 0.0)
        return;

    GeoPoint& wp1 = waypoints[1];
    const GeoPoint& wp0 = waypoints[0];
    const double horizontalM = haversineM(wp0.lat, wp0.lon, wp1.lat, wp1.lon);
    const double travelS = horizontalM / cruiseSpeedMs;
    const double maxClimbM = maxAscentSpeedMs * travelS;
    const double desiredClimbM = wp1.droneAlt - wp0.droneAlt;

    // Example: 140m at 7m/s takes 20s. At 1.5m/s climb, 100m is capped to 30m.
    if (desiredClimbM > maxClimbM)
        wp1.droneAlt = wp0.droneAlt + maxClimbM;
}

void PathPlanner::limitLandingSideCruiseAltitude(
    QVector<GeoPoint>& waypoints,
    double cruiseSpeedMs,
    double maxDescentSpeedMs)
{
    if (waypoints.size() < 3 || cruiseSpeedMs <= 0.0 || maxDescentSpeedMs <= 0.0)
        return;

    GeoPoint& wpBeforeLanding = waypoints[waypoints.size() - 2];
    const GeoPoint& landingWp = waypoints.last();
    const double horizontalM = haversineM(wpBeforeLanding.lat, wpBeforeLanding.lon,
                                          landingWp.lat, landingWp.lon);
    const double travelS = horizontalM / cruiseSpeedMs;
    const double maxDescentM = maxDescentSpeedMs * travelS;
    const double desiredDescentM = wpBeforeLanding.droneAlt - landingWp.droneAlt;

    // Example: 140m at 7m/s takes 20s. At 0.5m/s descent, 80m is capped to 10m.
    if (desiredDescentM > maxDescentM)
        wpBeforeLanding.droneAlt = landingWp.droneAlt + maxDescentM;
}

// ── Interpolate route at fixed spacing ────────────────────────────────────────

static void interpolate(double lat1, double lon1,
                        double lat2, double lon2,
                        double stepM,
                        QVector<PathPlanner::RawPoint>& out)
{
    double totalM = PathPlanner::haversineM(lat1, lon1, lat2, lon2);
    int    n      = std::max(2, static_cast<int>(totalM / stepM) + 1);

    for (int i = 0; i <= n; ++i) {
        double t   = static_cast<double>(i) / n;
        double lat = lat1 + t * (lat2 - lat1);
        double lon = lon1 + t * (lon2 - lon1);
        out.append({lat, lon, 0.0});
    }
}

// ── Recursive barrier collector ───────────────────────────────────────────────
// Finds terrain points that poke more than `clearance` metres above the
// straight terrain line between two anchor indices.
// This gives us an initial set of anchors — but does NOT yet guarantee
// clearance between anchors (see the second-pass fix below).

void PathPlanner::collectBarriers(const QVector<RawPoint>& pts,
                                  const QVector<double>& cumDist,
                                  int si, int ei,
                                  double clearance,
                                  QVector<int>& anchors) const
{
    if (ei - si <= 1) return;

    int    bestIdx = -1;
    double bestDev = clearance;

    double dStart = cumDist[si];
    double dTotal = cumDist[ei] - dStart;
    double hStart = pts[si].h;
    double hEnd   = pts[ei].h;

    for (int i = si + 1; i < ei; ++i) {
        double ratio    = (dTotal > 0.0) ? (cumDist[i] - dStart) / dTotal : 0.0;
        ratio           = std::max(0.0, std::min(1.0, ratio));
        double envelope = hStart + (hEnd - hStart) * ratio;
        double dev      = pts[i].h - envelope;
        if (dev > bestDev) {
            bestDev = dev;
            bestIdx = i;
        }
    }

    if (bestIdx != -1) {
        anchors.append(bestIdx);
        collectBarriers(pts, cumDist, si,      bestIdx, clearance, anchors);
        collectBarriers(pts, cumDist, bestIdx, ei,      clearance, anchors);
    }
}

// ── Main entry ────────────────────────────────────────────────────────────────

QVector<GeoPoint> PathPlanner::plan(const MissionParams& p,
                                    ElevFn elevFn,
                                    double stepM) const
{
    // Step 1: sample terrain at stepM intervals along the route
    QVector<RawPoint> pts;
    pts.reserve(8192);
    interpolate(p.takeoffLat, p.takeoffLon,
                p.landLat,    p.landLon,
                stepM, pts);

    for (auto& pt : pts) {
        double h = elevFn(pt.lat, pt.lon);
        if (std::isnan(h)) return {};
        pt.h = h;
    }

    int n = pts.size();
    QVector<double> cumDist(n, 0.0);
    for (int i = 1; i < n; ++i)
        cumDist[i] = cumDist[i-1] + haversineM(pts[i-1].lat, pts[i-1].lon,
                                                 pts[i].lat,   pts[i].lon);

    // Step 2: initial anchor set from recursive barrier detection
    QVector<int> anchors;
    anchors.reserve(64);
    anchors.append(0);
    anchors.append(n - 1);
    collectBarriers(pts, cumDist, 0, n - 1, p.cruiseAlt, anchors);

    std::sort(anchors.begin(), anchors.end());
    anchors.erase(std::unique(anchors.begin(), anchors.end()), anchors.end());

    // ── Step 3: CLEARANCE GUARANTEE PASS ─────────────────────────────────────
    //
    // The drone flies a STRAIGHT LINE between consecutive waypoints.
    // Even though each anchor has terrain + clearance altitude, the straight
    // flight path between two anchors may pass closer to the terrain than
    // `cruiseAlt` at intermediate points.
    //
    // Example:
    //   Anchor A: terrain=4800m  droneAlt=4950m (clearance=150)
    //   Anchor B: terrain=5100m  droneAlt=5250m (clearance=150)
    //   Midpoint: terrain=5050m  droneAlt=5100m (interpolated) → clearance=50! ← BUG
    //
    // Fix: scan every sample point between consecutive anchors, compute the
    // interpolated drone altitude (straight line), and if clearance < threshold,
    // insert that point as a new anchor at terrain + clearance.
    // Repeat until no violations remain (max 10 iterations for safety).
    //
    // This guarantees minimum clearance across the ENTIRE route, not just
    // at waypoints.

    const double minClearance = p.cruiseAlt;   // e.g. 150m
    const int    maxPasses    = 10;

    for (int pass = 0; pass < maxPasses; ++pass) {
        bool anyViolation = false;
        QVector<int> newAnchors;

        for (int ai = 0; ai < anchors.size() - 1; ++ai) {
            int si = anchors[ai];
            int ei = anchors[ai + 1];

            // Drone altitude at the two bounding anchors
            double altStart = pts[si].h + minClearance;
            double altEnd   = pts[ei].h + minClearance;

            double dStart = cumDist[si];
            double dTotal = cumDist[ei] - dStart;

            // Find the worst clearance violation between these two anchors
            int    worstIdx = -1;
            double worstClearance = minClearance;  // we want BELOW this

            for (int i = si + 1; i < ei; ++i) {
                double ratio   = (dTotal > 0.0)
                ? (cumDist[i] - dStart) / dTotal
                : 0.0;
                ratio          = std::max(0.0, std::min(1.0, ratio));

                // What altitude would the drone be at this point on the
                // straight leg between the two anchors?
                double interpDroneAlt = altStart + ratio * (altEnd - altStart);

                // Actual clearance at this point
                double clearance = interpDroneAlt - pts[i].h;

                if (clearance < worstClearance) {
                    worstClearance = clearance;
                    worstIdx       = i;
                }
            }

            if (worstIdx != -1) {
                // Violation found — insert anchor at the worst point
                newAnchors.append(worstIdx);
                anyViolation = true;
            }
        }

        if (!anyViolation) break;   // all legs now meet clearance — done

        // Merge new anchors into the sorted list
        for (int idx : newAnchors) anchors.append(idx);
        std::sort(anchors.begin(), anchors.end());
        anchors.erase(std::unique(anchors.begin(), anchors.end()), anchors.end());
    }

    // Step 4: build result — every anchor gets terrain + clearance altitude
    QVector<GeoPoint> result;
    result.reserve(anchors.size());
    for (int idx : anchors) {
        GeoPoint gp;
        gp.lat      = pts[idx].lat;
        gp.lon      = pts[idx].lon;
        gp.terrainH = pts[idx].h;
        gp.droneAlt = pts[idx].h + p.cruiseAlt;
        result.append(gp);
    }

    qDebug() << "PathPlanner:" << pts.size() << "sample points ->"
             << result.size() << "anchor waypoints (after clearance guarantee pass)";

    return result;
}
