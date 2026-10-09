#include <QtTest/QtTest>

#include "mavlink/Mavlink.h"
#include "vehicle/Vehicle.h"
#include "video/Video.h"

class VehicleTest : public QObject
{
    Q_OBJECT

private slots:
    void createsIndependentMavlinkAndVideoObjects();
    void heartbeatUpdatesArmedStateAndFlightMode();
    void attitudeMessageUpdatesAttitudeAndEmitsSignals();
    void batteryStatusMessageUpdatesBattery();
    void gpsRawMessageUpdatesFixAndQuality();
    void radioStatusUpdatesCommunicationQuality();
};

void VehicleTest::createsIndependentMavlinkAndVideoObjects()
{
    Vehicle firstVehicle;
    Vehicle secondVehicle;

    QVERIFY(firstVehicle.mavlink() != nullptr);
    QVERIFY(firstVehicle.video() != nullptr);
    QVERIFY(secondVehicle.mavlink() != nullptr);
    QVERIFY(secondVehicle.video() != nullptr);

    QVERIFY(qobject_cast<Mavlink*>(firstVehicle.mavlink()) != nullptr);
    QVERIFY(qobject_cast<Video*>(firstVehicle.video()) != nullptr);
    QVERIFY(firstVehicle.mavlink() != secondVehicle.mavlink());
    QVERIFY(firstVehicle.video() != secondVehicle.video());

    QCOMPARE(firstVehicle.mavlink()->parent(), &firstVehicle);
    QCOMPARE(firstVehicle.video()->parent(), &firstVehicle);
}

void VehicleTest::heartbeatUpdatesArmedStateAndFlightMode()
{
    Vehicle vehicle;
    QSignalSpy armedSpy(&vehicle, &Vehicle::armedChanged);
    QSignalSpy modeSpy(&vehicle, &Vehicle::flightModeChanged);

    mavlink_message_t msg = {};
    mavlink_msg_heartbeat_pack(
        1,
        1,
        &msg,
        MAV_TYPE_QUADROTOR,
        MAV_AUTOPILOT_ARDUPILOTMEGA,
        MAV_MODE_FLAG_SAFETY_ARMED,
        4,
        MAV_STATE_ACTIVE
    );

    vehicle.handleMessage(msg);

    QVERIFY(vehicle.armed());
    QCOMPARE(vehicle.flightMode(), QString("GUIDED"));
    QCOMPARE(armedSpy.count(), 1);
    QCOMPARE(modeSpy.count(), 1);
}

void VehicleTest::attitudeMessageUpdatesAttitudeAndEmitsSignals()
{
    Vehicle vehicle;
    QSignalSpy rollSpy(&vehicle, &Vehicle::rollChanged);
    QSignalSpy pitchSpy(&vehicle, &Vehicle::pitchChanged);
    QSignalSpy yawSpy(&vehicle, &Vehicle::yawChanged);

    mavlink_message_t msg = {};
    mavlink_msg_attitude_pack(
        1,
        1,
        &msg,
        0,
        1.25f,
        -0.50f,
        2.75f,
        0.0f,
        0.0f,
        0.0f
    );

    vehicle.handleMessage(msg);

    QCOMPARE(vehicle.roll(), 1.25);
    QCOMPARE(vehicle.pitch(), -0.50);
    QCOMPARE(vehicle.yaw(), 2.75);
    QCOMPARE(rollSpy.count(), 1);
    QCOMPARE(pitchSpy.count(), 1);
    QCOMPARE(yawSpy.count(), 1);
}

void VehicleTest::batteryStatusMessageUpdatesBattery()
{
    Vehicle vehicle;
    QSignalSpy batterySpy(&vehicle, &Vehicle::batteryChanged);

    mavlink_message_t msg = {};
    const uint16_t voltages[10] = {};
    const uint16_t voltagesExt[4] = {};

    mavlink_msg_battery_status_pack(
        1,
        1,
        &msg,
        0,
        MAV_BATTERY_FUNCTION_ALL,
        MAV_BATTERY_TYPE_LIPO,
        INT16_MAX,
        voltages,
        -1,
        -1,
        -1,
        72,
        0,
        MAV_BATTERY_CHARGE_STATE_OK,
        voltagesExt,
        MAV_BATTERY_MODE_UNKNOWN,
        0
    );

    vehicle.handleMessage(msg);

    QCOMPARE(vehicle.battery(), 72);
    QCOMPARE(batterySpy.count(), 1);
}

void VehicleTest::gpsRawMessageUpdatesFixAndQuality()
{
    Vehicle vehicle;
    QSignalSpy gpsSpy(&vehicle, &Vehicle::gpsChanged);

    mavlink_message_t msg = {};
    mavlink_msg_gps_raw_int_pack(
        1,
        1,
        &msg,
        0,
        GPS_FIX_TYPE_RTK_FIXED,
        123456789,
        987654321,
        54321,
        95,
        140,
        1234,
        27550,
        18,
        0,
        820,
        1350,
        450,
        125000,
        0
    );

    vehicle.handleMessage(msg);

    QCOMPARE(vehicle.gpsFix(), static_cast<int>(GPS_FIX_TYPE_RTK_FIXED));
    QCOMPARE(vehicle.satellites(), 18);
    QCOMPARE(vehicle.gpsHdop(), 0.95);
    QCOMPARE(vehicle.gpsVdop(), 1.40);
    QCOMPARE(vehicle.gpsHorizontalAccuracy(), 0.82);
    QCOMPARE(vehicle.gpsVerticalAccuracy(), 1.35);
    QCOMPARE(vehicle.gpsSpeedAccuracy(), 0.45);
    QCOMPARE(vehicle.gpsHeadingAccuracy(), 1.25);
    QCOMPARE(vehicle.gpsGroundSpeed(), 12.34);
    QCOMPARE(vehicle.gpsCourse(), 275.50);
    QCOMPARE(vehicle.gpsAltitude(), 54.321);
    QCOMPARE(gpsSpy.count(), 1);
}

void VehicleTest::radioStatusUpdatesCommunicationQuality()
{
    Vehicle vehicle;
    QSignalSpy qualitySpy(&vehicle, &Vehicle::commLinkQualityChanged);

    mavlink_message_t msg = {};
    mavlink_msg_radio_status_pack(1, 1, &msg, 200, 190, 100, 0, 0, 2, 98);
    vehicle.handleMessage(msg);

    QCOMPARE(vehicle.radioRssi(), 200);
    QCOMPARE(vehicle.radioPacketLoss(), 2.0);
    QVERIFY(vehicle.commLinkQuality() >= 70);
    QVERIFY(qualitySpy.count() >= 1);
}

QTEST_MAIN(VehicleTest)

#include "VehicleTest.moc"
