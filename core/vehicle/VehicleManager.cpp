#include "VehicleManager.h"
#include <QDebug>

VehicleManager::VehicleManager(QObject *parent)
    : QObject(parent)
{
    activeVehicleObject = new Vehicle(this);
}

QObject* VehicleManager::activeVehicle() const {
    return activeVehicleObject;
}
