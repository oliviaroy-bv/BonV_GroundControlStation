#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <limits>

#include "mavlink/Mavlink.h"
#include "parameters/ParameterManager.h"
#include "vehicle/Vehicle.h"
#include "video/Video.h"

namespace {
void heartbeat(Vehicle &vehicle, bool armed = true, uint8_t system = 42, uint8_t component = 1)
{
    mavlink_message_t msg = {};
    mavlink_msg_heartbeat_pack(system, component, &msg, MAV_TYPE_QUADROTOR,
                              MAV_AUTOPILOT_ARDUPILOTMEGA,
                              armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0, 4, MAV_STATE_ACTIVE);
    vehicle.handleMessage(msg);
}

void landed(Vehicle &vehicle, uint8_t state, uint8_t system = 42, uint8_t component = 1)
{
    mavlink_message_t msg = {};
    mavlink_msg_extended_sys_state_pack(system, component, &msg, MAV_VTOL_STATE_UNDEFINED, state);
    vehicle.handleMessage(msg);
}

void position(Vehicle &vehicle, int32_t lat = 123456789, uint8_t system = 42, uint8_t component = 1)
{
    mavlink_global_position_int_t value = {};
    value.lat = lat;
    value.lon = 765432100;
    value.relative_alt = 2000;
    mavlink_message_t msg = {};
    mavlink_msg_global_position_int_encode(system, component, &msg, &value);
    vehicle.handleMessage(msg);
}

bool connectVehicle(Vehicle &vehicle, QTcpServer &server, uint8_t system = 42)
{
    if (!server.isListening() && !server.listen(QHostAddress::LocalHost, 0)) return false;
    auto *mavlink = qobject_cast<Mavlink *>(vehicle.mavlink());
    mavlink->setTelemetryLoggingEnabled(false);
    mavlink->setFlightControllerLoggingEnabled(false);
    if (!mavlink->connectToVehicle("127.0.0.1", server.serverPort())) return false;
    QElapsedTimer wait;
    wait.start();
    while (!server.hasPendingConnections() && wait.elapsed() < 1500)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    QTcpSocket *peer = server.nextPendingConnection();
    if (!peer) return false;
    mavlink_message_t msg = {};
    mavlink_msg_heartbeat_pack(system, 1, &msg, MAV_TYPE_QUADROTOR,
                              MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_MODE_FLAG_SAFETY_ARMED, 4, MAV_STATE_ACTIVE);
    uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
    const int length = mavlink_msg_to_send_buffer(bytes, &msg);
    peer->write(reinterpret_cast<const char *>(bytes), length);
    peer->flush();
    while (!mavlink->connected() && wait.elapsed() < 1500)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return mavlink->connected();
}
}

class VehicleTest : public QObject
{
    Q_OBJECT

private slots:
    void flightTimeStartsAtTakeoff_data();
    void flightTimeStartsAtTakeoff();
    void createsIndependentMavlinkAndVideoObjects();
    void heartbeatUpdatesArmedStateAndFlightMode();
    void attitudeMessageUpdatesAttitudeAndEmitsSignals();
    void batteryStatusMessageUpdatesBattery();
    void gpsRawMessageUpdatesFixAndQuality();
    void radioStatusUpdatesCommunicationQuality();
    void communicationQualityDuringDestruction();
    void landedStateTransitionsAreConservative();
    void freshnessExpiresWithoutIncomingMessages();
    void wrongSourcesCannotRefreshFlightState();
    void disconnectAndSourceChangeResetFreshness();
    void invalidHeadingAndPositionDoNotBecomeFresh();
    void unavailableLandedStateDoesNotPreventTelemetry();
    void heartbeatLossRevokesVehicleParameterReadiness();
};

void VehicleTest::flightTimeStartsAtTakeoff_data()
{
    QTest::addColumn<int>("phase");
    QTest::newRow("takeoff-report") << int(MAV_LANDED_STATE_TAKEOFF);
    QTest::newRow("in-air-report") << int(MAV_LANDED_STATE_IN_AIR);
}

