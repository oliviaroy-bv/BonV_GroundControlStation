#pragma once
#include <QString>
#include <QVector>

// ── Drone catalogue ──────────────────────────────────────────────────────────
// All ranges are one-way distances (half of the two-way figure).
// Altitude defaults are what the operator typically uses; all are editable.

struct DroneModel {
    QString id;               // internal key
    QString displayName;      // shown in UI combo
    double  maxRangeM;        // one-way max range in metres
    int     rcSwitch;         // RC switch channel value for this vehicle

    // Altitude defaults (metres, AGL unless noted)
    double defaultTakeoffAlt;      // initial climb above ground
    double defaultNearHomeAlt;     // intermediate climb waypoint
    double defaultCruiseAlt;       // clearance above every terrain anchor
    double defaultLandAlt;         // final descent altitude above ground
    double defaultCruiseSpeed;     // m/s

    double minTakeoffAlt  = 10.0;
    double maxTakeoffAlt  = 500.0;
    double minNearHomeAlt = 20.0;
    double maxNearHomeAlt = 500.0;
    double minCruiseAlt   = 80.0;
    double maxCruiseAlt   = 800.0;
    double minLandAlt     = 80.0;
    double maxLandAlt     = 150.0;
    double minCruiseSpeed = 1.0;
    double maxCruiseSpeed = 15.0;
};

inline QVector<DroneModel> allDroneModels()
{
    return {
        { "AIR_ORCA",   "Air-Orca",   7500.0,  2,  30,  60, 120,  5, 5.0 },
        { "PELICAN",    "Pelican",    4000.0,  3,  30,  60, 120,  5, 5.0 },
        { "UTTAM",      "Uttam",     20000.0,  3,  40,  80, 150,  5, 7.0 },
        { "SARV_UTTAM", "Sarv-Uttam",20000.0,  3,  40,  80, 150,  5, 7.0 },
    };
}

// ── Mission types ────────────────────────────────────────────────────────────
enum class MissionType { Delivery, Drop, Paradrop };

inline QString missionTag(MissionType t) {
    switch (t) {
        case MissionType::Delivery: return "DEL";
        case MissionType::Drop:     return "DRP";
        case MissionType::Paradrop: return "PDR";
    }
    return "DEL";
}

inline QString missionLabel(MissionType t) {
    switch (t) {
        case MissionType::Delivery: return "Delivery";
        case MissionType::Drop:     return "Drop";
        case MissionType::Paradrop: return "Paradrop";
    }
    return "Delivery";
}

// ── Trip mode ────────────────────────────────────────────────────────────────
enum class TripMode { OneWay, Return };
