#include <QtTest/QtTest>

#include "mavlink/Mavlink.h"
#include "mavlink/MavlinkInspectorController.h"
#include "mavlink/MavlinkInspectorModel.h"
#include "mavlink/MavlinkMessageMetadata.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QClipboard>
#include <QGuiApplication>
#include <QtMath>

namespace {

QByteArray packetFor(const mavlink_message_t &message);

mavlink_message_t makeHeartbeat(quint8 systemId, quint8 componentId, quint8 sequence)
{
    mavlink_message_t message = {};
    mavlink_msg_heartbeat_pack(systemId, componentId, &message,
                               MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA,
                               MAV_MODE_FLAG_SAFETY_ARMED, 4, MAV_STATE_ACTIVE);
    Q_UNUSED(sequence)
    return message;
}

mavlink_message_t makeAttitude(quint8 systemId, quint8 componentId, quint8 sequence)
{
    mavlink_message_t message = {};
    mavlink_msg_attitude_pack(systemId, componentId, &message, 100, 1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f);
    Q_UNUSED(sequence)
    return message;
}

mavlink_message_t makeGlobalPositionInt(quint8 systemId, quint8 componentId, quint8 sequence)
{
    mavlink_message_t message = {};
    mavlink_msg_global_position_int_pack(systemId, componentId, &message, 100, 123456789, 234567890,
                                         10000, 5000, 100, 200, -50, 9000);
    Q_UNUSED(sequence)
    return message;
}

mavlink_message_t makeGpsRawInt(quint8 systemId, quint8 componentId, quint8 sequence)
{
    mavlink_message_t message = {};
    mavlink_msg_gps_raw_int_pack(systemId, componentId, &message, 100, 3, 123456789, 234567890,
                                 10000, 80, 90, 100, 9000, 12, 0, 0, 0, 0, 0, 0);
    Q_UNUSED(sequence)
    return message;
}

QByteArray mavlink1PacketFor(mavlink_message_t message, quint8 systemId, quint8 componentId, quint8 sequence)
{
    mavlink_status_t status = {};
    status.flags = MAVLINK_STATUS_FLAG_OUT_MAVLINK1;
    status.current_tx_seq = sequence;
    mavlink_finalize_message_buffer(&message, systemId, componentId, &status,
                                    MAVLINK_MSG_ID_HEARTBEAT_MIN_LEN,
                                    MAVLINK_MSG_ID_HEARTBEAT_LEN,
                                    MAVLINK_MSG_ID_HEARTBEAT_CRC);
    return packetFor(message);
}

QByteArray packetFor(const mavlink_message_t &message)
{
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN] = {};
    const int length = mavlink_msg_to_send_buffer(buffer, &message);
    return QByteArray(reinterpret_cast<const char *>(buffer), length);
}

MavlinkFrame frameFor(const mavlink_message_t &message, const QDateTime &timestamp,
                      const QString &link = QStringLiteral("test-link"))
{
    return MavlinkFrame(timestamp, link, message, packetFor(message));
}

QVariantMap fieldNamed(const QVariantMap &details, const QString &name)
{
    const QVariantList fields = details.value(QStringLiteral("fields")).toList();
    for (const QVariant &field : fields) {
        const QVariantMap map = field.toMap();
        if (map.value(QStringLiteral("name")).toString() == name) {
            return map;
        }
    }
    return {};
}

} // namespace

class MavlinkInspectorModelTest : public QObject
{
    Q_OBJECT

private slots:
    void observesFrameAndPreservesMetadata();
    void separatesMessageStreams();
    void calculatesRateAndClears();
    void observesActualParserRawBytes();
    void parserRecoversFromCorruptionAndPartialReads();
    void observesMultipleSystemIdsFromParser();
    void fiveSystemHighRateTrafficRemainsBounded();
    void separatesComponents();
    void exposesCommonDialectMetadata();
    void preservesMavlink1AndMavlink2RawFrames();
    void boundsStreamsAndBatchesModelRefresh();
    void exposesQmlFacingStateAndSelection();
    void decodesSelectedMessageFieldsGenerically();
    void filtersStreamsWithoutChangingInspectorState();
    void searchAndFilterOptionsFollowObservedTraffic();
    void filteringPreservesHiddenSelectionAndClearResetsOptions();
    void discoversAndGraphsNumericFieldsWithoutCrossContamination();
    void graphFollowsPauseAndClearSemantics();
    void graphHistoryIsBounded();
    void graphExposesFiniteOrderedPointsForConstantAndLargeValues();
    void graphsAttitudePositionAndTimestampFields();
    void rawPacketViewerPreservesWireBytesAndMetadata();
    void rawPacketViewerSeparatesSystemsAndComponents();
    void rawPacketHistoryRespectsPauseAndBound();
    void rawPacketClearAndFiltersPreserveSelection();
    void rawPacketCopyUsesDisplayedHex();
};

void MavlinkInspectorModelTest::observesFrameAndPreservesMetadata()
{
    MavlinkInspectorController controller;
    auto *model = qobject_cast<MavlinkInspectorModel *>(controller.model());
    QVERIFY(model);

    const mavlink_message_t heartbeat = makeHeartbeat(42, 17, 91);
    const MavlinkFrame frame = frameFor(heartbeat, QDateTime::currentDateTimeUtc());
    controller.observeFrame(frame);

    QTRY_COMPARE(model->rowCount(), 1);
    const QModelIndex index = model->index(0, 0);
    QCOMPARE(model->data(index, MavlinkInspectorModel::SystemIdRole).toUInt(), 42U);
    QCOMPARE(model->data(index, MavlinkInspectorModel::ComponentIdRole).toUInt(), 17U);
    QCOMPARE(model->data(index, MavlinkInspectorModel::MessageIdRole).toUInt(), quint32(MAVLINK_MSG_ID_HEARTBEAT));
    QCOMPARE(model->data(index, MavlinkInspectorModel::MessageNameRole).toString(), QStringLiteral("HEARTBEAT"));
    QCOMPARE(model->data(index, MavlinkInspectorModel::PacketSizeRole).toInt(), frame.frameLength());
    QCOMPARE(model->data(index, MavlinkInspectorModel::SequenceRole).toUInt(), heartbeat.seq);
    QCOMPARE(model->data(index, MavlinkInspectorModel::MavlinkVersionRole).toInt(), frame.mavlinkVersion());
    QCOMPARE(model->data(index, MavlinkInspectorModel::CountRole).toULongLong(), 1ULL);
    QCOMPARE(frame.rawPacket(), packetFor(heartbeat));

    const MavlinkMessageMetadata metadata = MavlinkMetadata::lookup(MAVLINK_MSG_ID_HEARTBEAT);
    QCOMPARE(metadata.name, QStringLiteral("HEARTBEAT"));
    QVERIFY(!metadata.fields.isEmpty());
}

