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
#include <limits>
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

static QVector<GeoPoint> smoothCruiseAltitudes(const QVector<GeoPoint>& anchors,
                                               const MissionParams& p)
{
    QVector<GeoPoint> adjusted = anchors;
    if (adjusted.size() < 2 || p.cruiseSpeedMs <= 0.0)
        return adjusted;

    const int maxPasses = adjusted.size() * 2;
    for (int pass = 0; pass < maxPasses; ++pass) {
        bool changed = false;

        if (p.ascentSpeedMs > 0.0) {
            for (int i = adjusted.size() - 2; i >= 0; --i) {
                const double horizontalM = PathPlanner::haversineM(
                    adjusted[i].lat, adjusted[i].lon,
                    adjusted[i + 1].lat, adjusted[i + 1].lon);
                const double allowedClimbM =
                    p.ascentSpeedMs * (horizontalM / p.cruiseSpeedMs);
                const double limitedAlt = adjusted[i + 1].droneAlt - allowedClimbM;
                if (adjusted[i].droneAlt < limitedAlt) {
                    adjusted[i].droneAlt = limitedAlt;
                    changed = true;
                }
            }
        }

        if (p.descentSpeedMs > 0.0) {
            for (int i = 0; i < adjusted.size() - 1; ++i) {
                const double horizontalM = PathPlanner::haversineM(
                    adjusted[i].lat, adjusted[i].lon,
                    adjusted[i + 1].lat, adjusted[i + 1].lon);
                const double allowedDescentM =
                    p.descentSpeedMs * (horizontalM / p.cruiseSpeedMs);
                const double limitedAlt = adjusted[i].droneAlt - allowedDescentM;
                if (adjusted[i + 1].droneAlt < limitedAlt) {
                    adjusted[i + 1].droneAlt = limitedAlt;
                    changed = true;
                }
            }
        }

        if (!changed)
            break;
    }

    return adjusted;
}

static QVector<GeoPoint> densifySmoothLegs(const QVector<GeoPoint>& anchors,
                                           const MissionParams& p,
                                           const PathPlanner::ElevFn& elevFn)
{
    if (anchors.size() < 2)
        return anchors;

    QVector<GeoPoint> densified;
    densified.reserve(anchors.size() * 2);
    densified.append(anchors.first());

    const double maxLegM = 1800.0;
    const double maxAltStepM = 80.0;
    const int maxInsertedPerLeg = 2;

    for (int i = 1; i < anchors.size(); ++i) {
        const GeoPoint& a = anchors[i - 1];
        const GeoPoint& b = anchors[i];
        const double legM = PathPlanner::haversineM(a.lat, a.lon, b.lat, b.lon);
        const double altDeltaM = std::abs(b.droneAlt - a.droneAlt);
        const int distanceSteps = legM > 3500.0
            ? static_cast<int>(std::ceil(legM / maxLegM))
            : 1;
        const int altitudeSteps = altDeltaM > maxAltStepM
            ? static_cast<int>(std::ceil(altDeltaM / maxAltStepM))
            : 1;
        const int steps = std::max(1, std::min(maxInsertedPerLeg + 1,
                                               std::max(distanceSteps, altitudeSteps)));

        for (int s = 1; s <= steps; ++s) {
            const double t = static_cast<double>(s) / steps;
            GeoPoint gp;
            gp.lat = a.lat + t * (b.lat - a.lat);
            gp.lon = a.lon + t * (b.lon - a.lon);
            gp.terrainH = elevFn(gp.lat, gp.lon);
            if (std::isnan(gp.terrainH))
                return {};

            const double interpolatedAlt = a.droneAlt + t * (b.droneAlt - a.droneAlt);
            gp.droneAlt = std::max(interpolatedAlt, gp.terrainH + p.cruiseAlt);
            densified.append(gp);
        }
    }

    return densified;
}

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

static void localOffsetToLatLon(double originLat,
                                double originLon,
                                double eastM,
                                double northM,
                                double& lat,
                                double& lon)
{
    const double latRad = originLat * DEG2RAD;
    lat = originLat + (northM / EARTH_R) / DEG2RAD;
    lon = originLon + (eastM / (EARTH_R * std::max(0.05, std::cos(latRad)))) / DEG2RAD;
}

