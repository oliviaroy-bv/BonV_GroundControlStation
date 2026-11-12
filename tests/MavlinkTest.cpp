#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtEndian>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QScopedPointer>
#include "vehicle/Vehicle.h"

#include "mavlink/Mavlink.h"
#include "parameters/ParameterManager.h"
#include "transport/udp/UdpLink.h"
#include "mission/MissionPlanModel.h"

namespace {

QVector<mavlink_command_long_t> readCommands(QTcpSocket *socket, uint16_t commandId, int minimumCount)
{
    QVector<mavlink_command_long_t> commands;
    mavlink_status_t status = {};
    QElapsedTimer timer;
    timer.start();

    while (commands.size() < minimumCount && timer.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (!socket->bytesAvailable()) {
            socket->waitForReadyRead(50);
        }

        const QByteArray bytes = socket->readAll();
        for (const char byte : bytes) {
            mavlink_message_t message = {};
            if (!mavlink_parse_char(MAVLINK_COMM_0,
                                    static_cast<uint8_t>(byte),
                                    &message,
                                    &status)) {
                continue;
            }

            if (message.msgid != MAVLINK_MSG_ID_COMMAND_LONG) {
                continue;
            }

            mavlink_command_long_t command = {};
            mavlink_msg_command_long_decode(&message, &command);
            if (command.command == commandId) {
                commands.append(command);
            }
        }
    }

    return commands;
}

QVector<mavlink_message_t> readMavlinkMessages(QTcpSocket *socket, int minimumCount, int timeoutMs = 1200)
{
    QVector<mavlink_message_t> messages;
    mavlink_status_t status = {};
    QElapsedTimer timer;
    timer.start();

    while (messages.size() < minimumCount && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (!socket->bytesAvailable()) {
            socket->waitForReadyRead(50);
        }

        const QByteArray bytes = socket->readAll();
        for (const char byte : bytes) {
            mavlink_message_t message = {};
            if (mavlink_parse_char(MAVLINK_COMM_0,
                                    static_cast<uint8_t>(byte),
                                    &message,
                                    &status)) {
                messages.append(message);
            }
        }
    }

    return messages;
}

void writeMavlinkMessage(QTcpSocket *socket, mavlink_message_t &message)
{
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN] = {};
    const int length = mavlink_msg_to_send_buffer(buffer, &message);
    QCOMPARE(socket->write(reinterpret_cast<const char*>(buffer), length), qint64(length));
    QVERIFY(socket->flush());
}

struct ConnectionHarness {
    qint64 now = 0;
    ParameterManager parameters{nullptr, [this] { return now; }};
    Mavlink mavlink{nullptr, &parameters, [this] { return now; }};
    QTcpServer server;
    QTcpSocket *peer = nullptr;
    QSignalSpy messages{&mavlink, &Mavlink::messageReceived};
    ConnectionHarness() {
        auto config = mavlink.connectionConfiguration();
        config.heartbeatTimeoutMs = 100;
        config.initialHeartbeatTimeoutMs = 500;
        if (!mavlink.setConnectionConfiguration(config)) qFatal("Invalid connection test configuration");
        mavlink.setTelemetryLoggingEnabled(false);
        mavlink.setFlightControllerLoggingEnabled(false);
    }
    bool open() {
        if (!server.isListening() && !server.listen(QHostAddress::LocalHost, 0)) return false;
        if (!mavlink.connectToVehicle("127.0.0.1", server.serverPort())) return false;
        QElapsedTimer wait; wait.start();
        while (!server.hasPendingConnections() && wait.elapsed() < 1000) QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        peer = server.nextPendingConnection();
        return peer != nullptr;
    }
    void send(mavlink_message_t message) {
        const auto before = messages.size();
        writeMavlinkMessage(peer, message);
        QTRY_VERIFY(messages.size() > before);
    }
    void heartbeat(uint8_t system = 42, uint8_t component = 1, uint8_t type = MAV_TYPE_QUADROTOR,
                   uint8_t autopilot = MAV_AUTOPILOT_ARDUPILOTMEGA) {
        mavlink_message_t message = {};
        mavlink_msg_heartbeat_pack(system, component, &message, type, autopilot, 0, 4, MAV_STATE_ACTIVE);
        send(message);
    }
    void parameter(const char *name, uint16_t index, uint16_t count = 3, float value = 1) {
        mavlink_message_t message = {};
        mavlink_msg_param_value_pack(42, 1, &message, name, value, MAV_PARAM_TYPE_REAL32, count, index);
        send(message);
    }
    void complete() { parameter("A", 0); parameter("B", 1); parameter("C", 2); }
    void expire() { now += 100; mavlink.processConnectionTimeout(mavlink.connectionGeneration()); }
};

bool connectTestMavlink(Mavlink &mavlink, QTcpServer &server, int timeoutMs = 1000, uint8_t systemId = 1)
{
    if (!mavlink.connectToVehicle(QStringLiteral("127.0.0.1"), server.serverPort())) {
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    while (!server.hasPendingConnections() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    // Leave the connection pending for existing callers, but supply the heartbeat
    // now required by production before reporting a vehicle connection.
    const auto sockets = server.findChildren<QTcpSocket *>();
    if (sockets.isEmpty()) return false;
    mavlink_message_t heartbeat = {};
    mavlink_msg_heartbeat_pack(systemId, MAV_COMP_ID_AUTOPILOT1, &heartbeat,
                              MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA,
                              MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 4, MAV_STATE_ACTIVE);
    writeMavlinkMessage(sockets.constLast(), heartbeat);
    while (!mavlink.connected() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return mavlink.connected() && server.hasPendingConnections();
}

QTimer *missionTransferTimer(Mavlink &mavlink)
{
    const auto timers = mavlink.findChildren<QTimer *>();
    for (QTimer *timer : timers) {
        if (timer->isSingleShot() && timer->interval() == 3000) {
            return timer;
        }
    }
    return nullptr;
}

void fireMissionTransferTimeout(Mavlink &mavlink)
{
    QTimer *timer = missionTransferTimer(mavlink);
    QVERIFY(timer != nullptr);
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
}

QByteArray oneWaypointWpl()
{
    return QByteArrayLiteral("QGC WPL 110\n"
                             "0\t1\t3\t16\t0\t0\t0\t0\t10\t20\t30\t1\n"
                             "1\t0\t3\t16\t0\t0\t0\t0\t10.1\t20.1\t40\t1\n");
}

bool loadOneWaypointMission(Mavlink &mavlink, QTemporaryFile &file)
{
    if (!file.open()) return false;
    if (file.write(oneWaypointWpl()) != oneWaypointWpl().size()) return false;
    if (!file.flush()) return false;
    return !mavlink.uploadMissionFile(QUrl::fromLocalFile(file.fileName()));
}

void sendMissionAck(QTcpSocket *socket, uint8_t senderSystem, uint8_t senderComponent, uint8_t result,
                    uint8_t missionType = MAV_MISSION_TYPE_MISSION)
{
    mavlink_message_t message = {};
    mavlink_msg_mission_ack_pack(senderSystem, senderComponent, &message, 255, 0, result,
                                 missionType, 0);
    writeMavlinkMessage(socket, message);
}

void sendMissionCount(QTcpSocket *socket, uint8_t senderSystem, uint8_t senderComponent, uint16_t count,
                      uint8_t missionType = MAV_MISSION_TYPE_MISSION)
{
    mavlink_message_t message = {};
    mavlink_msg_mission_count_pack(senderSystem, senderComponent, &message, 255, 0, count, missionType, 0);
    writeMavlinkMessage(socket, message);
}

void sendMissionItemInt(QTcpSocket *socket, uint8_t senderSystem, uint8_t senderComponent, uint16_t sequence,
                        uint8_t missionType = MAV_MISSION_TYPE_MISSION)
{
    mavlink_message_t message = {};
    mavlink_msg_mission_item_int_pack(senderSystem, senderComponent, &message,
                                      255, 0, sequence, MAV_FRAME_GLOBAL_RELATIVE_ALT,
                                      MAV_CMD_NAV_WAYPOINT, sequence == 0 ? 1 : 0, 1,
                                      0, 0, 0, 0,
                                      static_cast<int32_t>((10.0 + sequence * 0.01) * 10000000.0),
                                      static_cast<int32_t>((20.0 + sequence * 0.01) * 10000000.0),
                                      30.0f + sequence, missionType);
    writeMavlinkMessage(socket, message);
}

void sendMissionRequestInt(QTcpSocket *socket, uint8_t senderSystem, uint8_t senderComponent, uint16_t sequence,
                           uint8_t missionType = MAV_MISSION_TYPE_MISSION)
{
    mavlink_message_t message = {};
    mavlink_msg_mission_request_int_pack(senderSystem, senderComponent, &message, 255, 0, sequence, missionType);
    writeMavlinkMessage(socket, message);
}

void sendMissionRequest(QTcpSocket *socket, uint8_t senderSystem, uint8_t senderComponent, uint16_t sequence,
                        uint8_t missionType = MAV_MISSION_TYPE_MISSION)
{
    mavlink_message_t message = {};
    mavlink_msg_mission_request_pack(senderSystem, senderComponent, &message, 255, 0, sequence, missionType);
    writeMavlinkMessage(socket, message);
}

int mavlinkPacketLength(const QByteArray &bytes, int offset)
{
    if (offset >= bytes.size()) {
        return 0;
    }

    const uint8_t magic = static_cast<uint8_t>(bytes.at(offset));
    if (magic == 0xfe) {
        if (offset + 2 > bytes.size()) {
            return 0;
        }
        return 6 + static_cast<uint8_t>(bytes.at(offset + 1)) + 2;
    }

    if (magic == 0xfd) {
        if (offset + 3 > bytes.size()) {
            return 0;
        }
        const bool signedPacket = (static_cast<uint8_t>(bytes.at(offset + 2)) & 0x01) != 0;
        return 10 + static_cast<uint8_t>(bytes.at(offset + 1)) + 2 + (signedPacket ? 13 : 0);
    }

    return 0;
}

quint64 readBigEndianUint64(const QByteArray &bytes, int offset)
{
    quint64 value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | static_cast<uint8_t>(bytes.at(offset + i));
    }
    return value;
}

} // namespace

class MavlinkTest : public QObject
{
    Q_OBJECT

private:
    static void resetQualityBaseline(Mavlink &mavlink) { mavlink.resetLinkQuality(); }
    static mavlink_message_t qualityFrame(quint8 sequence, int system = 42, int component = 1) {
        mavlink_message_t frame{};
        mavlink_msg_attitude_pack(system, component, &frame, 0, 0, 0, 0, 0, 0, 0);
        frame.seq = sequence; // Direct metric hook; not serialized as a wire packet.
        return frame;
    }

private slots:
    void transportDiagnosticCounters();
    void transportDiagnosticRadio();
    void transportDiagnosticIsolation();
    void qualityExpiryAndPingContinuity();
    void qualityConfigurationValidation();
    void qualityCompositeWeightsAndMetricExpiry();
    void qualitySequenceWindow_data();
    void qualitySequenceWindow();
    void qualityFreshnessAndRadioCounters();
    void qualityPingValidationAndMedian();
    void qualityTimeFilteringAndHysteresis();
    void qualityEqualElapsedTimeAtDifferentRates();
    void manualControlRemainsIndependentOfParameters_data();
    void manualControlRemainsIndependentOfParameters();
    void realVehicleAndQmlLifecycle();
    void motorPrerequisitePresentationTracksBackend();
    void commandPrerequisites_data();
    void commandPrerequisites();
    void rcCalibrationPrerequisites_data();
    void rcCalibrationPrerequisites();
    void motorConfigurationChangeCancelsStartsButPreservesStop();
    void scheduledMotorStartsInvalidate();
    void missionAdmissionAndSafetyCommands();
    void defaultStateIsDisconnected();
    void vpsStatusAndCommandsUseConnectedLink();
    void disconnectWhileDisconnectedDoesNotEmit();
    void connectToVehicleUpdatesConnectionState();
    void receivedMavlinkBytesEmitMessage();
    void autopilotHeartbeatRequestsParameterList();
    void receivedParameterValuesUpdateCacheAndProgress();
    void parameterRoutingAndWritesPreserveSnapshots();
    void parameterCapabilitiesSelectEncoding();
    void parameterRecoveryRequestsUseSelectedTarget();
    void rcCalibrationConfirmation_data();
    void rcCalibrationConfirmation();
    void connectionRequiresValidHeartbeat_data();
    void connectionRequiresValidHeartbeat();
    void selectedHeartbeatRefreshesOnlyItsOwnLiveness();
    void telemetryCannotPreventHeartbeatExpiry();
    void heartbeatExpiryRevokesSnapshotAndReconnects();
    void heartbeatExpiryCancelsActiveAndQueuedWrites();
    void heartbeatExpiryCancelsRecoveryAndOldWork_data();
    void heartbeatExpiryCancelsRecoveryAndOldWork();
    void explicitDisconnectAndTransportFailureAreIdempotent();
    void replacementRejectsOldSourceAndCallbacks();
    void realHeartbeatTimerAndDestruction();
    void udpHeartbeatExpiryAndReconnect();
    void initialHeartbeatDeadlineAndConfiguration();
    void transportDestructionAndOwnerDestructionCancelWork();
    void telemetryLogCapturesTimestampedMavlinkFrames();
    void flightControllerLogCapturesStatusTextMessages();
    void disabledFlightControllerLogDoesNotCreateFile();
    void runAllMotorTestSendsFourDelayedMotorCommands();
    void uploadedMissionDisplaysOnlyNavigationWaypoints();
    void uploadedMissionStoresHomeLocation();
    void downloadedMissionStoresHomeLocation();
    void resetHomeToUploadedLocationSendsSetHomeCommand();
    void writeLoadedMissionStartsMissionUpload();
    void missionUploadWaitsForSelectedVehicleClearAck();
    void missionUploadFaultInjection();
    void missionUploadIgnoresUnrelatedMissionTypeRequests();
    void missionDownloadFaultInjection();
    void missionClearFaultInjection();
    void missionCancellationAndLateMessages();
    void missionDisconnectAndVehicleSwitchCancellation();
    void missionCompletionAndCanonicalModelFaults();
    void uploadedAbsoluteMissionTerrainProfileUsesMissionAltitudes();
    void uploadedRelativeMissionTerrainProfileConvertsPlanToAmsl();
    void landedStateRequestUsesSelectedSourceAndIgnoresOtherAutopilots();
};

void MavlinkTest::vpsStatusAndCommandsUseConnectedLink()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    mavlink.setFlightControllerLoggingEnabled(false);
    QVERIFY(!mavlink.startVpsAction(0));
    QVERIFY(mavlink.connectToVehicle(QStringLiteral("127.0.0.1"), server.serverPort()));
    QTRY_VERIFY(server.hasPendingConnections());
    QTcpSocket *peer = server.nextPendingConnection();
    QVERIFY(peer);

    mavlink_message_t heartbeat = {};
    mavlink_msg_heartbeat_pack(1, 1, &heartbeat, MAV_TYPE_QUADROTOR,
                               MAV_AUTOPILOT_ARDUPILOTMEGA, 0, 0, MAV_STATE_ACTIVE);
    writeMavlinkMessage(peer, heartbeat);
    QTRY_VERIFY(mavlink.connected());
    // This test expires VPS status while the autopilot connection stays alive.
    QTimer heartbeatTimer;
    heartbeatTimer.setInterval(1000);
    connect(&heartbeatTimer, &QTimer::timeout, &mavlink, [&] { writeMavlinkMessage(peer, heartbeat); });
    heartbeatTimer.start();
    QVERIFY(!mavlink.vpsConnected());
    QVERIFY(!mavlink.startVpsAction(0));

    uint8_t payload[128] = {};
    payload[89] = 2;
    payload[90] = 1;
    payload[91] = 3;
    payload[92] = 0;
    payload[104] = 1;
    mavlink_message_t status = {};
    QSignalSpy received(&mavlink, &Mavlink::messageReceived);
    mavlink_msg_tunnel_pack(1, 1, &status, 255, MAV_COMP_ID_MISSIONPLANNER,
                            100, 105, payload);
    writeMavlinkMessage(peer, status);
    QTRY_COMPARE(received.count(), 1);
    QVERIFY(!mavlink.vpsConnected());
    mavlink_msg_tunnel_pack(200, 14, &status, 255, MAV_COMP_ID_MISSIONPLANNER,
                            100, 104, payload);
    writeMavlinkMessage(peer, status);
    QTRY_COMPARE(received.count(), 2);
    QVERIFY(!mavlink.vpsConnected());
    mavlink_msg_tunnel_pack(200, 14, &status, 255, MAV_COMP_ID_MISSIONPLANNER,
                            100, 105, payload);
    writeMavlinkMessage(peer, status);
    QTRY_VERIFY(mavlink.vpsConnected());
    QCOMPARE(mavlink.vpsStatus().value("sensors").toString(), QStringLiteral("Running"));
    QCOMPARE(mavlink.vpsStatus().value("vio").toString(), QStringLiteral("Starting"));
    QCOMPARE(mavlink.vpsStatus().value("autoAid").toString(), QStringLiteral("On"));
    QVERIFY(!mavlink.startVpsAction(6));

    uint8_t configPayload[128] = {};
    configPayload[5] = 1;
    configPayload[22] = 3;
    configPayload[23] = 12;
    configPayload[61] = 1;
    mavlink_message_t config = {};
    mavlink_msg_tunnel_pack(200, 14, &config, 255, MAV_COMP_ID_MISSIONPLANNER, 102, 62, configPayload);
    writeMavlinkMessage(peer, config);
    QTRY_COMPARE(mavlink.vpsConfig().value("satCount").toInt(), 12);
    QCOMPARE(mavlink.vpsConfig().value("logging").toBool(), true);
    QCOMPARE(mavlink.vpsConfig().value("sensorSpeed").toInt(), 1);

    peer->readAll();
    for (int action = 0; action < 6; ++action)
        QVERIFY(mavlink.startVpsAction(action));
    QVERIFY(mavlink.refreshVpsConfig());
    QVERIFY(mavlink.setVpsHome(12.5, 77.5));
    QVERIFY(!mavlink.setVpsHome(91, 0));
    QVERIFY(mavlink.setVpsLogging(true));
    QVERIFY(mavlink.setVpsSensorSpeed(1));
    QVERIFY(!mavlink.setVpsSensorSpeed(2));
    QVERIFY(mavlink.setVpsGpsCheck({{"fixType", 3}, {"satCount", 12}, {"hdop", 2}, {"hAcc", 500}, {"vAcc", 400}, {"healthy", 1}, {"unhealthy", 0.5}}));
    QVERIFY(mavlink.setVpsGpsJamming({{"enabled", true}, {"time", 1}, {"hAcc", 500}, {"vAcc", 400}, {"unhealthy", 0.2}}));

    QVector<mavlink_tunnel_t> sent;
    mavlink_status_t parseStatus = {};
    QElapsedTimer timer;
    timer.start();
    while (sent.size() < 12 && timer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (!peer->bytesAvailable())
            peer->waitForReadyRead(20);
        const QByteArray bytes = peer->readAll();
        for (const char byte : bytes) {
            mavlink_message_t message = {};
            if (mavlink_parse_char(MAVLINK_COMM_3, static_cast<uint8_t>(byte), &message, &parseStatus)
                    && message.msgid == MAVLINK_MSG_ID_TUNNEL) {
                mavlink_tunnel_t tunnel = {};
                mavlink_msg_tunnel_decode(&message, &tunnel);
                sent.append(tunnel);
            }
        }
    }
    QCOMPARE(sent.size(), 12);
    for (int action = 0; action < 6; ++action) {
        QCOMPARE(sent[action].target_system, uint8_t(200));
        QCOMPARE(sent[action].target_component, uint8_t(14));
        QCOMPARE(sent[action].payload_type, uint16_t(action < 4 ? 0 : action == 4 ? 6 : 14));
        QCOMPARE(sent[action].payload_length, uint8_t(action < 4 ? 2 : 1));
        QCOMPARE(sent[action].payload[0], uint8_t(action < 4 ? action : 1));
        if (action < 4)
            QCOMPARE(sent[action].payload[1], uint8_t(1));
    }
    const uint16_t configTypes[] = {10, 1, 8, 19, 15, 16};
    for (int i = 0; i < 6; ++i)
        QCOMPARE(sent[i + 6].payload_type, configTypes[i]);

    QTRY_VERIFY_WITH_TIMEOUT(!mavlink.vpsConnected(), 6500);
    QVERIFY(mavlink.vpsConfig().isEmpty());
    QVERIFY(!mavlink.startVpsAction(0));
    writeMavlinkMessage(peer, status);
    QTRY_VERIFY(mavlink.vpsConnected());
    mavlink.disconnectFromVehicle();
    QVERIFY(!mavlink.vpsConnected());
    QVERIFY(!mavlink.startVpsAction(0));
}

