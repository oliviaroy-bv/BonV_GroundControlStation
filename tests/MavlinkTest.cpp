#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtEndian>

#include "mavlink/Mavlink.h"

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

bool connectTestMavlink(Mavlink &mavlink, QTcpServer &server, int timeoutMs = 1000)
{
    if (!mavlink.connectToVehicle(QStringLiteral("127.0.0.1"), server.serverPort())) {
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    while ((!mavlink.connected() || !server.hasPendingConnections()) && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }

    return mavlink.connected() && server.hasPendingConnections();
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

private slots:
    void defaultStateIsDisconnected();
    void vpsStatusAndCommandsUseConnectedLink();
    void disconnectWhileDisconnectedDoesNotEmit();
    void connectToVehicleUpdatesConnectionState();
    void receivedMavlinkBytesEmitMessage();
    void autopilotHeartbeatRequestsParameterList();
    void receivedParameterValuesUpdateCacheAndProgress();
    void telemetryLogCapturesTimestampedMavlinkFrames();
    void flightControllerLogCapturesStatusTextMessages();
    void disabledFlightControllerLogDoesNotCreateFile();
    void runAllMotorTestSendsFourDelayedMotorCommands();
    void uploadedMissionDisplaysOnlyNavigationWaypoints();
    void uploadedMissionStoresHomeLocation();
    void downloadedMissionStoresHomeLocation();
    void resetHomeToUploadedLocationSendsSetHomeCommand();
    void writeLoadedMissionStartsMissionUpload();
    void uploadedAbsoluteMissionTerrainProfileUsesMissionAltitudes();
    void uploadedRelativeMissionTerrainProfileConvertsPlanToAmsl();
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
    QVERIFY(connectTestMavlink(mavlink, server));

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
    const QVector<mavlink_message_t> messages = readMavlinkMessages(clientSocket, 2, 1600);
    for (const mavlink_message_t &message : messages) {
        if (message.msgid != MAVLINK_MSG_ID_PARAM_REQUEST_LIST) {
            continue;
        }

        mavlink_msg_param_request_list_decode(&message, &request);
        foundRequest = true;
        break;
    }

    QVERIFY(foundRequest);
    QCOMPARE(request.target_system, uint8_t(42));
    QCOMPARE(request.target_component, uint8_t(MAV_COMP_ID_AUTOPILOT1));
    QVERIFY(mavlink.parameterFetchInProgress());
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

    const QVariantMap frameClass = mavlink.parameterValue(QStringLiteral("FRAME_CLASS"));
    QCOMPARE(frameClass.value(QStringLiteral("value")).toDouble(), 1.0);
    QCOMPARE(frameClass.value(QStringLiteral("index")).toInt(), 0);
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
QTEST_MAIN(MavlinkTest)

#include "MavlinkTest.moc"
