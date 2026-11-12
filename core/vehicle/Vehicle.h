#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QElapsedTimer>
#include <QTimer>
#include "common/mavlink.h"

class Mavlink;
class ParameterManager;
class Video;

class Vehicle : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* mavlink READ mavlink CONSTANT)
    Q_PROPERTY(QObject* video READ video CONSTANT)
    Q_PROPERTY(QObject* parameterManager READ parameterManager CONSTANT)
    Q_PROPERTY(bool parametersReady READ parametersReady NOTIFY parametersReadyChanged)

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
    Q_PROPERTY(FlightPhase flightPhase READ flightPhase NOTIFY flightStateChanged)
    Q_PROPERTY(bool flightStateFresh READ flightStateFresh NOTIFY flightStateChanged)
    Q_PROPERTY(bool airborne READ airborne NOTIFY flightStateChanged)
    Q_PROPERTY(bool heartbeatFresh READ heartbeatFresh NOTIFY telemetryFreshnessChanged)
    Q_PROPERTY(bool positionFresh READ positionFresh NOTIFY telemetryFreshnessChanged)
    Q_PROPERTY(bool headingValid READ headingValid NOTIFY telemetryFreshnessChanged)
    Q_PROPERTY(bool headingFresh READ headingFresh NOTIFY telemetryFreshnessChanged)
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

    // Estimated transport quality, not RF strength. -1 means insufficient fresh evidence.
    Q_PROPERTY(QString commLinkCategory READ commLinkCategory NOTIFY commLinkQualityChanged)
    Q_PROPERTY(QVariantMap commLinkDiagnostics READ commLinkDiagnostics NOTIFY commLinkQualityChanged)
    Q_PROPERTY(int commLinkQuality READ commLinkQuality NOTIFY commLinkQualityChanged)
    Q_PROPERTY(int radioRssi READ radioRssi NOTIFY commLinkQualityChanged)
    Q_PROPERTY(double radioPacketLoss READ radioPacketLoss NOTIFY commLinkQualityChanged)

public:
    enum FlightPhase {
        Unknown = MAV_LANDED_STATE_UNDEFINED,
        OnGround = MAV_LANDED_STATE_ON_GROUND,
        InAir = MAV_LANDED_STATE_IN_AIR,
        TakingOff = MAV_LANDED_STATE_TAKEOFF,
        Landing = MAV_LANDED_STATE_LANDING
    };
    Q_ENUM(FlightPhase)

    // Conservative application defaults, not guarantees about a firmware/link.
    static constexpr int HeartbeatTimeoutMs = 3000;
    static constexpr int FlightStateTimeoutMs = 3500; // requested 1 Hz; tolerate two missed samples
    static constexpr int PositionTimeoutMs = 2000;
    static constexpr int HeadingTimeoutMs = 2000;
    FlightPhase flightPhase() const { return flightPhaseValue; }
    bool flightStateFresh() const { return flightStateFreshValue; }
    bool airborne() const { return airborneValue; }
    bool heartbeatFresh() const { return heartbeatFreshValue; }
    bool positionFresh() const { return positionFreshValue; }
    bool headingValid() const { return headingValidValue; }
    bool headingFresh() const { return headingFreshValue; }
    explicit Vehicle(QObject *parent = nullptr);
    ~Vehicle() override;

    QObject* mavlink() const;
    QObject* parameterManager() const;
    bool parametersReady() const;
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
    QString commLinkCategory() const;
    QVariantMap commLinkDiagnostics() const;
    int radioRssi() const;
    double radioPacketLoss() const;

    // MAVLink entry point
    void handleMessage(const mavlink_message_t &msg);

signals:
    void parametersReadyChanged();
    void flightStateChanged();
    void telemetryFreshnessChanged();
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
    void resetTelemetryState();
    void updateTelemetryState(const mavlink_message_t &msg);
    void expireTelemetryState();
    QTimer freshnessTimer;
    QElapsedTimer heartbeatReceipt, flightStateReceipt, positionReceipt, headingReceipt;
    FlightPhase flightPhaseValue = Unknown;
    bool flightStateFreshValue = false, airborneValue = false;
    bool heartbeatFreshValue = false, positionFreshValue = false;
    bool headingValidValue = false, headingFreshValue = false;
    bool selectedArmed = false, airborneConfirmed = false;
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


    ParameterManager *parameterManagerObject = nullptr;
    Mavlink *mavlinkObject = nullptr;
    Video *videoObject = nullptr;
};