void MavlinkTest::transportDiagnosticCounters()
{
    ConnectionHarness h;
    auto config = h.mavlink.connectionConfiguration(); config.heartbeatTimeoutMs = 60000;
    QVERIFY(h.mavlink.setConnectionConfiguration(config));
    QVERIFY(h.open()); h.heartbeat();
    const auto initial = h.mavlink.transportDiagnostics();
    QCOMPARE(initial.value("received").toULongLong(), quint64(1));
    QCOMPARE(initial.value("lastPacketAgeMs").toLongLong(), 0);
    QVERIFY(initial.value("endpoint").toString().startsWith("TCP:127.0.0.1:"));
    QQmlEngine engine;
    QQmlEngine::setObjectOwnership(&h.mavlink, QQmlEngine::CppOwnership);
    QQmlComponent binding(&engine);
    binding.setData("import QtQml; QtObject { required property var transport; property var snapshot: transport.transportDiagnosticSnapshot() }", QUrl());
    QScopedPointer<QObject> observer(binding.createWithInitialProperties({{"transport", QVariant::fromValue(static_cast<QObject *>(&h.mavlink))}}));
    QVERIFY2(observer, qPrintable(binding.errorString()));
    QCOMPARE(observer->property("snapshot").toMap().value("received").toULongLong(), quint64(1));
    const auto transmitted = initial.value("transmitted").toULongLong();
    h.mavlink.sendLinkPing();
    QCOMPARE(h.mavlink.transportDiagnostics().value("transmitted").toULongLong(), transmitted + 1);
    resetQualityBaseline(h.mavlink);
    for (auto seq : {254, 255, 0, 3, 3, 2, 4}) h.mavlink.updateMavlinkQuality(qualityFrame(seq));
    auto data = h.mavlink.transportDiagnostics();
    QCOMPARE(data.value("sequenceGaps").toULongLong(), quint64(2));
    QCOMPARE(data.value("duplicates").toULongLong(), quint64(1));
    QCOMPARE(data.value("reordered").toULongLong(), quint64(1));
    h.mavlink.updateMavlinkQuality(qualityFrame(20, 43, 1));
    h.mavlink.updateMavlinkQuality(qualityFrame(20, 42, 2));
    QCOMPARE(h.mavlink.transportDiagnostics().value("sequenceGaps"), data.value("sequenceGaps"));
    h.now = 1100;
    h.peer->write("not a MAVLink frame"); h.peer->flush();
    h.parameter("A", 0);
    data = h.mavlink.transportDiagnostics();
    QCOMPARE(data.value("received").toULongLong(), quint64(2));
    QCOMPARE(data.value("rate").toInt(), 1);
    h.now = 1250;
    QCOMPARE(h.mavlink.transportDiagnostics().value("lastPacketAgeMs").toLongLong(), 150);
    h.now = 2200;
    QCOMPARE(h.mavlink.transportDiagnostics().value("rate").toInt(), 0);
    h.mavlink.disconnectFromVehicle();
    QCOMPARE(h.mavlink.transportDiagnostics().value("received").toULongLong(), quint64(2));
    QVERIFY(h.open());
    data = h.mavlink.transportDiagnostics();
    QCOMPARE(data.value("received").toULongLong(), quint64(0));
    QCOMPARE(data.value("sequenceGaps").toULongLong(), quint64(0));
    QCOMPARE(data.value("lastPacketAgeMs").toInt(), -1);
}

void MavlinkTest::transportDiagnosticRadio()
{
    ConnectionHarness h;
    auto config = h.mavlink.connectionConfiguration(); config.heartbeatTimeoutMs = 60000;
    QVERIFY(h.mavlink.setConnectionConfiguration(config));
    QVERIFY(h.open()); h.heartbeat();
    QCOMPARE(h.mavlink.transportDiagnostics().value("radioState").toString(), QString("Unavailable"));
    QCOMPARE(h.mavlink.transportDiagnostics().value("rxerrors").toInt(), -1);
    mavlink_message_t radio{};
    mavlink_msg_radio_status_pack(42, 68, &radio, 200, 190, 80, 30, 40, 12, 13);
    h.send(radio);
    auto data = h.mavlink.transportDiagnostics();
    QCOMPARE(data.value("rssi").toInt(), 200);
    QCOMPARE(data.value("remoteRssi").toInt(), 190);
    QCOMPARE(data.value("noise").toInt(), 30);
    QCOMPARE(data.value("remoteNoise").toInt(), 40);
    QCOMPARE(data.value("txbuf").toInt(), 80);
    QCOMPARE(data.value("rxerrors").toInt(), 12);
    QCOMPARE(data.value("fixed").toInt(), 13);
    QCOMPARE(data.value("radioComponent").toInt(), 68);
    mavlink_msg_radio_status_pack(43, 68, &radio, 1, 2, 3, 4, 5, 6, 7);
    h.send(radio);
    QCOMPARE(h.mavlink.transportDiagnostics().value("rssi").toInt(), 200);
    mavlink_msg_radio_status_pack(42, 68, &radio, 255, 255, 255, 255, 255, 14, 15);
    h.send(radio);
    data = h.mavlink.transportDiagnostics();
    for (auto key : {"rssi", "remoteRssi", "noise", "remoteNoise", "txbuf"}) QCOMPARE(data.value(key).toInt(), -1);
    QCOMPARE(data.value("rxerrors").toInt(), 14);
    h.now = 6000;
    data = h.mavlink.transportDiagnostics();
    QCOMPARE(data.value("radioState").toString(), QString("Stale"));
    QCOMPARE(data.value("rxerrors").toInt(), -1);
    h.mavlink.disconnectFromVehicle();
    QCOMPARE(h.mavlink.transportDiagnostics().value("radioState").toString(), QString("Unavailable"));
}

void MavlinkTest::transportDiagnosticIsolation()
{
    ConnectionHarness h;
    QVERIFY(h.open()); h.heartbeat();
    const auto tcpEndpoint = h.mavlink.diagnosticEndpoint();
    QVERIFY(!tcpEndpoint.isEmpty());
    h.mavlink.disconnectFromVehicle();
    QVERIFY(h.mavlink.connectToVehicleProtocol("UDP", "127.0.0.1", 14555));
    QVERIFY(h.mavlink.diagnosticEndpoint().startsWith("UDP:"));
    QVERIFY(h.mavlink.diagnosticEndpoint() != tcpEndpoint);
    QCOMPARE(h.mavlink.transportDiagnostics().value("received").toULongLong(), quint64(0));
    QCOMPARE(h.mavlink.transportDiagnostics().value("latencyMs").toInt(), -1);
    QCOMPARE(h.mavlink.transportDiagnostics().value("rssi").toInt(), -1);
    h.mavlink.disconnectFromVehicle();
    delete h.mavlink.link;
    QVERIFY(h.mavlink.diagnosticEndpoint().isEmpty());
    QVERIFY(h.mavlink.transportDiagnostics().isEmpty());
}

void MavlinkTest::qualityCompositeWeightsAndMetricExpiry()
{
    ConnectionHarness h;
    auto connection = h.mavlink.connectionConfiguration(); connection.heartbeatTimeoutMs = 60000;
    QVERIFY(h.mavlink.setConnectionConfiguration(connection));
    auto config = h.mavlink.qualityConfiguration(); config.minimumSequenceExpected = 1;
    config.timeConstantSeconds = 0.001; config.sustainedCategoryMs = config.minimumCategoryDwellMs = 0;
    QVERIFY(h.mavlink.setQualityConfiguration(config));
    QVERIFY(h.open()); h.heartbeat(); resetQualityBaseline(h.mavlink);
    h.mavlink.updateMavlinkQuality(qualityFrame(0));
    h.mavlink.sendLinkPing();
    const auto seq = h.mavlink.nextPingSequence - 1;
    const auto stamp = h.mavlink.pendingQualityPings.value(seq).stamp;
    h.now = 500;
    mavlink_message_t response{};
    mavlink_msg_ping_pack(42, 1, &response, stamp, seq, 255, MAV_COMP_ID_MISSIONPLANNER);
    h.mavlink.handleLinkPing(response);
    h.mavlink.updateMavlinkQuality(qualityFrame(1)); h.mavlink.processLinkQuality();
    QCOMPARE(h.mavlink.estimatedLinkQuality(), 70);
    QCOMPARE(h.mavlink.linkQualityCategory(), QString("FAIR"));
    h.now = 6000;
    h.mavlink.updateMavlinkQuality(qualityFrame(200));
    h.mavlink.updateMavlinkQuality(qualityFrame(201)); h.mavlink.processLinkQuality();
    QCOMPARE(h.mavlink.linkLatencyMs(), -1);
    QCOMPARE(h.mavlink.estimatedLinkQuality(), 100); // Weights normalize over fresh eligible metrics only.
    QCOMPARE(h.mavlink.linkQualityCategory(), QString("GOOD"));
}

void MavlinkTest::qualityConfigurationValidation()
{
    Mavlink mavlink;
    auto config = mavlink.qualityConfiguration();
    config.tickMs = 0; QVERIFY(!mavlink.setQualityConfiguration(config));
    config = mavlink.qualityConfiguration(); config.timeConstantSeconds = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!mavlink.setQualityConfiguration(config));
    config = mavlink.qualityConfiguration(); config.poorExit = config.goodEntry;
    QVERIFY(!mavlink.setQualityConfiguration(config));
    config = mavlink.qualityConfiguration(); config.maximumForwardDelta = 128;
    QVERIFY(!mavlink.setQualityConfiguration(config));
    QCOMPARE(mavlink.estimatedLinkQuality(), -1);
}
void MavlinkTest::qualityExpiryAndPingContinuity()
{
    ConnectionHarness h;
    auto connection = h.mavlink.connectionConfiguration(); connection.heartbeatTimeoutMs = 60000;
    QVERIFY(h.mavlink.setConnectionConfiguration(connection));
    auto config = h.mavlink.qualityConfiguration(); config.minimumSequenceExpected = 1;
    QVERIFY(h.mavlink.setQualityConfiguration(config));
    QVERIFY(h.open()); h.heartbeat(); resetQualityBaseline(h.mavlink);
    h.mavlink.updateMavlinkQuality(qualityFrame(10));
    mavlink_message_t ping{};
    mavlink_msg_ping_pack(42, 1, &ping, 1, 0, 0, 0); ping.seq = 11;
    h.mavlink.updateMavlinkQuality(ping);
    h.mavlink.updateMavlinkQuality(qualityFrame(12));
    h.mavlink.processLinkQuality();
    QCOMPARE(h.mavlink.estimatedLinkQuality(), 100); // PING is part of sequence progression, never a false gap.
    QCOMPARE(h.mavlink.linkQualityDiagnostics().value("sequenceExpected").toInt(), 2);
    h.now = 3000; h.mavlink.processLinkQuality();
    QCOMPARE(h.mavlink.estimatedLinkQuality(), -1); QVERIFY(h.mavlink.connected());
    h.now = 6000;
    h.mavlink.updateMavlinkQuality(qualityFrame(200));
    h.mavlink.updateMavlinkQuality(qualityFrame(201));
    h.mavlink.processLinkQuality();
    QCOMPARE(h.mavlink.estimatedLinkQuality(), 100); // Fresh sequence data works without radio or latency.
    QCOMPARE(h.mavlink.linkLatencyMs(), -1);
}

