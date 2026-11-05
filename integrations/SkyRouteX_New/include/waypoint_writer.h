#pragma once
#include "path_planner.h"
#include "drone_models.h"
#include <QString>

class WaypointWriter {
public:
    // Write a complete QGC WPL 110 waypoint file.
    // For one-way missions: anchors is the outbound path.
    // For return missions: anchors is outbound, returnAnchors is the return leg.
    // The two legs are concatenated with delay/arm/disarm commands in between.
    bool write(const QString&        outputPath,
               const MissionParams&  p,
               const QVector<GeoPoint>& anchors,
               const QVector<GeoPoint>& returnAnchors,  // empty for one-way
               TripMode              trip,
               MissionType           mtype,
               const DroneModel&     drone) const;

    // Build the canonical output filename
    static QString buildFilename(const QString& outputDir,
                                 const MissionParams& p,
                                 const DroneModel& drone,
                                 MissionType mtype,
                                 TripMode trip,
                                 double distanceM);
};