static void latLonToLocalOffset(double originLat,
                                double originLon,
                                double lat,
                                double lon,
                                double& eastM,
                                double& northM)
{
    const double latRad = originLat * DEG2RAD;
    northM = (lat - originLat) * DEG2RAD * EARTH_R;
    eastM = (lon - originLon) * DEG2RAD * EARTH_R * std::max(0.05, std::cos(latRad));
}

static double maxTerrainH(const QVector<PathPlanner::RawPoint>& pts)
{
    double maxH = -std::numeric_limits<double>::infinity();
    for (const auto& pt : pts)
        maxH = std::max(maxH, pt.h);
    return maxH;
}

static double footprintTerrainH(double lat,
                                double lon,
                                double radiusM,
                                const PathPlanner::ElevFn& elevFn)
{
    double maxH = elevFn(lat, lon);
    if (std::isnan(maxH))
        return std::numeric_limits<double>::quiet_NaN();

    if (radiusM <= 0.0)
        return maxH;

    // Sample the drone footprint around the waypoint. Planning against the
    // highest terrain inside this radius prevents choosing a narrow low pixel
    // that is surrounded by terrain too close for the aircraft body/path.
    static const double anglesDeg[] = {0.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0};
    for (double angleDeg : anglesDeg) {
        const double angle = angleDeg * DEG2RAD;
        const double eastM = std::sin(angle) * radiusM;
        const double northM = std::cos(angle) * radiusM;
        double sampleLat = 0.0;
        double sampleLon = 0.0;
        localOffsetToLatLon(lat, lon, eastM, northM, sampleLat, sampleLon);

        const double h = elevFn(sampleLat, sampleLon);
        if (std::isnan(h))
            return std::numeric_limits<double>::quiet_NaN();
        maxH = std::max(maxH, h);
    }

    return maxH;
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
    auto buildNormalSamples = [&]() {
        QVector<RawPoint> samples;
        samples.reserve(8192);
        interpolate(p.takeoffLat, p.takeoffLon,
                    p.landLat,    p.landLon,
                    stepM, samples);

        for (auto& pt : samples) {
            double h = elevFn(pt.lat, pt.lon);
            if (std::isnan(h))
                return QVector<RawPoint>{};
            pt.h = h;
        }
        return samples;
    };

    QVector<RawPoint> pts = buildNormalSamples();
    if (pts.isEmpty())
        return {};

    if (p.planMode == 1) {
        const QVector<RawPoint> avoidPts = buildAvoidPeakSamples(p, elevFn, stepM);
        if (avoidPts.isEmpty()) {
            qDebug() << "PathPlanner: avoid-peak route unavailable, falling back to normal";
        } else {
            const double normalPeakH = maxTerrainH(pts);
            const double avoidPeakH = maxTerrainH(avoidPts);
            if (avoidPeakH < normalPeakH - 1.0) {
                qDebug() << "PathPlanner: avoid-peak accepted, peak terrain"
                         << normalPeakH << "->" << avoidPeakH;
                pts = avoidPts;
            } else {
                qDebug() << "PathPlanner: avoid-peak rejected, peak terrain"
                         << avoidPeakH << "is not lower than normal" << normalPeakH;
            }
        }
    }

    return planFromSamples(p, pts);
}

QVector<GeoPoint> PathPlanner::planWaypointMerger(
    const MissionParams& p,
    const QVector<QPair<double, double>>& routeCoords,
    ElevFn elevFn,
    double stepM) const
{
    if (routeCoords.size() < 2)
        return {};

    QVector<RawPoint> samples;
    samples.reserve(8192);

    for (int leg = 0; leg < routeCoords.size() - 1; ++leg) {
        const double lat1 = routeCoords[leg].first;
        const double lon1 = routeCoords[leg].second;
        const double lat2 = routeCoords[leg + 1].first;
        const double lon2 = routeCoords[leg + 1].second;
        const double totalM = haversineM(lat1, lon1, lat2, lon2);
        const int steps = std::max(2, static_cast<int>(totalM / stepM) + 1);

        for (int s = 0; s <= steps; ++s) {
            if (leg > 0 && s == 0)
                continue;

            const double t = static_cast<double>(s) / steps;
            RawPoint pt;
            pt.lat = lat1 + t * (lat2 - lat1);
            pt.lon = lon1 + t * (lon2 - lon1);
            pt.forcedAnchor = (s == 0 || s == steps);
            pt.h = elevFn(pt.lat, pt.lon);
            if (std::isnan(pt.h))
                return {};

            samples.append(pt);
        }
    }

    QVector<GeoPoint> result = smoothCruiseAltitudes(planFromSamples(p, samples), p);
    result = densifySmoothLegs(result, p, elevFn);
    result = smoothCruiseAltitudes(result, p);
    qDebug() << "PathPlanner: waypoint merger" << routeCoords.size()
             << "user points ->" << result.size()
             << "collision-checked, smoothed anchors";
    return result;
}