void MavlinkTest::qualitySequenceWindow_data()
{
    QTest::addColumn<int>("delta"); QTest::addColumn<int>("expected"); QTest::addColumn<int>("start");
    QTest::newRow("sequential") << 1 << 1 << 254;
    QTest::newRow("wraparound") << 1 << 1 << 255;
    QTest::newRow("single-gap") << 2 << 2 << 254;
    QTest::newRow("multiple-gap") << 5 << 5 << 254;
    QTest::newRow("large-gap-resync") << 64 << 0 << 254;
    QTest::newRow("duplicate") << 0 << 0 << 254;
    QTest::newRow("reordered") << 255 << 0 << 254;
}
void MavlinkTest::qualitySequenceWindow()
{
    QFETCH(int, delta); QFETCH(int, expected); QFETCH(int, start);
    ConnectionHarness h;
    auto connection = h.mavlink.connectionConfiguration(); connection.heartbeatTimeoutMs = 60000;
    QVERIFY(h.mavlink.setConnectionConfiguration(connection));
    auto config = h.mavlink.qualityConfiguration(); config.minimumSequenceExpected = 1;
    QVERIFY(h.mavlink.setQualityConfiguration(config));
    QVERIFY(h.open()); h.heartbeat();
    resetQualityBaseline(h.mavlink);
    auto frame = qualityFrame(quint8(start)); h.mavlink.updateMavlinkQuality(frame);
    h.mavlink.updateMavlinkQuality(qualityFrame(quint8(start + delta)));
    h.mavlink.processLinkQuality();
    const auto diagnostics = h.mavlink.linkQualityDiagnostics();
    QCOMPARE(diagnostics.value("sequenceExpected").toInt(), expected);
    if (expected) QCOMPARE(h.mavlink.estimatedLinkQuality(), qRound(100. / expected));
    else QCOMPARE(h.mavlink.estimatedLinkQuality(), -1);
    // Neither another component nor another system may affect continuity.
    h.mavlink.updateMavlinkQuality(qualityFrame(100, 43, 1));
    h.mavlink.updateMavlinkQuality(qualityFrame(100, 42, 2));
    h.mavlink.processLinkQuality();
    QCOMPARE(h.mavlink.linkQualityDiagnostics().value("sequenceExpected").toInt(), expected);
}
void MavlinkTest::qualityFreshnessAndRadioCounters()
{
    ConnectionHarness h;
    auto connection = h.mavlink.connectionConfiguration(); connection.heartbeatTimeoutMs = 60000;
    QVERIFY(h.mavlink.setConnectionConfiguration(connection));
    QVERIFY(h.open()); h.heartbeat(); resetQualityBaseline(h.mavlink);
    const auto radio = [&](quint16 errors, quint16 corrections, quint8 rssi, quint8 component = 68) {
        mavlink_message_t message{};
        mavlink_msg_radio_status_pack(42, component, &message, rssi, 190, 100, 0, 0, errors, corrections);
        h.mavlink.updateMavlinkQuality(message);
        h.mavlink.processLinkQuality();
    };
    radio(65534, 65533, 200);
    QCOMPARE(h.mavlink.rawRadioRssi(), 200);
    QCOMPARE(h.mavlink.estimatedLinkQuality(), -1);
    h.now = 100; radio(2, 3, 201);
    auto diagnostics = h.mavlink.linkQualityDiagnostics();
    QCOMPARE(diagnostics.value("radioErrorsDelta").toInt(), 4);
    QCOMPARE(diagnostics.value("radioCorrectionsDelta").toInt(), 6);
    h.now = 200; radio(1, 1, 255);
    QCOMPARE(h.mavlink.rawRadioRssi(), -1);
    diagnostics = h.mavlink.linkQualityDiagnostics();
    QCOMPARE(diagnostics.value("radioCounterResets").toInt(), 1);
    QCOMPARE(diagnostics.value("radioErrorsDelta").toInt(), 0);
    h.now = 300; radio(1, 1, 200);
    QCOMPARE(h.mavlink.estimatedLinkQuality(), -1); // Zero events do not imply perfect delivery.
    h.now = 400; radio(3, 5, 0);
    QCOMPARE(h.mavlink.linkQualityDiagnostics().value("radioErrorsDelta").toInt(), 2);
    QCOMPARE(h.mavlink.linkQualityDiagnostics().value("radioCorrectionsDelta").toInt(), 4);
    QCOMPARE(h.mavlink.rawRadioRssi(), 0);
    h.now = 500; radio(500, 500, 150, 1); // New radio source must establish its own baseline.
    QCOMPARE(h.mavlink.linkQualityDiagnostics().value("radioErrorsDelta").toInt(), 0);
    h.now = 6000; h.mavlink.processLinkQuality();
    QCOMPARE(h.mavlink.rawRadioRssi(), -1);
    QVERIFY(!h.mavlink.linkQualityDiagnostics().value("radioCountersFresh").toBool());
    QVERIFY(h.mavlink.connected());
}
void MavlinkTest::qualityPingValidationAndMedian()
{
    ConnectionHarness h;
    auto connection = h.mavlink.connectionConfiguration(); connection.heartbeatTimeoutMs = 60000;
    QVERIFY(h.mavlink.setConnectionConfiguration(connection));
    QVERIFY(h.open()); h.heartbeat(); resetQualityBaseline(h.mavlink);
    auto response = [&](quint32 sequence, quint64 stamp, int system = 42, int component = 1, int target = 255) {
        mavlink_message_t message{};
        mavlink_msg_ping_pack(system, component, &message, stamp, sequence, target, MAV_COMP_ID_MISSIONPLANNER);
        h.mavlink.handleLinkPing(message);
        h.mavlink.processLinkQuality();
    };
    const auto ping = [&](int delay, bool rejectFirst) {
        h.mavlink.sendLinkPing();
        const auto sequence = h.mavlink.nextPingSequence - 1;
        const auto stamp = h.mavlink.pendingQualityPings.value(sequence).stamp;
        h.now += delay;
        if (rejectFirst) {
            response(sequence, stamp, 43); response(sequence, stamp, 42, 2); response(sequence, stamp, 42, 1, 7);
            response(sequence + 1, stamp); response(sequence, stamp + 1);
            QCOMPARE(h.mavlink.linkLatencyMs(), -1);
            QCOMPARE(h.mavlink.transportDiagnostics().value("latencyMs").toInt(), -1);
        }
        response(sequence, stamp);
        response(sequence, stamp); // Duplicate reply ignored.
    };
    ping(10, true); QCOMPARE(h.mavlink.linkLatencyMs(), 10);
    ping(1000, false); ping(20, false);
    QCOMPARE(h.mavlink.linkLatencyMs(), 20);
    QCOMPARE(h.mavlink.transportDiagnostics().value("latencyMs").toInt(), 20);
    QCOMPARE(h.mavlink.transportDiagnostics().value("latencyState").toString(), QString("Available"));
    h.mavlink.sendLinkPing();
    const auto sequence = h.mavlink.nextPingSequence - 1;
    const auto stamp = h.mavlink.pendingQualityPings.value(sequence).stamp;
    h.now += 3001; response(sequence, stamp); // Too late to be accepted.
    QCOMPARE(h.mavlink.linkQualityDiagnostics().value("lastPingMonotonicMs").toLongLong(), qint64(1030));
    h.now = 7000; h.mavlink.processLinkQuality();
    QCOMPARE(h.mavlink.linkLatencyMs(), -1);
    QCOMPARE(h.mavlink.transportDiagnostics().value("latencyMs").toInt(), -1);
    QCOMPARE(h.mavlink.transportDiagnostics().value("latencyState").toString(), QString("Stale"));
    QCOMPARE(h.mavlink.estimatedLinkQuality(), -1); // PING alone is insufficient.
    h.mavlink.sendLinkPing(); // No response at all must remain unavailable, never zero milliseconds.
    h.now += 3001; h.mavlink.processLinkQuality();
    QVERIFY(h.mavlink.pendingQualityPings.isEmpty());
    QCOMPARE(h.mavlink.linkLatencyMs(), -1);
    QVERIFY(h.mavlink.connected());
}
void MavlinkTest::qualityTimeFilteringAndHysteresis()
{
    ConnectionHarness h;
    auto connection = h.mavlink.connectionConfiguration(); connection.heartbeatTimeoutMs = 60000;
    QVERIFY(h.mavlink.setConnectionConfiguration(connection));
    auto config = h.mavlink.qualityConfiguration(); config.minimumSequenceExpected = 1; config.sequenceWindowMs = 1000;
    config.timeConstantSeconds = 0.1; config.sustainedCategoryMs = 500; config.minimumCategoryDwellMs = 1000;
    QVERIFY(h.mavlink.setQualityConfiguration(config));
    QVERIFY(h.open()); h.heartbeat(); resetQualityBaseline(h.mavlink);
    int seq = 0;
    h.mavlink.updateMavlinkQuality(qualityFrame(seq));
    const auto tick = [&](int delta, int milliseconds) {
        h.now += milliseconds; seq += delta;
        h.mavlink.updateMavlinkQuality(qualityFrame(quint8(seq)));
        h.mavlink.processLinkQuality();
    };
    tick(1, 100); QCOMPARE(h.mavlink.linkQualityCategory(), QString());
    for (int i=0;i<6;++i) tick(1,100);
    QCOMPARE(h.mavlink.linkQualityCategory(), QString("GOOD"));
    tick(40,100);
    QVERIFY(h.mavlink.estimatedLinkQuality() < 75);
    QCOMPARE(h.mavlink.linkQualityCategory(), QString("GOOD")); // Even a below-exit spike cannot change category immediately.
    resetQualityBaseline(h.mavlink);
    h.mavlink.updateMavlinkQuality(qualityFrame(quint8(seq)));
    for (int i=0;i<7;++i) tick(1,100);
    QCOMPARE(h.mavlink.linkQualityCategory(), QString("GOOD"));
    for (int i=0;i<30;++i) tick(i%5 == 0 ? 2 : 1,100);
    QCOMPARE(h.mavlink.linkQualityCategory(), QString("GOOD")); // ~83 remains GOOD, below entry but above exit.
    for (int i=0;i<30;++i) tick(4,100);
    QCOMPARE(h.mavlink.linkQualityCategory(), QString("POOR"));
    for (int i=0;i<30;++i) tick(i%2 == 0 ? 2 : 1,100);
    QCOMPARE(h.mavlink.linkQualityCategory(), QString("FAIR"));
    for (int i=0;i<30;++i) tick(2,100);
    QCOMPARE(h.mavlink.linkQualityCategory(), QString("FAIR")); // 50 stays FAIR, above POOR entry.
    for (int i=0;i<30;++i) tick(1,100);
    QCOMPARE(h.mavlink.linkQualityCategory(), QString("GOOD"));
    const auto generation = h.mavlink.connectionGeneration();
    h.mavlink.disconnectFromVehicle();
    QCOMPARE(h.mavlink.estimatedLinkQuality(), -1); QVERIFY(h.mavlink.linkQualityCategory().isEmpty());
    QVERIFY(h.open()); h.heartbeat();
    QVERIFY(h.mavlink.connectionGeneration() != generation);
    QCOMPARE(h.mavlink.estimatedLinkQuality(), -1);
    h.now += 6000; h.mavlink.processLinkQuality();
    QVERIFY(h.mavlink.connected()); QCOMPARE(h.mavlink.estimatedLinkQuality(), -1);
}
void MavlinkTest::qualityEqualElapsedTimeAtDifferentRates()
{
    QVector<int> scores;
    for (int rate : {10, 100}) {
        ConnectionHarness h;
        auto connection = h.mavlink.connectionConfiguration(); connection.heartbeatTimeoutMs = 60000;
        QVERIFY(h.mavlink.setConnectionConfiguration(connection));
        auto config = h.mavlink.qualityConfiguration(); config.minimumSequenceExpected = 1; config.sequenceWindowMs = 1000;
        config.timeConstantSeconds = 2.; QVERIFY(h.mavlink.setQualityConfiguration(config));
        QVERIFY(h.open()); h.heartbeat(); resetQualityBaseline(h.mavlink);
        int seq=0; h.mavlink.updateMavlinkQuality(qualityFrame(0));
        for (int t=100;t<=5000;t+=100) {
            for (int i=0;i<rate/10;++i) {
                h.now = t-100+(i+1)*100/(rate/10);
                seq += t <= 2000 ? 1 : 2;
                h.mavlink.updateMavlinkQuality(qualityFrame(quint8(seq)));
            }
            h.mavlink.processLinkQuality();
        }
        scores.append(h.mavlink.estimatedLinkQuality());
    }
    QVERIFY(scores[0] >= 40 && scores[0] < 80);
    QVERIFY(scores[1] >= 40 && scores[1] < 80);
    QVERIFY(qAbs(scores[0]-scores[1]) <= 2);
}

void MavlinkTest::manualControlRemainsIndependentOfParameters_data()
{
    QTest::addColumn<int>("state");
    QTest::newRow("Loading") << int(ParameterManager::Loading);
    QTest::newRow("Recovering") << int(ParameterManager::Recovering);
    QTest::newRow("Ready") << int(ParameterManager::Ready);
    QTest::newRow("Failed") << int(ParameterManager::Failed);
}
void MavlinkTest::manualControlRemainsIndependentOfParameters()
{
    QFETCH(int, state);
    ConnectionHarness h;
    QVERIFY(!h.mavlink.sendManualControl(0, 0, 0, 0, 0));
    QVERIFY(!h.mavlink.sendRcChannelsOverride(0, 0, 0, 0));
    QVERIFY(h.open()); h.heartbeat(); h.mavlink.requestParameters();
    if (state == ParameterManager::Recovering) { h.parameter("A", 0); h.parameters.markRecovering(h.parameters.generation()); }
    else if (state == ParameterManager::Ready) h.complete();
    else if (state == ParameterManager::Failed) h.parameters.fail(h.parameters.generation(), "test");
    QCOMPARE(int(h.parameters.syncState()), state);
    const auto coverage = h.parameters.receivedParameterCount();
    readMavlinkMessages(h.peer, 1, 100);
    QVERIFY(h.mavlink.sendManualControl(100, -200, 300, -400, 0));
    QVERIFY(h.mavlink.sendRcChannelsOverride(0, 0, 0, 0)); // Existing override-release safety behavior.
    const auto messages = readMavlinkMessages(h.peer, 2, 500);
    bool manual = false, released = false;
    for (const auto &message : messages) {
        if (message.msgid == MAVLINK_MSG_ID_MANUAL_CONTROL) {
            mavlink_manual_control_t control{}; mavlink_msg_manual_control_decode(&message, &control);
            QCOMPARE(control.target, uint8_t(42)); QCOMPARE(control.x, int16_t(100)); QCOMPARE(control.y, int16_t(-200));
            manual = true;
        } else if (message.msgid == MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE) {
            mavlink_rc_channels_override_t control{}; mavlink_msg_rc_channels_override_decode(&message, &control);
            QCOMPARE(control.chan1_raw, uint16_t(0)); QCOMPARE(control.chan4_raw, uint16_t(0)); released = true;
        }
    }
    QVERIFY(manual && released);
    QCOMPARE(h.parameters.receivedParameterCount(), coverage);
}

void MavlinkTest::realVehicleAndQmlLifecycle()
{
    Vehicle vehicle;
    auto *mavlink = qobject_cast<Mavlink *>(vehicle.mavlink());
    auto *manager = qobject_cast<ParameterManager *>(vehicle.parameterManager());
    QVERIFY(mavlink && manager);
    QCOMPARE(manager->parent(), &vehicle);
    QCOMPARE(mavlink->parent(), &vehicle);
    mavlink->setTelemetryLoggingEnabled(false);
    mavlink->setFlightControllerLoggingEnabled(false);
    QQmlEngine engine;
    QQmlEngine::setObjectOwnership(&vehicle, QQmlEngine::CppOwnership);
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../ui/qml/accessories/ParameterSyncStatus.qml")));
    QTRY_VERIFY(component.status() != QQmlComponent::Loading);
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> status(component.createWithInitialProperties({{"vehicle", QVariant::fromValue(static_cast<QObject *>(&vehicle))}}));
    QVERIFY2(status, qPrintable(component.errorString()));
    QVERIFY(!status->property("ready").toBool());
    QStringList events;
    connect(mavlink, &Mavlink::connectionStatusChanged, this, [&](bool connected) {
        if (connected) { events.append("connected"); QVERIFY(!manager->parametersReady()); }
    });
    connect(manager, &ParameterManager::requestList, this, [&] { events.append("list"); });
    connect(manager, &ParameterManager::snapshotPublished, this, [&] {
        events.append("published"); QVERIFY(manager->parametersReady()); QVERIFY(vehicle.parametersReady());
    });
    QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QVERIFY(connectTestMavlink(*mavlink, server, 1000, 42));
    auto *peer = server.nextPendingConnection(); QVERIFY(peer);
    mavlink->requestParameters();
    QSignalSpy rejected(mavlink, &Mavlink::commandPrerequisiteRejected);
    mavlink->testMotor(1, 10, 1);
    QCOMPARE(rejected.size(), 1);
    QVERIFY(!status->property("ready").toBool());
    QVERIFY(!mavlink->motorTestPrerequisites().value("available").toBool());
    bool listOnWire = false;
    QElapsedTimer wait; wait.start();
    while (!listOnWire && wait.elapsed() < 1000) {
        for (const auto &message : readMavlinkMessages(peer, 1, 100))
            listOnWire |= message.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST;
    }
    QVERIFY(listOnWire);
    const auto deliver = [&](const char *name, int index, float value) {
        mavlink_message_t message{};
        mavlink_msg_param_value_pack(42, 1, &message, name, value, MAV_PARAM_TYPE_REAL32, 2, index);
        writeMavlinkMessage(peer, message);
    };
    deliver("FRAME_TYPE", 1, 1); // Deliberately out of order.
    QTRY_COMPARE(manager->receivedParameterCount(), 1);
    QVERIFY(!status->property("ready").toBool());
    QVERIFY(status->property("progressValue").toInt() < 100);
    QVERIFY(!mavlink->motorTestPrerequisites().value("available").toBool());
    deliver("FRAME_CLASS", 0, 1);
    QTRY_VERIFY(status->property("ready").toBool());
    QCOMPARE(status->property("progressValue").toInt(), 100);
    QVERIFY(events.indexOf("connected") < events.indexOf("list"));
    QVERIFY(events.indexOf("list") < events.indexOf("published"));
    QCOMPARE(events.count("published"), 1);
    const auto oldGeneration = mavlink->connectionGeneration();
    mavlink->disconnectFromVehicle();
    QVERIFY(!status->property("ready").toBool());
    QVERIFY(status->property("statusText").toString().contains("stale"));
    QVERIFY(manager->hasCompleteSnapshot());
    QVERIFY(connectTestMavlink(*mavlink, server, 1000, 42));
    peer = server.nextPendingConnection(); QVERIFY(peer);
    QVERIFY(mavlink->connectionGeneration() != oldGeneration);
    mavlink->requestParameters();
    QVERIFY(!status->property("ready").toBool());
    QVERIFY(!manager->snapshotProvenance().value("current").toBool());
    deliver("FRAME_CLASS", 0, 1); deliver("FRAME_TYPE", 1, 1);
    QTRY_VERIFY(status->property("ready").toBool());
    QCOMPARE(manager->snapshotProvenance().value("sourceConnectionGeneration").toULongLong(), mavlink->connectionGeneration());
    QCOMPARE(events.count("published"), 2);
}

void MavlinkTest::motorPrerequisitePresentationTracksBackend()
{
    ConnectionHarness h;
    QSignalSpy changed(&h.mavlink, &Mavlink::motorTestPrerequisitesChanged);
    QVERIFY(!h.mavlink.motorTestPrerequisites().value("available").toBool());
    QVERIFY(h.open()); h.heartbeat(); h.mavlink.requestParameters();
    QVERIFY(!h.mavlink.motorTestPrerequisites().value("available").toBool());
    h.parameter("FRAME_CLASS", 0, 2, 1); h.parameter("FRAME_TYPE", 1, 2, 1);
    QVERIFY(h.mavlink.motorTestPrerequisites().value("available").toBool());
    QCOMPARE(h.mavlink.motorTestPrerequisites().value("motorCount").toInt(), 4);
    QVERIFY(changed.count() > 0);
    changed.clear(); h.mavlink.requestParameters();
    QVERIFY(changed.count() > 0);
    QVERIFY(!h.mavlink.motorTestPrerequisites().value("available").toBool());
    QVERIFY(h.mavlink.motorTestPrerequisites().value("reason").toString().contains("loading"));
    h.mavlink.disconnectFromVehicle();
    QVERIFY(!h.mavlink.motorTestPrerequisites().value("available").toBool());
}

void MavlinkTest::commandPrerequisites_data()
{
    QTest::addColumn<int>("scenario");
    QTest::addColumn<bool>("accepted");
    const QStringList names{"disconnected", "loading", "recovering", "failed", "ready", "missing-frame", "stale", "invalid-index", "invalid-throttle", "invalid-duration", "invalid-count", "unsupported-frame", "missing-type", "tri-frame", "armed"};
    for (int i = 0; i < names.size(); ++i) QTest::newRow(qPrintable(names[i])) << i << (i == 4 || i == 13);
}