void MavlinkInspectorModelTest::separatesMessageStreams()
{
    MavlinkInspectorController controller;
    auto *model = qobject_cast<MavlinkInspectorModel *>(controller.model());
    QVERIFY(model);
    const QDateTime now = QDateTime::currentDateTimeUtc();

    mavlink_message_t attitude = {};
    mavlink_msg_attitude_pack(1, 1, &attitude, 0, 0, 0, 0, 0, 0, 0);
    controller.observeFrame(frameFor(makeHeartbeat(1, 1, 1), now));
    controller.observeFrame(frameFor(makeHeartbeat(2, 1, 2), now));
    controller.observeFrame(frameFor(makeHeartbeat(1, 2, 3), now));
    controller.observeFrame(frameFor(attitude, now));

    QTRY_COMPARE(model->rowCount(), 4);
}

void MavlinkInspectorModelTest::calculatesRateAndClears()
{
    MavlinkInspectorController controller;
    auto *model = qobject_cast<MavlinkInspectorModel *>(controller.model());
    QVERIFY(model);
    const QDateTime start = QDateTime::currentDateTimeUtc();
    const mavlink_message_t heartbeat = makeHeartbeat(3, 4, 10);
    controller.observeFrame(frameFor(heartbeat, start));
    controller.observeFrame(frameFor(makeHeartbeat(3, 4, 11), start.addMSecs(1000)));

    QTRY_COMPARE(model->rowCount(), 1);
    const QModelIndex index = model->index(0, 0);
    QCOMPARE(model->data(index, MavlinkInspectorModel::CountRole).toULongLong(), 2ULL);
    QCOMPARE(model->data(index, MavlinkInspectorModel::RateHzRole).toDouble(), 1.0);
    QVERIFY(model->data(index, MavlinkInspectorModel::BytesPerSecondRole).toDouble() > 0.0);

    controller.clear();
    QCOMPARE(model->rowCount(), 0);
}

void MavlinkInspectorModelTest::observesActualParserRawBytes()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    MavlinkInspectorController controller(&mavlink);
    QSignalSpy frameSpy(&mavlink, &Mavlink::frameObserved);

    QVERIFY(mavlink.connectToVehicle(QStringLiteral("127.0.0.1"), server.serverPort()));
    QTRY_VERIFY(server.hasPendingConnections());
    QTcpSocket *socket = server.nextPendingConnection();
    QVERIFY(socket);

    mavlink_message_t heartbeat = makeHeartbeat(9, 10, 77);
    const QByteArray raw = packetFor(heartbeat);
    QCOMPARE(socket->write(raw), qint64(raw.size()));
    QVERIFY(socket->flush());

    QTRY_COMPARE(frameSpy.count(), 1);
    const MavlinkFrame observed = qvariant_cast<MavlinkFrame>(frameSpy.takeFirst().at(0));
    QCOMPARE(observed.rawPacket(), raw);
    QCOMPARE(observed.systemId(), quint8(9));
    QCOMPARE(observed.componentId(), quint8(10));
    QCOMPARE(observed.sequence(), heartbeat.seq);
    QVERIFY(controller.selectStream(observed.linkIdentity(), observed.systemId(), observed.componentId(), observed.messageId()));
    QCOMPARE(controller.selectedRawPacket().value(QStringLiteral("hex")).toString(),
             QString::fromLatin1(raw.toHex(' ')).toUpper());
}

void MavlinkInspectorModelTest::parserRecoversFromCorruptionAndPartialReads()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    QSignalSpy frameSpy(&mavlink, &Mavlink::frameObserved);
    QVERIFY(mavlink.connectToVehicle(QStringLiteral("127.0.0.1"), server.serverPort()));
    QTRY_VERIFY(server.hasPendingConnections());
    QTcpSocket *socket = server.nextPendingConnection();
    QVERIFY(socket);

    // A complete MAVLink 2-shaped packet with an invalid checksum must not
    // appear at the observer boundary or contaminate the following frame.
    const QByteArray malformed = QByteArray::fromHex("fd0000000101010000000000");
    const mavlink_message_t validMessage = makeAttitude(55, 4, 7);
    const QByteArray valid = packetFor(validMessage);
    QCOMPARE(socket->write(malformed + valid.left(valid.size() / 2)),
             qint64(malformed.size() + valid.size() / 2));
    QVERIFY(socket->flush());
    QTest::qWait(25);
    QCOMPARE(frameSpy.count(), 0);

    QCOMPARE(socket->write(valid.mid(valid.size() / 2)), qint64(valid.size() - valid.size() / 2));
    QVERIFY(socket->flush());
    QTRY_COMPARE(frameSpy.count(), 1);
    const MavlinkFrame observed = qvariant_cast<MavlinkFrame>(frameSpy.takeFirst().at(0));
    QCOMPARE(observed.systemId(), quint8(55));
    QCOMPARE(observed.componentId(), quint8(4));
    QCOMPARE(observed.rawPacket(), valid);
}

