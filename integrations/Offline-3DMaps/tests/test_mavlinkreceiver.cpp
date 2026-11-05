#include "mavlinkreceiver.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QtEndian>

namespace {

QByteArray heartbeatFrame(quint8 systemId)
{
    QByteArray payload(9, '\0');
    qToLittleEndian<quint32>(4, reinterpret_cast<uchar *>(payload.data()));
    payload[6] = static_cast<char>(0x80);

    QByteArray frame;
    frame.reserve(6 + payload.size() + 2);
    frame.append(static_cast<char>(0xfe));
    frame.append(static_cast<char>(payload.size()));
    frame.append('\0');
    frame.append(static_cast<char>(systemId));
    frame.append('\1');
    frame.append('\0');
    frame.append(payload);
    frame.append('\0');
    frame.append('\0');
    return frame;
}

QByteArray mavlink2Frame(quint32 messageId, quint8 systemId, const QByteArray &payload)
{
    QByteArray frame;
    frame.reserve(10 + payload.size() + 2);
    frame.append(static_cast<char>(0xfd));
    frame.append(static_cast<char>(payload.size()));
    frame.append('\0');
    frame.append('\0');
    frame.append('\1');
    frame.append(static_cast<char>(systemId));
    frame.append('\1');
    frame.append(static_cast<char>(messageId & 0xff));
    frame.append(static_cast<char>((messageId >> 8) & 0xff));
    frame.append(static_cast<char>((messageId >> 16) & 0xff));
    frame.append(payload);
    frame.append('\0');
    frame.append('\0');
    return frame;
}

} // namespace

class MavlinkReceiverTests : public QObject
{
    Q_OBJECT

private slots:
    void tcpReassemblesFragmentedMavlinkFrame();
};

void MavlinkReceiverTests::tcpReassemblesFragmentedMavlinkFrame()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    MavlinkReceiver receiver;
    receiver.setDemoMode(false);
    receiver.setTcpHost(QStringLiteral("127.0.0.1"));
    receiver.setTcpPort(server.serverPort());
    receiver.setTcpEnabled(true);

    QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 2000);
    QTcpSocket *peer = server.nextPendingConnection();
    QVERIFY(peer);
    QTRY_VERIFY_WITH_TIMEOUT(receiver.tcpConnected(), 2000);

    const QByteArray frame = heartbeatFrame(42);
    QCOMPARE(peer->write(frame.first(5)), 5);
    QVERIFY(peer->flush());
    QTest::qWait(25);
    QCOMPARE(receiver.systemId(), 0);

    QCOMPARE(peer->write(frame.sliced(5)), frame.size() - 5);
    QVERIFY(peer->flush());
    QTRY_COMPARE_WITH_TIMEOUT(receiver.systemId(), 42, 2000);
    QVERIFY(receiver.armed());
    QCOMPARE(receiver.flightMode(), QStringLiteral("GUIDED"));

    QByteArray positionPayload(20, '\0');
    qToLittleEndian<qint32>(129715987,
                            reinterpret_cast<uchar *>(positionPayload.data() + 4));
    qToLittleEndian<qint32>(775945660,
                            reinterpret_cast<uchar *>(positionPayload.data() + 8));
    qToLittleEndian<qint32>(920250,
                            reinterpret_cast<uchar *>(positionPayload.data() + 12));
    qToLittleEndian<qint32>(70250,
                            reinterpret_cast<uchar *>(positionPayload.data() + 16));
    const QByteArray positionFrame = mavlink2Frame(33, 42, positionPayload);
    QCOMPARE(peer->write(positionFrame), positionFrame.size());
    QVERIFY(peer->flush());

    QTRY_VERIFY_WITH_TIMEOUT(qAbs(receiver.latitude() - 12.9715987) < 1.0e-7, 2000);
    QVERIFY(qAbs(receiver.longitude() - 77.594566) < 1.0e-7);
    QCOMPARE(receiver.altitudeMsl(), 920.25);
    QCOMPARE(receiver.relativeAltitude(), 70.25);

    receiver.setTcpEnabled(false);
}

QTEST_GUILESS_MAIN(MavlinkReceiverTests)
#include "test_mavlinkreceiver.moc"
