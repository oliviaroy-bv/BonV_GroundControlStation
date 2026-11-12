#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include "mavlink/Mavlink.h"
#include "mavlink/MavlinkConsoleController.h"
#include "parameters/ParameterManager.h"

class MavlinkConsoleTest : public QObject
{
    Q_OBJECT
private:
    static void transmit(QTcpSocket *socket, mavlink_message_t &msg) {
        uint8_t data[MAVLINK_MAX_PACKET_LEN]{};
        const int size = mavlink_msg_to_send_buffer(data, &msg);
        socket->write(reinterpret_cast<char *>(data), size);
        socket->flush();
    }
    static mavlink_message_t heartbeat(int system, int firmware) {
        mavlink_message_t msg{};
        mavlink_msg_heartbeat_pack(uint8_t(system), 1, &msg, MAV_TYPE_QUADROTOR,
            uint8_t(firmware), 0, 0, MAV_STATE_ACTIVE);
        return msg;
    }
    static void reply(QTcpSocket *socket, const QByteArray &text, int system = 42,
                      int component = 1, int device = SERIAL_CONTROL_DEV_SHELL, int flags = SERIAL_CONTROL_FLAG_REPLY) {
        mavlink_message_t msg{};
        uint8_t data[70]{};
        memcpy(data, text.constData(), size_t(qMin(70, int(text.size()))));
        mavlink_msg_serial_control_pack(uint8_t(system), uint8_t(component), &msg,
            uint8_t(device), uint8_t(flags), 0, 0, uint8_t(qMin(70, int(text.size()))), data, 255, MAV_COMP_ID_MISSIONPLANNER);
        transmit(socket, msg);
    }
private slots:
    void streamAndLifecycle() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        Mavlink mavlink;
        auto *console = qobject_cast<MavlinkConsoleController *>(mavlink.console());
        QVERIFY(console);
        QVERIFY(!console->start());
        QVERIFY(mavlink.connectToVehicle("127.0.0.1", server.serverPort()));
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QTcpSocket> socket(server.nextPendingConnection());
        auto hb = heartbeat(42, MAV_AUTOPILOT_PX4);
        transmit(socket.data(), hb);
        QTRY_VERIFY(console->available());
        QCOMPARE(console->target(), QString("System 42 / Component 1"));
        QVERIFY(!console->running());
        QVERIFY(!console->send("help"));
        socket->readAll();
        QVERIFY(console->start());
        QVERIFY(!console->start());
        QTRY_VERIFY(socket->bytesAvailable() > 0);
        auto parseSerial = [](const QByteArray &bytes) {
            QList<mavlink_serial_control_t> result;
            mavlink_status_t state{};
            for (char byte : bytes) {
                mavlink_message_t msg{};
                if (mavlink_parse_char(MAVLINK_COMM_2, uint8_t(byte), &msg, &state)
                    && msg.msgid == MAVLINK_MSG_ID_SERIAL_CONTROL) {
                    mavlink_serial_control_t packet{};
                    mavlink_msg_serial_control_decode(&msg, &packet);
                    result.append(packet);
                }
            }
            return result;
        };
        auto packets = parseSerial(socket->readAll());
        QCOMPARE(packets.size(), 1);
        QCOMPARE(packets[0].device, uint8_t(SERIAL_CONTROL_DEV_SHELL));
        QCOMPARE(packets[0].target_system, uint8_t(42));
        QCOMPARE(packets[0].target_component, uint8_t(1));
        QCOMPARE(packets[0].flags, uint8_t(SERIAL_CONTROL_FLAG_EXCLUSIVE | SERIAL_CONTROL_FLAG_RESPOND));
        QCOMPARE(packets[0].count, uint8_t(1));
        QCOMPARE(packets[0].data[0], uint8_t('\n'));
        reply(socket.data(), "other", 43);
        reply(socket.data(), "other", 42, 2);
        reply(socket.data(), "other", 42, 1, SERIAL_CONTROL_DEV_TELEM1);
        reply(socket.data(), "other", 42, 1, SERIAL_CONTROL_DEV_SHELL, 0);
        reply(socket.data(), "nsh> ");
        QTRY_COMPARE(console->history().size(), 1);
        QCOMPARE(console->history()[0].toMap()["text"].toString(), QString("nsh> "));
        QVERIFY(console->send(QString(145, 'a')));
        QTRY_VERIFY(socket->bytesAvailable() >= 3 * 80);
        packets = parseSerial(socket->readAll());
        QCOMPARE(packets.size(), 3);
        QCOMPARE(packets[0].count, uint8_t(70));
        QCOMPARE(packets[1].count, uint8_t(70));
        QCOMPARE(packets[2].count, uint8_t(6));
        reply(socket.data(), "part");
        reply(socket.data(), "ial\r\n");
        reply(socket.data(), QByteArray::fromHex("e282"));
        reply(socket.data(), QByteArray::fromHex("ac"));
        QTRY_COMPARE(console->history().last().toMap()["text"].toString(), QString::fromUtf8("partial\n€"));
        // Repeated open/close cycles must not create additional message connections.
        for (int i = 0; i < 3; ++i) {
            console->stop();
            QVERIFY(console->start());
            console->clear();
            reply(socket.data(), "one");
            QTRY_COMPARE(console->history().size(), 1);
            QCOMPARE(console->history()[0].toMap()["text"].toString(), QString("one"));
        }
        for (int i = 0; i < 220; ++i) {
            QVERIFY(console->send("x"));
            reply(socket.data(), QByteArray(70, 'z'));
            QTest::qWait(1);
        }
        QVERIFY(console->history().size() <= 200);
        for (int i = 0; i < 250; ++i) reply(socket.data(), QByteArray(70, 'z'));
        QTest::qWait(100);
        QVERIFY(console->history().last().toMap()["text"].toString().size() <= 16384);
        mavlink.disconnectFromVehicle();
        QTRY_VERIFY(!console->available());
        QVERIFY(!console->running());
        QVERIFY(console->history().isEmpty());
        QVERIFY(!console->send("help"));
        socket.reset();
        QVERIFY(mavlink.connectToVehicle("127.0.0.1", server.serverPort()));
        QTRY_VERIFY(server.hasPendingConnections());
        socket.reset(server.nextPendingConnection());
        hb = heartbeat(77, MAV_AUTOPILOT_PX4);
        transmit(socket.data(), hb);
        QTRY_VERIFY(console->available());
        QCOMPARE(console->target(), QString("System 77 / Component 1"));
        QVERIFY(!console->running());
        QVERIFY(console->start());
        reply(socket.data(), "old vehicle", 42);
        reply(socket.data(), "new vehicle", 77);
        QTRY_COMPARE(console->history().size(), 1);
        QCOMPARE(console->history()[0].toMap()["text"].toString(), QString("new vehicle"));
    }
    void unsupportedAndTimeout() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        Mavlink mavlink;
        auto *console = qobject_cast<MavlinkConsoleController *>(mavlink.console());
        QVERIFY(mavlink.connectToVehicle("127.0.0.1", server.serverPort()));
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QTcpSocket> socket(server.nextPendingConnection());
        auto hb = heartbeat(42, MAV_AUTOPILOT_GENERIC);
        transmit(socket.data(), hb);
        QTRY_VERIFY(mavlink.connected());
        QVERIFY(!console->available());
        QVERIFY(console->status().contains("unknown"));
        QVERIFY(!console->start());
        hb = heartbeat(42, MAV_AUTOPILOT_PX4);
        transmit(socket.data(), hb);
        QTRY_VERIFY(console->available());
        QVERIFY(console->start());
        QTRY_VERIFY_WITH_TIMEOUT(!console->running(), 5000);
        QVERIFY(console->status().contains("no response"));
        QVERIFY(!console->send("help"));
    }
    void ardupilotCommands() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        Mavlink mavlink;
        auto *console = qobject_cast<MavlinkConsoleController *>(mavlink.console());
        QVERIFY(!console->send("arm"));
        QVERIFY(mavlink.connectToVehicle("127.0.0.1", server.serverPort()));
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QTcpSocket> socket(server.nextPendingConnection());
        auto hb = heartbeat(42, MAV_AUTOPILOT_ARDUPILOTMEGA);
        transmit(socket.data(), hb);
        QTimer heartbeatTimer;
        heartbeatTimer.setInterval(1000);
        connect(&heartbeatTimer, &QTimer::timeout, this, [&] { transmit(socket.data(), hb); });
        heartbeatTimer.start();
        QTRY_VERIFY(console->available());
        QVERIFY(console->commandMode());
        QCOMPARE(console->modeLabel(), QString("ArduPilot MAVLink Console"));
        QVERIFY(!console->start()); // ArduPilot never initializes an NSH session.
        auto read = [&]() {
            QTest::qWait(50);
            QList<mavlink_message_t> messages;
            mavlink_status_t status{};
            for (char c : socket->readAll()) {
                mavlink_message_t message{};
                if (mavlink_parse_char(MAVLINK_COMM_2, uint8_t(c), &message, &status)) messages.append(message);
            }
            return messages;
        };
        auto text = [&]() {
            QString result;
            for (const auto &entry : console->history()) result += entry.toMap()["text"].toString() + '\n';
            return result;
        };
        auto command = [&](int id) {
            mavlink_command_long_t result{};
            for (const auto &message : read()) {
                if (message.msgid != MAVLINK_MSG_ID_COMMAND_LONG) continue;
                mavlink_command_long_t candidate{};
                mavlink_msg_command_long_decode(&message, &candidate);
                if (candidate.command == id) result = candidate;
            }
            return result;
        };
        auto ack = [&](int id, int result, int system = 42, int component = 1, int recipient = 255) {
            mavlink_command_ack_t packet{};
            packet.command = uint16_t(id); packet.result = uint8_t(result); packet.target_system = uint8_t(recipient);
            mavlink_message_t message{};
            mavlink_msg_command_ack_encode(uint8_t(system), uint8_t(component), &message, &packet);
            transmit(socket.data(), message);
        };
        for (const auto &message : read()) {
            QVERIFY(message.msgid != MAVLINK_MSG_ID_SERIAL_CONTROL);
            QVERIFY(message.msgid != MAVLINK_MSG_ID_PARAM_SET);
            if (message.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
                mavlink_command_long_t packet{};
                mavlink_msg_command_long_decode(&message, &packet);
                QVERIFY(packet.command != MAV_CMD_COMPONENT_ARM_DISARM);
            }
        }
        QVERIFY(console->send("status"));
        auto packet = command(MAV_CMD_REQUEST_MESSAGE);
        QCOMPARE(packet.command, uint16_t(MAV_CMD_REQUEST_MESSAGE));
        QCOMPARE(packet.target_system, uint8_t(42));
        QCOMPARE(packet.param1, float(MAVLINK_MSG_ID_HEARTBEAT));
        transmit(socket.data(), hb);
        QTRY_VERIFY(text().contains("HEARTBEAT: DISARMED"));
        QVERIFY(console->send("gps status"));
        QCOMPARE(command(MAV_CMD_REQUEST_MESSAGE).param1, float(MAVLINK_MSG_ID_GPS_RAW_INT));
        mavlink_gps_raw_int_t gps{};
        gps.fix_type = 3; gps.satellites_visible = 12; gps.eph = 80;
        mavlink_message_t response{};
        mavlink_msg_gps_raw_int_encode(43, 1, &response, &gps);
        transmit(socket.data(), response);
        QTest::qWait(30);
        QVERIFY(!text().contains("GPS_RAW_INT:"));
        mavlink_msg_gps_raw_int_encode(42, 1, &response, &gps);
        transmit(socket.data(), response);
        QTRY_VERIFY(text().contains("satellites 12; HDOP 0.80"));
        QVERIFY(!console->armingAllowed());
        QVERIFY(console->send("arm"));
        QVERIFY(text().contains("Arming blocked"));
        for (const auto &message : read()) if (message.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
            mavlink_command_long_t candidate{};
            mavlink_msg_command_long_decode(&message, &candidate);
            QVERIFY(candidate.command != MAV_CMD_COMPONENT_ARM_DISARM);
        }
        console->setArmingAllowed(true);
        QVERIFY(console->send("arm"));
        packet = command(MAV_CMD_COMPONENT_ARM_DISARM);
        QCOMPARE(packet.command, uint16_t(MAV_CMD_COMPONENT_ARM_DISARM));
        QCOMPARE(packet.param1, 1.0f); QCOMPARE(packet.param2, 0.0f);
        ack(MAV_CMD_COMPONENT_ARM_DISARM, MAV_RESULT_DENIED, 43);
        ack(MAV_CMD_COMPONENT_ARM_DISARM, MAV_RESULT_DENIED, 42, 2);
        ack(MAV_CMD_COMPONENT_ARM_DISARM, MAV_RESULT_DENIED, 42, 1, 99);
        QTest::qWait(30);
        QVERIFY(!text().contains("COMMAND_ACK:"));
        ack(MAV_CMD_COMPONENT_ARM_DISARM, MAV_RESULT_IN_PROGRESS);
        QTRY_VERIFY(text().contains("IN_PROGRESS"));
        ack(MAV_CMD_COMPONENT_ARM_DISARM, MAV_RESULT_ACCEPTED);
        QTRY_VERIFY(text().contains("ACCEPTED"));
        QVERIFY(console->send("disarm"));
        QCOMPARE(command(MAV_CMD_COMPONENT_ARM_DISARM).param1, 0.0f);
        ack(MAV_CMD_COMPONENT_ARM_DISARM, MAV_RESULT_DENIED);
        QTRY_VERIFY(text().contains("DENIED"));
        QVERIFY(console->send("mode GUIDED"));
        bool setMode = false;
        for (const auto &message : read()) if (message.msgid == MAVLINK_MSG_ID_SET_MODE) {
            mavlink_set_mode_t mode{};
            mavlink_msg_set_mode_decode(&message, &mode);
            QCOMPARE(mode.target_system, uint8_t(42)); QCOMPARE(mode.custom_mode, uint32_t(4));
            setMode = true;
        }
        QVERIFY(setMode);
        mavlink_msg_heartbeat_pack(42, 1, &hb, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA,
                                  MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 4, MAV_STATE_ACTIVE);
        transmit(socket.data(), hb);
        QTRY_VERIFY(text().contains("requested mode observed"));
        QVERIFY(console->send("mode RTL"));
        QCOMPARE(command(MAV_CMD_NAV_RETURN_TO_LAUNCH).command, uint16_t(MAV_CMD_NAV_RETURN_TO_LAUNCH));
        ack(MAV_CMD_NAV_RETURN_TO_LAUNCH, MAV_RESULT_ACCEPTED);
        QTest::qWait(30);
        mavlink.requestParameters();
        QSignalSpy managedWrites(&mavlink, &Mavlink::parameterWriteFinished);
        QVERIFY(console->send("param get TEST_PARAM"));
        bool requested = false;
        for (const auto &message : read()) if (message.msgid == MAVLINK_MSG_ID_PARAM_REQUEST_READ) {
            mavlink_param_request_read_t parameter{};
            mavlink_msg_param_request_read_decode(&message, &parameter);
            if (QByteArray(parameter.param_id, 10) == "TEST_PARAM") {
                QCOMPARE(parameter.param_index, int16_t(-1));
                QCOMPARE(parameter.target_system, uint8_t(42));
                QCOMPARE(parameter.target_component, uint8_t(1)); requested = true;
            }
        }
        QVERIFY(requested);
        mavlink_param_value_t parameter{};
        memcpy(parameter.param_id, "TEST_PARAM", 10);
        parameter.param_value = 3; parameter.param_type = MAV_PARAM_TYPE_REAL32;
        parameter.param_count = 1; parameter.param_index = 0;
        mavlink_msg_param_value_encode(43, 1, &response, &parameter);
        transmit(socket.data(), response);
        QTest::qWait(30);
        QVERIFY(!text().contains("PARAM_VALUE:"));
        mavlink_msg_param_value_encode(42, 1, &response, &parameter);
        transmit(socket.data(), response);
        QTRY_VERIFY(text().contains("TEST_PARAM = 3"));
        QVERIFY(console->send("param set TEST_PARAM 12.5"));
        bool written = false;
        for (const auto &message : read()) if (message.msgid == MAVLINK_MSG_ID_PARAM_SET) {
            mavlink_param_set_t parameterSet{};
            mavlink_msg_param_set_decode(&message, &parameterSet);
            QCOMPARE(parameterSet.param_value, 12.5f); QCOMPARE(parameterSet.param_type, uint8_t(MAV_PARAM_TYPE_REAL32));
            written = true;
        }
        QVERIFY(written);
        QVERIFY(!text().contains("write confirmed")); // Local cache updates are not vehicle replies.
        parameter.param_value = 12.5f;
        mavlink_msg_param_value_encode(42, 1, &response, &parameter);
        transmit(socket.data(), response);
        QTRY_VERIFY(text().contains("write confirmed by vehicle"));
        QCOMPARE(managedWrites.size(), 1);
        QCOMPARE(managedWrites[0][0].toMap().value("outcome").toInt(), int(ParameterManager::Confirmed));
        parameter.param_type = MAV_PARAM_TYPE_INT16;
        mavlink_msg_param_value_encode(42, 1, &response, &parameter);
        transmit(socket.data(), response);
        QTest::qWait(30);
        // A type change invalidates the table; establish fresh confirmed metadata
        // before exercising the integer write path.
        mavlink.requestParameters();
        parameter.param_value = 3;
        mavlink_msg_param_value_encode(42, 1, &response, &parameter);
        transmit(socket.data(), response);
        QTRY_VERIFY(mavlink.parameterManager()->parametersReady());
        const QStringList invalid{"version", "gps", "mode AUTO", "arm extra", "param get BAD-NAME",
                                  "param set UNKNOWN 1", "param set TEST_PARAM nan", "param set TEST_PARAM inf",
                                  "param set TEST_PARAM 1e300", "param set TEST_PARAM 1.5", "param set TEST_PARAM 32768",
                                  "param get TOOLONGPARAMETERNAME", "arm\ndisarm"};
        read();
        for (const auto &input : invalid) QVERIFY(console->send(input));
        for (const auto &message : read()) {
            QVERIFY(message.msgid != MAVLINK_MSG_ID_SERIAL_CONTROL);
            QVERIFY(message.msgid != MAVLINK_MSG_ID_PARAM_SET);
            QVERIFY(message.msgid != MAVLINK_MSG_ID_SET_MODE);
            if (message.msgid == MAVLINK_MSG_ID_COMMAND_LONG) {
                mavlink_command_long_t candidate{}; mavlink_msg_command_long_decode(&message, &candidate);
                QVERIFY(candidate.command != MAV_CMD_COMPONENT_ARM_DISARM);
            }
        }
        QVERIFY(text().contains("Unsupported command"));
        managedWrites.clear(); // Invalid requests above also produce explicit managed outcomes.
        QVERIFY(console->send("param set TEST_PARAM 12"));
        read();
        parameter.param_value = 13;
        mavlink_msg_param_value_encode(42, 1, &response, &parameter);
        transmit(socket.data(), response);
        QTRY_VERIFY(text().contains("Parameter write Rejected"));
        QCOMPARE(managedWrites.size(), 1);
        QCOMPARE(managedWrites[0][0].toMap().value("outcome").toInt(), int(ParameterManager::Rejected));
        QCOMPARE(mavlink.parameterValue("TEST_PARAM").value("value").toDouble(), 13.);
        QVERIFY(console->send("gps status"));
        QTRY_VERIFY_WITH_TIMEOUT(text().contains("response timeout"), 5000);
        mavlink.disconnectFromVehicle();
        QVERIFY(!console->available());
        QVERIFY(!console->send("arm"));
        QVERIFY(console->history().isEmpty());
    }
};
QTEST_GUILESS_MAIN(MavlinkConsoleTest)
#include "MavlinkConsoleTest.moc"