void MavlinkTest::commandPrerequisites()
{
    QFETCH(int, scenario); QFETCH(bool, accepted);
    ConnectionHarness h;
    QSignalSpy rejected(&h.mavlink, &Mavlink::commandPrerequisiteRejected);
    if (scenario != 0) {
        QVERIFY(h.open()); h.heartbeat(); h.mavlink.requestParameters();
        if (scenario == 2) { h.parameter("A", 0, 2); h.parameters.markRecovering(h.parameters.generation()); QCOMPARE(h.parameters.syncState(), ParameterManager::Recovering); }
        else if (scenario == 3) h.parameters.fail(h.parameters.generation(), "test failure");
        else if (scenario >= 4) {
            if (scenario == 5) h.complete();
            else if (scenario == 12 || scenario == 13) h.parameter("FRAME_CLASS", 0, 1, scenario == 13 ? 7 : 1);
            else { h.parameter("FRAME_CLASS", 0, 2, scenario == 11 ? 6 : 1); h.parameter("FRAME_TYPE", 1, 2, 1); }
            if (scenario == 6) h.mavlink.disconnectFromVehicle();
            if (scenario == 14) {
                mavlink_message_t armed{};
                mavlink_msg_heartbeat_pack(42, 1, &armed, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_MODE_FLAG_SAFETY_ARMED, 4, MAV_STATE_ACTIVE);
                h.send(armed);
            }
        }
    }
    const auto coverage = h.parameters.receivedParameterCount();
    if (scenario == 10) h.mavlink.runAllMotorTest(10, 1, 8);
    else h.mavlink.testMotor(scenario == 7 ? 5 : 1, scenario == 8 ? 21 : 10, scenario == 9 ? 0 : 1);
    QCOMPARE(rejected.isEmpty(), accepted);
    if (!accepted) {
        QVERIFY(!rejected.last()[1].toString().isEmpty());
        QVERIFY(rejected.last()[2].toMap().contains("parameterState"));
    } else QCOMPARE(readCommands(h.peer, MAV_CMD_DO_MOTOR_TEST, 1).size(), 1);
    QCOMPARE(h.parameters.receivedParameterCount(), coverage);
    if (scenario >= 1 && scenario <= 3) {
        rejected.clear();
        h.mavlink.stopAllMotorTests(4);
        QVERIFY(rejected.isEmpty());
        const auto stops = readCommands(h.peer, MAV_CMD_DO_MOTOR_TEST, 4);
        QCOMPARE(stops.size(), 4);
        for (const auto &stop : stops) QCOMPARE(stop.param3, 0.f);
        QVERIFY(h.mavlink.requestParameterRead("FRAME_CLASS"));
        QVERIFY(!h.mavlink.requestParameterWrite("FRAME_CLASS", 1, MAV_PARAM_TYPE_REAL32).value("accepted").toBool());
    }
    if (scenario == 0) {
        QVERIFY(!h.mavlink.requestParameterRead("FRAME_CLASS"));
        h.mavlink.requestParameters();
        QVERIFY(rejected.last()[0].toString() == "parameterRefresh");
    }
}

void MavlinkTest::rcCalibrationPrerequisites_data()
{
    QTest::addColumn<int>("scenario");
    const QStringList names{"disconnected", "loading", "recovering", "failed", "missing", "partial-metadata", "stale", "invalid-channel", "invalid-value"};
    for (int i = 0; i < names.size(); ++i) QTest::newRow(qPrintable(names[i])) << i;
}

void MavlinkTest::rcCalibrationPrerequisites()
{
    QFETCH(int, scenario);
    ConnectionHarness h;
    if (scenario) {
        QVERIFY(h.open()); h.heartbeat(); h.mavlink.requestParameters();
        if (scenario == 2) { h.parameter("A", 0, 2); h.parameters.markRecovering(h.parameters.generation()); QCOMPARE(h.parameters.syncState(), ParameterManager::Recovering); }
        if (scenario == 3) h.parameters.fail(h.parameters.generation(), "test");
        if (scenario == 4) h.complete();
        if (scenario == 5) h.parameter("RC1_MIN", 0, 1, 1100);
        if (scenario >= 6) {
            h.parameter("RC1_MIN", 0, 3, 1100); h.parameter("RC1_MAX", 1, 3, 1900); h.parameter("RC1_TRIM", 2, 3, 1500);
            if (scenario == 6) h.mavlink.disconnectFromVehicle();
        }
    }
    const auto coverage = h.parameters.receivedParameterCount();
    QSignalSpy rejected(&h.mavlink, &Mavlink::commandPrerequisiteRejected);
    QSignalSpy batch(&h.mavlink, &Mavlink::rcCalibrationWriteFinished);
    const auto result = h.mavlink.requestRcCalibrationWrite({QVariantMap{{"channel", scenario == 7 ? 17 : 1},
        {"minimum", scenario == 8 ? 500 : 1000}, {"maximum", 2000}, {"trim", 1500}}});
    QVERIFY(!result.value("accepted").toBool());
    QCOMPARE(result.value("id").toULongLong(), quint64(0));
    QVERIFY(!result.value("reason").toString().isEmpty());
    QCOMPARE(batch.size(), 0); // No invalid batch is created.
    QCOMPARE(h.parameters.pendingWriteCount(), 0);
    QCOMPARE(h.parameters.receivedParameterCount(), coverage);
    QCOMPARE(rejected.size(), 1);
}

void MavlinkTest::motorConfigurationChangeCancelsStartsButPreservesStop()
{
    ConnectionHarness h; QVERIFY(h.open()); h.heartbeat(); h.mavlink.requestParameters();
    h.parameter("FRAME_CLASS", 0, 2, 1); h.parameter("FRAME_TYPE", 1, 2, 1);
    h.mavlink.runAllMotorTest(10, 1, 4);
    QCOMPARE(readCommands(h.peer, MAV_CMD_DO_MOTOR_TEST, 1).size(), 1);
    QSignalSpy rejected(&h.mavlink, &Mavlink::commandPrerequisiteRejected);
    h.parameter("FRAME_TYPE", 1, 2, 0); // Authoritative live configuration change.
    QCOMPARE(rejected.size(), 1);
    QTimer *automaticStop = nullptr;
    for (auto *timer : h.mavlink.findChildren<QTimer *>())
        if (timer->isSingleShot() && timer->interval() == 1490) automaticStop = timer;
    QVERIFY(automaticStop);
    QVERIFY(automaticStop->isActive());
    QVERIFY(QMetaObject::invokeMethod(automaticStop, "timeout", Qt::DirectConnection));
    const auto stops = readCommands(h.peer, MAV_CMD_DO_MOTOR_TEST, 4);
    QCOMPARE(stops.size(), 4);
    for (const auto &command : stops) QCOMPARE(command.param3, 0.f);
    h.mavlink.runSequentialMotorTest(10, 1, 4);
    QCOMPARE(readCommands(h.peer, MAV_CMD_DO_MOTOR_TEST, 1).size(), 1);
    QTimer *sequence = nullptr;
    for (auto *timer : h.mavlink.findChildren<QTimer *>())
        if (timer->isSingleShot() && timer->interval() == 1250) sequence = timer;
    QVERIFY(sequence);
    h.mavlink.requestParameters();
    QVERIFY(!sequence->isActive());
    const auto rejectedCount = rejected.size();
    QVERIFY(QMetaObject::invokeMethod(sequence, "timeout", Qt::DirectConnection));
    QCOMPARE(rejected.size(), rejectedCount);
}

void MavlinkTest::scheduledMotorStartsInvalidate()
{
    ConnectionHarness h; QVERIFY(h.open()); h.heartbeat(); h.mavlink.requestParameters();
    h.parameter("FRAME_CLASS", 0, 2, 1); h.parameter("FRAME_TYPE", 1, 2, 1);
    QSignalSpy rejected(&h.mavlink, &Mavlink::commandPrerequisiteRejected);
    h.mavlink.runAllMotorTest(10, 1, 4);
    h.mavlink.requestParameters(); // Invalidate before the zero-delay first start.
    QVERIFY(!rejected.isEmpty());
    QTest::qWait(300);
    for (const auto &message : readMavlinkMessages(h.peer, 1, 50)) {
        if (message.msgid != MAVLINK_MSG_ID_COMMAND_LONG) continue;
        mavlink_command_long_t command{}; mavlink_msg_command_long_decode(&message, &command);
        QVERIFY(command.command != MAV_CMD_DO_MOTOR_TEST || command.param3 == 0);
    }
    h.parameter("FRAME_CLASS", 0, 2, 1); h.parameter("FRAME_TYPE", 1, 2, 1);
    h.mavlink.runAllMotorTest(10, 1, 4);
    h.mavlink.disconnectFromVehicle();
    QVERIFY(h.open()); h.heartbeat();
    QTest::qWait(300);
    for (const auto &message : readMavlinkMessages(h.peer, 1, 50)) {
        if (message.msgid != MAVLINK_MSG_ID_COMMAND_LONG) continue;
        mavlink_command_long_t command{}; mavlink_msg_command_long_decode(&message, &command);
        QVERIFY(command.command != MAV_CMD_DO_MOTOR_TEST || command.param3 == 0);
    }
}

void MavlinkTest::missionAdmissionAndSafetyCommands()
{
    ConnectionHarness h;
    QSignalSpy rejected(&h.mavlink, &Mavlink::commandPrerequisiteRejected);
    h.mavlink.downloadMissionFromVehicle();
    QVERIFY(!rejected.isEmpty());
    QVERIFY(!h.mavlink.writeLoadedMissionToVehicle());
    QVERIFY(h.open()); h.heartbeat(); h.mavlink.requestParameters();
    h.mavlink.downloadMissionFromVehicle();
    const auto transfer = h.mavlink.missionTransferId();
    rejected.clear();
    h.mavlink.downloadMissionFromVehicle();
    QVERIFY(!rejected.isEmpty());
    QCOMPARE(h.mavlink.missionTransferId(), transfer);
    h.mavlink.clearMission();
    QCOMPARE(h.mavlink.missionTransferId(), transfer);
    rejected.clear();
    h.mavlink.armVehicle(false);
    h.mavlink.returnToLaunch();
    h.mavlink.land();
    h.mavlink.emergencyStop();
    QCOMPARE(readCommands(h.peer, MAV_CMD_COMPONENT_ARM_DISARM, 2).size(), 2);
    QVERIFY(rejected.isEmpty());
    h.mavlink.cancelMissionTransfer();
    h.parameters.fail(h.parameters.generation(), "test failure");
    rejected.clear(); h.mavlink.downloadMissionFromVehicle();
    QVERIFY(rejected.isEmpty()); // Mission transfer has no parameter dependency.
    const auto oldGeneration = h.mavlink.connectionGeneration();
    h.mavlink.disconnectFromVehicle(); QVERIFY(h.open()); h.heartbeat();
    QVERIFY(h.mavlink.connectionGeneration() != oldGeneration);
    fireMissionTransferTimeout(h.mavlink);
    QVERIFY(h.mavlink.missionTransferState() != Mavlink::Downloading);
}

void MavlinkTest::defaultStateIsDisconnected()
{
    Mavlink mavlink;

    QVERIFY(!mavlink.connected());
}

void MavlinkTest::disconnectWhileDisconnectedDoesNotEmit()
{
    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QSignalSpy spy(&mavlink, &Mavlink::connectionStatusChanged);

    mavlink.disconnectFromVehicle();

    QVERIFY(!mavlink.connected());
    QCOMPARE(spy.count(), 0);
}

void MavlinkTest::connectToVehicleUpdatesConnectionState()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QSignalSpy spy(&mavlink, &Mavlink::connectionStatusChanged);

    QVERIFY(connectTestMavlink(mavlink, server));

    QVERIFY(mavlink.connected());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);

    QTcpSocket *clientSocket = server.nextPendingConnection();
    QVERIFY(clientSocket != nullptr);

    mavlink.disconnectFromVehicle();

    QVERIFY(!mavlink.connected());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), false);
}

void MavlinkTest::receivedMavlinkBytesEmitMessage()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QSignalSpy messageSpy(&mavlink, &Mavlink::messageReceived);

    QVERIFY(connectTestMavlink(mavlink, server));
    QVERIFY(mavlink.connected());

    messageSpy.clear(); // Ignore the connection fixture's initial heartbeat.

    QTcpSocket *clientSocket = server.nextPendingConnection();
    QVERIFY(clientSocket != nullptr);

    mavlink_message_t heartbeat = {};

    mavlink_msg_heartbeat_pack(
        1,
        1,
        &heartbeat,
        MAV_TYPE_QUADROTOR,
        MAV_AUTOPILOT_ARDUPILOTMEGA,
        MAV_MODE_FLAG_SAFETY_ARMED,
        4,
        MAV_STATE_ACTIVE
    );

    writeMavlinkMessage(clientSocket, heartbeat);

    QTRY_COMPARE(messageSpy.count(), 1);

    const mavlink_message_t received = qvariant_cast<mavlink_message_t>(messageSpy.takeFirst().at(0));
    QCOMPARE(received.msgid, uint32_t(MAVLINK_MSG_ID_HEARTBEAT));
}

void MavlinkTest::autopilotHeartbeatRequestsParameterList()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(connectTestMavlink(mavlink, server, 1000, 42));

    QTcpSocket *clientSocket = server.nextPendingConnection();
    QVERIFY(clientSocket != nullptr);

    mavlink_message_t heartbeat = {};
    mavlink_msg_heartbeat_pack(
        42,
        MAV_COMP_ID_AUTOPILOT1,
        &heartbeat,
        MAV_TYPE_QUADROTOR,
        MAV_AUTOPILOT_ARDUPILOTMEGA,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
        4,
        MAV_STATE_ACTIVE
    );
    writeMavlinkMessage(clientSocket, heartbeat);

    mavlink_param_request_list_t request = {};
    bool foundRequest = false;
    QElapsedTimer requestWait;
    requestWait.start();
    while (!foundRequest && requestWait.elapsed() < 1600) {
        const auto messages = readMavlinkMessages(clientSocket, 1, 200);
        for (const auto &message : messages) {
            if (message.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
                mavlink_msg_param_request_list_decode(&message, &request);
                foundRequest = true;
                break;
            }
        }
    }

    QVERIFY(foundRequest);
    QCOMPARE(request.target_system, uint8_t(42));
    QCOMPARE(request.target_component, uint8_t(MAV_COMP_ID_AUTOPILOT1));
    QVERIFY(mavlink.parameterFetchInProgress());
    QCOMPARE(mavlink.parameterManager()->syncState(), ParameterManager::Loading);
    QVERIFY(!mavlink.parameterManager()->parametersReady());
}

void MavlinkTest::receivedParameterValuesUpdateCacheAndProgress()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(connectTestMavlink(mavlink, server));

    QTcpSocket *clientSocket = server.nextPendingConnection();
    QVERIFY(clientSocket != nullptr);

    QSignalSpy finishedSpy(&mavlink, &Mavlink::parameterFetchFinished);
    QSignalSpy updatedSpy(&mavlink, &Mavlink::parameterUpdated);

    mavlink.requestParameters();

    mavlink_message_t first = {};
    mavlink_msg_param_value_pack(
        1,
        MAV_COMP_ID_AUTOPILOT1,
        &first,
        "FRAME_CLASS",
        1.0f,
        MAV_PARAM_TYPE_INT8,
        2,
        0
    );
    writeMavlinkMessage(clientSocket, first);

    mavlink_message_t second = {};
    mavlink_msg_param_value_pack(
        1,
        MAV_COMP_ID_AUTOPILOT1,
        &second,
        "FRAME_TYPE",
        2.0f,
        MAV_PARAM_TYPE_INT8,
        2,
        1
    );
    writeMavlinkMessage(clientSocket, second);

    QTRY_COMPARE(finishedSpy.count(), 1);
    QCOMPARE(updatedSpy.count(), 2);
    QCOMPARE(mavlink.parameterCount(), 2);
    QCOMPARE(mavlink.loadedParameterCount(), 2);
    QCOMPARE(mavlink.parameterFetchProgress(), 100);
    QVERIFY(!mavlink.parameterFetchInProgress());
    QVERIFY(mavlink.parameterManager()->parametersReady());

    const QVariantMap frameClass = mavlink.parameterValue(QStringLiteral("FRAME_CLASS"));
    QCOMPARE(frameClass.value(QStringLiteral("value")).toDouble(), 1.0);
    QCOMPARE(frameClass.value(QStringLiteral("index")).toInt(), 0);
}

void MavlinkTest::parameterRoutingAndWritesPreserveSnapshots()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(connectTestMavlink(mavlink, server, 1000, 42));
    auto *peer = server.nextPendingConnection();
    QVERIFY(peer);
    mavlink.requestParameters();
    auto *manager = mavlink.parameterManager();
    QSignalSpy published(manager, &ParameterManager::snapshotPublished);
    QSignalSpy updated(&mavlink, &Mavlink::parameterUpdated);
    const auto send = [&](uint8_t system, uint8_t component, const char *name, uint16_t index, float value = 1) {
        mavlink_message_t message = {};
        mavlink_msg_param_value_pack(system, component, &message, name, value, MAV_PARAM_TYPE_REAL32, 2, index);
        writeMavlinkMessage(peer, message);
    };
    send(43, 1, "FOREIGN", 0);
    send(42, 2, "OTHER_COMPONENT", 1);
    send(42, 1, "A", 0);
    QTRY_COMPARE(mavlink.loadedParameterCount(), 1);
    QVERIFY(mavlink.parameters().isEmpty());
    QCOMPARE(updated.size(), 0);
    QVERIFY(!mavlink.setParameter("B", 9)); // Configuration writes are rejected during initial synchronization.
    QVERIFY(!mavlink.setParameter("12345678901234567", 9));
    QCOMPARE(mavlink.loadedParameterCount(), 1);
    QVERIFY(mavlink.parameters().isEmpty());
    QVERIFY(!manager->parametersReady());
    send(42, 1, "B", 1, 3);
    QTRY_VERIFY(manager->parametersReady());
    QCOMPARE(published.size(), 1);
    QCOMPARE(mavlink.parameterValue("B")["value"].toDouble(), 3.);
    const auto generation = manager->generation();
    send(42, 1, "B", 1, 4);
    QTRY_COMPARE(mavlink.parameterValue("B")["value"].toDouble(), 4.);
    QCOMPARE(manager->generation(), generation);
    QCOMPARE(published.size(), 1);
    QVERIFY(!mavlink.parameterFetchInProgress());
    mavlink.requestParameters();
    QVERIFY(!manager->parametersReady());
    QCOMPARE(mavlink.parameterValue("B")["value"].toDouble(), 4.);
    manager->fail(manager->generation(), "test refresh failure");
    QCOMPARE(mavlink.parameterValue("B")["value"].toDouble(), 4.);
    mavlink.disconnectFromVehicle();
    QVERIFY(!manager->parametersReady());
    QVERIFY(manager->hasCompleteSnapshot());
}

