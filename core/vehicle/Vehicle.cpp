#include "Vehicle.h"
#include "parameters/ParameterManager.h"
#include "mavlink/Mavlink.h"
#include "video/Video.h"
#include <QByteArray>
#include <QtMath>

namespace {

double distanceMeters(double fromLat, double fromLon, double toLat, double toLon)
{
    const double earthRadiusMeters = 6371000.0;
    const double lat1 = qDegreesToRadians(fromLat);
    const double lat2 = qDegreesToRadians(toLat);
    const double deltaLat = qDegreesToRadians(toLat - fromLat);
    const double deltaLon = qDegreesToRadians(toLon - fromLon);
    const double a = qSin(deltaLat / 2) * qSin(deltaLat / 2)
            + qCos(lat1) * qCos(lat2) * qSin(deltaLon / 2) * qSin(deltaLon / 2);
    const double c = 2 * qAtan2(qSqrt(a), qSqrt(1 - a));
    return earthRadiusMeters * c;
}

} // namespace

// ───── Constructor ─────
Vehicle::Vehicle(QObject *parent)
    : QObject(parent)
    , parameterManagerObject(new ParameterManager(this))
    , mavlinkObject(new Mavlink(this, parameterManagerObject))
    , videoObject(new Video(this))
{
    connect(parameterManagerObject, &ParameterManager::statusChanged, this, &Vehicle::parametersReadyChanged);
    connect(mavlinkObject, &Mavlink::messageReceived,
            this, &Vehicle::handleMessage);
    connect(mavlinkObject, &Mavlink::homeLocationResetRequested,
            this, [this](double latitude, double longitude, double altitude) {
        Q_UNUSED(altitude)
        setHomeLocation(latitude, longitude);
    });
    connect(mavlinkObject, &Mavlink::linkQualityChanged,
            this, &Vehicle::updateCommLinkQuality);
    connect(mavlinkObject, &Mavlink::selectedVehicleChanged,
            this, &Vehicle::resetTelemetryState);
    connect(mavlinkObject, &Mavlink::connectionStatusChanged, this, [this](bool connected) {
        if (!connected) resetTelemetryState();
    });
    freshnessTimer.setInterval(100);
    connect(&freshnessTimer, &QTimer::timeout, this, &Vehicle::expireTelemetryState);
    freshnessTimer.start();
}

Vehicle::~Vehicle()
{
    // Shut down connection work while the vehicle and its parameter manager are
    // still alive, before QObject starts deleting children in creation order.
    auto *transport = mavlinkObject;
    mavlinkObject = nullptr; // Quality observers may query Vehicle during transport teardown.
    delete transport;
}

void Vehicle::resetTelemetryState()
{
    const bool flightChanged = flightPhaseValue != Unknown || flightStateFreshValue || airborneValue;
    const bool telemetryChanged = heartbeatFreshValue || positionFreshValue || headingValidValue || headingFreshValue;
    heartbeatReceipt.invalidate();
    flightStateReceipt.invalidate();
    positionReceipt.invalidate();
    headingReceipt.invalidate();
    flightPhaseValue = Unknown;
    flightStateFreshValue = airborneValue = false;
    heartbeatFreshValue = positionFreshValue = headingValidValue = headingFreshValue = false;
    selectedArmed = airborneConfirmed = false;
    if (flightChanged) emit flightStateChanged();
    if (telemetryChanged) emit telemetryFreshnessChanged();
}

void Vehicle::expireTelemetryState()
{
    const auto fresh = [](const QElapsedTimer &receipt, int limit) {
        return receipt.isValid() && receipt.elapsed() < limit;
    };
    const bool heartbeat = fresh(heartbeatReceipt, HeartbeatTimeoutMs);
    const bool position = fresh(positionReceipt, PositionTimeoutMs);
    const bool heading = headingValidValue && fresh(headingReceipt, HeadingTimeoutMs);
    const bool flight = fresh(flightStateReceipt, FlightStateTimeoutMs);
    const auto previousPhase = flightPhaseValue;
    if (!flight) {
        flightPhaseValue = Unknown;
        airborneConfirmed = false;
        flightStateReceipt.invalidate();
    }
    if (!heartbeat) {
        selectedArmed = false;
        airborneConfirmed = false;
    }
    const bool inAir = flight && heartbeat && selectedArmed && airborneConfirmed;
    const bool flightChanged = previousPhase != flightPhaseValue
            || flightStateFreshValue != flight || airborneValue != inAir;
    const bool telemetryChanged = heartbeatFreshValue != heartbeat
            || positionFreshValue != position || headingFreshValue != heading;
    heartbeatFreshValue = heartbeat;
    positionFreshValue = position;
    headingFreshValue = heading;
    flightStateFreshValue = flight;
    airborneValue = inAir;
    if (flightChanged) emit flightStateChanged();
    if (telemetryChanged) emit telemetryFreshnessChanged();
}