void MavlinkInspectorModelTest::observesMultipleSystemIdsFromParser()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    auto *controller = qobject_cast<MavlinkInspectorController *>(mavlink.inspector());
    auto *model = controller ? qobject_cast<MavlinkInspectorModel *>(controller->model()) : nullptr;
    QVERIFY(model);

    QVERIFY(mavlink.connectToVehicle(QStringLiteral("127.0.0.1"), server.serverPort()));
    QTRY_VERIFY(server.hasPendingConnections());
    QTcpSocket *socket = server.nextPendingConnection();
    QVERIFY(socket);

    QByteArray packets;
    for (quint8 systemId = 1; systemId <= 5; ++systemId) {
        packets.append(packetFor(makeHeartbeat(systemId, 1, systemId)));
    }
    packets.append(packetFor(makeAttitude(1, 1, 10)));
    packets.append(packetFor(makeAttitude(2, 1, 11)));
    packets.append(packetFor(makeGlobalPositionInt(3, 1, 12)));
    packets.append(packetFor(makeGpsRawInt(4, 1, 13)));
    QCOMPARE(socket->write(packets), qint64(packets.size()));
    QVERIFY(socket->flush());

    QTRY_COMPARE(model->rowCount(), 9);
    for (quint8 systemId = 1; systemId <= 5; ++systemId) {
        bool foundHeartbeat = false;
        for (int row = 0; row < model->rowCount(); ++row) {
            const QModelIndex index = model->index(row, 0);
            foundHeartbeat = foundHeartbeat || (model->data(index, MavlinkInspectorModel::SystemIdRole).toUInt() == systemId
                && model->data(index, MavlinkInspectorModel::ComponentIdRole).toUInt() == 1U
                && model->data(index, MavlinkInspectorModel::MessageIdRole).toUInt() == MAVLINK_MSG_ID_HEARTBEAT);
        }
        QVERIFY(foundHeartbeat);
    }
}

void MavlinkInspectorModelTest::fiveSystemHighRateTrafficRemainsBounded()
{
    MavlinkInspectorController controller;
    const QDateTime start = QDateTime::currentDateTimeUtc();
    for (quint8 systemId = 1; systemId <= 5; ++systemId) {
        controller.observeFrame(frameFor(makeAttitude(systemId, 1, systemId), start,
                                         QStringLiteral("stress-link")));
    }
    QTRY_COMPARE(controller.streamCount(), 5);
    QVERIFY(controller.selectStream(QStringLiteral("stress-link"), 3, 1, MAVLINK_MSG_ID_ATTITUDE));
    QVERIFY(controller.selectGraphField(QStringLiteral("roll")));

    // 1,000 synthetic packets represent five SYSIDs at 50 Hz each. The
    // controller must aggregate them into five streams while retaining only
    // its fixed 500 raw-frame history and one bounded graph series.
    for (int sample = 0; sample < 200; ++sample) {
        for (quint8 systemId = 1; systemId <= 5; ++systemId) {
            mavlink_message_t message = {};
            mavlink_msg_attitude_pack(systemId, 1, &message, sample,
                                      static_cast<float>(sample), 0.0f, 0.0f,
                                      0.0f, 0.0f, 0.0f);
            controller.observeFrame(frameFor(message, start.addMSecs(sample * 20),
                                             QStringLiteral("stress-link")));
        }
    }

    QTRY_COMPARE(controller.streamCount(), 5);
    QCOMPARE(controller.rawPacketCount(), 500);
    QVERIFY(controller.graphModel()->sampleCount() <= MavlinkInspectorGraphModel::MaximumSamples);
    controller.setVehicleFilter(5);
    QTRY_COMPARE(controller.filteredStreamCount(), 1);
    controller.clearFilters();
    QTRY_COMPARE(controller.filteredStreamCount(), 5);
}

void MavlinkInspectorModelTest::separatesComponents()
{
    MavlinkInspectorController controller;
    auto *model = qobject_cast<MavlinkInspectorModel *>(controller.model());
    QVERIFY(model);
    const QDateTime now = QDateTime::currentDateTimeUtc();
    controller.observeFrame(frameFor(makeAttitude(1, 1, 10), now));
    controller.observeFrame(frameFor(makeAttitude(1, 2, 11), now));

    QTRY_COMPARE(model->rowCount(), 2);
    QCOMPARE(model->data(model->index(0, 0), MavlinkInspectorModel::ComponentIdRole).toUInt(), 1U);
    QCOMPARE(model->data(model->index(1, 0), MavlinkInspectorModel::ComponentIdRole).toUInt(), 2U);
}

void MavlinkInspectorModelTest::exposesCommonDialectMetadata()
{
    const QList<quint32> messageIds {
        MAVLINK_MSG_ID_HEARTBEAT,
        MAVLINK_MSG_ID_ATTITUDE,
        MAVLINK_MSG_ID_GLOBAL_POSITION_INT,
        MAVLINK_MSG_ID_GPS_RAW_INT,
    };
    const QStringList expectedNames {
        QStringLiteral("HEARTBEAT"), QStringLiteral("ATTITUDE"),
        QStringLiteral("GLOBAL_POSITION_INT"), QStringLiteral("GPS_RAW_INT"),
    };

    for (qsizetype index = 0; index < messageIds.size(); ++index) {
        const MavlinkMessageMetadata metadata = MavlinkMetadata::lookup(messageIds.at(index));
        QCOMPARE(metadata.messageId, messageIds.at(index));
        QCOMPARE(metadata.name, expectedNames.at(index));
        QVERIFY(!metadata.fields.isEmpty());
        for (const MavlinkFieldMetadata &field : metadata.fields) {
            QVERIFY(!field.name.isEmpty());
            QVERIFY(field.type > 0);
        }
    }
}

