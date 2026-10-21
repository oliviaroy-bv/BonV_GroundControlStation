#include "MavlinkTcpLink.h"
#include <QtEndian>
#include <cstring>

namespace {
constexpr quint8  MAVLINK_STX_V2       = 0xFD;
constexpr quint32 MSG_HEARTBEAT        = 0;
constexpr quint32 MSG_TUNNEL           = 385;
constexpr quint32 MSG_GPS_RAW_INT      = 24;
constexpr quint32 MSG_GLOBAL_POSITION_INT = 33;
constexpr quint8  CRC_EXTRA_HEARTBEAT  = 50;
constexpr quint8  CRC_EXTRA_TUNNEL     = 147;
constexpr quint8  CRC_EXTRA_GPS_RAW_INT = 24;
constexpr quint8  CRC_EXTRA_GLOBAL_POSITION_INT = 104;
constexpr int     TUNNEL_PAYLOAD_MAX   = 128;
constexpr int     TUNNEL_WIRE_LEN      = 2 + 1 + 1 + 1 + TUNNEL_PAYLOAD_MAX; // 133

inline void crcAccumulate(quint8 data, quint16 &crc)
{
    quint8 tmp = data ^ quint8(crc & 0xFF);
    tmp ^= quint8(tmp << 4);
    crc = quint16((crc >> 8) ^ (quint16(tmp) << 8) ^ (quint16(tmp) << 3) ^ (tmp >> 4));
}

bool crcExtraFor(quint32 msgId, quint8 &extra)
{
    switch (msgId) {
    case MSG_HEARTBEAT: extra = CRC_EXTRA_HEARTBEAT; return true;
    case MSG_TUNNEL:    extra = CRC_EXTRA_TUNNEL;    return true;
    case MSG_GPS_RAW_INT: extra = CRC_EXTRA_GPS_RAW_INT; return true;
    case MSG_GLOBAL_POSITION_INT: extra = CRC_EXTRA_GLOBAL_POSITION_INT; return true;
    default: return false;
    }
}
} // namespace

MavlinkTcpLink::MavlinkTcpLink(QObject *parent) : QObject(parent)
{
    connect(&m_sock, &QTcpSocket::connected, this, [this] {
        m_buf.clear();
        m_rxOk = m_rxBad = 0;
        emit opened();
    });
    connect(&m_sock, &QTcpSocket::disconnected, this, &MavlinkTcpLink::closed);
    connect(&m_sock, &QTcpSocket::readyRead, this, &MavlinkTcpLink::onReadyRead);
    connect(&m_sock, &QAbstractSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { emit error(m_sock.errorString()); });
    connect(&m_hbTimer, &QTimer::timeout, this, &MavlinkTcpLink::sendHeartbeat);
    m_hbTimer.setInterval(1000);
}

void MavlinkTcpLink::open(const QString &host, quint16 port)
{
    close();
    m_sock.connectToHost(host, port);
}

void MavlinkTcpLink::close()
{
    if (m_sock.state() != QAbstractSocket::UnconnectedState)
        m_sock.abort();
}

bool MavlinkTcpLink::isOpen() const
{
    return m_sock.state() == QAbstractSocket::ConnectedState;
}

void MavlinkTcpLink::setHeartbeatEnabled(bool on)
{
    if (on) m_hbTimer.start(); else m_hbTimer.stop();
}

void MavlinkTcpLink::sendHeartbeat()
{
    if (!isOpen()) return;
    QByteArray p(9, 0);
    p[4] = 6;   // MAV_TYPE_GCS
    p[5] = 8;   // MAV_AUTOPILOT_INVALID
    p[8] = 3;   // mavlink_version
    sendPacket(MSG_HEARTBEAT, p, CRC_EXTRA_HEARTBEAT);
}

bool MavlinkTcpLink::sendTunnel(quint8 targetSys, quint8 targetComp, quint16 type, const QByteArray &payload)
{
    if (!isOpen() || payload.size() > TUNNEL_PAYLOAD_MAX) return false;

    QByteArray w(TUNNEL_WIRE_LEN, 0);
    // wire order (sorted by field size): payload_type, target_system, target_component, payload_length, payload
    qToLittleEndian<quint16>(type, reinterpret_cast<uchar *>(w.data()));
    w[2] = char(targetSys);
    w[3] = char(targetComp);
    w[4] = char(payload.size());
    memcpy(w.data() + 5, payload.constData(), size_t(payload.size()));

    // MAVLink 2 payload truncation: strip trailing zero bytes (keep at least 1)
    int n = w.size();
    while (n > 1 && w[n - 1] == 0) --n;
    w.truncate(n);

    return sendPacket(MSG_TUNNEL, w, CRC_EXTRA_TUNNEL);
}

