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
    double waitDelayS;
    int    rcTriggerCount = 1;  // number of RC trigger pairs (from drone config)
    QString fromName;
    QString toName;
};

class PathPlanner {
public:
    using ElevFn = std::function<double(double lat, double lon)>;

    QVector<GeoPoint> plan(const MissionParams& p,
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

    struct RawPoint { double lat, lon, h; };

private:
    void collectBarriers(const QVector<RawPoint>& pts,
                         const QVector<double>& cumDist,
                         int si, int ei,
                         double clearance,
                         QVector<int>& anchors) const;
};