void MavlinkTest::parameterCapabilitiesSelectEncoding()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(connectTestMavlink(mavlink, server, 1000, 42));
    auto *peer = server.nextPendingConnection();
    QVERIFY(peer);
    mavlink.requestParameters();
    auto *manager = mavlink.parameterManager();
    const auto generation = manager->generation();
    mavlink_autopilot_version_t version = {};
    version.capabilities = MAV_PROTOCOL_CAPABILITY_PARAM_ENCODE_BYTEWISE;
    mavlink_message_t message = {};
    mavlink_msg_autopilot_version_encode(42, 1, &message, &version);
    writeMavlinkMessage(peer, message);
    QTRY_VERIFY(manager->generation() > generation);
    mavlink_param_union_t integer = {};
    integer.param_int32 = -1;
    mavlink_msg_param_value_pack(42, 1, &message, "INTEGER", integer.param_float, MAV_PARAM_TYPE_INT32, 1, 0);
    writeMavlinkMessage(peer, message);
    QTRY_VERIFY(manager->parametersReady());
    QCOMPARE(mavlink.parameterValue("INTEGER")["value"].toDouble(), -1.);
    QCOMPARE(manager->snapshotProvenance()["encoding"].toInt(), int(ParameterManager::Bytewise));
    QVERIFY(mavlink.setParameter("INTEGER", 2147483647., MAV_PARAM_TYPE_INT32));
    QCOMPARE(mavlink.parameterValue("INTEGER")["value"].toDouble(), -1.);
    QSignalSpy writes(&mavlink, &Mavlink::parameterWriteFinished);
    bool sent = false;
    for (const auto &frame : readMavlinkMessages(peer, 1, 200)) if (frame.msgid == MAVLINK_MSG_ID_PARAM_SET) {
        mavlink_param_set_t set = {};
        mavlink_msg_param_set_decode(&frame, &set);
        uint32_t bits = 0;
        memcpy(&bits, &set.param_value, sizeof(bits));
        QCOMPARE(bits, uint32_t(0x7fffffff));
        mavlink_msg_param_value_pack(42, 1, &message, "INTEGER", set.param_value, MAV_PARAM_TYPE_INT32, 1, 0);
        writeMavlinkMessage(peer, message);
        sent = true;
    }
    QVERIFY(sent);
    QTRY_COMPARE(writes.size(), 1);
    QCOMPARE(writes[0][0].toMap().value("outcome").toInt(), int(ParameterManager::Confirmed));
    QCOMPARE(mavlink.parameterValue("INTEGER")["value"].toDouble(), 2147483647.);
    const auto readyGeneration = manager->generation();
    version.capabilities = MAV_PROTOCOL_CAPABILITY_PARAM_ENCODE_C_CAST;
    mavlink_msg_autopilot_version_encode(42, 1, &message, &version);
    writeMavlinkMessage(peer, message);
    QTRY_VERIFY(manager->generation() > readyGeneration);
    QVERIFY(!manager->parametersReady());
    mavlink_msg_param_value_pack(42, 1, &message, "INTEGER", 42, MAV_PARAM_TYPE_INT32, 1, 0);
    writeMavlinkMessage(peer, message);
    QTRY_VERIFY(manager->parametersReady());
    QCOMPARE(mavlink.parameterValue("INTEGER")["value"].toDouble(), 42.);
    QCOMPARE(manager->snapshotProvenance()["encoding"].toInt(), int(ParameterManager::NumericCast));
}

void MavlinkTest::parameterRecoveryRequestsUseSelectedTarget()
{
    qint64 now = 0;
    ParameterManager manager(nullptr, [&] { return now; });
    auto config = manager.recoveryConfiguration();
    config.initialResponseTimeoutMs = 100;
    config.streamInactivityTimeoutMs = 50;
    config.requestSpacingMs = 10;
    QVERIFY(manager.setRecoveryConfiguration(config));
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink(nullptr, &manager);
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(connectTestMavlink(mavlink, server, 1000, 42));
    auto *peer = server.nextPendingConnection();
    QVERIFY(peer);
    mavlink.requestParameters();
    auto messages = readMavlinkMessages(peer, 1, 200);
    int listCount = 0;
    for (const auto &message : messages) if (message.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST) ++listCount;
    QCOMPARE(listCount, 1);
    now = 100;
    manager.processRecovery(manager.generation());
    messages = readMavlinkMessages(peer, 1, 200);
    listCount = 0;
    for (const auto &message : messages) if (message.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
        mavlink_param_request_list_t request = {};
        mavlink_msg_param_request_list_decode(&message, &request);
        QCOMPARE(request.target_system, uint8_t(42));
        QCOMPARE(request.target_component, uint8_t(1));
        ++listCount;
    }
    QCOMPARE(listCount, 1);
    QCOMPARE(manager.listRequestAttempts(), 2);
    const auto send = [&](const char *name, int index) {
        mavlink_message_t message = {};
        mavlink_msg_param_value_pack(42, 1, &message, name, 1, MAV_PARAM_TYPE_REAL32, 3, uint16_t(index));
        writeMavlinkMessage(peer, message);
    };
    send("C", 2);
    send("A", 0);
    QTRY_COMPARE(manager.receivedParameterCount(), 2);
    now = 150;
    manager.processRecovery(manager.generation());
    QCOMPARE(manager.syncState(), ParameterManager::Recovering);
    now = 160;
    manager.processRecovery(manager.generation());
    messages = readMavlinkMessages(peer, 1, 200);
    int readCount = 0;
    for (const auto &message : messages) if (message.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_READ) {
        mavlink_param_request_read_t request = {};
        mavlink_msg_param_request_read_decode(&message, &request);
        QCOMPARE(request.target_system, uint8_t(42));
        QCOMPARE(request.target_component, uint8_t(1));
        QCOMPARE(request.param_index, int16_t(1));
        QCOMPARE(QByteArray(request.param_id, 16), QByteArray(16, '\0'));
        ++readCount;
    }
    QCOMPARE(readCount, 1);
    // The transport also rejects a delayed request for an already validated index.
    manager.requestRead(42, 1, 0, manager.generation());
    send("B", 1);
    QTRY_VERIFY(manager.parametersReady());
    const auto old = manager.generation();
    now = 2000;
    manager.processRecovery(old);
    QCOMPARE(manager.recoveryRequestCount(), 1);
    manager.refresh();
    manager.requestRead(42, 1, 1, old);
    messages = readMavlinkMessages(peer, 1, 200);
    for (const auto &message : messages) QVERIFY(message.msgid != MAVLINK_MSG_ID_PARAM_REQUEST_READ);
}

void MavlinkTest::rcCalibrationConfirmation_data()
{
    QTest::addColumn<int>("mode");
    QTest::newRow("confirmed") << 0;
    QTest::newRow("partial-rejection") << 1;
    QTest::newRow("cancelled") << 2;
    QTest::newRow("timed-out") << 3;
}

void MavlinkTest::rcCalibrationConfirmation()
{
    QFETCH(int, mode);
    qint64 now = 0;
    ParameterManager manager(nullptr, [&] { return now; });
    Mavlink mavlink(nullptr, &manager);
    mavlink.setTelemetryLoggingEnabled(false);
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QVERIFY(connectTestMavlink(mavlink, server, 1000, 42));
    auto *peer = server.nextPendingConnection(); QVERIFY(peer);
    mavlink.requestParameters();
    const QStringList names{"RC1_MIN", "RC1_MAX", "RC1_TRIM", "RC1_REVERSED"};
    const float before[] = {1100, 1900, 1500, 0};
    const float wanted[] = {1000, 2000, 1500, 1};
    const auto respond = [&](int index, float value) {
        mavlink_message_t message = {};
        const auto name = names[index].toLatin1();
        mavlink_msg_param_value_pack(42, 1, &message, name.constData(), value,
                                     index == 3 ? MAV_PARAM_TYPE_INT8 : MAV_PARAM_TYPE_INT16, 4, uint16_t(index));
        writeMavlinkMessage(peer, message);
    };
    for (int i = 0; i < 4; ++i) respond(i, before[i]);
    QTRY_VERIFY(manager.parametersReady());
    readMavlinkMessages(peer, 1, 100);
    QSignalSpy batch(&mavlink, &Mavlink::rcCalibrationWriteFinished);
    QSignalSpy writes(&mavlink, &Mavlink::parameterWriteFinished);
    const QVariantMap channel{{"channel", 1}, {"minimum", 1000}, {"maximum", 2000}, {"trim", 1500}, {"reversed", true}};
    if (mode == 3) {
        auto config = manager.writeConfiguration();
        config.responseTimeoutMs = 30; config.requestSpacingMs = 10; config.maxAttempts = 1;
        QVERIFY(manager.setWriteConfiguration(config));
    }
    const auto request = mavlink.requestRcCalibrationWrite({channel});
    QVERIFY(request.value("accepted").toBool());
    QCOMPARE(batch.size(), 0);
    QCOMPARE(manager.parameterValue("RC1_MIN")["value"].toDouble(), 1100.);
    if (mode == 3) {
        for (now = 0; now <= 60; now += 10) manager.processWrites(manager.generation());
        QCOMPARE(batch.size(), 1);
        QVERIFY(!batch[0][0].toMap().value("success").toBool());
        QCOMPARE(batch[0][0].toMap().value("confirmedCount").toInt(), 0);
        const auto results = batch[0][0].toMap().value("results").toList();
        QCOMPARE(results.size(), 4);
        for (const auto &result : results) QCOMPARE(result.toMap().value("outcome").toInt(), int(ParameterManager::TimedOut));
        QCOMPARE(manager.parameterValue("RC1_MIN").value("value").toDouble(), 1100.);
    } else if (mode == 2) {
        mavlink.disconnectFromVehicle();
        QCOMPARE(batch.size(), 1);
        QCOMPARE(batch[0][0].toMap().value("status").toString(), QString("Cancelled"));
    } else {
        for (int index = 0; index < 4; ++index) {
            now = index * 40;
            manager.processWrites(manager.generation());
            const auto messages = readMavlinkMessages(peer, 1, 200);
            bool found = false;
            for (const auto &message : messages) if (message.msgid == MAVLINK_MSG_ID_PARAM_SET) {
                mavlink_param_set_t set = {};
                mavlink_msg_param_set_decode(&message, &set);
                QCOMPARE(set.target_system, uint8_t(42)); QCOMPARE(set.target_component, uint8_t(1));
                QCOMPARE(QByteArray(set.param_id, names[index].size()), names[index].toLatin1());
                QCOMPARE(set.param_value, wanted[index]);
                QCOMPARE(set.param_type, uint8_t(index == 3 ? MAV_PARAM_TYPE_INT8 : MAV_PARAM_TYPE_INT16));
                found = true;
            }
            QVERIFY(found);
            QCOMPARE(batch.size(), 0);
            respond(index, mode == 1 && index == 1 ? before[index] : wanted[index]);
            QTRY_COMPARE(writes.size(), index + 1);
        }
        QCOMPARE(batch.size(), 1);
        QCOMPARE(batch[0][0].toMap().value("success").toBool(), mode == 0);
        QCOMPARE(batch[0][0].toMap().value("confirmedCount").toInt(), mode == 0 ? 4 : 3);
        QCOMPARE(manager.parameterValue("RC1_MIN")["value"].toDouble(), 1000.);
        if (mode == 1) QVERIFY(batch[0][0].toMap().value("reason").toString().contains("RC1_MAX: Rejected"));
        QVERIFY(manager.parametersReady());
        QCOMPARE(manager.receivedParameterCount(), 4);
    }
    QCOMPARE(writes.size(), 4);
}

void MavlinkTest::connectionRequiresValidHeartbeat_data()
{
    QTest::addColumn<int>("system"); QTest::addColumn<int>("component");
    QTest::addColumn<int>("type"); QTest::addColumn<int>("autopilot"); QTest::addColumn<bool>("valid");
    QTest::newRow("valid") << 42 << 1 << int(MAV_TYPE_QUADROTOR) << int(MAV_AUTOPILOT_ARDUPILOTMEGA) << true;
    QTest::newRow("zero-system") << 0 << 1 << int(MAV_TYPE_QUADROTOR) << int(MAV_AUTOPILOT_ARDUPILOTMEGA) << false;
    QTest::newRow("zero-component") << 42 << 0 << int(MAV_TYPE_QUADROTOR) << int(MAV_AUTOPILOT_ARDUPILOTMEGA) << false;
    QTest::newRow("gcs") << 42 << 1 << int(MAV_TYPE_GCS) << int(MAV_AUTOPILOT_ARDUPILOTMEGA) << false;
    QTest::newRow("invalid-autopilot") << 42 << 1 << int(MAV_TYPE_QUADROTOR) << int(MAV_AUTOPILOT_INVALID) << false;
}

void MavlinkTest::connectionRequiresValidHeartbeat()
{
    QFETCH(int, system); QFETCH(int, component); QFETCH(int, type); QFETCH(int, autopilot); QFETCH(bool, valid);
    ConnectionHarness h;
    QVERIFY(h.open());
    QCOMPARE(h.mavlink.connectionState(), Mavlink::AwaitingHeartbeat);
    QVERIFY(!h.mavlink.connected());
    mavlink_message_t telemetry = {};
    mavlink_attitude_t attitude = {};
    mavlink_msg_attitude_encode(42, 1, &telemetry, &attitude);
    h.send(telemetry);
    QVERIFY(!h.mavlink.connected());
    h.heartbeat(uint8_t(system), uint8_t(component), uint8_t(type), uint8_t(autopilot));
    QCOMPARE(h.mavlink.connected(), valid);
    QCOMPARE(h.mavlink.connectionState(), valid ? Mavlink::Connected : Mavlink::AwaitingHeartbeat);
}

void MavlinkTest::selectedHeartbeatRefreshesOnlyItsOwnLiveness()
{
    ConnectionHarness h; QVERIFY(h.open()); h.heartbeat();
    const auto generation = h.mavlink.connectionGeneration();
    const auto parameterGeneration = h.parameters.generation();
    h.now = 60; h.heartbeat(43, 1); h.heartbeat(42, 2);
    QCOMPARE(h.mavlink.lastHeartbeatMonotonicMs(), qint64(0));
    h.now = 90; h.heartbeat();
    QCOMPARE(h.mavlink.connectionGeneration(), generation);
    QCOMPARE(h.parameters.generation(), parameterGeneration);
    QCOMPARE(h.mavlink.lastHeartbeatMonotonicMs(), qint64(90));
    QVERIFY(h.mavlink.lastHeartbeatTimestampMs() > 0);
    h.now = 100; h.mavlink.processConnectionTimeout(generation);
    QVERIFY(h.mavlink.connected());
    h.now = 190; h.mavlink.processConnectionTimeout(generation);
    QVERIFY(!h.mavlink.connected());
    QCOMPARE(h.mavlink.heartbeatExpiryCount(), quint64(1));
    h.mavlink.processConnectionTimeout(generation);
    h.mavlink.processConnectionTimeout(h.mavlink.connectionGeneration());
    QCOMPARE(h.mavlink.heartbeatExpiryCount(), quint64(1));
}

void MavlinkTest::telemetryCannotPreventHeartbeatExpiry()
{
    ConnectionHarness h; QVERIFY(h.open()); h.heartbeat(); h.mavlink.requestParameters();
    QSignalSpy disconnected(&h.mavlink, &Mavlink::connectionStatusChanged);
    for (int tick = 1; tick <= 5; ++tick) {
        h.now = tick * 20;
        mavlink_message_t message = {};
        mavlink_global_position_int_t position = {}; position.lat = 123456789; position.lon = 765432100;
        mavlink_msg_global_position_int_encode(42, 1, &message, &position); h.send(message);
        mavlink_attitude_t attitude = {};
        mavlink_msg_attitude_encode(42, 1, &message, &attitude); h.send(message);
        h.parameter("A", 0);
    }
    QCOMPARE(h.mavlink.connectionState(), Mavlink::Disconnected);
    QCOMPARE(disconnected.size(), 1);
    QCOMPARE(h.parameters.syncState(), ParameterManager::Idle);
    QVERIFY(!h.parameters.parametersReady());
}

void MavlinkTest::heartbeatExpiryRevokesSnapshotAndReconnects()
{
    ConnectionHarness h; QVERIFY(h.open()); h.heartbeat(); h.mavlink.requestParameters(); h.complete();
    QVERIFY(h.parameters.parametersReady());
    const auto oldSnapshot = h.parameters.parameters();
    const auto old = h.mavlink.connectionGeneration();
    QCOMPARE(h.parameters.snapshotProvenance().value("sourceConnectionGeneration").toULongLong(), old);
    h.expire();
    QVERIFY(!h.parameters.parametersReady()); QVERIFY(h.parameters.hasCompleteSnapshot());
    QCOMPARE(h.parameters.parameters(), oldSnapshot);
    QVERIFY(!h.parameters.snapshotProvenance().value("current").toBool());
    h.now = 150; h.heartbeat(43, 1); QVERIFY(!h.mavlink.connected());
    h.heartbeat();
    QVERIFY(h.mavlink.connected()); QVERIFY(h.mavlink.connectionGeneration() > old);
    QVERIFY(!h.parameters.parametersReady());
    h.mavlink.processConnectionTimeout(old); QVERIFY(h.mavlink.connected());
    h.mavlink.requestParameters(); h.parameter("B", 1); h.parameter("A", 0);
    QVERIFY(!h.parameters.parametersReady()); h.parameter("C", 2);
    QVERIFY(h.parameters.parametersReady());
    QCOMPARE(h.parameters.snapshotProvenance().value("sourceConnectionGeneration").toULongLong(), h.mavlink.connectionGeneration());
}

void MavlinkTest::heartbeatExpiryCancelsActiveAndQueuedWrites()
{
    ConnectionHarness h; QVERIFY(h.open()); h.heartbeat(); h.mavlink.requestParameters(); h.complete();
    QSignalSpy results(&h.mavlink, &Mavlink::parameterWriteFinished);
    QSignalSpy sets(&h.parameters, &ParameterManager::requestSet);
    QSignalSpy batches(&h.parameters, &ParameterManager::writeBatchFinished);
    QVERIFY(h.parameters.requestWriteBatch({QVariantMap{{"name", "A"}, {"value", 2}, {"type", MAV_PARAM_TYPE_REAL32}},
                                           QVariantMap{{"name", "B"}, {"value", 3}, {"type", MAV_PARAM_TYPE_REAL32}}}).value("accepted").toBool());
    QVERIFY(h.mavlink.setParameter("A", 4));
    h.parameters.processWrites(h.parameters.generation());
    QVERIFY(sets.size() >= 1);
    const auto old = h.parameters.generation();
    h.expire();
    QCOMPARE(results.size(), 3); QCOMPARE(batches.size(), 1);
    QCOMPARE(batches[0][0].toMap().value("status").toString(), QString("Cancelled"));
    for (const auto &row : results) QCOMPARE(row[0].toMap().value("outcome").toInt(), int(ParameterManager::Cancelled));
    const auto sent = sets.size();
    h.heartbeat(); h.mavlink.requestParameters(); h.complete();
    h.parameters.processWrites(old);
    QCOMPARE(sets.size(), sent); QCOMPARE(results.size(), 3);
}