void MavlinkInspectorModelTest::preservesMavlink1AndMavlink2RawFrames()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    MavlinkInspectorController controller(&mavlink);
    QSignalSpy frameSpy(&mavlink, &Mavlink::frameObserved);
    QVERIFY(mavlink.connectToVehicle(QStringLiteral("127.0.0.1"), server.serverPort()));
    QTRY_VERIFY(server.hasPendingConnections());
    QTcpSocket *socket = server.nextPendingConnection();
    QVERIFY(socket);

    const QByteArray mavlink1 = mavlink1PacketFor(makeHeartbeat(21, 22, 31), 21, 22, 31);
    const mavlink_message_t mavlink2Message = makeHeartbeat(23, 24, 32);
    const QByteArray mavlink2 = packetFor(mavlink2Message);
    QCOMPARE(static_cast<quint8>(mavlink1.at(0)), quint8(MAVLINK_STX_MAVLINK1));
    QCOMPARE(static_cast<quint8>(mavlink2.at(0)), quint8(MAVLINK_STX));
    QCOMPARE(socket->write(mavlink1 + mavlink2), qint64(mavlink1.size() + mavlink2.size()));
    QVERIFY(socket->flush());

    QTRY_COMPARE(frameSpy.count(), 2);
    const MavlinkFrame first = qvariant_cast<MavlinkFrame>(frameSpy.at(0).at(0));
    const MavlinkFrame second = qvariant_cast<MavlinkFrame>(frameSpy.at(1).at(0));
    QCOMPARE(first.rawPacket(), mavlink1);
    QCOMPARE(second.rawPacket(), mavlink2);
    QCOMPARE(first.mavlinkVersion(), 1);
    QCOMPARE(second.mavlinkVersion(), 2);
    QCOMPARE(static_cast<quint8>(first.rawPacket().at(2)), quint8(31));
    QCOMPARE(first.systemId(), quint8(21));
    QCOMPARE(first.componentId(), quint8(22));
    QCOMPARE(second.sequence(), mavlink2Message.seq);
    QVERIFY(controller.selectStream(first.linkIdentity(), 21, 22, MAVLINK_MSG_ID_HEARTBEAT));
    QCOMPARE(controller.selectedRawPacket().value(QStringLiteral("mavlinkVersion")).toInt(), 1);
    QCOMPARE(controller.selectedRawPacket().value(QStringLiteral("hex")).toString(), QString::fromLatin1(mavlink1.toHex(' ')).toUpper());
    QVERIFY(controller.selectedRawPacket().value(QStringLiteral("formatted")).toString().startsWith(QStringLiteral("0000  FE")));
    QVERIFY(controller.selectStream(second.linkIdentity(), 23, 24, MAVLINK_MSG_ID_HEARTBEAT));
    QCOMPARE(controller.selectedRawPacket().value(QStringLiteral("mavlinkVersion")).toInt(), 2);
    QCOMPARE(controller.selectedRawPacket().value(QStringLiteral("hex")).toString(), QString::fromLatin1(mavlink2.toHex(' ')).toUpper());
}

void MavlinkInspectorModelTest::boundsStreamsAndBatchesModelRefresh()
{
    MavlinkInspectorController controller;
    auto *model = qobject_cast<MavlinkInspectorModel *>(controller.model());
    QVERIFY(model);
    QSignalSpy resetSpy(model, &QAbstractItemModel::modelReset);
    const QDateTime now = QDateTime::currentDateTimeUtc();

    for (int messageId = 0; messageId <= 4096; ++messageId) {
        mavlink_message_t message = makeHeartbeat(1, 1, static_cast<quint8>(messageId));
        message.msgid = static_cast<quint32>(messageId);
        controller.observeFrame(frameFor(message, now));
    }

    QTRY_COMPARE(model->rowCount(), 4096);
    QCOMPARE(resetSpy.count(), 1);
    controller.clear();
    QCOMPARE(model->rowCount(), 0);
    controller.observeFrame(frameFor(makeHeartbeat(1, 1, 99), now));
    QTRY_COMPARE(model->rowCount(), 1);
}

void MavlinkInspectorModelTest::exposesQmlFacingStateAndSelection()
{
    MavlinkInspectorController controller;
    const QDateTime start = QDateTime::currentDateTimeUtc();
    controller.observeFrame(frameFor(makeHeartbeat(1, 1, 10), start));
    controller.observeFrame(frameFor(makeHeartbeat(1, 1, 11), start.addMSecs(1000)));

    QTRY_COMPARE(controller.streamCount(), 1);
    QCOMPARE(controller.totalRxRate(), 1.0);
    QVERIFY(controller.totalRxBytesPerSecond() > 0.0);
    QVERIFY(controller.selectRow(0));
    QVERIFY(controller.hasSelection());
    const QVariantMap details = controller.selectedDetails();
    QCOMPARE(details.value(QStringLiteral("sysid")).toUInt(), 1U);
    QCOMPARE(details.value(QStringLiteral("compid")).toUInt(), 1U);
    QCOMPARE(details.value(QStringLiteral("messageId")).toUInt(), quint32(MAVLINK_MSG_ID_HEARTBEAT));

    controller.pause();
    QVERIFY(controller.paused());
    controller.observeFrame(frameFor(makeHeartbeat(2, 1, 12), start.addMSecs(2000)));
    QTest::qWait(120);
    QCOMPARE(controller.streamCount(), 1);
    controller.resume();
    QVERIFY(!controller.paused());
    controller.clear();
    QCOMPARE(controller.streamCount(), 0);
    QVERIFY(!controller.hasSelection());
    QVERIFY(controller.selectedDetails().isEmpty());
}

