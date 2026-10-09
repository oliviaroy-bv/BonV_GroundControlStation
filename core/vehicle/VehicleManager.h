#pragma once

#include <QObject>
#include <QMap>
#include "Vehicle.h"

class VehicleManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(QObject* activeVehicle READ activeVehicle NOTIFY activeVehicleChanged)

public:
    explicit VehicleManager(QObject *parent = nullptr);

    QObject* activeVehicle() const;

signals:
    void activeVehicleChanged();

private:
    QMap<int, Vehicle*> vehicles;
    Vehicle* activeVehicleObject = nullptr;
};