void MavlinkTest::heartbeatExpiryCancelsRecoveryAndOldWork_data()
{
    QTest::addColumn<bool>("recovering");
    QTest::newRow("Loading") << false;
    QTest::newRow("Recovering") << true;
}

void MavlinkTest::heartbeatExpiryCancelsRecoveryAndOldWork()
{
    QFETCH(bool, recovering);
    ConnectionHarness h;
    auto config = h.parameters.recoveryConfiguration(); config.streamInactivityTimeoutMs = 20;
    QVERIFY(h.parameters.setRecoveryConfiguration(config));
    QVERIFY(h.open()); h.heartbeat(); h.mavlink.requestParameters(); h.parameter("A", 0);
    if (recovering) {
        h.now = 20; h.parameters.processRecovery(h.parameters.generation());
        QCOMPARE(h.parameters.syncState(), ParameterManager::Recovering);
        h.now = 60; h.parameters.processRecovery(h.parameters.generation());
    } else QCOMPARE(h.parameters.syncState(), ParameterManager::Loading);
    const auto old = h.parameters.generation();
    QSignalSpy reads(&h.parameters, &ParameterManager::requestRead);
    h.now = 100; h.mavlink.processConnectionTimeout(h.mavlink.connectionGeneration());
    QCOMPARE(h.parameters.syncState(), ParameterManager::Idle);
    QCOMPARE(h.parameters.inFlightRequestCount(), 0);
    h.heartbeat(); h.mavlink.requestParameters();
    h.parameters.processRecovery(old); h.parameters.processWrites(old);
    QCOMPARE(reads.size(), 0);
}

void MavlinkTest::explicitDisconnectAndTransportFailureAreIdempotent()
{
    ConnectionHarness h; QVERIFY(h.open()); h.heartbeat();
    QSignalSpy status(&h.mavlink, &Mavlink::connectionStatusChanged);
    h.mavlink.disconnectFromVehicle();
    const auto generation = h.mavlink.connectionGeneration();
    h.mavlink.disconnectFromVehicle();
    QCoreApplication::processEvents();
    QCOMPARE(h.mavlink.connectionGeneration(), generation); QCOMPARE(status.size(), 1);
    QCOMPARE(h.mavlink.lastDisconnectReason(), QString("Explicit disconnect"));
    QVERIFY(h.open()); h.heartbeat();
    h.peer->abort();
    QTRY_VERIFY(!h.mavlink.connected());
    const auto failed = h.mavlink.connectionGeneration();
    QCoreApplication::processEvents();
    QCOMPARE(h.mavlink.connectionGeneration(), failed);
    QCOMPARE(h.parameters.syncState(), ParameterManager::Idle);
    QVERIFY(h.mavlink.lastDisconnectReason().contains("Transport"));
}

void MavlinkTest::replacementRejectsOldSourceAndCallbacks()
{
    ConnectionHarness h; QVERIFY(h.open()); h.heartbeat();
    const auto old = h.mavlink.connectionGeneration();
    const auto oldParameters = h.parameters.generation();
    QVERIFY(h.open()); h.heartbeat(7, 2);
    const auto generation = h.mavlink.connectionGeneration();
    QVERIFY(generation > old);
    h.now = 50; h.heartbeat(42, 1); h.heartbeat(7, 1);
    QCOMPARE(h.mavlink.lastHeartbeatMonotonicMs(), qint64(0));
    h.mavlink.processConnectionTimeout(old);
    h.parameters.processRecovery(oldParameters); h.parameters.processWrites(oldParameters);
    QVERIFY(h.mavlink.connected()); QCOMPARE(h.mavlink.selectedSystemId(), 7); QCOMPARE(h.mavlink.selectedComponentId(), 2);
    h.mavlink.requestParameters();
    QSignalSpy lists(&h.parameters, &ParameterManager::requestList);
    QTest::qWait(280); // Both old and new startup timers would be due; only the manual request remains.
    QCOMPARE(lists.size(), 0);
}

void MavlinkTest::realHeartbeatTimerAndDestruction()
{
    QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    int changes = 0;
    auto mavlink = std::make_unique<Mavlink>();
    auto config = mavlink->connectionConfiguration(); config.heartbeatTimeoutMs = 40;
    QVERIFY(mavlink->setConnectionConfiguration(config));
    mavlink->setTelemetryLoggingEnabled(false);
    QVERIFY(connectTestMavlink(*mavlink, server));
    connect(mavlink.get(), &Mavlink::connectionStatusChanged, this, [&](bool) { ++changes; });
    QTRY_VERIFY_WITH_TIMEOUT(!mavlink->connected(), 1000);
    QCOMPARE(changes, 1); QCOMPARE(mavlink->heartbeatExpiryCount(), quint64(1));
    mavlink.reset();
    QTest::qWait(50);
    QCOMPARE(changes, 1);
}

void MavlinkTest::udpHeartbeatExpiryAndReconnect()
{
    qint64 now = 0;
    Mavlink mavlink(nullptr, nullptr, [&] { return now; });
    auto config = mavlink.connectionConfiguration(); config.heartbeatTimeoutMs = 100;
    QVERIFY(mavlink.setConnectionConfiguration(config));
    mavlink.setTelemetryLoggingEnabled(false); mavlink.setFlightControllerLoggingEnabled(false);
    QUdpSocket peer;
    QVERIFY(peer.bind(QHostAddress::LocalHost, 0));
    QVERIFY(mavlink.connectToVehicleProtocol("UDP", "127.0.0.1", peer.localPort()));
    auto *transport = mavlink.findChild<UdpLink *>(); QVERIFY(transport);
    auto *socket = transport->findChild<QUdpSocket *>(); QVERIFY(socket);
    QTRY_COMPARE(socket->state(), QAbstractSocket::ConnectedState);
    QVERIFY(socket->localPort() != 0);
    const auto heartbeat = [&] {
        mavlink_message_t message = {};
        mavlink_msg_heartbeat_pack(42, 1, &message, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA, 0, 4, MAV_STATE_ACTIVE);
        uint8_t data[MAVLINK_MAX_PACKET_LEN];
        const auto length = mavlink_msg_to_send_buffer(data, &message);
        QCOMPARE(peer.writeDatagram(reinterpret_cast<const char *>(data), length, QHostAddress::LocalHost, socket->localPort()), qint64(length));
    };
    heartbeat(); QTRY_VERIFY(mavlink.connected());
    const auto old = mavlink.connectionGeneration();
    now = 100; mavlink.processConnectionTimeout(old);
    QVERIFY(!mavlink.connected()); QVERIFY(socket->isOpen());
    heartbeat(); QTRY_VERIFY(mavlink.connected());
    QVERIFY(mavlink.connectionGeneration() > old);
    mavlink.disconnectFromVehicle(); QVERIFY(!mavlink.connected());
}

void MavlinkTest::initialHeartbeatDeadlineAndConfiguration()
{
    ConnectionHarness h; QVERIFY(h.open());
    auto config = h.mavlink.connectionConfiguration();
    QVERIFY(!h.mavlink.setConnectionConfiguration(config));
    h.now = 500; h.mavlink.processConnectionTimeout(h.mavlink.connectionGeneration());
    QCOMPARE(h.mavlink.connectionState(), Mavlink::Disconnected);
    QVERIFY(h.mavlink.lastDisconnectReason().contains("Initial"));
    QCOMPARE(h.mavlink.heartbeatExpiryCount(), quint64(0));
    QVERIFY(h.mavlink.setConnectionConfiguration(config));
    config.heartbeatTimeoutMs = 0;
    QVERIFY(!h.mavlink.setConnectionConfiguration(config));
    QTcpServer refusing;
    QVERIFY(refusing.listen(QHostAddress::LocalHost, 0));
    const auto unusedPort = refusing.serverPort();
    refusing.close();
    Mavlink failed;
    QVERIFY(failed.connectToVehicle("127.0.0.1", unusedPort));
    QTRY_COMPARE(failed.connectionState(), Mavlink::Disconnected);
    QVERIFY(failed.lastDisconnectReason().contains("Transport"));
}

void MavlinkTest::transportDestructionAndOwnerDestructionCancelWork()
{
    {
        ConnectionHarness h; QVERIFY(h.open()); h.heartbeat();
        QSignalSpy status(&h.mavlink, &Mavlink::connectionStatusChanged);
        for (auto *transport : h.mavlink.findChildren<ILink *>()) if (transport->isConnected()) { delete transport; break; }
        QVERIFY(!h.mavlink.connected()); QCOMPARE(status.size(), 1);
        QVERIFY(h.mavlink.lastDisconnectReason().contains("Transport"));
    }
    ParameterManager parameters;
    QSignalSpy results(&parameters, &ParameterManager::writeFinished);
    QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    auto mavlink = std::make_unique<Mavlink>(nullptr, &parameters);
    mavlink->setTelemetryLoggingEnabled(false); mavlink->setFlightControllerLoggingEnabled(false);
    QVERIFY(connectTestMavlink(*mavlink, server));
    parameters.refresh();
    ParameterManager::Sample sample{QByteArray("A").leftJustified(16, '\0'), 1, MAV_PARAM_TYPE_REAL32, 0, 1};
    parameters.receive(1, 1, parameters.generation(), sample);
    QVERIFY(parameters.parametersReady());
    QVERIFY(mavlink->setParameter("A", 2));
    mavlink.reset();
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0][0].toMap().value("outcome").toInt(), int(ParameterManager::Cancelled));
    QVERIFY(!parameters.parametersReady()); QVERIFY(parameters.hasCompleteSnapshot());
    QTest::qWait(30);
    QCOMPARE(results.size(), 1);
}

void MavlinkTest::telemetryLogCapturesTimestampedMavlinkFrames()
{
    QTemporaryDir logDirectory;
    QVERIFY(logDirectory.isValid());

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLogDirectory(logDirectory.path());
    QVERIFY(connectTestMavlink(mavlink, server));
    QVERIFY(mavlink.connected());
    QVERIFY(mavlink.telemetryLogActive());

    const QString logPath = mavlink.currentTelemetryLogPath();
    QVERIFY(logPath.endsWith(QStringLiteral(".tlog")));
    QVERIFY(QFileInfo::exists(logPath));

    QTcpSocket *clientSocket = server.nextPendingConnection();
    QVERIFY(clientSocket != nullptr);

    QSignalSpy messageSpy(&mavlink, &Mavlink::messageReceived);
    mavlink_message_t heartbeat = {};
    mavlink_msg_heartbeat_pack(
        1,
        MAV_COMP_ID_AUTOPILOT1,
        &heartbeat,
        MAV_TYPE_QUADROTOR,
        MAV_AUTOPILOT_ARDUPILOTMEGA,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
        4,
        MAV_STATE_ACTIVE
    );
    writeMavlinkMessage(clientSocket, heartbeat);
    QTRY_COMPARE(messageSpy.count(), 1);

    mavlink.disconnectFromVehicle();
    QVERIFY(!mavlink.telemetryLogActive());

    QFile logFile(logPath);
    QVERIFY(logFile.open(QIODevice::ReadOnly));
    const QByteArray logBytes = logFile.readAll();
    QVERIFY(logBytes.size() > 8);

    int offset = 0;
    int parsedMessages = 0;
    bool foundHeartbeat = false;
    mavlink_status_t status = {};

    while (offset + 8 < logBytes.size()) {
        const quint64 timestampUsec = readBigEndianUint64(logBytes, offset);
        QVERIFY(timestampUsec > 0);
        offset += 8;

        const int packetLength = mavlinkPacketLength(logBytes, offset);
        QVERIFY(packetLength > 0);
        QVERIFY(offset + packetLength <= logBytes.size());

        for (int i = 0; i < packetLength; ++i) {
            mavlink_message_t parsed = {};
            if (mavlink_parse_char(MAVLINK_COMM_0,
                                    static_cast<uint8_t>(logBytes.at(offset + i)),
                                    &parsed,
                                    &status)) {
                ++parsedMessages;
                foundHeartbeat = foundHeartbeat || parsed.msgid == MAVLINK_MSG_ID_HEARTBEAT;
            }
        }

        offset += packetLength;
    }

    QCOMPARE(offset, logBytes.size());
    QVERIFY(parsedMessages >= 2);
    QVERIFY(foundHeartbeat);
    QCOMPARE(mavlink.telemetryLogFiles(), QStringList({ QFileInfo(logPath).absoluteFilePath() }));
}

void MavlinkTest::flightControllerLogCapturesStatusTextMessages()
{
    QTemporaryDir logDirectory;
    QVERIFY(logDirectory.isValid());

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    mavlink.setFlightControllerLogDirectory(logDirectory.path());
    mavlink.setFlightControllerLoggingEnabled(true);
    QVERIFY(connectTestMavlink(mavlink, server));

    QTcpSocket *clientSocket = server.nextPendingConnection();
    QVERIFY(clientSocket != nullptr);

    QSignalSpy messageSpy(&mavlink, &Mavlink::messageReceived);
    mavlink_message_t status = {};
    mavlink_msg_statustext_pack(
        42,
        MAV_COMP_ID_AUTOPILOT1,
        &status,
        MAV_SEVERITY_ERROR,
        "PreArm: GPS not locked",
        0,
        0
    );
    writeMavlinkMessage(clientSocket, status);

    QTRY_COMPARE(messageSpy.count(), 1);
    QVERIFY(mavlink.flightControllerLogActive());
    const QString logPath = mavlink.currentFlightControllerLogPath();
    QVERIFY(logPath.endsWith(QStringLiteral(".log")));

    mavlink.disconnectFromVehicle();
    QVERIFY(!mavlink.flightControllerLogActive());

    QFile logFile(logPath);
    QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString logText = QString::fromUtf8(logFile.readAll());
    QVERIFY(logText.contains(QStringLiteral("ERROR")));
    QVERIFY(logText.contains(QStringLiteral("sys=42")));
    QVERIFY(logText.contains(QStringLiteral("PreArm: GPS not locked")));
    QCOMPARE(mavlink.flightControllerLogFiles(), QStringList({ QFileInfo(logPath).absoluteFilePath() }));
}

void MavlinkTest::disabledFlightControllerLogDoesNotCreateFile()
{
    QTemporaryDir logDirectory;
    QVERIFY(logDirectory.isValid());

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    mavlink.setFlightControllerLogDirectory(logDirectory.path());
    mavlink.setFlightControllerLoggingEnabled(false);
    QVERIFY(connectTestMavlink(mavlink, server));

    QTcpSocket *clientSocket = server.nextPendingConnection();
    QVERIFY(clientSocket != nullptr);

    QSignalSpy messageSpy(&mavlink, &Mavlink::messageReceived);
    mavlink_message_t status = {};
    mavlink_msg_statustext_pack(
        42,
        MAV_COMP_ID_AUTOPILOT1,
        &status,
        MAV_SEVERITY_WARNING,
        "EKF variance",
        0,
        0
    );
    writeMavlinkMessage(clientSocket, status);

    QTRY_COMPARE(messageSpy.count(), 1);
    QVERIFY(!mavlink.flightControllerLogActive());
    QVERIFY(mavlink.flightControllerLogFiles().isEmpty());
    mavlink.setFlightControllerLoggingEnabled(true);
}

void MavlinkTest::runAllMotorTestSendsFourDelayedMotorCommands()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(connectTestMavlink(mavlink, server));
    QVERIFY(mavlink.connected());

    QTcpSocket *clientSocket = server.nextPendingConnection();
    QVERIFY(clientSocket != nullptr);

    mavlink.requestParameters();
    for (int i = 0; i < 2; ++i) {
        mavlink_message_t value{};
        mavlink_msg_param_value_pack(1, 1, &value, i ? "FRAME_TYPE" : "FRAME_CLASS", i ? 1 : 1, MAV_PARAM_TYPE_REAL32, 2, i);
        writeMavlinkMessage(clientSocket, value);
    }
    QTRY_VERIFY(mavlink.parameterValue("FRAME_TYPE").size() > 0);
    mavlink.runAllMotorTest(10, 2, 4);

    const QVector<mavlink_command_long_t> commands = readCommands(clientSocket, MAV_CMD_DO_MOTOR_TEST, 4);
    QCOMPARE(commands.size(), 4);

    for (int i = 0; i < commands.size(); ++i) {
        const mavlink_command_long_t command = commands.at(i);
        QCOMPARE(command.command, uint16_t(MAV_CMD_DO_MOTOR_TEST));
        QCOMPARE(command.param1, float(i + 1));
        QCOMPARE(command.param2, float(MOTOR_TEST_THROTTLE_PERCENT));
        QCOMPARE(command.param3, 10.0f);
        QCOMPARE(command.param4, 2.0f);
        QCOMPARE(command.param5, 0.0f);
        QCOMPARE(command.param6, float(MOTOR_TEST_ORDER_DEFAULT));
    }
}