void MavlinkInspectorModelTest::decodesSelectedMessageFieldsGenerically()
{
    MavlinkInspectorController controller;
    const QDateTime now = QDateTime::currentDateTimeUtc();

    mavlink_message_t heartbeat = makeHeartbeat(1, 1, 1);
    controller.observeFrame(frameFor(heartbeat, now));
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_HEARTBEAT));
    QVariantMap details = controller.selectedDetails();
    QCOMPARE(fieldNamed(details, QStringLiteral("custom_mode")).value(QStringLiteral("value")).toUInt(), 4U);
    QCOMPARE(fieldNamed(details, QStringLiteral("type")).value(QStringLiteral("value")).toUInt(), uint(MAV_TYPE_QUADROTOR));

    const mavlink_message_t attitude = makeAttitude(1, 1, 2);
    controller.observeFrame(frameFor(attitude, now));
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_ATTITUDE));
    details = controller.selectedDetails();
    QCOMPARE(fieldNamed(details, QStringLiteral("time_boot_ms")).value(QStringLiteral("value")).toUInt(), 100U);
    QCOMPARE(fieldNamed(details, QStringLiteral("roll")).value(QStringLiteral("value")).toFloat(), 1.0f);
    QCOMPARE(fieldNamed(details, QStringLiteral("yawspeed")).value(QStringLiteral("value")).toFloat(), 0.3f);

    const mavlink_message_t position = makeGlobalPositionInt(1, 1, 3);
    controller.observeFrame(frameFor(position, now));
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_GLOBAL_POSITION_INT));
    details = controller.selectedDetails();
    QCOMPARE(fieldNamed(details, QStringLiteral("lat")).value(QStringLiteral("value")).toInt(), 123456789);
    QCOMPARE(fieldNamed(details, QStringLiteral("relative_alt")).value(QStringLiteral("value")).toInt(), 5000);
    QCOMPARE(fieldNamed(details, QStringLiteral("hdg")).value(QStringLiteral("value")).toUInt(), 9000U);

    const mavlink_message_t gps = makeGpsRawInt(1, 1, 4);
    controller.observeFrame(frameFor(gps, now));
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_GPS_RAW_INT));
    details = controller.selectedDetails();
    QCOMPARE(fieldNamed(details, QStringLiteral("fix_type")).value(QStringLiteral("value")).toUInt(), 3U);
    QCOMPARE(fieldNamed(details, QStringLiteral("satellites_visible")).value(QStringLiteral("value")).toUInt(), 12U);

    mavlink_message_t sysStatus = {};
    mavlink_msg_sys_status_pack(1, 1, &sysStatus, 1, 2, 3, 400, 12000, -230, 87, 6, 7, 8, 9, 10, 0, 0, 0, 0);
    controller.observeFrame(frameFor(sysStatus, now));
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_SYS_STATUS));
    details = controller.selectedDetails();
    QCOMPARE(fieldNamed(details, QStringLiteral("voltage_battery")).value(QStringLiteral("value")).toUInt(), 12000U);
    QCOMPARE(fieldNamed(details, QStringLiteral("current_battery")).value(QStringLiteral("value")).toInt(), -230);

    const uint16_t voltages[10] { 12000, 11900, 0, 0, 0, 0, 0, 0, 0, 0 };
    const uint16_t voltagesExt[4] { 0, 0, 0, 0 };
    mavlink_message_t battery = {};
    mavlink_msg_battery_status_pack(1, 1, &battery, 2, MAV_BATTERY_FUNCTION_ALL, MAV_BATTERY_TYPE_LIPO,
                                    2500, voltages, -100, 1000, 2000, 77, 300, MAV_BATTERY_CHARGE_STATE_OK,
                                    voltagesExt, MAV_BATTERY_MODE_UNKNOWN, 0);
    controller.observeFrame(frameFor(battery, now));
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_BATTERY_STATUS));
    details = controller.selectedDetails();
    const QVariantMap voltageArray = fieldNamed(details, QStringLiteral("voltages"));
    QCOMPARE(voltageArray.value(QStringLiteral("arrayLength")).toUInt(), 10U);
    QCOMPARE(voltageArray.value(QStringLiteral("value")).toList().at(1).toUInt(), 11900U);

    mavlink_message_t hud = {};
    mavlink_msg_vfr_hud_pack(1, 1, &hud, 12.5f, 10.0f, 270, 42, 123.0f, -1.5f);
    controller.observeFrame(frameFor(hud, now));
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_VFR_HUD));
    details = controller.selectedDetails();
    QCOMPARE(fieldNamed(details, QStringLiteral("heading")).value(QStringLiteral("value")).toInt(), 270);
    QCOMPARE(fieldNamed(details, QStringLiteral("climb")).value(QStringLiteral("value")).toFloat(), -1.5f);

    mavlink_message_t statusText = {};
    mavlink_msg_statustext_pack(1, 1, &statusText, MAV_SEVERITY_WARNING, "Motor warning", 0, 0);
    controller.observeFrame(frameFor(statusText, now));
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_STATUSTEXT));
    details = controller.selectedDetails();
    QCOMPARE(fieldNamed(details, QStringLiteral("text")).value(QStringLiteral("value")).toString(), QStringLiteral("Motor warning"));

    mavlink_message_t updatedAttitude = {};
    mavlink_msg_attitude_pack(2, 1, &updatedAttitude, 101, -2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    controller.observeFrame(frameFor(updatedAttitude, now));
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 2, 1, MAVLINK_MSG_ID_ATTITUDE));
    controller.observeFrame(frameFor(makeAttitude(1, 1, 5), now.addMSecs(100)));
    QCOMPARE(fieldNamed(controller.selectedDetails(), QStringLiteral("roll")).value(QStringLiteral("value")).toFloat(), -2.0f);
    mavlink_message_t refreshedAttitude = {};
    mavlink_msg_attitude_pack(2, 1, &refreshedAttitude, 102, -3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    controller.observeFrame(frameFor(refreshedAttitude, now.addMSecs(200)));
    // Selected field values are intentionally refreshed through the
    // controller's 10 Hz batch, not decoded on every received frame.
    QTRY_COMPARE(fieldNamed(controller.selectedDetails(), QStringLiteral("roll")).value(QStringLiteral("value")).toFloat(), -3.0f);
    controller.clear();
    QVERIFY(controller.selectedDetails().isEmpty());
}

