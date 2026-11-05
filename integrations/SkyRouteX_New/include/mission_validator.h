#pragma once
#include "drone_models.h"
#include "path_planner.h"
#include <QString>
#include <QStringList>

struct ValidationResult {
    bool      ok;
    QStringList errors;
    QStringList warnings;
    double    distanceM;   // computed one-way distance
};

class MissionValidator {
public:
    // Validate params against drone model limits before generating
    ValidationResult validate(const MissionParams& p,
                              const DroneModel& drone,
                              TripMode trip) const;
};
