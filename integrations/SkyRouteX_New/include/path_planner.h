// // #pragma once

// // #include <QVector>
// // #include <QString>
// // #include <functional>

// // struct GeoPoint {
// //     double lat;
// //     double lon;
// //     double terrainH;
// //     double droneAlt;
// // };

// // struct MissionParams {
// //     double takeoffLat, takeoffLon;
// //     double landLat,    landLon;
// //     double takeoffAlt;
// //     double nearHomeAlt;
// //     double cruiseAlt;
// //     double landAlt;
// //     double cruiseSpeedMs;
// //     double waitDelayS;
// //     QString fromName;
// //     QString toName;
// // };

// // class PathPlanner {
// // public:
// //     using ElevFn = std::function<double(double lat, double lon)>;

// //     QVector<GeoPoint> plan(const MissionParams& p,
// //                            ElevFn elevFn,
// //                            double stepM = 10.0) const;

// //     static double haversineM(double lat1, double lon1,
// //                               double lat2, double lon2);

// //     struct RawPoint { double lat, lon, h; };

// // private:
// //     void collectBarriers(const QVector<RawPoint>& pts,
// //                          const QVector<double>& cumDist,
// //                          int si, int ei,
// //                          double clearance,
// //                          QVector<int>& anchors) const;
// // };



// #pragma once

// #include <QVector>
// #include <QString>
// #include <functional>

// struct GeoPoint {
//     double lat;
//     double lon;
//     double terrainH;
//     double droneAlt;
// };

// struct MissionParams {
//     double takeoffLat, takeoffLon;
//     double landLat,    landLon;
//     double takeoffAlt;
//     double nearHomeAlt;
//     double cruiseAlt;
//     double dropAlt       = 30.0;  // payload release altitude (Drop/Paradrop)
//     double landAlt;
//     double cruiseSpeedMs;
//     double waitDelayS;
//     QString fromName;
//     QString toName;
// };

// class PathPlanner {
// public:
//     using ElevFn = std::function<double(double lat, double lon)>;

//     QVector<GeoPoint> plan(const MissionParams& p,
//                            ElevFn elevFn,
//                            double stepM = 10.0) const;

//     static double haversineM(double lat1, double lon1,
//                               double lat2, double lon2);

//     struct RawPoint { double lat, lon, h; };

// private:
//     void collectBarriers(const QVector<RawPoint>& pts,
//                          const QVector<double>& cumDist,
//                          int si, int ei,
//                          double clearance,
//                          QVector<int>& anchors) const;
// };



#pragma once

#include <QVector>
#include <QString>
#include <QPair>
#include <functional>

struct GeoPoint {
    double lat;
    double lon;
    double terrainH;
    double droneAlt;
};

struct MissionParams {
    double takeoffLat, takeoffLon;
    double landLat,    landLon;
    double takeoffAlt;
    double nearHomeAlt;
    double cruiseAlt;
    double dropAlt       = 30.0;  // payload release altitude (Drop/Paradrop)
    double landAlt;
    double cruiseSpeedMs;
    double ascentSpeedMs  = 1.5;
    double descentSpeedMs = 0.5;
    double verticalUpSpeedMs = 1.0;
    double verticalDownSpeedMs = 0.5;
    double waitDelayS;
    int    rcTriggerCount = 1;  // number of RC trigger pairs (from drone config)
    int    planMode = 0;        // 0=Normal, 1=PeakX, 2=RouteX, 3=NexusX, 4=FusionX
    QString fromName;
    QString toName;
};

class PathPlanner {
public:
    using ElevFn = std::function<double(double lat, double lon)>;

    QVector<GeoPoint> plan(const MissionParams& p,
                           ElevFn elevFn,
                           double stepM = 10.0) const;

    QVector<GeoPoint> planWaypointMerger(
        const MissionParams& p,
        const QVector<QPair<double, double>>& routeCoords,
        ElevFn elevFn,
        double stepM = 10.0) const;

    static double haversineM(double lat1, double lon1,
                              double lat2, double lon2);

    static QVector<GeoPoint> withVerticalRateLimitedCruiseEnds(
        const QVector<GeoPoint>& waypoints,
        double cruiseSpeedMs,
        double maxAscentSpeedMs = 1.5,
        double maxDescentSpeedMs = 0.5);

    static void limitTakeoffSideCruiseAltitude(
        QVector<GeoPoint>& waypoints,
        double cruiseSpeedMs,
        double maxAscentSpeedMs = 1.5);

    static void limitLandingSideCruiseAltitude(
        QVector<GeoPoint>& waypoints,
        double cruiseSpeedMs,
        double maxDescentSpeedMs = 0.5);

    struct RawPoint {
        double lat = 0.0;
        double lon = 0.0;
        double h = 0.0;
        bool forcedAnchor = false;
    };

private:
    QVector<GeoPoint> planFromSamples(const MissionParams& p,
                                      const QVector<RawPoint>& samples) const;
    QVector<RawPoint> buildAvoidPeakSamples(const MissionParams& p,
                                            ElevFn elevFn,
                                            double stepM) const;

    void collectBarriers(const QVector<RawPoint>& pts,
                         const QVector<double>& cumDist,
                         int si, int ei,
                         double clearance,
                         QVector<int>& anchors) const;
};