void MavlinkInspectorModelTest::filtersStreamsWithoutChangingInspectorState()
{
    MavlinkInspectorController controller;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    controller.observeFrame(frameFor(makeHeartbeat(1, 1, 1), now, QStringLiteral("udp-a")));
    controller.observeFrame(frameFor(makeAttitude(2, 1, 2), now, QStringLiteral("udp-a")));
    controller.observeFrame(frameFor(makeAttitude(2, 50, 3), now, QStringLiteral("serial-b")));
    controller.observeFrame(frameFor(makeHeartbeat(3, 1, 4), now, QStringLiteral("serial-b")));
    QTRY_COMPARE(controller.streamCount(), 4);

    const auto *filtered = controller.filteredModel();
    QVERIFY(filtered);
    QTRY_COMPARE(filtered->rowCount(), 4);
    controller.setVehicleFilter(2);
    QTRY_COMPARE(filtered->rowCount(), 2);
    controller.setComponentFilter(1);
    QTRY_COMPARE(filtered->rowCount(), 1);
    controller.setMessageFilter(MAVLINK_MSG_ID_ATTITUDE);
    QTRY_COMPARE(filtered->rowCount(), 1);
    controller.setLinkFilter(QStringLiteral("udp-a"));
    QTRY_COMPARE(filtered->rowCount(), 1);

    // Filtering is only a proxy view: the source statistics and streams stay intact.
    QCOMPARE(controller.streamCount(), 4);
    QCOMPARE(qobject_cast<MavlinkInspectorModel *>(controller.model())->rowCount(), 4);
    controller.clearFilters();
    QTRY_COMPARE(filtered->rowCount(), 4);
}

void MavlinkInspectorModelTest::searchAndFilterOptionsFollowObservedTraffic()
{
    MavlinkInspectorController controller;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    controller.observeFrame(frameFor(makeAttitude(3, 50, 1), now, QStringLiteral("link-3")));
    controller.observeFrame(frameFor(makeHeartbeat(4, 191, 2), now, QStringLiteral("link-4")));
    QTRY_COMPARE(controller.streamCount(), 2);

    const QVariantList vehicles = controller.vehicleFilterOptions();
    const QVariantList components = controller.componentFilterOptions();
    const QVariantList messages = controller.messageFilterOptions();
    const QVariantList links = controller.linkFilterOptions();
    QCOMPARE(vehicles.size(), 3); // ALL, SYSID 3, SYSID 4
    QCOMPARE(components.size(), 3); // ALL, COMP 50, COMP 191
    QCOMPARE(messages.size(), 3); // ALL, ATTITUDE, HEARTBEAT
    QCOMPARE(links.size(), 3); // ALL, two observed links

    controller.setSearchText(QStringLiteral("ATTITUDE"));
    QTRY_COMPARE(controller.filteredStreamCount(), 1);
    controller.setSearchText(QString::number(MAVLINK_MSG_ID_ATTITUDE));
    QTRY_COMPARE(controller.filteredStreamCount(), 1);
    controller.setSearchText(QStringLiteral("roll"));
    QTRY_COMPARE(controller.filteredStreamCount(), 1);
    controller.setSearchText(QStringLiteral("SYSID 3"));
    QTRY_COMPARE(controller.filteredStreamCount(), 1);
}

void MavlinkInspectorModelTest::filteringPreservesHiddenSelectionAndClearResetsOptions()
{
    MavlinkInspectorController controller;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    controller.observeFrame(frameFor(makeAttitude(3, 1, 1), now, QStringLiteral("test-link")));
    controller.observeFrame(frameFor(makeHeartbeat(4, 1, 2), now, QStringLiteral("test-link")));
    QTRY_COMPARE(controller.streamCount(), 2);
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 3, 1, MAVLINK_MSG_ID_ATTITUDE));
    controller.setVehicleFilter(4);
    QTRY_COMPARE(controller.filteredStreamCount(), 1);
    // A hidden row remains the inspector selection; filtering never selects another vehicle/stream.
    QVERIFY(controller.hasSelection());
    QCOMPARE(controller.selectedDetails().value(QStringLiteral("sysid")).toInt(), 3);

    controller.clearFilters();
    QTRY_COMPARE(controller.filteredStreamCount(), 2);
    controller.clear();
    QCOMPARE(controller.vehicleFilterOptions().size(), 1);
    QCOMPARE(controller.componentFilterOptions().size(), 1);
    QCOMPARE(controller.messageFilterOptions().size(), 1);
    QCOMPARE(controller.linkFilterOptions().size(), 1);
}

void MavlinkInspectorModelTest::discoversAndGraphsNumericFieldsWithoutCrossContamination()
{
    MavlinkInspectorController controller;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    controller.observeFrame(frameFor(makeAttitude(1, 1, 1), now));
    controller.observeFrame(frameFor(makeAttitude(2, 1, 2), now));
    mavlink_message_t text = {};
    mavlink_msg_statustext_pack(1, 1, &text, MAV_SEVERITY_INFO, "not graphable", 0, 0);
    controller.observeFrame(frameFor(text, now));
    QTRY_COMPARE(controller.streamCount(), 3);

    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_ATTITUDE));
    QStringList numericNames;
    for (const QVariant &field : controller.availableGraphFields()) numericNames.append(field.toMap().value(QStringLiteral("value")).toString());
    QVERIFY(numericNames.contains(QStringLiteral("roll")));
    QVERIFY(numericNames.contains(QStringLiteral("pitch")));
    QVERIFY(numericNames.contains(QStringLiteral("yaw")));
    QVERIFY(controller.selectGraphField(QStringLiteral("roll")));
    auto *graph = controller.graphModel();
    QCOMPARE(graph->systemId(), 1);
    QCOMPARE(graph->componentId(), 1);
    QCOMPARE(graph->messageId(), quint32(MAVLINK_MSG_ID_ATTITUDE));
    QCOMPARE(graph->fieldName(), QStringLiteral("roll"));
    QCOMPARE(graph->sampleCount(), 1);
    QCOMPARE(graph->latestValue(), 1.0);

    // Other SYSIDs and different numeric fields cannot feed the selected series.
    mavlink_message_t secondSystem = makeAttitude(2, 1, 3);
    mavlink_msg_attitude_pack(2, 1, &secondSystem, 200, 99.0f, 8.0f, 7.0f, 0, 0, 0);
    controller.observeFrame(frameFor(secondSystem, now.addMSecs(100)));
    QTest::qWait(130);
    QCOMPARE(graph->sampleCount(), 1);

    mavlink_message_t firstSystem = makeAttitude(1, 1, 4);
    mavlink_msg_attitude_pack(1, 1, &firstSystem, 300, -2.0f, 8.0f, 7.0f, 0, 0, 0);
    controller.observeFrame(frameFor(firstSystem, now.addMSecs(200)));
    QTRY_COMPARE(graph->sampleCount(), 2);
    QCOMPARE(graph->latestValue(), -2.0);

    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_STATUSTEXT));
    QStringList statusNumericNames;
    for (const QVariant &field : controller.availableGraphFields()) statusNumericNames.append(field.toMap().value(QStringLiteral("value")).toString());
    QVERIFY(statusNumericNames.contains(QStringLiteral("severity")));
    QVERIFY(!statusNumericNames.contains(QStringLiteral("text"))); // char arrays are never graphable.
}

