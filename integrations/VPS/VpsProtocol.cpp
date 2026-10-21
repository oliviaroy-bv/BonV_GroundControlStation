#include "VpsProtocol.h"
#include <QtGlobal>
#include <cstring>

static_assert(Q_BYTE_ORDER == Q_LITTLE_ENDIAN, "Protocol assumes a little-endian host");

namespace Vps {
namespace {

template <typename T> T rd(const QByteArray &b, int off)
{
    T v{};
    if (off + int(sizeof(T)) <= b.size())
        memcpy(&v, b.constData() + off, sizeof(T));
    return v;
}
template <typename T> void wr(QByteArray &b, int off, T v)
{
    memcpy(b.data() + off, &v, sizeof(T));
}
} // namespace

QString processStatusText(quint8 v)
{
    switch (v) {
    case 0: return "Not Running";
    case 1: return "Starting";
    case 2: return "Running";
    case 3: return "Stopping";
    default: return QString("Unknown (%1)").arg(v);
    }
}

QString gpsFixText(quint8 v)
{
    switch (v) {
    case 3: return "3D_FIX";
    case 4: return "DGPS";
    case 5: return "RTK_FLOAT";
    case 6: return "RTK_FIXED";
    default: return QString("%1").arg(v);
    }
}

bool parseStatus(const QByteArray &p, Status &s)
{
    if (p.size() < 105) return false;     // up to Aid Flag; VPS ID (105-108) is read if present
    s.vioLat = rd<double>(p, 0);   s.vioLon = rd<double>(p, 8);
    s.vprLat = rd<double>(p, 16);  s.vprLon = rd<double>(p, 24);
    s.fusedLat = rd<double>(p, 32); s.fusedLon = rd<double>(p, 40);
    s.health = rd<float>(p, 48);   s.featureCount = rd<float>(p, 52);
    s.posX = rd<float>(p, 56); s.posY = rd<float>(p, 60); s.posZ = rd<float>(p, 64);
    s.velX = rd<float>(p, 68); s.velY = rd<float>(p, 72); s.velZ = rd<float>(p, 76);
    s.gte  = rd<float>(p, 80);
    s.gpsInUse = rd<quint8>(p, 84) != 0;
    s.sensorStatus = rd<quint8>(p, 89); s.vioStatus = rd<quint8>(p, 90);
    s.vprStatus = rd<quint8>(p, 91);    s.logStatus = rd<quint8>(p, 92);
    s.chip1Temp = rd<quint8>(p, 100);   s.chip2Temp = rd<quint8>(p, 101);
    s.diskGB    = rd<quint8>(p, 102);   s.chip3Temp = rd<quint8>(p, 103);
    s.aidFlag   = rd<quint8>(p, 104) != 0;
    s.vpsId     = rd<float>(p, 105);
    return true;
}

bool parseConfig(const QByteArray &p, Config &c)
{
    if (p.size() < 62) return false;
    c.gpsSwitchThreshold = rd<float>(p, 0);
    c.rcChannel    = rd<quint8>(p, 4);
    c.logAutoStart = rd<quint8>(p, 5);
    c.initLat = rd<double>(p, 6);
    c.initLon = rd<double>(p, 14);
    c.gps.fixType      = rd<quint8>(p, 22);
    c.gps.satCount     = rd<quint8>(p, 23);
    c.gps.hdop         = rd<float>(p, 24);
    c.gps.hAccMm       = rd<qint32>(p, 28);
    c.gps.vAccMmS      = rd<qint32>(p, 32);
    c.gps.healthySec   = rd<float>(p, 36);
    c.gps.unhealthySec = rd<float>(p, 40);
    c.jam.enable        = rd<quint8>(p, 44) != 0;
    c.jam.timeSampleSec = rd<float>(p, 45);
    c.jam.deltaHAccMm   = rd<qint32>(p, 49);
    c.jam.deltaVAccMmS  = rd<qint32>(p, 53);
    c.jam.unhealthySec  = rd<float>(p, 57);
    c.sensorSpeed       = rd<quint8>(p, 61);
    return true;
}

bool parseAnchorMeta(const QByteArray &p, AnchorMeta &a)
{
    if (p.size() < 18) return false;
    a.zoom = rd<quint8>(p, 0);  a.index = rd<quint8>(p, 1);
    a.tlLat = rd<float>(p, 2);  a.tlLon = rd<float>(p, 6);
    a.brLat = rd<float>(p, 10); a.brLon = rd<float>(p, 14);
    return true;
}

bool parseVersion(const QByteArray &p, VersionInfo &v)
{
    if (p.size() < 23) return false;
    auto n = [&](int i) { return QString::number(quint8(p[i])); };
    v.vps   = QString("%1.%2.%3").arg(n(0), n(1), n(2));
    v.obc2  = QString("%1.%2.%3").arg(n(3), n(4), n(5));
    v.mcuSw = QString("%1.%2.%3").arg(n(6), n(7), n(8));
    v.mcuHw = QString("%1.%2").arg(n(9), n(10));
    v.board = QString("%1.%2.%3").arg(n(11), n(12), n(13));
    // Guide says "9 chars concatenated": show printable bytes as characters, otherwise as numbers
    QString sn;
    for (int i = 14; i <= 22; ++i) {
        const quint8 b = quint8(p[i]);
        sn += (b >= 0x20 && b < 0x7F) ? QString(QChar(b)) : QString::number(b);
    }
    v.serial = sn;
    return true;
}

QByteArray buildProcess(Process proc, bool start)
{
    QByteArray b(2, 0);
    b[0] = char(proc);
    b[1] = start ? 1 : 0;
    return b;
}

QByteArray buildHome(double lat, double lon)
{
    QByteArray b(16, 0);
    wr(b, 0, lat);
    wr(b, 8, lon);
    return b;
}

QByteArray buildByte(quint8 v) { return QByteArray(1, char(v)); }

QByteArray buildFloat(float v)
{
    QByteArray b(4, 0);
    wr(b, 0, v);
    return b;
}

QByteArray buildGpsCheck(const GpsCheck &g)
{
    QByteArray b(22, 0);
    b[0] = char(g.fixType);
    b[1] = char(g.satCount);
    wr(b, 2, g.hdop);
    wr(b, 6, g.hAccMm);
    wr(b, 10, g.vAccMmS);
    wr(b, 14, g.healthySec);
    wr(b, 18, g.unhealthySec);
    return b;
}

QByteArray buildGpsJamming(const GpsJamming &j)
{
    QByteArray b(17, 0);
    b[0] = j.enable ? 1 : 0;
    wr(b, 1, j.timeSampleSec);
    wr(b, 5, j.deltaHAccMm);
    wr(b, 9, j.deltaVAccMmS);
    wr(b, 13, j.unhealthySec);
    return b;
}

} // namespace Vps
