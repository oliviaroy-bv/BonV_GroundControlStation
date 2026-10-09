#pragma once

#include <QObject>
#include <QString>
#include <QElapsedTimer>
#include "common/mavlink.h"

class Mavlink;
class Video;

class Vehicle : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* mavlink READ mavlink CONSTANT)
    Q_PROPERTY(QObject* video READ video CONSTANT)

    // ───── Attitude ─────
    Q_PROPERTY(double roll READ roll NOTIFY rollChanged)
    Q_PROPERTY(double pitch READ pitch NOTIFY pitchChanged)
    Q_PROPERTY(double yaw READ yaw NOTIFY yawChanged)

    // ───── Position ─────
    Q_PROPERTY(double latitude READ latitude NOTIFY positionChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY positionChanged)
    Q_PROPERTY(double altitude READ altitude NOTIFY positionChanged)
    Q_PROPERTY(double distanceToHome READ distanceToHome NOTIFY distanceToHomeChanged)
    Q_PROPERTY(double flightDistance READ flightDistance NOTIFY flightDistanceChanged)
    Q_PROPERTY(int flightTime READ flightTime NOTIFY flightTimeChanged)

    // ───── Status ─────
    Q_PROPERTY(int battery READ battery NOTIFY batteryChanged)
    Q_PROPERTY(double batteryVoltage READ batteryVoltage NOTIFY batteryChanged)
    Q_PROPERTY(double batteryCurrent READ batteryCurrent NOTIFY batteryChanged)
    Q_PROPERTY(bool armed READ armed NOTIFY armedChanged)
    Q_PROPERTY(QString flightMode READ flightMode NOTIFY flightModeChanged)
    Q_PROPERTY(QString vehicleType READ vehicleType NOTIFY vehicleTypeChanged)
    Q_PROPERTY(int motorCount READ motorCount NOTIFY vehicleTypeChanged)

    // ───── Flight ─────
    Q_PROPERTY(double airspeed READ airspeed NOTIFY vfrHudChanged)
    Q_PROPERTY(double groundspeed READ groundspeed NOTIFY vfrHudChanged)
    Q_PROPERTY(int heading READ heading NOTIFY vfrHudChanged)
    Q_PROPERTY(int throttle READ throttle NOTIFY vfrHudChanged)
    Q_PROPERTY(double climb READ climb NOTIFY vfrHudChanged)

    // ───── GPS ─────
    Q_PROPERTY(int gpsFix READ gpsFix NOTIFY gpsChanged)
    Q_PROPERTY(int satellites READ satellites NOTIFY gpsChanged)
    Q_PROPERTY(double gpsHdop READ gpsHdop NOTIFY gpsChanged)
    Q_PROPERTY(double gpsVdop READ gpsVdop NOTIFY gpsChanged)
    Q_PROPERTY(double gpsHorizontalAccuracy READ gpsHorizontalAccuracy NOTIFY gpsChanged)
    Q_PROPERTY(double gpsVerticalAccuracy READ gpsVerticalAccuracy NOTIFY gpsChanged)
    Q_PROPERTY(double gpsSpeedAccuracy READ gpsSpeedAccuracy NOTIFY gpsChanged)
    Q_PROPERTY(double gpsHeadingAccuracy READ gpsHeadingAccuracy NOTIFY gpsChanged)
    Q_PROPERTY(double gpsGroundSpeed READ gpsGroundSpeed NOTIFY gpsChanged)
    Q_PROPERTY(double gpsCourse READ gpsCourse NOTIFY gpsChanged)
    Q_PROPERTY(double gpsAltitude READ gpsAltitude NOTIFY gpsChanged)

    // ───── Velocity ─────
    Q_PROPERTY(double vx READ vx NOTIFY velocityChanged)
    Q_PROPERTY(double vy READ vy NOTIFY velocityChanged)
    Q_PROPERTY(double vz READ vz NOTIFY velocityChanged)

    // Communication link quality. -1 means no quality telemetry has arrived yet.
    Q_PROPERTY(int commLinkQuality READ commLinkQuality NOTIFY commLinkQualityChanged)
    Q_PROPERTY(int radioRssi READ radioRssi NOTIFY commLinkQualityChanged)
    Q_PROPERTY(double radioPacketLoss READ radioPacketLoss NOTIFY commLinkQualityChanged)

