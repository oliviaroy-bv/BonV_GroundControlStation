#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>

// A decoded MAVLink TUNNEL (msg id 385) message
struct TunnelMsg {
    quint8  srcSys = 0, srcComp = 0;          // sender of the MAVLink packet
    quint8  targetSys = 0, targetComp = 0;
    quint16 payloadType = 0;
    QByteArray payload;                        // exactly payload_length bytes (padded to 128 internally)
};
struct GpsPosition {
    quint8 srcSys = 0, srcComp = 0, fixType = 0;
    double lat = 0, lon = 0;
    float altitudeM = 0;
};

// Minimal MAVLink 2 framer/parser over a TCP client socket.
// Understands HEARTBEAT (0) and TUNNEL (385); everything else is skipped.
class MavlinkTcpLink : public QObject
{
    Q_OBJECT
public:
    explicit MavlinkTcpLink(QObject *parent = nullptr);

    void open(const QString &host, quint16 port);
    void close();
    bool isOpen() const;

    void setIdentity(quint8 sysId, quint8 compId) { m_sysId = sysId; m_compId = compId; }
    void setHeartbeatEnabled(bool on);

    bool sendTunnel(quint8 targetSys, quint8 targetComp, quint16 payloadType, const QByteArray &payload);

    quint64 rxPackets() const { return m_rxOk; }
    quint64 badCrc() const    { return m_rxBad; }

signals:
    void opened();
    void closed();
    void error(const QString &msg);
    void tunnelReceived(const TunnelMsg &msg);
    void heartbeatReceived(quint8 sysId, quint8 compId);
    void gpsPositionReceived(const GpsPosition &position);
    void txPacket(const QByteArray &raw);

private slots:
    void onReadyRead();
    void sendHeartbeat();

private:
    void parse();
    bool sendPacket(quint32 msgId, const QByteArray &payload, quint8 crcExtra);

    QTcpSocket m_sock;
    QTimer     m_hbTimer;
    QByteArray m_buf;
    quint8     m_seq = 0;
    quint8     m_sysId = 255;   // GCS
    quint8     m_compId = 190;  // MAV_COMP_ID_MISSIONPLANNER
    quint64    m_rxOk = 0, m_rxBad = 0;
};
