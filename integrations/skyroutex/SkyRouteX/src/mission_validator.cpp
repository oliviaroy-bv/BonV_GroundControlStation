#include "mission_validator.h"
#include "path_planner.h"
#include <cmath>

ValidationResult MissionValidator::validate(const MissionParams& p,
                                             const DroneModel& drone,
                                             TripMode trip) const
{
    ValidationResult r;
    r.ok = true;

    auto validLat = [](double v){ return v >= -90.0  && v <= 90.0; };
    auto validLon = [](double v){ return v >= -180.0 && v <= 180.0; };

    if (!validLat(p.takeoffLat) || !validLon(p.takeoffLon))
        r.errors << "Takeoff coordinates are out of range.";
    if (!validLat(p.landLat) || !validLon(p.landLon))
        r.errors << "Landing coordinates are out of range.";

    if (p.fromName.trimmed().isEmpty())
        r.errors << "Takeoff location name is required.";
    else if (p.fromName.length() > 7)
        r.errors << "Takeoff location name must be 7 characters or less.";

    if (p.toName.trimmed().isEmpty())
        r.errors << "Landing location name is required.";
    else if (p.toName.length() > 7)
        r.errors << "Landing location name must be 7 characters or less.";

    r.distanceM = PathPlanner::haversineM(p.takeoffLat, p.takeoffLon,
                                           p.landLat,    p.landLon);

    double tripM = (trip == TripMode::Return) ? r.distanceM * 2.0 : r.distanceM;

    if (r.distanceM < 10.0)
        r.errors << "Takeoff and landing coordinates are too close.";

    if (tripM > drone.maxRangeM) {
        r.errors << QString("%1 max range is %2 km. This mission needs %3 km.")
                    .arg(drone.displayName)
                    .arg(drone.maxRangeM / 1000.0, 0, 'f', 1)
                    .arg(tripM           / 1000.0, 0, 'f', 1);
    } else if (tripM > drone.maxRangeM * 0.85) {
        r.warnings << QString("Mission uses %1% of %2's range — check battery.")
                      .arg(static_cast<int>(tripM / drone.maxRangeM * 100))
                      .arg(drone.displayName);
    }

    auto requireRange = [&r](double value, double minValue, double maxValue,
                             const QString& label, const QString& unit) {
        if (minValue > maxValue) {
            double tmp = minValue;
            minValue = maxValue;
            maxValue = tmp;
        }

        if (value < minValue || value > maxValue) {
            r.errors << QString("%1 must be between %2 and %3 %4.")
                        .arg(label)
                        .arg(minValue, 0, 'f', 0)
                        .arg(maxValue, 0, 'f', 0)
                        .arg(unit);
        }
    };

    requireRange(p.takeoffAlt, drone.minTakeoffAlt, drone.maxTakeoffAlt,
                 "Takeoff altitude", "m AGL");
    requireRange(p.nearHomeAlt, drone.minNearHomeAlt, drone.maxNearHomeAlt,
                 "Near-home altitude", "m AGL");
    requireRange(p.cruiseAlt, drone.minCruiseAlt, drone.maxCruiseAlt,
                 "Terrain clearance", "m");
    requireRange(p.landAlt, drone.minLandAlt, drone.maxLandAlt,
                 "Land altitude", "m AGL");
    requireRange(p.cruiseSpeedMs, drone.minCruiseSpeed, drone.maxCruiseSpeed,
                 "Cruise speed", "m/s");
    if (p.dropAlt > 0.0) {
        requireRange(p.dropAlt, drone.minTakeoffAlt, drone.maxTakeoffAlt,
                     "Dropping altitude", "m AGL");
    }

    if (p.nearHomeAlt <= p.takeoffAlt)
        r.errors << "Near-home altitude must be greater than takeoff altitude.";
    if (p.cruiseAlt <= p.nearHomeAlt)
        r.errors << "Cruise clearance must be greater than near-home altitude.";
    if (trip == TripMode::Return && p.waitDelayS < 0.0)
        r.errors << "Wait delay cannot be negative.";

    r.ok = r.errors.isEmpty();
    return r;
}