public:
    explicit Vehicle(QObject *parent = nullptr);

    QObject* mavlink() const;
    QObject* video() const;

    // ───── Getters ─────
    double roll() const;
    double pitch() const;
    double yaw() const;

    double latitude() const;
    double longitude() const;
    double altitude() const;
    double distanceToHome() const;
    double flightDistance() const;
    int flightTime() const;

    int battery() const;
    double batteryVoltage() const;
    double batteryCurrent() const;
    bool armed() const;
    QString flightMode() const;
    QString vehicleType() const;
    int motorCount() const;

    double airspeed() const;
    double groundspeed() const;
    int heading() const;
    int throttle() const;
    double climb() const;

    int gpsFix() const;
    int satellites() const;
    double gpsHdop() const;
    double gpsVdop() const;
    double gpsHorizontalAccuracy() const;
    double gpsVerticalAccuracy() const;
    double gpsSpeedAccuracy() const;
    double gpsHeadingAccuracy() const;
    double gpsGroundSpeed() const;
    double gpsCourse() const;
    double gpsAltitude() const;

    double vx() const;
    double vy() const;
    double vz() const;
    int commLinkQuality() const;
    int radioRssi() const;
    double radioPacketLoss() const;

    // MAVLink entry point
    void handleMessage(const mavlink_message_t &msg);

signals:
    void rollChanged();
    void pitchChanged();
    void yawChanged();

    void positionChanged();
    void distanceToHomeChanged();
    void flightDistanceChanged();
    void flightTimeChanged();
    void batteryChanged();
    void armedChanged();
    void flightModeChanged();
    void vehicleTypeChanged();

    void vfrHudChanged();
    void gpsChanged();
    void velocityChanged();
    void commLinkQualityChanged();

    void statusTextReceived(QString msg);
    void mavlinkMessageReceived(QString msgid, QString details);

private:
    void setHomeLocation(double latitude, double longitude);
    void updateCommLinkQuality();

    // ───── Data ─────
    double rollValue = 0, pitchValue = 0, yawValue = 0;

    double latitudeValue = 0, longitudeValue = 0, altitudeValue = 0;
    double homeLatitude = 0, homeLongitude = 0, distanceToHomeValue = 0;
    double lastLatitude = 0, lastLongitude = 0, flightDistanceValue = 0;
    bool homeSet = false;
    bool lastPositionSet = false;
    bool flightStarted = false;
    QElapsedTimer flightTimer;

    int batteryValue = -1;
    double batteryVoltageValue = -1;
    double batteryCurrentValue = -1;
    bool armedState = false;
    QString flightModeValue = "UNKNOWN";
    QString vehicleTypeValue = "UNKNOWN";
    int motorCountValue = 4;

    double airspeedValue = 0;
    double groundspeedValue = 0;
    int headingValue = 0;
    int throttleValue = 0;
    double climbValue = 0;

    int gpsFixValue = 0;
    int satellitesValue = -1;
    double gpsHdopValue = -1;
    double gpsVdopValue = -1;
    double gpsHorizontalAccuracyValue = -1;
    double gpsVerticalAccuracyValue = -1;
    double gpsSpeedAccuracyValue = -1;
    double gpsHeadingAccuracyValue = -1;
    double gpsGroundSpeedValue = -1;
    double gpsCourseValue = -1;
    double gpsAltitudeValue = -1;

    double vxValue = 0, vyValue = 0, vzValue = 0;

    int commLinkQualityValue = -1;
    double smoothedCommLinkQualityValue = -1.0;
    int radioRssiValue = -1;
    double radioPacketLossValue = -1;

    Mavlink *mavlinkObject = nullptr;
    Video *videoObject = nullptr;
};