void VehicleTest::flightTimeStartsAtTakeoff()
{
    QFETCH(int, phase);
    Vehicle vehicle;
    QTcpServer server;
    QVERIFY(connectVehicle(vehicle, server));
    QSignalSpy timer(&vehicle, &Vehicle::flightTimeChanged);
    landed(vehicle, MAV_LANDED_STATE_ON_GROUND);
    position(vehicle); // Existing fixture has positive relative altitude.
    heartbeat(vehicle, true);
    position(vehicle);
    QCOMPARE(timer.count(), 0);
    QCOMPARE(vehicle.flightTime(), 0);
    landed(vehicle, uint8_t(phase), 43); // Another vehicle cannot start this timer.
    QCOMPARE(timer.count(), 0);
    heartbeat(vehicle, false);
    landed(vehicle, uint8_t(phase));
    QCOMPARE(timer.count(), 0);
    heartbeat(vehicle, true);
    landed(vehicle, uint8_t(phase));
    QCOMPARE(timer.count(), 1);
    landed(vehicle, uint8_t(phase)); // Repeated reports do not restart the clock.
    QCOMPARE(timer.count(), 1);
}

void VehicleTest::createsIndependentMavlinkAndVideoObjects()
{
    Vehicle firstVehicle;
    auto *parameters = qobject_cast<ParameterManager *>(firstVehicle.parameterManager());
    QVERIFY(parameters);
    QCOMPARE(parameters->parent(), &firstVehicle);
    QCOMPARE(qobject_cast<Mavlink *>(firstVehicle.mavlink())->parameterManager(), parameters);
    QVERIFY(!firstVehicle.parametersReady());
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

void VehicleTest::communicationQualityDuringDestruction()
{
    auto *vehicle = new Vehicle;
    bool observed = false;
    connect(vehicle->mavlink(), &QObject::destroyed, this, [&] {
        observed = true;
        QCOMPARE(vehicle->commLinkQuality(), -1);
        QCOMPARE(vehicle->radioRssi(), -1);
        QVERIFY(vehicle->commLinkCategory().isEmpty());
        QVERIFY(vehicle->commLinkDiagnostics().isEmpty());
    });
    delete vehicle;
    QVERIFY(observed);
}

void VehicleTest::radioStatusUpdatesCommunicationQuality()
{
    Vehicle vehicle;
    QTcpServer server;
    QVERIFY(connectVehicle(vehicle, server));
    QSignalSpy qualitySpy(&vehicle, &Vehicle::commLinkQualityChanged);
    const auto sockets = server.findChildren<QTcpSocket *>();
    QVERIFY(!sockets.isEmpty());
    mavlink_message_t message{};
    mavlink_msg_radio_status_pack(42, 1, &message, 200, 190, 100, 0, 0, 2, 98);
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    const auto length = mavlink_msg_to_send_buffer(buffer, &message);
    sockets.first()->write(reinterpret_cast<const char *>(buffer), length);
    sockets.first()->flush();
    QTRY_COMPARE(vehicle.radioRssi(), 200);
    QCOMPARE(vehicle.radioPacketLoss(), -1.); // Error/correction events are not total packet counts.
    QCOMPARE(vehicle.commLinkQuality(), -1); // Raw uncalibrated RSSI alone cannot establish quality.
    QTRY_VERIFY(qualitySpy.count() >= 1);
}

void VehicleTest::landedStateTransitionsAreConservative()
{
    Vehicle vehicle;
    QTcpServer server;
    QVERIFY(connectVehicle(vehicle, server));
    QCOMPARE(vehicle.flightPhase(), Vehicle::Unknown);
    QVERIFY(!vehicle.flightStateFresh());
    QVERIFY(!vehicle.airborne());
    position(vehicle); // Armed and positive altitude do not prove airborne.
    QVERIFY(!vehicle.airborne());
    landed(vehicle, MAV_LANDED_STATE_ON_GROUND);
    QCOMPARE(vehicle.flightPhase(), Vehicle::OnGround);
    QVERIFY(vehicle.flightStateFresh());
    landed(vehicle, MAV_LANDED_STATE_TAKEOFF);
    QCOMPARE(vehicle.flightPhase(), Vehicle::TakingOff);
    QVERIFY(!vehicle.airborne());
    landed(vehicle, MAV_LANDED_STATE_LANDING);
    QVERIFY(!vehicle.airborne());
    landed(vehicle, MAV_LANDED_STATE_IN_AIR);
    QVERIFY(vehicle.airborne());
    landed(vehicle, MAV_LANDED_STATE_LANDING);
    QCOMPARE(vehicle.flightPhase(), Vehicle::Landing);
    QVERIFY(vehicle.airborne());
    QSignalSpy changed(&vehicle, &Vehicle::flightStateChanged);
    landed(vehicle, MAV_LANDED_STATE_ON_GROUND);
    QCOMPARE(vehicle.flightPhase(), Vehicle::OnGround); // Synchronous, no timer/debounce.
    QVERIFY(!vehicle.airborne());
    QVERIFY(changed.count() > 0);
    landed(vehicle, MAV_LANDED_STATE_IN_AIR);
    heartbeat(vehicle, false);
    QVERIFY(!vehicle.airborne());
    heartbeat(vehicle, true);
    QVERIFY(!vehicle.airborne()); // Rearming must not resurrect the old confirmation.
    landed(vehicle, MAV_LANDED_STATE_IN_AIR);
    QVERIFY(vehicle.airborne());
    landed(vehicle, MAV_LANDED_STATE_UNDEFINED);
    QCOMPARE(vehicle.flightPhase(), Vehicle::Unknown);
    QVERIFY(!vehicle.flightStateFresh());
    QVERIFY(!vehicle.airborne());
    landed(vehicle, 255);
    QCOMPARE(vehicle.flightPhase(), Vehicle::Unknown);
}

void VehicleTest::freshnessExpiresWithoutIncomingMessages()
{
    Vehicle vehicle;
    QTcpServer server;
    QVERIFY(connectVehicle(vehicle, server));
    position(vehicle);
    landed(vehicle, MAV_LANDED_STATE_IN_AIR);
    QSignalSpy freshness(&vehicle, &Vehicle::telemetryFreshnessChanged);
    QSignalSpy flight(&vehicle, &Vehicle::flightStateChanged);
    QTRY_VERIFY_WITH_TIMEOUT(!vehicle.positionFresh(), Vehicle::PositionTimeoutMs + 1000);
    QVERIFY(freshness.count() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(!vehicle.heartbeatFresh(), Vehicle::HeartbeatTimeoutMs + 1000);
    QVERIFY(!vehicle.airborne());
    QTRY_VERIFY_WITH_TIMEOUT(!vehicle.flightStateFresh(), Vehicle::FlightStateTimeoutMs + 1000);
    QVERIFY(!vehicle.flightStateFresh());
    QCOMPARE(vehicle.flightPhase(), Vehicle::Unknown);
    QVERIFY(!vehicle.airborne());
    QVERIFY(flight.count() > 0);
    // Fresh ground telemetry is recorded immediately even if heartbeat is stale.
    landed(vehicle, MAV_LANDED_STATE_ON_GROUND);
    QCOMPARE(vehicle.flightPhase(), Vehicle::OnGround);
    QVERIFY(vehicle.flightStateFresh());
    QVERIFY(!vehicle.heartbeatFresh());
    QVERIFY(!vehicle.airborne());
    heartbeat(vehicle);
    landed(vehicle, MAV_LANDED_STATE_LANDING);
    QVERIFY(!vehicle.airborne());
    landed(vehicle, MAV_LANDED_STATE_IN_AIR);
    QVERIFY(vehicle.airborne());
    // Keep heartbeat/position alive while the independent landed-state deadline expires.
    QTimer keepAlive;
    keepAlive.setInterval(200);
    connect(&keepAlive, &QTimer::timeout, [&]() {
        // Refresh connection liveness through the transport, not the telemetry-only test helper.
        mavlink_message_t message = {};
        mavlink_msg_heartbeat_pack(42, 1, &message, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA,
                                  MAV_MODE_FLAG_SAFETY_ARMED, 4, MAV_STATE_ACTIVE);
        uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
        const auto length = mavlink_msg_to_send_buffer(bytes, &message);
        for (auto *peer : server.findChildren<QTcpSocket *>()) {
            peer->write(reinterpret_cast<const char *>(bytes), length); peer->flush();
        }
        position(vehicle);
    });
    keepAlive.start();
    QTRY_VERIFY_WITH_TIMEOUT(!vehicle.flightStateFresh(), Vehicle::FlightStateTimeoutMs + 1000);
    QVERIFY(vehicle.heartbeatFresh());
    QVERIFY(vehicle.positionFresh());
    QVERIFY(!vehicle.airborne());
}

void VehicleTest::wrongSourcesCannotRefreshFlightState()
{
    Vehicle vehicle;
    QTcpServer server;
    QVERIFY(connectVehicle(vehicle, server));
    landed(vehicle, MAV_LANDED_STATE_IN_AIR, 7);
    position(vehicle, 123456789, 7);
    QVERIFY(!vehicle.flightStateFresh());
    QVERIFY(!vehicle.positionFresh());
    landed(vehicle, MAV_LANDED_STATE_IN_AIR, 42, 100);
    QVERIFY(!vehicle.flightStateFresh());
    landed(vehicle, MAV_LANDED_STATE_IN_AIR);
    QVERIFY(vehicle.airborne());
    heartbeat(vehicle, false, 7);
    QVERIFY(vehicle.armed());
    QVERIFY(vehicle.airborne());
    QTimer otherSource;
    otherSource.setInterval(150);
    connect(&otherSource, &QTimer::timeout, [&]() {
        heartbeat(vehicle, true, 7);
        landed(vehicle, MAV_LANDED_STATE_IN_AIR, 42, 100);
        position(vehicle, 123456789, 7);
    });
    otherSource.start();
    QTRY_VERIFY_WITH_TIMEOUT(!vehicle.heartbeatFresh(), Vehicle::HeartbeatTimeoutMs + 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!vehicle.flightStateFresh(), Vehicle::FlightStateTimeoutMs + 1000);
    QVERIFY(!vehicle.flightStateFresh());
    QVERIFY(!vehicle.positionFresh());
    QVERIFY(!vehicle.airborne());
}

void VehicleTest::disconnectAndSourceChangeResetFreshness()
{
    Vehicle vehicle;
    QTcpServer server;
    QVERIFY(connectVehicle(vehicle, server));
    landed(vehicle, MAV_LANDED_STATE_IN_AIR);
    position(vehicle);
    QVERIFY(vehicle.airborne());
    auto *mavlink = qobject_cast<Mavlink *>(vehicle.mavlink());
    mavlink->disconnectFromVehicle();
    QVERIFY(!vehicle.heartbeatFresh());
    QVERIFY(!vehicle.positionFresh());
    QCOMPARE(vehicle.flightPhase(), Vehicle::Unknown);
    landed(vehicle, MAV_LANDED_STATE_IN_AIR);
    QVERIFY(!vehicle.airborne());
    QVERIFY(connectVehicle(vehicle, server, 7));
    QCOMPARE(mavlink->selectedSystemId(), 7);
    QVERIFY(!vehicle.flightStateFresh());
    landed(vehicle, MAV_LANDED_STATE_IN_AIR); // Old source.
    QVERIFY(!vehicle.flightStateFresh());
    landed(vehicle, MAV_LANDED_STATE_IN_AIR, 7);
    QVERIFY(vehicle.airborne());
    // Replacing a live transport invalidates immediately, before a new heartbeat.
    QVERIFY(mavlink->connectToVehicle("127.0.0.1", server.serverPort()));
    QVERIFY(!vehicle.airborne());
    QVERIFY(!vehicle.heartbeatFresh());
}

void VehicleTest::invalidHeadingAndPositionDoNotBecomeFresh()
{
    Vehicle vehicle;
    QTcpServer server;
    QVERIFY(connectVehicle(vehicle, server));
    position(vehicle);
    QVERIFY(vehicle.positionFresh());
    position(vehicle, 910000000);
    QVERIFY(!vehicle.positionFresh());
    mavlink_message_t msg = {};
    mavlink_msg_vfr_hud_pack(42, 1, &msg, 0, 0, 0, 0, 0, 0);
    vehicle.handleMessage(msg);
    QVERIFY(vehicle.headingValid()); // North is valid.
    QVERIFY(vehicle.headingFresh());
    mavlink_msg_vfr_hud_pack(42, 1, &msg, 0, 0, 360, 0, 0, 0);
    vehicle.handleMessage(msg);
    QVERIFY(vehicle.headingValid()); // The vendored VFR_HUD contract includes 360.
    mavlink_msg_vfr_hud_pack(7, 1, &msg, 0, 0, -1, 0, 0, 0);
    vehicle.handleMessage(msg);
    QVERIFY(vehicle.headingFresh());
    mavlink_msg_attitude_pack(42, 1, &msg, 0, 0, 0,
                             std::numeric_limits<float>::quiet_NaN(), 0, 0, 0);
    vehicle.handleMessage(msg);
    QVERIFY(!vehicle.headingValid());
    QVERIFY(!vehicle.headingFresh());
    mavlink_msg_vfr_hud_pack(42, 1, &msg, 0, 0, 0, 0, 0, 0);
    vehicle.handleMessage(msg);
    QTRY_VERIFY_WITH_TIMEOUT(!vehicle.headingFresh(), Vehicle::HeadingTimeoutMs + 1000);
}

void VehicleTest::unavailableLandedStateDoesNotPreventTelemetry()
{
    Vehicle vehicle;
    QTcpServer server;
    QVERIFY(connectVehicle(vehicle, server));
    mavlink_message_t ack = {};
    mavlink_msg_command_ack_pack(42, 1, &ack, MAV_CMD_SET_MESSAGE_INTERVAL,
                                MAV_RESULT_UNSUPPORTED, 0, 0, 255, 0);
    vehicle.handleMessage(ack);
    position(vehicle);
    QVERIFY(vehicle.positionFresh());
    QCOMPARE(vehicle.latitude(), 12.3456789);
    QVERIFY(!vehicle.flightStateFresh());
    QVERIFY(!vehicle.airborne());
    // An accepted request also cannot substitute for an actual landed-state message.
    mavlink_msg_command_ack_pack(42, 1, &ack, MAV_CMD_SET_MESSAGE_INTERVAL,
                                MAV_RESULT_ACCEPTED, 0, 0, 255, 0);
    vehicle.handleMessage(ack);
    QVERIFY(!vehicle.airborne());
}

void VehicleTest::heartbeatLossRevokesVehicleParameterReadiness()
{
    Vehicle vehicle;
    auto *mavlink = qobject_cast<Mavlink *>(vehicle.mavlink());
    auto config = mavlink->connectionConfiguration(); config.heartbeatTimeoutMs = 80;
    QVERIFY(mavlink->setConnectionConfiguration(config));
    QTcpServer server;
    QVERIFY(connectVehicle(vehicle, server));
    auto *parameters = qobject_cast<ParameterManager *>(vehicle.parameterManager());
    parameters->refresh();
    ParameterManager::Sample sample{QByteArray("A").leftJustified(16, '\0'), 1, MAV_PARAM_TYPE_REAL32, 0, 1};
    parameters->receive(42, 1, parameters->generation(), sample);
    QVERIFY(vehicle.parametersReady());
    position(vehicle);
    QSignalSpy readiness(&vehicle, &Vehicle::parametersReadyChanged);
    QTRY_VERIFY_WITH_TIMEOUT(!mavlink->connected(), 1000);
    QVERIFY(!vehicle.parametersReady()); QVERIFY(parameters->hasCompleteSnapshot());
    QVERIFY(!vehicle.positionFresh()); QVERIFY(!vehicle.heartbeatFresh());
    QVERIFY(readiness.size() > 0);
}

QTEST_MAIN(VehicleTest)

#include "VehicleTest.moc"