void MavlinkInspectorModelTest::graphFollowsPauseAndClearSemantics()
{
    MavlinkInspectorController controller;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    controller.observeFrame(frameFor(makeAttitude(1, 2, 1), now));
    QTRY_COMPARE(controller.streamCount(), 1);
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 2, MAVLINK_MSG_ID_ATTITUDE));
    QVERIFY(controller.selectGraphField(QStringLiteral("roll")));
    auto *graph = controller.graphModel();
    QCOMPARE(graph->sampleCount(), 1);

    controller.pause();
    controller.observeFrame(frameFor(makeAttitude(1, 2, 2), now.addMSecs(100)));
    QTest::qWait(130);
    QCOMPARE(graph->sampleCount(), 1);
    controller.resume();
    controller.observeFrame(frameFor(makeAttitude(1, 2, 3), now.addMSecs(200)));
    QTRY_COMPARE(graph->sampleCount(), 2);

    controller.clearGraph();
    QCOMPARE(graph->sampleCount(), 0);
    QVERIFY(controller.streamCount() > 0);
    controller.clearFilters();
    QCOMPARE(graph->sampleCount(), 0);
    controller.clear();
    QVERIFY(!graph->active());
    QCOMPARE(graph->sampleCount(), 0);
}

void MavlinkInspectorModelTest::graphHistoryIsBounded()
{
    MavlinkInspectorGraphModel graph;
    graph.selectSeries(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_ATTITUDE,
                       QStringLiteral("ATTITUDE"), QStringLiteral("roll"));
    for (int value = 0; value < MavlinkInspectorGraphModel::MaximumSamples + 25; ++value) {
        graph.appendSample(value);
    }
    QCOMPARE(graph.sampleCount(), MavlinkInspectorGraphModel::MaximumSamples);
    QCOMPARE(graph.fieldName(), QStringLiteral("roll"));
    QVERIFY(graph.maximum() >= graph.minimum());
}

void MavlinkInspectorModelTest::graphExposesFiniteOrderedPointsForConstantAndLargeValues()
{
    MavlinkInspectorGraphModel graph;
    graph.selectSeries(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_GPS_RAW_INT,
                       QStringLiteral("GPS_RAW_INT"), QStringLiteral("time_usec"));
    graph.appendSample(1390096300000.0);
    QTest::qWait(2);
    graph.appendSample(1391756300000.0);

    const QVariantList largePoints = graph.points();
    QCOMPARE(largePoints.size(), 2);
    double previousTime = -1.0;
    for (const QVariant &pointValue : largePoints) {
        const QVariantList point = pointValue.toList();
        QCOMPARE(point.size(), 2);
        const double time = point.at(0).toDouble();
        const double value = point.at(1).toDouble();
        QVERIFY(qIsFinite(time));
        QVERIFY(qIsFinite(value));
        QVERIFY(time >= previousTime);
        previousTime = time;
    }
    QVERIFY(graph.maximum() > graph.minimum());

    graph.selectSeries(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_ATTITUDE,
                       QStringLiteral("ATTITUDE"), QStringLiteral("roll"));
    graph.appendSample(0.25);
    QTest::qWait(2);
    graph.appendSample(0.25);
    QCOMPARE(graph.points().size(), 2);
    QVERIFY(graph.maximum() > graph.minimum());
}

void MavlinkInspectorModelTest::graphsAttitudePositionAndTimestampFields()
{
    MavlinkInspectorController controller;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    controller.observeFrame(frameFor(makeAttitude(1, 1, 1), now));
    controller.observeFrame(frameFor(makeGlobalPositionInt(1, 1, 2), now));

    mavlink_message_t gps = {};
    mavlink_msg_gps_raw_int_pack(1, 1, &gps, 1391756300000ULL, 3, 123456789, 234567890,
                                 10000, 80, 90, 100, 9000, 12, 0, 0, 0, 0, 0, 0);
    controller.observeFrame(frameFor(gps, now));
    QTRY_COMPARE(controller.streamCount(), 3);

    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_ATTITUDE));
    QVERIFY(controller.selectGraphField(QStringLiteral("pitch")));
    QCOMPARE(controller.graphModel()->latestValue(), 2.0);
    QVERIFY(controller.selectGraphField(QStringLiteral("yaw")));
    QCOMPARE(controller.graphModel()->latestValue(), 3.0);

    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_GLOBAL_POSITION_INT));
    QVERIFY(controller.selectGraphField(QStringLiteral("relative_alt")));
    QCOMPARE(controller.graphModel()->latestValue(), 5000.0);

    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_GPS_RAW_INT));
    QVERIFY(controller.selectGraphField(QStringLiteral("time_usec")));
    QCOMPARE(controller.graphModel()->latestValue(), 1391756300000.0);
    QVERIFY(!controller.graphModel()->points().isEmpty());
}