void MavlinkTest::uploadedMissionDisplaysOnlyNavigationWaypoints()
{
    QTemporaryFile missionFile;
    QVERIFY(missionFile.open());

    const QByteArray missionData = QByteArrayLiteral(
        "QGC WPL 110\n"
        "0\t1\t0\t16\t0\t0\t0\t0\t34.8077610\t76.7980650\t3407.88\t1\n"
        "1\t0\t0\t22\t0\t0\t0\t0\t34.8077610\t76.7980650\t3437.88\t1\n"
        "2\t0\t0\t16\t0\t0\t0\t0\t34.8077610\t76.7980650\t3467.88\t1\n"
        "3\t0\t2\t178\t0\t10\t-1\t0\t0\t0\t0\t1\n"
        "4\t0\t0\t16\t0\t0\t0\t0\t34.8055995\t76.7814159\t4494.61\t1\n"
        "5\t0\t0\t16\t0\t0\t0\t0\t34.8055854\t76.7813078\t4497.11\t1\n"
        "6\t0\t0\t21\t0\t0\t0\t0\t34.8043222\t76.7715778\t4282.28\t1\n"
        "7\t0\t2\t218\t153\t0\t0\t0\t0\t0\t0\t1\n"
        "8\t0\t0\t16\t0\t0\t0\t0\t34.8077610\t76.7980650\t4342.21\t1\n");
    QCOMPARE(missionFile.write(missionData), qint64(missionData.size()));
    QVERIFY(missionFile.flush());

    Mavlink mavlink;
    QVERIFY(!mavlink.uploadMissionFile(QUrl::fromLocalFile(missionFile.fileName())));

    const QVariantList waypoints = mavlink.missionWaypoints();
    QCOMPARE(waypoints.size(), 4);
    QCOMPARE(waypoints.at(0).toMap().value(QStringLiteral("command")).toUInt(), uint(MAV_CMD_NAV_TAKEOFF));
    QCOMPARE(waypoints.at(1).toMap().value(QStringLiteral("command")).toUInt(), uint(MAV_CMD_NAV_WAYPOINT));
    QCOMPARE(waypoints.at(2).toMap().value(QStringLiteral("command")).toUInt(), uint(MAV_CMD_NAV_LAND));
    QCOMPARE(waypoints.at(3).toMap().value(QStringLiteral("command")).toUInt(), uint(MAV_CMD_NAV_WAYPOINT));

    const QVariantList elevationWaypoints = mavlink.missionElevationWaypoints();
    QCOMPARE(elevationWaypoints.size(), 6);
    QCOMPARE(elevationWaypoints.at(0).toMap().value(QStringLiteral("command")).toUInt(), uint(MAV_CMD_NAV_TAKEOFF));
    QCOMPARE(elevationWaypoints.at(1).toMap().value(QStringLiteral("command")).toUInt(), uint(MAV_CMD_NAV_WAYPOINT));
    QCOMPARE(elevationWaypoints.at(2).toMap().value(QStringLiteral("seq")).toUInt(), uint(4));
    QCOMPARE(elevationWaypoints.at(3).toMap().value(QStringLiteral("seq")).toUInt(), uint(5));
    QCOMPARE(elevationWaypoints.at(4).toMap().value(QStringLiteral("command")).toUInt(), uint(MAV_CMD_NAV_LAND));
    QCOMPARE(elevationWaypoints.at(5).toMap().value(QStringLiteral("seq")).toUInt(), uint(8));
}

void MavlinkTest::uploadedMissionStoresHomeLocation()
{
    QTemporaryFile missionFile;
    QVERIFY(missionFile.open());

    const QByteArray missionData = QByteArrayLiteral(
        "QGC WPL 110\n"
        "0\t1\t0\t16\t0\t0\t0\t0\t34.8077610\t76.7980650\t3407.88\t1\n"
        "1\t0\t0\t22\t0\t0\t0\t0\t34.8077610\t76.7980650\t3437.88\t1\n"
        "2\t0\t0\t16\t0\t0\t0\t0\t34.8055995\t76.7814159\t4494.61\t1\n");
    QCOMPARE(missionFile.write(missionData), qint64(missionData.size()));
    QVERIFY(missionFile.flush());

    Mavlink mavlink;
    QVERIFY(!mavlink.uploadMissionFile(QUrl::fromLocalFile(missionFile.fileName())));

    QVERIFY(mavlink.uploadedHomeLocationAvailable());
    const QVariantMap homeLocation = mavlink.uploadedHomeLocation();
    QCOMPARE(homeLocation.value(QStringLiteral("lat")).toDouble(), 34.8077610);
    QCOMPARE(homeLocation.value(QStringLiteral("lon")).toDouble(), 76.7980650);
    QVERIFY(qAbs(homeLocation.value(QStringLiteral("alt")).toDouble() - 3407.88) < 0.01);
}

void MavlinkTest::downloadedMissionStoresHomeLocation()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(connectTestMavlink(mavlink, server));

    QTcpSocket *clientSocket = server.nextPendingConnection();
    QVERIFY(clientSocket != nullptr);

    mavlink.downloadMissionFromVehicle();

    mavlink_message_t count = {};
    mavlink_msg_mission_count_pack(
        1,
        MAV_COMP_ID_AUTOPILOT1,
        &count,
        255,
        0,
        3,
        MAV_MISSION_TYPE_MISSION,
        0
    );
    writeMavlinkMessage(clientSocket, count);

    mavlink_message_t home = {};
    mavlink_msg_mission_item_int_pack(
        1,
        MAV_COMP_ID_AUTOPILOT1,
        &home,
        255,
        0,
        0,
        MAV_FRAME_GLOBAL,
        MAV_CMD_NAV_WAYPOINT,
        1,
        1,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        static_cast<int32_t>(qRound64(34.8077610 * 10000000.0)),
        static_cast<int32_t>(qRound64(76.7980650 * 10000000.0)),
        3407.88f,
        MAV_MISSION_TYPE_MISSION
    );
    writeMavlinkMessage(clientSocket, home);

    mavlink_message_t takeoff = {};
    mavlink_msg_mission_item_int_pack(
        1,
        MAV_COMP_ID_AUTOPILOT1,
        &takeoff,
        255,
        0,
        1,
        MAV_FRAME_GLOBAL,
        MAV_CMD_NAV_TAKEOFF,
        0,
        1,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        static_cast<int32_t>(qRound64(34.8077610 * 10000000.0)),
        static_cast<int32_t>(qRound64(76.7980650 * 10000000.0)),
        3437.88f,
        MAV_MISSION_TYPE_MISSION
    );
    writeMavlinkMessage(clientSocket, takeoff);

    mavlink_message_t waypoint = {};
    mavlink_msg_mission_item_int_pack(
        1,
        MAV_COMP_ID_AUTOPILOT1,
        &waypoint,
        255,
        0,
        2,
        MAV_FRAME_GLOBAL,
        MAV_CMD_NAV_WAYPOINT,
        0,
        1,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        static_cast<int32_t>(qRound64(34.8055995 * 10000000.0)),
        static_cast<int32_t>(qRound64(76.7814159 * 10000000.0)),
        4494.61f,
        MAV_MISSION_TYPE_MISSION
    );
    writeMavlinkMessage(clientSocket, waypoint);

    QTRY_VERIFY(mavlink.uploadedHomeLocationAvailable());
    const QVariantMap homeLocation = mavlink.uploadedHomeLocation();
    QCOMPARE(homeLocation.value(QStringLiteral("lat")).toDouble(), 34.8077610);
    QCOMPARE(homeLocation.value(QStringLiteral("lon")).toDouble(), 76.7980650);
    QVERIFY(qAbs(homeLocation.value(QStringLiteral("alt")).toDouble() - 3407.88) < 0.01);
}

void MavlinkTest::resetHomeToUploadedLocationSendsSetHomeCommand()
{
    QTemporaryFile missionFile;
    QVERIFY(missionFile.open());

    const QByteArray missionData = QByteArrayLiteral(
        "QGC WPL 110\n"
        "0\t1\t0\t16\t0\t0\t0\t0\t34.8077610\t76.7980650\t3407.88\t1\n"
        "1\t0\t0\t22\t0\t0\t0\t0\t34.8077610\t76.7980650\t3437.88\t1\n"
        "2\t0\t0\t16\t0\t0\t0\t0\t34.8055995\t76.7814159\t4494.61\t1\n");
    QCOMPARE(missionFile.write(missionData), qint64(missionData.size()));
    QVERIFY(missionFile.flush());

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(!mavlink.uploadMissionFile(QUrl::fromLocalFile(missionFile.fileName())));
    QVERIFY(connectTestMavlink(mavlink, server));

    QTcpSocket *clientSocket = server.nextPendingConnection();
    QVERIFY(clientSocket != nullptr);

    QVERIFY(mavlink.resetHomeToUploadedLocation());
    const QVector<mavlink_command_long_t> commands = readCommands(clientSocket, MAV_CMD_DO_SET_HOME, 1);
    QCOMPARE(commands.size(), 1);

    const mavlink_command_long_t command = commands.first();
    QCOMPARE(command.target_system, uint8_t(1));
    QCOMPARE(command.target_component, uint8_t(MAV_COMP_ID_AUTOPILOT1));
    QCOMPARE(command.param1, 0.0f);
    QVERIFY(qAbs(command.param5 - 34.8077610f) < 0.0001f);
    QVERIFY(qAbs(command.param6 - 76.7980650f) < 0.0001f);
    QVERIFY(qAbs(command.param7 - 3407.88f) < 0.01f);
}

void MavlinkTest::writeLoadedMissionStartsMissionUpload()
{
    QTemporaryFile missionFile;
    QVERIFY(missionFile.open());

    const QByteArray missionData = QByteArrayLiteral(
        "QGC WPL 110\n"
        "0\t1\t0\t16\t0\t0\t0\t0\t34.8077610\t76.7980650\t3407.88\t1\n"
        "1\t0\t0\t22\t0\t0\t0\t0\t34.8077610\t76.7980650\t3437.88\t1\n"
        "2\t0\t0\t16\t0\t0\t0\t0\t34.8055995\t76.7814159\t4494.61\t1\n");
    QCOMPARE(missionFile.write(missionData), qint64(missionData.size()));
    QVERIFY(missionFile.flush());

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(!mavlink.uploadMissionFile(QUrl::fromLocalFile(missionFile.fileName())));
    QVERIFY(connectTestMavlink(mavlink, server));

    QTcpSocket *clientSocket = server.nextPendingConnection();
    QVERIFY(clientSocket != nullptr);

    QVERIFY(mavlink.writeLoadedMissionToVehicle());

    bool foundClearAll = false;
    const QVector<mavlink_message_t> messages = readMavlinkMessages(clientSocket, 3, 1600);
    for (const mavlink_message_t &message : messages) {
        if (message.msgid != MAVLINK_MSG_ID_MISSION_CLEAR_ALL) {
            continue;
        }

        mavlink_mission_clear_all_t clearAll = {};
        mavlink_msg_mission_clear_all_decode(&message, &clearAll);
        QCOMPARE(clearAll.target_system, uint8_t(1));
        QCOMPARE(clearAll.target_component, uint8_t(MAV_COMP_ID_AUTOPILOT1));
        foundClearAll = true;
        break;
    }

    QVERIFY(foundClearAll);
}

void MavlinkTest::missionUploadWaitsForSelectedVehicleClearAck()
{
    QTemporaryFile missionFile;
    QVERIFY(missionFile.open());
    const QByteArray missionData = QByteArrayLiteral(
        "QGC WPL 110\n"
        "0\t1\t0\t16\t0\t0\t0\t0\t34.8077610\t76.7980650\t3407.88\t1\n"
        "1\t0\t3\t16\t0\t0\t0\t0\t34.8078610\t76.7981650\t3437.88\t1\n");
    QCOMPARE(missionFile.write(missionData), qint64(missionData.size()));
    QVERIFY(missionFile.flush());

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(!mavlink.uploadMissionFile(QUrl::fromLocalFile(missionFile.fileName())));
    QVERIFY(connectTestMavlink(mavlink, server));
    QTcpSocket *peer = server.nextPendingConnection();
    QVERIFY(peer);

    QVERIFY(mavlink.writeLoadedMissionToVehicle());
    QCOMPARE(mavlink.missionTransferState(), Mavlink::ClearingBeforeUpload);

    mavlink_message_t foreignAck = {};
    mavlink_msg_mission_ack_pack(2, MAV_COMP_ID_AUTOPILOT1, &foreignAck,
                                 255, 0, MAV_MISSION_ACCEPTED, MAV_MISSION_TYPE_MISSION, 0);
    writeMavlinkMessage(peer, foreignAck);
    QTest::qWait(100);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::ClearingBeforeUpload);

    mavlink_message_t clearAck = {};
    mavlink_msg_mission_ack_pack(1, MAV_COMP_ID_AUTOPILOT1, &clearAck,
                                 255, 0, MAV_MISSION_ACCEPTED, MAV_MISSION_TYPE_MISSION, 0);
    writeMavlinkMessage(peer, clearAck);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Uploading);

    const auto messages = readMavlinkMessages(peer, 1);
    QVERIFY(std::any_of(messages.cbegin(), messages.cend(), [](const mavlink_message_t &message) {
        return message.msgid == MAVLINK_MSG_ID_MISSION_COUNT;
    }));
}

void MavlinkTest::missionUploadFaultInjection()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QTemporaryFile file;
    QVERIFY(loadOneWaypointMission(mavlink, file));
    QVERIFY(connectTestMavlink(mavlink, server));
    QTcpSocket *peer = server.nextPendingConnection(); QVERIFY(peer);

    QSignalSpy terminalSpy(&mavlink, &Mavlink::missionTransferFinished);
    QVERIFY(mavlink.writeLoadedMissionToVehicle());
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Uploading);
    sendMissionRequestInt(peer, 1, MAV_COMP_ID_CAMERA, 0);
    QTest::qWait(20);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Uploading);

    mavlink.cancelMissionTransfer();
    QVERIFY(mavlink.writeLoadedMissionToVehicle());
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Uploading);
    terminalSpy.clear();
    sendMissionRequestInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 7);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Failed);
    QCOMPARE(mavlink.missionTransferErrorCode(), QStringLiteral("invalid-request"));
    QCOMPARE(terminalSpy.count(), 1);

    QVERIFY(mavlink.writeLoadedMissionToVehicle());
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Uploading);
    readMavlinkMessages(peer, 1);
    terminalSpy.clear();
    fireMissionTransferTimeout(mavlink);
    fireMissionTransferTimeout(mavlink);
    fireMissionTransferTimeout(mavlink);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Uploading);
    const QVector<mavlink_message_t> retryMessages = readMavlinkMessages(peer, 3);
    int retryCounts = 0;
    for (const auto &message : retryMessages) if (message.msgid == MAVLINK_MSG_ID_MISSION_COUNT) ++retryCounts;
    QCOMPARE(retryCounts, 3);
    fireMissionTransferTimeout(mavlink);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Failed);
    QCOMPARE(mavlink.missionTransferErrorCode(), QStringLiteral("timeout"));
    QCOMPARE(terminalSpy.count(), 1);

    QVERIFY(mavlink.writeLoadedMissionToVehicle());
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Uploading);
    terminalSpy.clear();
    sendMissionRequestInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0);
    sendMissionRequestInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0);
    QTRY_COMPARE(mavlink.missionTransferProgress(), 1);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Uploading);
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Completed);
    QCOMPARE(terminalSpy.count(), 1);
}

void MavlinkTest::missionUploadIgnoresUnrelatedMissionTypeRequests()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QTemporaryFile file;
    QVERIFY(loadOneWaypointMission(mavlink, file));
    QVERIFY(connectTestMavlink(mavlink, server));
    QTcpSocket *peer = server.nextPendingConnection(); QVERIFY(peer);

    QSignalSpy terminalSpy(&mavlink, &Mavlink::missionTransferFinished);
    QVERIFY(mavlink.writeLoadedMissionToVehicle());
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Uploading);
    readMavlinkMessages(peer, 1); // Consume MISSION_COUNT.

    const quint64 transferId = mavlink.missionTransferId();
    const int progress = mavlink.missionTransferProgress();
    const int total = mavlink.missionTransferTotal();

    // Legacy fence and INT rally requests from the selected autopilot belong
    // to different MAVLink mission transactions and must be ignored.
    sendMissionRequest(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0, MAV_MISSION_TYPE_FENCE);
    sendMissionRequestInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0, MAV_MISSION_TYPE_RALLY);
    // Source matching remains strict independently of mission type.
    sendMissionRequest(peer, 1, MAV_COMP_ID_CAMERA, 0);
    sendMissionRequestInt(peer, 2, MAV_COMP_ID_AUTOPILOT1, 0);
    // Unrelated ACKs keep their existing ignored behavior.
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED, MAV_MISSION_TYPE_FENCE);
    QTest::qWait(40);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Uploading);
    QCOMPARE(mavlink.missionTransferId(), transferId);
    QCOMPARE(mavlink.missionTransferProgress(), progress);
    QCOMPARE(mavlink.missionTransferTotal(), total);
    QCOMPARE(terminalSpy.count(), 0);
    const QVector<mavlink_message_t> ignoredResponses = readMavlinkMessages(peer, 1, 60);
    QVERIFY(std::none_of(ignoredResponses.cbegin(), ignoredResponses.cend(), [](const mavlink_message_t &message) {
        return message.msgid == MAVLINK_MSG_ID_MISSION_ITEM || message.msgid == MAVLINK_MSG_ID_MISSION_ITEM_INT;
    }));

    // A valid request for the active normal mission still advances upload.
    sendMissionRequest(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0);
    QTRY_COMPARE(mavlink.missionTransferProgress(), 1);
    const QVector<mavlink_message_t> itemResponses = readMavlinkMessages(peer, 1);
    QVERIFY(std::any_of(itemResponses.cbegin(), itemResponses.cend(), [](const mavlink_message_t &message) {
        return message.msgid == MAVLINK_MSG_ID_MISSION_ITEM;
    }));
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Completed);
    QCOMPARE(terminalSpy.count(), 1);

    // Expected-type requests retain strict sequence validation and one
    // terminal failure result.
    QVERIFY(mavlink.writeLoadedMissionToVehicle());
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Uploading);
    terminalSpy.clear();
    sendMissionRequestInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 9);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Failed);
    QCOMPARE(mavlink.missionTransferErrorCode(), QStringLiteral("invalid-request"));
    QCOMPARE(terminalSpy.count(), 1);
}