void Vehicle::updateTelemetryState(const mavlink_message_t &msg)
{
    // Expire before refreshing: a new packet must not bridge an unknown interval
    // and resurrect an old airborne confirmation.
    expireTelemetryState();
    if (!mavlinkObject->connected() || msg.sysid != mavlinkObject->selectedSystemId()
            || msg.compid != mavlinkObject->selectedComponentId()) return;
    const auto previousPhase = flightPhaseValue;
    const bool previousHeadingValid = headingValidValue;
    switch (msg.msgid) {
    case MAVLINK_MSG_ID_HEARTBEAT: {
        mavlink_heartbeat_t hb = {};
        mavlink_msg_heartbeat_decode(&msg, &hb);
        if (hb.autopilot == MAV_AUTOPILOT_INVALID || hb.type == MAV_TYPE_GCS) return;
        heartbeatReceipt.start();
        selectedArmed = (hb.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
        if (!selectedArmed) {
            airborneConfirmed = false;
            // A later rearm cannot restore confirmation without a new IN_AIR sample.
        }
        break;
    }
    case MAVLINK_MSG_ID_EXTENDED_SYS_STATE: {
        mavlink_extended_sys_state_t state = {};
        mavlink_msg_extended_sys_state_decode(&msg, &state);
        switch (state.landed_state) {
        case MAV_LANDED_STATE_ON_GROUND:
            flightPhaseValue = OnGround;
            airborneConfirmed = false;
            break;
        case MAV_LANDED_STATE_TAKEOFF:
            flightPhaseValue = TakingOff;
            airborneConfirmed = false;
            break;
        case MAV_LANDED_STATE_IN_AIR:
            flightPhaseValue = InAir;
            airborneConfirmed = selectedArmed && heartbeatFreshValue;
            break;
        case MAV_LANDED_STATE_LANDING:
            flightPhaseValue = Landing;
            break; // Preserve only a confirmation from this uninterrupted flight.
        default:
            flightPhaseValue = Unknown;
            airborneConfirmed = false;
            break;
        }
        if (flightPhaseValue == Unknown) flightStateReceipt.invalidate();
        else flightStateReceipt.start();
        // Arming or a positive barometric altitude does not establish takeoff.
        // Use the selected autopilot's flight phase with a fresh armed heartbeat.
        if (!flightStarted && selectedArmed && heartbeatFreshValue
                && (flightPhaseValue == TakingOff || flightPhaseValue == InAir)) {
            flightStarted = true;
            flightTimer.start();
            emit flightTimeChanged();
        }
        break;
    }
    case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
        mavlink_global_position_int_t position = {};
        mavlink_msg_global_position_int_decode(&msg, &position);
        if (position.lat >= -900000000 && position.lat <= 900000000
                && position.lon >= -1800000000 && position.lon <= 1800000000
                && (position.lat != 0 || position.lon != 0)) positionReceipt.start();
        else positionReceipt.invalidate(); // All-zero is the existing no-position sentinel.
        break;
    }
    case MAVLINK_MSG_ID_ATTITUDE: {
        mavlink_attitude_t attitude = {};
        mavlink_msg_attitude_decode(&msg, &attitude);
        headingValidValue = qIsFinite(attitude.yaw);
        if (headingValidValue) headingReceipt.start();
        else headingReceipt.invalidate();
        break;
    }
    case MAVLINK_MSG_ID_VFR_HUD: {
        mavlink_vfr_hud_t hud = {};
        mavlink_msg_vfr_hud_decode(&msg, &hud);
        headingValidValue = hud.heading >= 0 && hud.heading <= 360;
        if (headingValidValue) headingReceipt.start();
        else headingReceipt.invalidate();
        break;
    }
    default:
        return;
    }
    expireTelemetryState();
    if (previousPhase != flightPhaseValue) emit flightStateChanged();
    if (previousHeadingValid != headingValidValue) emit telemetryFreshnessChanged();
}