void MavlinkInspectorModelTest::rawPacketViewerPreservesWireBytesAndMetadata()
{
    MavlinkInspectorController controller;
    mavlink_message_t message = makeAttitude(2, 1, 91);
    const QByteArray wire = packetFor(message);
    controller.observeFrame(MavlinkFrame(QDateTime::currentDateTimeUtc(), QStringLiteral("TCP"), message, wire));

    QVERIFY(controller.selectStream(QStringLiteral("TCP"), 2, 1, MAVLINK_MSG_ID_ATTITUDE));
    const QVariantMap raw = controller.selectedRawPacket();
    QCOMPARE(raw.value(QStringLiteral("link")).toString(), QStringLiteral("TCP"));
    QCOMPARE(raw.value(QStringLiteral("sysid")).toUInt(), 2U);
    QCOMPARE(raw.value(QStringLiteral("compid")).toUInt(), 1U);
    QCOMPARE(raw.value(QStringLiteral("messageId")).toUInt(), quint32(MAVLINK_MSG_ID_ATTITUDE));
    QCOMPARE(raw.value(QStringLiteral("messageName")).toString(), QStringLiteral("ATTITUDE"));
    QCOMPARE(raw.value(QStringLiteral("packetLength")).toInt(), wire.size());
    QCOMPARE(raw.value(QStringLiteral("mavlinkVersion")).toInt(), 2);
    QCOMPARE(raw.value(QStringLiteral("sequence")).toUInt(), message.seq);
    QCOMPARE(raw.value(QStringLiteral("hex")).toString(), QString::fromLatin1(wire.toHex(' ')).toUpper());
    QVERIFY(raw.value(QStringLiteral("formatted")).toString().startsWith(QStringLiteral("0000  FD")));
}

void MavlinkInspectorModelTest::rawPacketViewerSeparatesSystemsAndComponents()
{
    MavlinkInspectorController controller;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    mavlink_message_t one = makeAttitude(1, 1, 1);
    mavlink_message_t two = makeAttitude(2, 1, 2);
    mavlink_message_t componentTwo = makeAttitude(1, 2, 3);
    mavlink_msg_attitude_pack(1, 1, &one, 10, 1.0f, 0, 0, 0, 0, 0);
    mavlink_msg_attitude_pack(2, 1, &two, 20, 2.0f, 0, 0, 0, 0, 0);
    mavlink_msg_attitude_pack(1, 2, &componentTwo, 30, 3.0f, 0, 0, 0, 0, 0);
    const QByteArray oneWire = packetFor(one);
    const QByteArray twoWire = packetFor(two);
    const QByteArray componentTwoWire = packetFor(componentTwo);
    controller.observeFrame(MavlinkFrame(now, QStringLiteral("UDP"), one, oneWire));
    controller.observeFrame(MavlinkFrame(now, QStringLiteral("UDP"), two, twoWire));
    controller.observeFrame(MavlinkFrame(now, QStringLiteral("UDP"), componentTwo, componentTwoWire));

    QVERIFY(controller.selectStream(QStringLiteral("UDP"), 1, 1, MAVLINK_MSG_ID_ATTITUDE));
    QCOMPARE(controller.selectedRawPacket().value(QStringLiteral("hex")).toString(), QString::fromLatin1(oneWire.toHex(' ')).toUpper());
    QVERIFY(controller.selectStream(QStringLiteral("UDP"), 2, 1, MAVLINK_MSG_ID_ATTITUDE));
    QCOMPARE(controller.selectedRawPacket().value(QStringLiteral("hex")).toString(), QString::fromLatin1(twoWire.toHex(' ')).toUpper());
    QVERIFY(controller.selectStream(QStringLiteral("UDP"), 1, 2, MAVLINK_MSG_ID_ATTITUDE));
    QCOMPARE(controller.selectedRawPacket().value(QStringLiteral("hex")).toString(), QString::fromLatin1(componentTwoWire.toHex(' ')).toUpper());
}

void MavlinkInspectorModelTest::rawPacketHistoryRespectsPauseAndBound()
{
    MavlinkInspectorController controller;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (int index = 0; index < 501; ++index) {
        controller.observeFrame(frameFor(makeHeartbeat(1, 1, static_cast<quint8>(index)), now.addMSecs(index)));
    }
    QCOMPARE(controller.rawPacketCount(), 500);
    controller.pause();
    controller.observeFrame(frameFor(makeHeartbeat(1, 1, 9), now.addSecs(1)));
    QCOMPARE(controller.rawPacketCount(), 500);
    controller.resume();
    controller.observeFrame(frameFor(makeHeartbeat(1, 1, 10), now.addSecs(2)));
    QCOMPARE(controller.rawPacketCount(), 500);
}

void MavlinkInspectorModelTest::rawPacketClearAndFiltersPreserveSelection()
{
    MavlinkInspectorController controller;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    controller.observeFrame(frameFor(makeAttitude(2, 1, 1), now));
    controller.observeFrame(frameFor(makeAttitude(3, 1, 2), now));
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 2, 1, MAVLINK_MSG_ID_ATTITUDE));
    const QVariantMap selected = controller.selectedRawPacket();
    controller.setVehicleFilter(3);
    QCOMPARE(controller.selectedRawPacket().value(QStringLiteral("sysid")).toInt(), 2);
    QCOMPARE(controller.selectedRawPacket().value(QStringLiteral("hex")).toString(), selected.value(QStringLiteral("hex")).toString());
    controller.clearFilters();
    QCOMPARE(controller.selectedRawPacket().value(QStringLiteral("sysid")).toInt(), 2);
    controller.clear();
    QCOMPARE(controller.rawPacketCount(), 0);
    QVERIFY(!controller.hasRawPacket());
    QVERIFY(controller.selectedRawPacket().isEmpty());
}

void MavlinkInspectorModelTest::rawPacketCopyUsesDisplayedHex()
{
    MavlinkInspectorController controller;
    const mavlink_message_t message = makeHeartbeat(1, 1, 1);
    controller.observeFrame(frameFor(message, QDateTime::currentDateTimeUtc()));
    QVERIFY(controller.selectStream(QStringLiteral("test-link"), 1, 1, MAVLINK_MSG_ID_HEARTBEAT));
    const QString expected = controller.selectedRawPacket().value(QStringLiteral("hex")).toString();
    QVERIFY(controller.copySelectedRawHex());
    QCOMPARE(QGuiApplication::clipboard()->text(), expected);
}

QTEST_MAIN(MavlinkInspectorModelTest)

#include "MavlinkInspectorModelTest.moc"
