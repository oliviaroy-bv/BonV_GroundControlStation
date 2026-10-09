#include <QtTest/QtTest>

#include "mavlink/MavlinkForwarder.h"
#include "mavlink/Mavlink.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QElapsedTimer>

namespace {

MavlinkFrame frameFor(const QByteArray &bytes, quint8 systemId)
{
    mavlink_message_t message = {};
    message.magic = bytes.isEmpty() ? MAVLINK_STX : static_cast<quint8>(bytes.front());
    message.sysid = systemId;
    message.compid = 1;
    message.msgid = MAVLINK_MSG_ID_HEARTBEAT;
    return MavlinkFrame(QDateTime::currentDateTimeUtc(), QStringLiteral("test-link"), message, bytes);
}

QByteArray receiveDatagram(QUdpSocket &receiver)
{
    QElapsedTimer timer;
    timer.start();
    while (!receiver.hasPendingDatagrams() && timer.elapsed() < 1500) {
        receiver.waitForReadyRead(20);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    if (!receiver.hasPendingDatagrams()) {
        return {};
    }
    QByteArray result;
    result.resize(static_cast<int>(receiver.pendingDatagramSize()));
    receiver.readDatagram(result.data(), result.size());
    return result;
}

} // namespace

class MavlinkForwarderTest : public QObject
{
    Q_OBJECT

private slots:
    void disabledForwardingSendsNothing();
    void rejectsInvalidConfiguration();
    void preservesMavlink1Bytes();
    void preservesMavlink2AndSignatureBytes();
    void forwardsAllSystemIdsAndTracksCounters();
    void observesTheParserBoundaryWithoutInspectorDependency();
    void disableStopsNewDatagrams();
    void resetStatisticsClearsSessionCounters();
};

void MavlinkForwarderTest::disabledForwardingSendsNothing()
{
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
    MavlinkForwarder forwarder;
    QVERIFY(forwarder.apply(false, QStringLiteral("127.0.0.1"), receiver.localPort()));
    forwarder.forwardFrame(frameFor(QByteArray::fromHex("fe090101000000000000000000"), 1));
    QTest::qWait(100);
    QVERIFY(!receiver.hasPendingDatagrams());
    QCOMPARE(forwarder.forwardedPackets(), 0ULL);
}

void MavlinkForwarderTest::rejectsInvalidConfiguration()
{
    MavlinkForwarder forwarder;
    QVERIFY(!forwarder.apply(true, QString(), 14550));
    QCOMPARE(forwarder.state(), QStringLiteral("ERROR"));
    QVERIFY(!forwarder.enabled());
    QVERIFY(!forwarder.apply(true, QStringLiteral("localhost"), 0));
    QVERIFY(!forwarder.enabled());
    QVERIFY(forwarder.apply(true, QStringLiteral("127.0.0.1"), 14550));
    QVERIFY(forwarder.enabled());
    QVERIFY(!forwarder.apply(true, QString(), 14550));
    QVERIFY(!forwarder.enabled());
}

void MavlinkForwarderTest::preservesMavlink1Bytes()
{
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
    MavlinkForwarder forwarder;
    QVERIFY(forwarder.apply(true, QStringLiteral("127.0.0.1"), receiver.localPort()));
    const QByteArray original = QByteArray::fromHex("fe090701014d010000000300030e71");
    forwarder.forwardFrame(frameFor(original, 1));
    QCOMPARE(receiveDatagram(receiver), original);
}

void MavlinkForwarderTest::preservesMavlink2AndSignatureBytes()
{
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
    MavlinkForwarder forwarder;
    QVERIFY(forwarder.apply(true, QStringLiteral("127.0.0.1"), receiver.localPort()));
    // MAVLink 2 header followed by payload/checksum and all 13 signature bytes.
    const QByteArray original = QByteArray::fromHex("fd09010011010100000000000000030003aabb0102030405060708090a0b0c0d");
    forwarder.forwardFrame(frameFor(original, 77));
    QCOMPARE(receiveDatagram(receiver), original);
}

void MavlinkForwarderTest::forwardsAllSystemIdsAndTracksCounters()
{
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
    MavlinkForwarder forwarder;
    QVERIFY(forwarder.apply(true, QStringLiteral("127.0.0.1"), receiver.localPort()));
    quint64 expectedBytes = 0;
    for (quint8 systemId = 1; systemId <= 5; ++systemId) {
        const QByteArray packet = QByteArray::fromHex("fd0200000101") + QByteArray(1, static_cast<char>(systemId)) + QByteArray::fromHex("000000aabbcc");
        expectedBytes += static_cast<quint64>(packet.size());
        forwarder.forwardFrame(frameFor(packet, systemId));
        QCOMPARE(receiveDatagram(receiver), packet);
    }
    QCOMPARE(forwarder.forwardedPackets(), 5ULL);
    QCOMPARE(forwarder.forwardedBytes(), expectedBytes);
    QCOMPARE(forwarder.failedPackets(), 0ULL);
    QVERIFY(forwarder.lastForwardedPacketTime().isValid());
}

void MavlinkForwarderTest::observesTheParserBoundaryWithoutInspectorDependency()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));

    Mavlink mavlink;
    mavlink.setTelemetryLoggingEnabled(false);
    auto *forwarder = qobject_cast<MavlinkForwarder *>(mavlink.forwarder());
    QVERIFY(forwarder);
    QVERIFY(forwarder->apply(true, QStringLiteral("127.0.0.1"), receiver.localPort()));
    QVERIFY(mavlink.connectToVehicle(QStringLiteral("127.0.0.1"), server.serverPort()));
    QTRY_VERIFY(server.hasPendingConnections());
    QTcpSocket *source = server.nextPendingConnection();
    QVERIFY(source);

    mavlink_message_t heartbeat = {};
    mavlink_msg_heartbeat_pack(99, 1, &heartbeat, MAV_TYPE_QUADROTOR,
                               MAV_AUTOPILOT_ARDUPILOTMEGA, 0, 0, MAV_STATE_ACTIVE);
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN] = {};
    const int length = mavlink_msg_to_send_buffer(buffer, &heartbeat);
    const QByteArray original(reinterpret_cast<const char *>(buffer), length);
    QCOMPARE(source->write(original), qint64(original.size()));
    QVERIFY(source->flush());

    QCOMPARE(receiveDatagram(receiver), original);
    QCOMPARE(forwarder->forwardedPackets(), 1ULL);
    forwarder->disable();
}

