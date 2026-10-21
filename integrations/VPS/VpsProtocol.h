#pragma once
#include <QByteArray>
#include <QString>

// VPS <-> GCS protocol carried inside MAVLink TUNNEL payloads
// (see "VPS Integration Guide using MAVLink" v0.2.2)
namespace Vps {

// payload_type values, VPS -> GCS
enum RxType : quint16 {
    RX_STATUS       = 100,
    RX_ANCHOR_COUNT = 101,
    RX_CONFIG       = 102,
    RX_ANCHOR_META  = 107,
    RX_VERSION      = 108,
};

// payload_type values, GCS -> VPS
enum TxType : quint16 {
    TX_PROCESS         = 0,
    TX_SET_HOME        = 1,   // also "Set VPS Initial Lat/Lon" (5.10 uses the same id)
    TX_AUTO_POSITION   = 2,
    TX_ALIGN_HEADING   = 3,
    TX_GPS_THRESHOLD   = 4,
    TX_ANCHOR_COUNT    = 5,
    TX_AIDING          = 6,
    TX_RC_CHANNEL      = 7,
    TX_LOG_AUTOSTART   = 8,
    TX_REFRESH_CONFIG  = 10,
    TX_AUTO_AIDING     = 14,
    TX_GPS_CHECK       = 15,
    TX_GPS_JAMMING     = 16,
    TX_ANCHOR_META     = 17,
    TX_VERSION         = 18,
    TX_SENSOR_SPEED    = 19,
};

enum Process { PROC_SENSORS = 0, PROC_VIO = 1, PROC_VPR = 2, PROC_LOGGING = 3 };

QString processStatusText(quint8 v);   // 0..3
QString gpsFixText(quint8 v);          // 3..6

struct Status {
    double vioLat, vioLon, vprLat, vprLon, fusedLat, fusedLon;
    float  health, featureCount;
    float  posX, posY, posZ, velX, velY, velZ;
    float  gte;
    bool   gpsInUse;
    quint8 sensorStatus, vioStatus, vprStatus, logStatus;
    quint8 chip1Temp, chip2Temp, diskGB, chip3Temp;
    bool   aidFlag;
    float  vpsId;
};

struct GpsCheck {
    quint8 fixType = 3, satCount = 10;
    float  hdop = 2.0f;
    qint32 hAccMm = 2000, vAccMmS = 500;
    float  healthySec = 5.0f, unhealthySec = 2.0f;
};

struct GpsJamming {
    bool   enable = false;
    float  timeSampleSec = 1.0f;
    qint32 deltaHAccMm = 1000, deltaVAccMmS = 500;
    float  unhealthySec = 2.0f;
};

struct Config {
    float  gpsSwitchThreshold;
    quint8 rcChannel, logAutoStart;
    double initLat, initLon;
    GpsCheck gps;
    GpsJamming jam;
    quint8 sensorSpeed;
};

struct AnchorMeta {
    quint8 zoom, index;
    float  tlLat, tlLon, brLat, brLon;
};

struct VersionInfo {
    QString vps, obc2, mcuSw, mcuHw, board, serial;
};

// ---- decoders (return false if payload is too short) ----
bool parseStatus(const QByteArray &p, Status &out);
bool parseConfig(const QByteArray &p, Config &out);
bool parseAnchorMeta(const QByteArray &p, AnchorMeta &out);
bool parseVersion(const QByteArray &p, VersionInfo &out);

// ---- encoders (return the TUNNEL payload bytes) ----
QByteArray buildProcess(Process proc, bool start);           // type 0
QByteArray buildHome(double lat, double lon);                // type 1
QByteArray buildByte(quint8 v);                              // 1-byte payloads
QByteArray buildFloat(float v);                              // type 4
QByteArray buildGpsCheck(const GpsCheck &g);                 // type 15 (22 bytes)
QByteArray buildGpsJamming(const GpsJamming &j);             // type 16 (17 bytes)

} // namespace Vps
