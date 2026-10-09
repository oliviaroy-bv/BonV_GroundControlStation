#include <QtTest/QtTest>

#include "mavlink/Mavlink.h"
#include "vehicle/Vehicle.h"
#include "vehicle/VehicleManager.h"
#include "video/Video.h"

class VehicleManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void createsActiveVehicle();
    void activeVehicleOwnsIndependentSubsystems();
};

void VehicleManagerTest::createsActiveVehicle()
{
    VehicleManager manager;

    auto vehicle = qobject_cast<Vehicle*>(manager.activeVehicle());

    QVERIFY(vehicle != nullptr);
    QCOMPARE(vehicle->parent(), &manager);
}

void VehicleManagerTest::activeVehicleOwnsIndependentSubsystems()
{
    VehicleManager manager;
    auto vehicle = qobject_cast<Vehicle*>(manager.activeVehicle());

    QVERIFY(vehicle != nullptr);
    QVERIFY(qobject_cast<Mavlink*>(vehicle->mavlink()) != nullptr);
    QVERIFY(qobject_cast<Video*>(vehicle->video()) != nullptr);
    QCOMPARE(vehicle->mavlink()->parent(), vehicle);
    QCOMPARE(vehicle->video()->parent(), vehicle);
}

QTEST_MAIN(VehicleManagerTest)

#include "VehicleManagerTest.moc"