void MavlinkForwarderTest::disableStopsNewDatagrams()
{
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
    MavlinkForwarder forwarder;
    QVERIFY(forwarder.apply(true, QStringLiteral("127.0.0.1"), receiver.localPort()));
    const QByteArray packet = QByteArray::fromHex("fd000000010101000000aabb");
    forwarder.forwardFrame(frameFor(packet, 99));
    QCOMPARE(receiveDatagram(receiver), packet);
    forwarder.disable();
    forwarder.forwardFrame(frameFor(packet, 99));
    QTest::qWait(100);
    QVERIFY(!receiver.hasPendingDatagrams());
}

void MavlinkForwarderTest::resetStatisticsClearsSessionCounters()
{
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
    MavlinkForwarder forwarder;
    QVERIFY(forwarder.apply(true, QStringLiteral("127.0.0.1"), receiver.localPort()));
    forwarder.forwardFrame(frameFor(QByteArray::fromHex("fd000000010101000000aabb"), 1));
    receiveDatagram(receiver);
    forwarder.resetStatistics();
    QCOMPARE(forwarder.forwardedPackets(), 0ULL);
    QCOMPARE(forwarder.forwardedBytes(), 0ULL);
    QCOMPARE(forwarder.failedPackets(), 0ULL);
    QVERIFY(!forwarder.lastForwardedPacketTime().isValid());
}

QTEST_MAIN(MavlinkForwarderTest)

#include "MavlinkForwarderTest.moc"