QObject* Vehicle::parameterManager() const { return parameterManagerObject; }
bool Vehicle::parametersReady() const { return parameterManagerObject->parametersReady(); }

QObject* Vehicle::mavlink() const
{
    return mavlinkObject;
}

QObject* Vehicle::video() const
{
    return videoObject;
}

// ───── Getters ─────
double Vehicle::roll() const { return rollValue; }
double Vehicle::pitch() const { return pitchValue; }
double Vehicle::yaw() const { return yawValue; }

double Vehicle::latitude() const { return latitudeValue; }
double Vehicle::longitude() const { return longitudeValue; }
double Vehicle::altitude() const { return altitudeValue; }
double Vehicle::distanceToHome() const { return distanceToHomeValue; }
double Vehicle::flightDistance() const { return flightDistanceValue; }
int Vehicle::flightTime() const { return flightTimer.isValid() ? static_cast<int>(flightTimer.elapsed() / 1000) : 0; }

int Vehicle::battery() const { return batteryValue; }
double Vehicle::batteryVoltage() const { return batteryVoltageValue; }
double Vehicle::batteryCurrent() const { return batteryCurrentValue; }
bool Vehicle::armed() const { return armedState; }
QString Vehicle::flightMode() const { return flightModeValue; }
QString Vehicle::vehicleType() const { return vehicleTypeValue; }
int Vehicle::motorCount() const { return motorCountValue; }

double Vehicle::airspeed() const { return airspeedValue; }
double Vehicle::groundspeed() const { return groundspeedValue; }
int Vehicle::heading() const { return headingValue; }
int Vehicle::throttle() const { return throttleValue; }
double Vehicle::climb() const { return climbValue; }

int Vehicle::gpsFix() const { return gpsFixValue; }
int Vehicle::satellites() const { return satellitesValue; }
double Vehicle::gpsHdop() const { return gpsHdopValue; }
double Vehicle::gpsVdop() const { return gpsVdopValue; }
double Vehicle::gpsHorizontalAccuracy() const { return gpsHorizontalAccuracyValue; }
double Vehicle::gpsVerticalAccuracy() const { return gpsVerticalAccuracyValue; }
double Vehicle::gpsSpeedAccuracy() const { return gpsSpeedAccuracyValue; }
double Vehicle::gpsHeadingAccuracy() const { return gpsHeadingAccuracyValue; }
double Vehicle::gpsGroundSpeed() const { return gpsGroundSpeedValue; }
double Vehicle::gpsCourse() const { return gpsCourseValue; }
double Vehicle::gpsAltitude() const { return gpsAltitudeValue; }

double Vehicle::vx() const { return vxValue; }
double Vehicle::vy() const { return vyValue; }
double Vehicle::vz() const { return vzValue; }
int Vehicle::commLinkQuality() const { return mavlinkObject ? mavlinkObject->estimatedLinkQuality() : -1; }
QString Vehicle::commLinkCategory() const { return mavlinkObject ? mavlinkObject->linkQualityCategory() : QString(); }
QVariantMap Vehicle::commLinkDiagnostics() const { return mavlinkObject ? mavlinkObject->linkQualityDiagnostics() : QVariantMap(); }
int Vehicle::radioRssi() const { return mavlinkObject ? mavlinkObject->rawRadioRssi() : -1; }
// These counters do not measure total packets; a true loss rate is unavailable.
double Vehicle::radioPacketLoss() const { return -1; }
void Vehicle::updateCommLinkQuality() { emit commLinkQualityChanged(); }

void Vehicle::setHomeLocation(double latitude, double longitude)
{
    if (!qIsFinite(latitude) || !qIsFinite(longitude)
            || latitude < -90.0 || latitude > 90.0
            || longitude < -180.0 || longitude > 180.0
            || (qFuzzyIsNull(latitude) && qFuzzyIsNull(longitude))) {
        return;
    }

    homeLatitude = latitude;
    homeLongitude = longitude;
    homeSet = true;

    if (latitudeValue != 0 && longitudeValue != 0) {
        distanceToHomeValue = distanceMeters(homeLatitude, homeLongitude, latitudeValue, longitudeValue);
    } else {
        distanceToHomeValue = 0;
    }
    emit distanceToHomeChanged();
}