bool MavlinkTcpLink::sendPacket(quint32 msgId, const QByteArray &payload, quint8 crcExtra)
{
    QByteArray pkt;
    pkt.reserve(12 + payload.size());
    pkt.append(char(MAVLINK_STX_V2));
    pkt.append(char(payload.size()));
    pkt.append(char(0));            // incompat flags (no signing)
    pkt.append(char(0));            // compat flags
    pkt.append(char(m_seq++));
    pkt.append(char(m_sysId));
    pkt.append(char(m_compId));
    pkt.append(char(msgId & 0xFF));
    pkt.append(char((msgId >> 8) & 0xFF));
    pkt.append(char((msgId >> 16) & 0xFF));
    pkt.append(payload);

    quint16 crc = 0xFFFF;
    for (int i = 1; i < pkt.size(); ++i) crcAccumulate(quint8(pkt[i]), crc);
    crcAccumulate(crcExtra, crc);
    pkt.append(char(crc & 0xFF));
    pkt.append(char(crc >> 8));

    emit txPacket(pkt);
    return m_sock.write(pkt) == pkt.size();
}

void MavlinkTcpLink::onReadyRead()
{
    m_buf.append(m_sock.readAll());
    parse();
}

void MavlinkTcpLink::parse()
{
    while (true) {
        int i = m_buf.indexOf(char(MAVLINK_STX_V2));
        if (i < 0) { m_buf.clear(); return; }
        if (i > 0) m_buf.remove(0, i);
        if (m_buf.size() < 12) return;

        const uchar *d = reinterpret_cast<const uchar *>(m_buf.constData());
        const int len = d[1];
        const bool signedPkt = d[2] & 0x01;
        const int total = 10 + len + 2 + (signedPkt ? 13 : 0);
        if (m_buf.size() < total) return;

        const quint32 msgId = quint32(d[7]) | (quint32(d[8]) << 8) | (quint32(d[9]) << 16);

        quint8 extra = 0;
        if (crcExtraFor(msgId, extra)) {
            quint16 crc = 0xFFFF;
            for (int k = 1; k < 10 + len; ++k) crcAccumulate(d[k], crc);
            crcAccumulate(extra, crc);
            const quint16 rx = quint16(d[10 + len]) | (quint16(d[11 + len]) << 8);
            if (crc != rx) { ++m_rxBad; m_buf.remove(0, 1); continue; }

            ++m_rxOk;
            const quint8 sys = d[5], comp = d[6];
            QByteArray pl(reinterpret_cast<const char *>(d + 10), len);

            if (msgId == MSG_HEARTBEAT) {
                emit heartbeatReceived(sys, comp);
            } else if (msgId == MSG_GPS_RAW_INT && pl.size() >= 29) {
                const qint32 lat = qFromLittleEndian<qint32>(reinterpret_cast<const uchar *>(pl.constData() + 8));
                const qint32 lon = qFromLittleEndian<qint32>(reinterpret_cast<const uchar *>(pl.constData() + 12));
                const qint32 alt = qFromLittleEndian<qint32>(reinterpret_cast<const uchar *>(pl.constData() + 16));
                GpsPosition g; g.srcSys = sys; g.srcComp = comp; g.fixType = quint8(pl[28]);
                g.lat = lat / 1e7; g.lon = lon / 1e7; g.altitudeM = alt / 1000.f;
                if (g.fixType >= 2) emit gpsPositionReceived(g);
            } else if (msgId == MSG_GLOBAL_POSITION_INT && pl.size() >= 16) {
                const qint32 lat = qFromLittleEndian<qint32>(reinterpret_cast<const uchar *>(pl.constData() + 4));
                const qint32 lon = qFromLittleEndian<qint32>(reinterpret_cast<const uchar *>(pl.constData() + 8));
                const qint32 alt = qFromLittleEndian<qint32>(reinterpret_cast<const uchar *>(pl.constData() + 12));
                GpsPosition g; g.srcSys = sys; g.srcComp = comp; g.fixType = 3;
                g.lat = lat / 1e7; g.lon = lon / 1e7; g.altitudeM = alt / 1000.f;
                emit gpsPositionReceived(g);
            } else if (msgId == MSG_TUNNEL) {
                if (pl.size() < TUNNEL_WIRE_LEN) pl.append(QByteArray(TUNNEL_WIRE_LEN - pl.size(), 0));  // restore truncated zeros
                TunnelMsg t;
                t.srcSys = sys; t.srcComp = comp;
                t.payloadType = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(pl.constData()));
                t.targetSys   = quint8(pl[2]);
                t.targetComp  = quint8(pl[3]);
                const int plen = qMin<int>(quint8(pl[4]), TUNNEL_PAYLOAD_MAX);
                t.payload = pl.mid(5, plen);
                emit tunnelReceived(t);
            }
        }
        m_buf.remove(0, total);
    }
}