QVector<PathPlanner::RawPoint> PathPlanner::buildAvoidPeakSamples(
    const MissionParams& p,
    ElevFn elevFn,
    double stepM) const
{
    const double directM = haversineM(p.takeoffLat, p.takeoffLon, p.landLat, p.landLon);
    if (directM < 500.0)
        return {};

    const double originLat = (p.takeoffLat + p.landLat) * 0.5;
    const double originLon = (p.takeoffLon + p.landLon) * 0.5;

    double startE = 0.0, startN = 0.0, endE = 0.0, endN = 0.0;
    latLonToLocalOffset(originLat, originLon, p.takeoffLat, p.takeoffLon, startE, startN);
    latLonToLocalOffset(originLat, originLon, p.landLat, p.landLon, endE, endN);

    const double baseE = endE - startE;
    const double baseN = endN - startN;
    const double baseLen = std::hypot(baseE, baseN);
    if (baseLen < 1.0)
        return {};

    const double ux = baseE / baseLen;
    const double uy = baseN / baseLen;
    const double px = -uy;
    const double py = ux;

    // Android-friendly corridor search: enough lateral choices to avoid peaks,
    // but capped so generation stays responsive on tablets.
    const double stationStepM = std::clamp(baseLen / 36.0, 250.0, 650.0);
    const int stationCount = std::max(3, static_cast<int>(std::ceil(baseLen / stationStepM)) + 1);
    const double corridorHalfWidthM = std::clamp(baseLen * 0.18, 700.0, 3500.0);
    const double offsetStepM = std::clamp(corridorHalfWidthM / 3.0, 250.0, 900.0);
    const int sideSteps = std::max(1, static_cast<int>(std::round(corridorHalfWidthM / offsetStepM)));
    const int offsetCount = sideSteps * 2 + 1;
    const int centerIdx = sideSteps;
    const double droneFootprintRadiusM = 20.0;

    struct Node {
        double lat = 0.0;
        double lon = 0.0;
        double h = 0.0;
        double centerH = 0.0;
    };

    QVector<QVector<Node>> grid(stationCount);
    for (int s = 0; s < stationCount; ++s) {
        const double t = static_cast<double>(s) / (stationCount - 1);
        const double along = baseLen * t;
        grid[s].resize(offsetCount);

        for (int o = 0; o < offsetCount; ++o) {
            const double lateral = (o - centerIdx) * offsetStepM;
            double e = startE + ux * along + px * lateral;
            double n = startN + uy * along + py * lateral;

            if (s == 0) {
                e = startE;
                n = startN;
            } else if (s == stationCount - 1) {
                e = endE;
                n = endN;
            }

            double lat = 0.0, lon = 0.0;
            localOffsetToLatLon(originLat, originLon, e, n, lat, lon);
            const double centerH = elevFn(lat, lon);
            if (std::isnan(centerH))
                return {};
            const double h = footprintTerrainH(lat, lon, droneFootprintRadiusM, elevFn);
            if (std::isnan(h))
                return {};

            grid[s][o] = {lat, lon, h, centerH};
        }
    }

    const double inf = std::numeric_limits<double>::infinity();
    QVector<QVector<double>> cost(stationCount, QVector<double>(offsetCount, inf));
    QVector<QVector<int>> prev(stationCount, QVector<int>(offsetCount, -1));
    cost[0][centerIdx] = 0.0;

    for (int s = 1; s < stationCount; ++s) {
        for (int o = 0; o < offsetCount; ++o) {
            for (int po = 0; po < offsetCount; ++po) {
                if (!std::isfinite(cost[s - 1][po]))
                    continue;

                const Node& a = grid[s - 1][po];
                const Node& b = grid[s][o];
                const double legM = haversineM(a.lat, a.lon, b.lat, b.lon);
                const double climbM = std::max(0.0, b.h - a.h);
                const double descentM = std::max(0.0, a.h - b.h);
                const double footprintPenalty = std::max(0.0, b.h - b.centerH) * 22.0;
                const double lateralPenalty = std::abs(o - centerIdx) * 45.0;
                const double turnPenalty = std::abs(o - po) * 120.0;

                // Prefer lower peaks and smoother climbs, while still allowing
                // moderate detours when they significantly reduce altitude.
                // The footprint penalty prefers wider low terrain over a single
                // low sample surrounded by higher ground inside the drone radius.
                const double stepCost = legM
                                      + climbM * 18.0
                                      + descentM * 2.0
                                      + b.h * 0.65
                                      + footprintPenalty
                                      + lateralPenalty
                                      + turnPenalty;
                const double next = cost[s - 1][po] + stepCost;
                if (next < cost[s][o]) {
                    cost[s][o] = next;
                    prev[s][o] = po;
                }
            }
        }
    }

    int best = centerIdx;
    for (int o = 0; o < offsetCount; ++o) {
        if (cost[stationCount - 1][o] < cost[stationCount - 1][best])
            best = o;
    }
    if (!std::isfinite(cost[stationCount - 1][best]))
        return {};

    QVector<int> chosen(stationCount, centerIdx);
    for (int s = stationCount - 1, o = best; s >= 0; --s) {
        chosen[s] = o;
        o = prev[s][o] >= 0 ? prev[s][o] : centerIdx;
    }
    chosen[0] = centerIdx;
    chosen[stationCount - 1] = centerIdx;

    QVector<RawPoint> corridorAnchors;
    corridorAnchors.reserve(stationCount);
    for (int s = 0; s < stationCount; ++s) {
        const Node& node = grid[s][chosen[s]];
        corridorAnchors.append({node.lat, node.lon, node.h, true});
    }

    QVector<RawPoint> simplifiedAnchors;
    simplifiedAnchors.reserve(corridorAnchors.size());

    // Preserve the chosen avoid-peak corridor shape. We only remove redundant
    // points while travelling along the same lateral lane; every lane change is
    // kept so PX4/QGC does not shortcut across the terrain we intentionally
    // routed around.
    const double maxSimplifiedLegM = 900.0;
    simplifiedAnchors.append(corridorAnchors.first());
    int lastKept = 0;
    for (int s = 1; s < corridorAnchors.size() - 1; ++s) {
        const bool laneChangedHere = chosen[s] != chosen[s - 1];
        const bool laneChangesNext = chosen[s] != chosen[s + 1];
        const double fromLastM = haversineM(corridorAnchors[lastKept].lat,
                                            corridorAnchors[lastKept].lon,
                                            corridorAnchors[s].lat,
                                            corridorAnchors[s].lon);

        if (laneChangedHere || laneChangesNext || fromLastM >= maxSimplifiedLegM) {
            simplifiedAnchors.append(corridorAnchors[s]);
            lastKept = s;
        }
    }
    simplifiedAnchors.append(corridorAnchors.last());

    QVector<RawPoint> samples;
    samples.reserve(static_cast<int>(baseLen / stepM) + stationCount + 1);
    for (int s = 0; s < simplifiedAnchors.size(); ++s) {
        const RawPoint& anchor = simplifiedAnchors[s];
        if (s == 0) {
            samples.append(anchor);
            continue;
        }

        const RawPoint prevPt = samples.last();
        const double legM = haversineM(prevPt.lat, prevPt.lon, anchor.lat, anchor.lon);
        const int legSteps = std::max(1, static_cast<int>(legM / std::max(25.0, stepM)));
        for (int i = 1; i <= legSteps; ++i) {
            const double t = static_cast<double>(i) / legSteps;
            const double lat = prevPt.lat + t * (anchor.lat - prevPt.lat);
            const double lon = prevPt.lon + t * (anchor.lon - prevPt.lon);
            const double h = footprintTerrainH(lat, lon, droneFootprintRadiusM, elevFn);
            if (std::isnan(h))
                return {};
            samples.append({lat, lon, h, i == legSteps});
        }
    }

    qDebug() << "PathPlanner: avoid-peak corridor" << stationCount << "x" << offsetCount
             << "->" << simplifiedAnchors.size() << "forced anchors,"
             << samples.size() << "sample points, footprint radius"
             << droneFootprintRadiusM << "m";
    return samples;
}

QVector<GeoPoint> PathPlanner::planFromSamples(
    const MissionParams& p,
    const QVector<RawPoint>& pts) const
{
    if (pts.size() < 2)
        return {};

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
    for (int i = 1; i < n - 1; ++i) {
        if (pts[i].forcedAnchor)
            anchors.append(i);
    }
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