void Vehicle::handleMessage(const mavlink_message_t &msg)
{
    // Protect selected autopilot motion/arming data without suppressing legitimate
    // radio, payload or other-component messages handled elsewhere in this model.
    const bool motionMessage = msg.msgid == MAVLINK_MSG_ID_HEARTBEAT
            || msg.msgid == MAVLINK_MSG_ID_EXTENDED_SYS_STATE
            || msg.msgid == MAVLINK_MSG_ID_GLOBAL_POSITION_INT
            || msg.msgid == MAVLINK_MSG_ID_ATTITUDE || msg.msgid == MAVLINK_MSG_ID_VFR_HUD;
    if (motionMessage && mavlinkObject->selectedSystemId() != 0
            && (msg.sysid != mavlinkObject->selectedSystemId()
                || msg.compid != mavlinkObject->selectedComponentId())) return;
    updateTelemetryState(msg);
    switch (msg.msgid)
    {
    case MAVLINK_MSG_ID_HEARTBEAT:
    {
        mavlink_heartbeat_t hb;
        mavlink_msg_heartbeat_decode(&msg, &hb);

        if (hb.type == MAV_TYPE_GCS || hb.autopilot == MAV_AUTOPILOT_INVALID) {
            break;
        }

        bool newArmed = hb.base_mode & MAV_MODE_FLAG_SAFETY_ARMED;

        if (armedState != newArmed) {
            armedState = newArmed;
            emit armedChanged();
        }

        QString newVehicleType = QStringLiteral("UNKNOWN");
        int newMotorCount = 4;
        switch (hb.type) {
        case MAV_TYPE_TRICOPTER:
            newVehicleType = QStringLiteral("TRICOPTER");
            newMotorCount = 3;
            break;
        case MAV_TYPE_QUADROTOR:
        case MAV_TYPE_VTOL_TAILSITTER_QUADROTOR:
            newVehicleType = QStringLiteral("QUADROTOR");
            newMotorCount = 4;
            break;
        case MAV_TYPE_HEXAROTOR:
            newVehicleType = QStringLiteral("HEXAROTOR");
            newMotorCount = 6;
            break;
        case MAV_TYPE_OCTOROTOR:
            newVehicleType = QStringLiteral("OCTOROTOR");
            newMotorCount = 8;
            break;
        case MAV_TYPE_DODECAROTOR:
            newVehicleType = QStringLiteral("DODECAROTOR");
            newMotorCount = 12;
            break;
        case MAV_TYPE_COAXIAL:
            newVehicleType = QStringLiteral("COAXIAL");
            newMotorCount = 2;
            break;
        case MAV_TYPE_HELICOPTER:
            newVehicleType = QStringLiteral("HELICOPTER");
            newMotorCount = 1;
            break;
        default:
            newVehicleType = QStringLiteral("TYPE %1").arg(hb.type);
            break;
        }

        if (vehicleTypeValue != newVehicleType || motorCountValue != newMotorCount) {
            vehicleTypeValue = newVehicleType;
            motorCountValue = newMotorCount;
            emit vehicleTypeChanged();
        }

        QString newFlightMode = "UNKNOWN";
        switch (hb.custom_mode) {
        case 0: newFlightMode = "STABILIZE"; break;
        case 2: newFlightMode = "ALT HOLD"; break;
        case 3: newFlightMode = "AUTO"; break;
        case 4: newFlightMode = "GUIDED"; break;
        case 5: newFlightMode = "LOITER"; break;
        case 6: newFlightMode = "RTL"; break;
        case 9: newFlightMode = "LAND"; break;
        case 11: newFlightMode = "DRIFT"; break;
        case 13: newFlightMode = "SPORT"; break;
        case 14: newFlightMode = "FLIP"; break;
        case 15: newFlightMode = "AUTOTUNE"; break;
        case 16: newFlightMode = "POSHOLD"; break;
        case 17: newFlightMode = "BRAKE"; break;
        case 18: newFlightMode = "THROW"; break;
        case 19: newFlightMode = "AVOID ADSB"; break;
        case 20: newFlightMode = "GUIDED NOGPS"; break;
        case 21: newFlightMode = "SMART RTL"; break;
        default: newFlightMode = QString("MODE %1").arg(hb.custom_mode); break;
        }

        if (flightModeValue != newFlightMode) {
            flightModeValue = newFlightMode;
            emit flightModeChanged();
        }

        if (flightStarted) {
            emit flightTimeChanged();
        }

        emit mavlinkMessageReceived("HEARTBEAT",
                                   QString("%1 %2").arg(armedState ? "ARMED" : "DISARMED", flightModeValue));
        break;
    }

    case MAVLINK_MSG_ID_ATTITUDE:
    {
        mavlink_attitude_t att;
        mavlink_msg_attitude_decode(&msg, &att);

        rollValue  = att.roll;
        pitchValue = att.pitch;
        yawValue   = att.yaw;

        emit rollChanged();
        emit pitchChanged();
        emit yawChanged();

        emit mavlinkMessageReceived(
            "ATTITUDE",
            QString("roll=%1 pitch=%2 yaw=%3")
                .arg(rollValue, 0, 'f', 2)
                .arg(pitchValue, 0, 'f', 2)
                .arg(yawValue, 0, 'f', 2)
        );
        break;
    }

    case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
    {
        mavlink_global_position_int_t pos;
        mavlink_msg_global_position_int_decode(&msg, &pos);

        latitudeValue  = pos.lat / 1e7;
        longitudeValue = pos.lon / 1e7;
        altitudeValue  = pos.relative_alt / 1000.0;

        if (!homeSet && latitudeValue != 0 && longitudeValue != 0) {
            homeLatitude = latitudeValue;
            homeLongitude = longitudeValue;
            homeSet = true;
        }

        if (homeSet) {
            distanceToHomeValue = distanceMeters(homeLatitude, homeLongitude, latitudeValue, longitudeValue);
            emit distanceToHomeChanged();
        }

        if (flightStarted && lastPositionSet && latitudeValue != 0 && longitudeValue != 0) {
            const double segmentDistance = distanceMeters(lastLatitude, lastLongitude, latitudeValue, longitudeValue);
            if (segmentDistance > 0.05 && segmentDistance < 100.0) {
                flightDistanceValue += segmentDistance;
                emit flightDistanceChanged();
            }
        }
        if (latitudeValue != 0 && longitudeValue != 0) {
            lastLatitude = latitudeValue;
            lastLongitude = longitudeValue;
            lastPositionSet = true;
        }

        emit positionChanged();

        vxValue = pos.vx / 100.0;
        vyValue = pos.vy / 100.0;
        vzValue = pos.vz / 100.0;

        emit velocityChanged();

        emit mavlinkMessageReceived(
            "GLOBAL_POSITION",
            QString("lat=%1 lon=%2 alt=%3")
                .arg(latitudeValue, 0, 'f', 6)
                .arg(longitudeValue, 0, 'f', 6)
                .arg(altitudeValue, 0, 'f', 2)
        );
        break;
    }

    case MAVLINK_MSG_ID_SYS_STATUS:
    {
        mavlink_sys_status_t sys;
        mavlink_msg_sys_status_decode(&msg, &sys);

        const int newBattery = sys.battery_remaining >= 0 ? sys.battery_remaining : -1;
        const double newVoltage = sys.voltage_battery > 0 ? sys.voltage_battery / 1000.0 : -1;
        const double newCurrent = sys.current_battery >= 0 ? sys.current_battery / 100.0 : -1;
        if (batteryValue != newBattery
                || !qFuzzyCompare(batteryVoltageValue + 1, newVoltage + 1)
                || !qFuzzyCompare(batteryCurrentValue + 1, newCurrent + 1)) {
            batteryValue = newBattery;
            batteryVoltageValue = newVoltage;
            batteryCurrentValue = newCurrent;
            emit batteryChanged();
        }

        emit mavlinkMessageReceived(
            "SYS_STATUS",
            QString("battery=%1% voltage=%2V current=%3A").arg(batteryValue).arg(batteryVoltageValue).arg(batteryCurrentValue)
        );
        break;
    }

    case MAVLINK_MSG_ID_BATTERY_STATUS:
    {
        mavlink_battery_status_t battery;
        mavlink_msg_battery_status_decode(&msg, &battery);

        const int newBattery = battery.battery_remaining >= 0 ? battery.battery_remaining : -1;
        const double newVoltage = battery.voltages[0] != UINT16_MAX && battery.voltages[0] > 0 ? battery.voltages[0] / 1000.0 : -1;
        const double newCurrent = battery.current_battery >= 0 ? battery.current_battery / 100.0 : -1;
        if (batteryValue != newBattery
                || !qFuzzyCompare(batteryVoltageValue + 1, newVoltage + 1)
                || !qFuzzyCompare(batteryCurrentValue + 1, newCurrent + 1)) {
            batteryValue = newBattery;
            batteryVoltageValue = newVoltage;
            batteryCurrentValue = newCurrent;
            emit batteryChanged();
        }

        emit mavlinkMessageReceived(
            "BATTERY_STATUS",
            QString("battery=%1% voltage=%2V current=%3A").arg(batteryValue).arg(batteryVoltageValue).arg(batteryCurrentValue)
        );
        break;
    }

    case MAVLINK_MSG_ID_VFR_HUD:
    {
        mavlink_vfr_hud_t vfr;
        mavlink_msg_vfr_hud_decode(&msg, &vfr);

        airspeedValue    = vfr.airspeed;
        groundspeedValue = vfr.groundspeed;
        headingValue     = vfr.heading;
        throttleValue    = vfr.throttle;
        climbValue       = vfr.climb;

        emit vfrHudChanged();

        emit mavlinkMessageReceived(
            "VFR_HUD",
            QString("as=%1 gs=%2 hdg=%3 thr=%4")
                .arg(airspeedValue)
                .arg(groundspeedValue)
                .arg(headingValue)
                .arg(throttleValue)
        );
        break;
    }

    case MAVLINK_MSG_ID_GPS_RAW_INT:
    {
        mavlink_gps_raw_int_t gps;
        mavlink_msg_gps_raw_int_decode(&msg, &gps);

        gpsFixValue = gps.fix_type;
        satellitesValue = gps.satellites_visible == UINT8_MAX ? -1 : gps.satellites_visible;
        gpsHdopValue = gps.eph == UINT16_MAX ? -1 : gps.eph / 100.0;
        gpsVdopValue = gps.epv == UINT16_MAX ? -1 : gps.epv / 100.0;
        gpsHorizontalAccuracyValue = gps.h_acc == UINT32_MAX ? -1 : gps.h_acc / 1000.0;
        gpsVerticalAccuracyValue = gps.v_acc == UINT32_MAX ? -1 : gps.v_acc / 1000.0;
        gpsSpeedAccuracyValue = gps.vel_acc == UINT32_MAX ? -1 : gps.vel_acc / 1000.0;
        gpsHeadingAccuracyValue = gps.hdg_acc == UINT32_MAX ? -1 : gps.hdg_acc / 100000.0;
        gpsGroundSpeedValue = gps.vel == UINT16_MAX ? -1 : gps.vel / 100.0;
        gpsCourseValue = gps.cog == UINT16_MAX ? -1 : gps.cog / 100.0;
        gpsAltitudeValue = gps.alt == INT32_MAX ? -1 : gps.alt / 1000.0;

        emit gpsChanged();

        emit mavlinkMessageReceived(
            "GPS",
            QString("fix=%1 sats=%2 hdop=%3 vdop=%4")
                .arg(gpsFixValue)
                .arg(satellitesValue)
                .arg(gpsHdopValue)
                .arg(gpsVdopValue)
        );
        break;
    }

    case MAVLINK_MSG_ID_STATUSTEXT:
    {
        mavlink_statustext_t txt;
        mavlink_msg_statustext_decode(&msg, &txt);

        const QByteArray rawText(txt.text, MAVLINK_MSG_STATUSTEXT_FIELD_TEXT_LEN);
        const int terminatorIndex = rawText.indexOf('\0');
        const int textLength = terminatorIndex >= 0 ? terminatorIndex : rawText.size();
        QString text = QString::fromLatin1(rawText.constData(), textLength).trimmed();

        emit statusTextReceived(text);

        emit mavlinkMessageReceived("STATUSTEXT", text);
        break;
    }

    default:
        break;
    }
}