void MavlinkTest::missionDownloadFaultInjection()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(connectTestMavlink(mavlink, server));
    QTcpSocket *peer = server.nextPendingConnection(); QVERIFY(peer);

    QSignalSpy terminalSpy(&mavlink, &Mavlink::missionTransferFinished);
    mavlink.downloadMissionFromVehicle();
    sendMissionCount(peer, 1, MAV_COMP_ID_AUTOPILOT1, 1);
    QTRY_COMPARE(mavlink.missionTransferTotal(), 1);
    readMavlinkMessages(peer, 1);
    fireMissionTransferTimeout(mavlink);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Downloading);
    const auto retry = readMavlinkMessages(peer, 1);
    QVERIFY(std::any_of(retry.cbegin(), retry.cend(), [](const mavlink_message_t &message) {
        return message.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_INT;
    }));
    sendMissionItemInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Completed);
    QCOMPARE(terminalSpy.count(), 1);

    mavlink.downloadMissionFromVehicle();
    sendMissionCount(peer, 1, MAV_COMP_ID_AUTOPILOT1, 2);
    QTRY_COMPARE(mavlink.missionTransferTotal(), 2);
    terminalSpy.clear();
    sendMissionItemInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 1);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Failed);
    QCOMPARE(mavlink.missionTransferErrorCode(), QStringLiteral("unexpected-sequence"));
    QCOMPARE(terminalSpy.count(), 1);

    mavlink.downloadMissionFromVehicle();
    sendMissionCount(peer, 1, MAV_COMP_ID_AUTOPILOT1, 2);
    sendMissionItemInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0);
    QTRY_COMPARE(mavlink.missionTransferProgress(), 1);
    terminalSpy.clear();
    sendMissionItemInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Failed);
    QCOMPARE(mavlink.missionTransferErrorCode(), QStringLiteral("duplicate-sequence"));
    QCOMPARE(terminalSpy.count(), 1);

    mavlink.downloadMissionFromVehicle();
    terminalSpy.clear();
    sendMissionCount(peer, 1, MAV_COMP_ID_CAMERA, 1);
    sendMissionCount(peer, 1, MAV_COMP_ID_AUTOPILOT1, 1, MAV_MISSION_TYPE_FENCE);
    QTest::qWait(20);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Downloading);
    QCOMPARE(mavlink.missionTransferTotal(), 0);
    readMavlinkMessages(peer, 1);
    for (int retryNumber = 0; retryNumber < 3; ++retryNumber) fireMissionTransferTimeout(mavlink);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Downloading);
    fireMissionTransferTimeout(mavlink);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Failed);
    QCOMPARE(mavlink.missionTransferErrorCode(), QStringLiteral("timeout"));
    QCOMPARE(terminalSpy.count(), 1);
}

void MavlinkTest::missionClearFaultInjection()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(connectTestMavlink(mavlink, server));
    QTcpSocket *peer = server.nextPendingConnection(); QVERIFY(peer);
    QSignalSpy terminalSpy(&mavlink, &Mavlink::missionTransferFinished);

    mavlink.clearMission();
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Clearing);
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_DENIED);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Failed);
    QCOMPARE(mavlink.missionTransferErrorCode(), QStringLiteral("clear-rejected"));
    QCOMPARE(terminalSpy.count(), 1);

    mavlink.clearMission();
    terminalSpy.clear();
    readMavlinkMessages(peer, 1);
    for (int retryNumber = 0; retryNumber < 3; ++retryNumber) fireMissionTransferTimeout(mavlink);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Clearing);
    const auto retries = readMavlinkMessages(peer, 3);
    int clearCount = 0;
    for (const auto &message : retries) if (message.msgid == MAVLINK_MSG_ID_MISSION_CLEAR_ALL) ++clearCount;
    QCOMPARE(clearCount, 3);
    fireMissionTransferTimeout(mavlink);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Failed);
    QCOMPARE(mavlink.missionTransferErrorCode(), QStringLiteral("timeout"));
    QCOMPARE(terminalSpy.count(), 1);
}

void MavlinkTest::missionCancellationAndLateMessages()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QTemporaryFile file; QVERIFY(loadOneWaypointMission(mavlink, file));
    QVERIFY(connectTestMavlink(mavlink, server));
    QTcpSocket *peer = server.nextPendingConnection(); QVERIFY(peer);
    QSignalSpy terminalSpy(&mavlink, &Mavlink::missionTransferFinished);

    QVERIFY(mavlink.writeLoadedMissionToVehicle());
    mavlink.cancelMissionTransfer();
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Cancelled);
    QCOMPARE(terminalSpy.count(), 1);
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED);
    QTest::qWait(20);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Cancelled);
    QCOMPARE(terminalSpy.count(), 1);

    mavlink.downloadMissionFromVehicle();
    mavlink.cancelMissionTransfer();
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Cancelled);
    QCOMPARE(terminalSpy.count(), 2);
    sendMissionCount(peer, 1, MAV_COMP_ID_AUTOPILOT1, 1);
    sendMissionItemInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0);
    QTest::qWait(20);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Cancelled);
    QCOMPARE(terminalSpy.count(), 2);

    mavlink.downloadMissionFromVehicle();
    sendMissionCount(peer, 1, MAV_COMP_ID_AUTOPILOT1, 1);
    sendMissionItemInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Completed);
    const int completedTerminalCount = terminalSpy.count();
    sendMissionItemInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0);
    QTest::qWait(20);
    QCOMPARE(mavlink.missionTransferState(), Mavlink::Completed);
    QCOMPARE(terminalSpy.count(), completedTerminalCount);
}

void MavlinkTest::missionDisconnectAndVehicleSwitchCancellation()
{
    // Disconnect terminates each active transfer state once.
    {
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        Mavlink mavlink; mavlink.setTelemetryLoggingEnabled(false);
        QTemporaryFile file; QVERIFY(loadOneWaypointMission(mavlink, file));
        QVERIFY(connectTestMavlink(mavlink, server));
        QTcpSocket *peer = server.nextPendingConnection(); QVERIFY(peer);
        QSignalSpy terminalSpy(&mavlink, &Mavlink::missionTransferFinished);
        QVERIFY(mavlink.writeLoadedMissionToVehicle());
        QCOMPARE(mavlink.missionTransferState(), Mavlink::ClearingBeforeUpload);
        mavlink.disconnectFromVehicle();
        QCOMPARE(mavlink.missionTransferState(), Mavlink::Cancelled);
        QCOMPARE(terminalSpy.count(), 1);
    }
    {
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        Mavlink mavlink; mavlink.setTelemetryLoggingEnabled(false);
        QTemporaryFile file; QVERIFY(loadOneWaypointMission(mavlink, file));
        QVERIFY(connectTestMavlink(mavlink, server));
        QTcpSocket *peer = server.nextPendingConnection(); QVERIFY(peer);
        QSignalSpy terminalSpy(&mavlink, &Mavlink::missionTransferFinished);
        QVERIFY(mavlink.writeLoadedMissionToVehicle());
        sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED);
        QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Uploading);
        mavlink.disconnectFromVehicle();
        QCOMPARE(mavlink.missionTransferState(), Mavlink::Cancelled);
        QCOMPARE(terminalSpy.count(), 1);
    }
    {
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        Mavlink mavlink; mavlink.setTelemetryLoggingEnabled(false);
        QVERIFY(connectTestMavlink(mavlink, server));
        QTcpSocket *peer = server.nextPendingConnection(); QVERIFY(peer); Q_UNUSED(peer)
        QSignalSpy terminalSpy(&mavlink, &Mavlink::missionTransferFinished);
        mavlink.downloadMissionFromVehicle();
        mavlink.disconnectFromVehicle();
        QCOMPARE(mavlink.missionTransferState(), Mavlink::Cancelled);
        QCOMPARE(terminalSpy.count(), 1);
    }
    {
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        Mavlink mavlink; mavlink.setTelemetryLoggingEnabled(false);
        QVERIFY(connectTestMavlink(mavlink, server));
        QTcpSocket *peer = server.nextPendingConnection(); QVERIFY(peer); Q_UNUSED(peer)
        QSignalSpy terminalSpy(&mavlink, &Mavlink::missionTransferFinished);
        mavlink.clearMission();
        mavlink.disconnectFromVehicle();
        QCOMPARE(mavlink.missionTransferState(), Mavlink::Cancelled);
        QCOMPARE(terminalSpy.count(), 1);
    }

    // Replacing the transport/selected vehicle cancels old work before a
    // second source can become active.
    QTcpServer firstServer; QTcpServer secondServer;
    QVERIFY(firstServer.listen(QHostAddress::LocalHost, 0));
    QVERIFY(secondServer.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink; mavlink.setTelemetryLoggingEnabled(false);
    QTemporaryFile file; QVERIFY(loadOneWaypointMission(mavlink, file));
    QVERIFY(connectTestMavlink(mavlink, firstServer, 1000, 1));
    QTcpSocket *firstPeer = firstServer.nextPendingConnection(); QVERIFY(firstPeer);
    QSignalSpy terminalSpy(&mavlink, &Mavlink::missionTransferFinished);
    QVERIFY(mavlink.writeLoadedMissionToVehicle());
    QVERIFY(mavlink.connectToVehicle(QStringLiteral("127.0.0.1"), secondServer.serverPort()));
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Cancelled);
    QCOMPARE(terminalSpy.count(), 1);
    QTRY_VERIFY(secondServer.hasPendingConnections());
}

void MavlinkTest::missionCompletionAndCanonicalModelFaults()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(connectTestMavlink(mavlink, server));
    QTcpSocket *peer = server.nextPendingConnection(); QVERIFY(peer);

    MissionPlanModel model;
    MissionPlanItem item;
    item.id = QUuid::createUuid(); item.latitude = 10.0; item.longitude = 20.0; item.altitude = 30.0f;
    QVERIFY(model.insertMissionItem(item));
    model.markCurrentRevisionSaved();
    mavlink.setMissionPlanModel(&model);
    QSignalSpy terminalSpy(&mavlink, &Mavlink::missionTransferFinished);
    QVERIFY(mavlink.uploadMissionPlan(&model));
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Uploading);
    MissionPlanItem changed = *model.missionItemAt(0);
    changed.altitude = 40.0f;
    QVERIFY(model.updateMissionItem(changed.id, changed));
    QVERIFY(model.dirty());
    sendMissionAck(peer, 1, MAV_COMP_ID_AUTOPILOT1, MAV_MISSION_ACCEPTED);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Completed);
    QVERIFY(model.dirty());
    QCOMPARE(terminalSpy.count(), 1);

    // A completed download may not overwrite an unsaved local draft.  The
    // transfer reports the conversion failure and the canonical revision is
    // preserved for an explicit replacement decision.
    MissionPlanItem local = *model.missionItemAt(0);
    const quint64 localRevision = model.revision();
    terminalSpy.clear();
    mavlink.downloadMissionFromVehicle();
    sendMissionCount(peer, 1, MAV_COMP_ID_AUTOPILOT1, 1);
    sendMissionItemInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Failed);
    QCOMPARE(mavlink.missionTransferErrorCode(), QStringLiteral("model-import"));
    QCOMPARE(model.revision(), localRevision);
    QCOMPARE(model.missionItemAt(0)->id, local.id);
    QCOMPARE(terminalSpy.count(), 1);

    // A confirmed replacement commits only after the complete download is
    // received. The new vehicle plan becomes clean and retains raw fields.
    terminalSpy.clear();
    mavlink.downloadMissionFromVehicle(true);
    sendMissionCount(peer, 1, MAV_COMP_ID_AUTOPILOT1, 1);
    sendMissionItemInt(peer, 1, MAV_COMP_ID_AUTOPILOT1, 0);
    QTRY_COMPARE(mavlink.missionTransferState(), Mavlink::Completed);
    QCOMPARE(model.sourceState(), MissionPlanModel::DownloadedVehicleMission);
    QVERIFY(!model.dirty());
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.missionItemAt(0)->latitude, 10.0);
    QCOMPARE(terminalSpy.count(), 1);
}

void MavlinkTest::uploadedAbsoluteMissionTerrainProfileUsesMissionAltitudes()
{
    QTemporaryFile missionFile;
    QVERIFY(missionFile.open());

    const QByteArray missionData = QByteArrayLiteral(
        "QGC WPL 110\n"
        "0\t1\t0\t16\t0\t0\t0\t0\t28.9120147\t82.5073242\t1349.25\t1\n"
        "1\t0\t0\t22\t0\t0\t0\t0\t28.9120147\t82.5073242\t1399.25\t1\n"
        "2\t0\t0\t16\t0\t0\t0\t0\t28.9165816\t82.5144556\t2116.65\t1\n");
    QCOMPARE(missionFile.write(missionData), qint64(missionData.size()));
    QVERIFY(missionFile.flush());

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(!mavlink.uploadMissionFile(QUrl::fromLocalFile(missionFile.fileName())));

    const QVariantList waypoints = mavlink.missionWaypoints();
    QCOMPARE(waypoints.size(), 2);
    QCOMPARE(waypoints.first().toMap().value(QStringLiteral("frame")).toUInt(), uint(MAV_FRAME_GLOBAL));
    QVERIFY(qAbs(waypoints.first().toMap().value(QStringLiteral("alt")).toDouble() - 1399.25) < 0.01);
    QVERIFY(qAbs(waypoints.last().toMap().value(QStringLiteral("alt")).toDouble() - 2116.65) < 0.01);

    const QVariantList elevationWaypoints = mavlink.missionElevationWaypoints();
    QCOMPARE(elevationWaypoints.size(), 2);
    QVERIFY(qAbs(elevationWaypoints.first().toMap().value(QStringLiteral("alt")).toDouble() - 1399.25) < 0.01);
}

void MavlinkTest::uploadedRelativeMissionTerrainProfileConvertsPlanToAmsl()
{
    QTemporaryFile missionFile;
    QVERIFY(missionFile.open());

    const QByteArray missionData = QByteArrayLiteral(
        "QGC WPL 110\n"
        "0\t1\t0\t16\t0\t0\t0\t0\t10.99\t20.01\t100\t1\n"
        "1\t0\t3\t22\t0\t0\t0\t0\t10.99\t20.01\t30\t1\n"
        "2\t0\t3\t16\t0\t0\t0\t0\t10.01\t20.99\t60\t1\n");
    QCOMPARE(missionFile.write(missionData), qint64(missionData.size()));
    QVERIFY(missionFile.flush());

    QTemporaryDir terrainDirectory;
    QVERIFY(terrainDirectory.isValid());
    QFile terrainTile(QDir(terrainDirectory.path()).filePath(QStringLiteral("N10E020.hgt")));
    QVERIFY(terrainTile.open(QIODevice::WriteOnly));
    const QVector<qint16> samples = { 100, 200, 300, 400 };
    for (const qint16 sample : samples) {
        char bytes[2] = {};
        qToBigEndian(sample, bytes);
        QCOMPARE(terrainTile.write(bytes, sizeof(bytes)), qint64(sizeof(bytes)));
    }
    terrainTile.close();
    qputenv("BONV_TERRAIN_DIR", terrainDirectory.path().toLocal8Bit());

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QVERIFY(!mavlink.uploadMissionFile(QUrl::fromLocalFile(missionFile.fileName())));

    const QVariantList waypoints = mavlink.missionWaypoints();
    QCOMPARE(waypoints.size(), 2);
    QCOMPARE(waypoints.first().toMap().value(QStringLiteral("frame")).toUInt(), uint(MAV_FRAME_GLOBAL_RELATIVE_ALT));
    QCOMPARE(waypoints.first().toMap().value(QStringLiteral("alt")).toDouble(), 30.0);

    const QVariantList elevationWaypoints = mavlink.missionElevationWaypoints();
    QCOMPARE(elevationWaypoints.size(), 2);
    QCOMPARE(elevationWaypoints.first().toMap().value(QStringLiteral("frame")).toUInt(), uint(MAV_FRAME_GLOBAL_RELATIVE_ALT));

    const QVariantList profile = mavlink.missionTerrainProfile();
    QVERIFY(profile.size() >= 2);
    const QVariantMap first = profile.first().toMap();
    QVERIFY(!first.contains(QStringLiteral("alt")));
    QVERIFY(qAbs(first.value(QStringLiteral("terrain")).toDouble() - 103.0) < 0.001);
}
void MavlinkTest::landedStateRequestUsesSelectedSourceAndIgnoresOtherAutopilots()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    mavlink.setFlightControllerLoggingEnabled(false);
    QVERIFY(connectTestMavlink(mavlink, server, 1000, 42));
    QTcpSocket *peer = server.nextPendingConnection();
    QVERIFY(peer);
    QCOMPARE(mavlink.selectedSystemId(), 42);
    QCOMPARE(mavlink.selectedComponentId(), int(MAV_COMP_ID_AUTOPILOT1));
    const auto commands = readCommands(peer, MAV_CMD_SET_MESSAGE_INTERVAL, 2);
    bool landedRequest = false;
    bool rcRequest = false;
    for (const auto &command : commands) {
        QCOMPARE(command.target_system, uint8_t(42));
        QCOMPARE(command.target_component, uint8_t(MAV_COMP_ID_AUTOPILOT1));
        if (command.param1 == MAVLINK_MSG_ID_EXTENDED_SYS_STATE) {
            QCOMPARE(command.param2, 1000000.0f);
            landedRequest = true;
        }
        if (command.param1 == MAVLINK_MSG_ID_RC_CHANNELS) {
            QCOMPARE(command.param2, 20000.0f);
            rcRequest = true;
        }
    }
    QVERIFY(landedRequest);
    QVERIFY(rcRequest);
    QSignalSpy selectionSpy(&mavlink, &Mavlink::selectedVehicleChanged);
    QSignalSpy frames(&mavlink, &Mavlink::messageReceived);
    mavlink_message_t other = {};
    mavlink_msg_heartbeat_pack(7, 1, &other, MAV_TYPE_QUADROTOR,
                              MAV_AUTOPILOT_ARDUPILOTMEGA, 0, 0, MAV_STATE_ACTIVE);
    writeMavlinkMessage(peer, other);
    QTRY_VERIFY(frames.count() >= 1);
    QCOMPARE(mavlink.selectedSystemId(), 42);
    QCOMPARE(selectionSpy.count(), 0);
    mavlink.disconnectFromVehicle();
    QCOMPARE(mavlink.selectedSystemId(), 0);
    QCOMPARE(selectionSpy.count(), 1);
    QVERIFY(connectTestMavlink(mavlink, server, 1000, 7));
    QCOMPARE(mavlink.selectedSystemId(), 7);
}

QTEST_MAIN(MavlinkTest)

#include "MavlinkTest.moc"
