#include "Mavlink.h"

#include "mavlink/MavlinkInspectorController.h"
#include "mavlink/MavlinkForwarder.h"

#include "transport/serial/SerialLink.h"
#include "transport/tcp/TcpLink.h"
#include "transport/udp/UdpLink.h"
#include "storage/AppDataPaths.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QSettings>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QVariantMap>
#include <QtConcurrent>
#include <QtMath>
#include <QtGlobal>

#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace {

constexpr int simultaneousMotorCommandDelayMs = 80;
constexpr int terrainRetryIntervalMs = 3000;

bool isDisplayableNavigationCommand(uint16_t command)
{
    switch (command) {
    case MAV_CMD_NAV_WAYPOINT:
    case MAV_CMD_NAV_LOITER_UNLIM:
    case MAV_CMD_NAV_LOITER_TURNS:
    case MAV_CMD_NAV_LOITER_TIME:
    case MAV_CMD_NAV_RETURN_TO_LAUNCH:
    case MAV_CMD_NAV_LAND:
    case MAV_CMD_NAV_TAKEOFF:
    case MAV_CMD_NAV_SPLINE_WAYPOINT:
    case MAV_CMD_NAV_VTOL_TAKEOFF:
    case MAV_CMD_NAV_VTOL_LAND:
        return true;
    default:
        return false;
    }
}

bool isGlobalFrame(uint8_t frame)
{
    switch (frame) {
    case MAV_FRAME_GLOBAL:
    case MAV_FRAME_GLOBAL_INT:
    case MAV_FRAME_GLOBAL_RELATIVE_ALT:
    case MAV_FRAME_GLOBAL_RELATIVE_ALT_INT:
    case MAV_FRAME_GLOBAL_TERRAIN_ALT:
    case MAV_FRAME_GLOBAL_TERRAIN_ALT_INT:
        return true;
    default:
        return false;
    }
}

bool isRelativeAltitudeFrame(uint8_t frame)
{
    return frame == MAV_FRAME_GLOBAL_RELATIVE_ALT
            || frame == MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;
}

bool isTerrainAltitudeFrame(uint8_t frame)
{
    return frame == MAV_FRAME_GLOBAL_TERRAIN_ALT
            || frame == MAV_FRAME_GLOBAL_TERRAIN_ALT_INT;
}

bool hasValidCoordinate(double latitude, double longitude)
{
    return qIsFinite(latitude) && qIsFinite(longitude)
            && latitude >= -90.0 && latitude <= 90.0
            && longitude >= -180.0 && longitude <= 180.0
            && !(qFuzzyIsNull(latitude) && qFuzzyIsNull(longitude));
}

int navigationDisplayPriority(uint16_t command)
{
    switch (command) {
    case MAV_CMD_NAV_TAKEOFF:
        return 4;
    case MAV_CMD_NAV_LAND:
    case MAV_CMD_NAV_RETURN_TO_LAUNCH:
        return 3;
    case MAV_CMD_NAV_WAYPOINT:
    case MAV_CMD_NAV_SPLINE_WAYPOINT:
        return 2;
    default:
        return 1;
    }
}

double distanceMeters(double firstLat, double firstLon, double secondLat, double secondLon)
{
    constexpr double earthRadiusMeters = 6371000.0;
    const double firstLatRadians = qDegreesToRadians(firstLat);
    const double secondLatRadians = qDegreesToRadians(secondLat);
    const double deltaLatRadians = qDegreesToRadians(secondLat - firstLat);
    const double deltaLonRadians = qDegreesToRadians(secondLon - firstLon);

    const double a = qSin(deltaLatRadians / 2.0) * qSin(deltaLatRadians / 2.0)
            + qCos(firstLatRadians) * qCos(secondLatRadians)
            * qSin(deltaLonRadians / 2.0) * qSin(deltaLonRadians / 2.0);
    const double c = 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
    return earthRadiusMeters * c;
}

double metersToLatitudeDegrees(double meters)
{
    constexpr double metersPerDegreeLatitude = 111320.0;
    return meters / metersPerDegreeLatitude;
}

double metersToLongitudeDegrees(double meters, double latitude)
{
    constexpr double metersPerDegreeAtEquator = 111320.0;
    const double scale = qMax(0.01, qCos(qDegreesToRadians(latitude)));
    return meters / (metersPerDegreeAtEquator * scale);
}

QVariantMap makeDisplayWaypoint(uint16_t seq, uint16_t command, uint8_t frame, double lat, double lon, float alt)
{
    QVariantMap waypoint;
    waypoint["lat"] = lat;
    waypoint["lon"] = lon;
    waypoint["alt"] = alt;
    waypoint["frame"] = frame;
    waypoint["seq"] = seq + 1;
    waypoint["command"] = command;
    waypoint["hasCoordinate"] = true;
    return waypoint;
}

QVariantMap makeRcChannel(int channel, uint16_t value)
{
    QVariantMap rcChannel;
    rcChannel.insert(QStringLiteral("channel"), channel);
    rcChannel.insert(QStringLiteral("value"), value == UINT16_MAX || value == 0 ? -1 : int(value));
    return rcChannel;
}

} // namespace

class Mavlink::BufferedLogWriter
{
public:
    BufferedLogWriter()
    {
        workerContext.moveToThread(&workerThread);
        workerThread.start();
    }

    ~BufferedLogWriter()
    {
        stop();
        workerThread.quit();
        workerThread.wait();
    }

    bool start(const QString &path, QIODevice::OpenMode mode, QString *error)
    {
        if (open) {
            return true;
        }

        bool opened = false;
        QString openError;
        QMetaObject::invokeMethod(&workerContext, [this, &opened, &openError, path, mode]() {
            file.setFileName(path);
            opened = file.open(mode);
            if (!opened) {
                openError = file.errorString();
            }
        }, Qt::BlockingQueuedConnection);

        open = opened;
        if (!opened && error) {
            *error = openError;
        }
        return opened;
    }

    void append(const QByteArray &data)
    {
        if (!open || data.isEmpty()) {
            return;
        }

        bool scheduleDrain = false;
        {
            QMutexLocker locker(&mutex);
            buffer.append(data);
            if (!drainScheduled) {
                drainScheduled = true;
                scheduleDrain = true;
            }
        }

        if (scheduleDrain) {
            QMetaObject::invokeMethod(&workerContext, [this]() { drain(); }, Qt::QueuedConnection);
        }
    }

    void stop()
    {
        if (!open) {
            return;
        }

        QMetaObject::invokeMethod(&workerContext, [this]() {
            drain();
            file.flush();
            file.close();
        }, Qt::BlockingQueuedConnection);
        open = false;
    }

    bool isOpen() const
    {
        return open;
    }

private:
    void drain()
    {
        QByteArray pending;
        {
            QMutexLocker locker(&mutex);
            pending.swap(buffer);
        }

        if (!pending.isEmpty() && file.isOpen()) {
            file.write(pending);
        }

        bool scheduleAnotherDrain = false;
        {
            QMutexLocker locker(&mutex);
            if (buffer.isEmpty()) {
                drainScheduled = false;
            } else {
                scheduleAnotherDrain = true;
            }
        }

        if (scheduleAnotherDrain) {
            QMetaObject::invokeMethod(&workerContext, [this]() { drain(); }, Qt::QueuedConnection);
        }
    }

    QThread workerThread;
    QObject workerContext;
    QFile file;
    QMutex mutex;
    QByteArray buffer;
    bool drainScheduled = false;
    bool open = false;
};

Mavlink::Mavlink(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<MavlinkFrame>("MavlinkFrame");
    inspectorController = new MavlinkInspectorController(this, this);
    forwarderObject = new MavlinkForwarder(this);
    connect(this, &Mavlink::frameObserved, forwarderObject, &MavlinkForwarder::forwardFrame);
    telemetryLogWriter = std::make_unique<BufferedLogWriter>();
    flightControllerLogWriter = std::make_unique<BufferedLogWriter>();
    QSettings settings;
    telemetryLoggingEnabledState = settings.value(QStringLiteral("Log/autoTelemetryLoggingEnabled"), true).toBool();
    flightControllerLoggingEnabledState = settings.value(QStringLiteral("General/flightControllerLoggingEnabled"), true).toBool();

    serialPortRefreshWatcher = new QFutureWatcher<QStringList>(this);
    connect(serialPortRefreshWatcher, &QFutureWatcher<QStringList>::finished, this, [this]() {
        const QStringList ports = serialPortRefreshWatcher->result();
        serialPortRefreshActive = false;
        if (serialPortNames == ports) {
            return;
        }

        serialPortNames = ports;
        emit serialPortsChanged();
    });

    setLink(new TcpLink(this));
    refreshSerialPorts();

    QTimer *serialPortRefreshTimer = new QTimer(this);
    serialPortRefreshTimer->setInterval(1500);
    connect(serialPortRefreshTimer, &QTimer::timeout, this, &Mavlink::refreshSerialPorts);
    serialPortRefreshTimer->start();

    motorTestSequenceTimer = new QTimer(this);
    motorTestSequenceTimer->setSingleShot(true);
    connect(motorTestSequenceTimer, &QTimer::timeout, this, &Mavlink::runNextSequentialMotorTest);

    motorTestStopTimer = new QTimer(this);
    motorTestStopTimer->setSingleShot(true);
    connect(motorTestStopTimer, &QTimer::timeout, this, [this]() {
        const int stopCount = qBound(1, activeMotorTestStopCount, 12);
        activeMotorTestStopCount = 0;
        stopAllMotorTests(stopCount);
    });

    terrainRetryTimer = new QTimer(this);
    terrainRetryTimer->setInterval(terrainRetryIntervalMs);
    connect(terrainRetryTimer, &QTimer::timeout, this, &Mavlink::retryPendingTerrainRequests);
    connect(&terrainProvider, &TerrainProvider::tileAvailable, this, [this]() {
        if (!pendingTerrainBlocks.isEmpty()) {
            retryPendingTerrainRequests();
        }

        const QVariantList &profileWaypoints = !loadedMissionElevationWaypoints.isEmpty()
                ? loadedMissionElevationWaypoints
                : loadedMissionWaypoints;
        if (!profileWaypoints.isEmpty()) {
            loadedMissionTerrainProfile = buildMissionTerrainProfile(profileWaypoints);
            emit missionTerrainProfileChanged();
        }
    });
    connect(&terrainProvider, &TerrainProvider::tileDownloadFailed, this,
            [](int baseLatitude, int baseLongitude, const QString &reason) {
        qWarning() << "Terrain tile download failed"
                   << baseLatitude << baseLongitude << reason;
    });

    parameterFetchIdleTimer = new QTimer(this);
    parameterFetchIdleTimer->setSingleShot(true);
    parameterFetchIdleTimer->setInterval(5000);
    connect(parameterFetchIdleTimer, &QTimer::timeout, this, [this]() {
        finishParameterFetch(expectedParameterCount > 0 && parameterCache.size() >= expectedParameterCount);
    });

    missingParameterRetryTimer = new QTimer(this);
    missingParameterRetryTimer->setSingleShot(true);
    missingParameterRetryTimer->setInterval(1800);
    connect(missingParameterRetryTimer, &QTimer::timeout, this, &Mavlink::requestMissingParameters);

    linkPingClock.start();
    linkPingTimer = new QTimer(this);
    linkPingTimer->setInterval(1000);
    connect(linkPingTimer, &QTimer::timeout, this, &Mavlink::sendLinkPing);
}

bool Mavlink::connected() const
{
    return connectedState;
}

int Mavlink::linkLatencyMs() const
{
    return linkLatencyMsValue;
}

int Mavlink::mavlinkQuality() const
{
    return mavlinkQualityValue < 0 ? -1 : qRound(mavlinkQualityValue);
}

QVariantList Mavlink::missionWaypoints() const
{
    return loadedMissionWaypoints;
}

QVariantList Mavlink::missionElevationWaypoints() const
{
    return loadedMissionElevationWaypoints;
}

QVariantList Mavlink::missionTerrainProfile() const
{
    return loadedMissionTerrainProfile;
}

Mavlink::~Mavlink() = default;

QVariantMap Mavlink::uploadedHomeLocation() const
{
    return loadedUploadedHomeLocation;
}

bool Mavlink::uploadedHomeLocationAvailable() const
{
    return !loadedUploadedHomeLocation.isEmpty();
}

QString Mavlink::missionStatus() const
{
    return missionStatusText;
}


bool Mavlink::missionUploaded() const
{
    return missionUploadedState;
}

QStringList Mavlink::serialPorts() const
{
    return serialPortNames;
}

bool Mavlink::parameterFetchInProgress() const
{
    return parameterFetchActive;
}

int Mavlink::parameterFetchProgress() const
{
    if (expectedParameterCount <= 0) {
        return parameterFetchActive ? 1 : 0;
    }

    return qBound(0, qRound((parameterCache.size() * 100.0) / expectedParameterCount), 100);
}

int Mavlink::parameterCount() const
{
    return expectedParameterCount > 0 ? expectedParameterCount : 0;
}

int Mavlink::loadedParameterCount() const
{
    return parameterCache.size();
}

QVariantList Mavlink::rcChannels() const
{
    return latestRcChannels;
}

bool Mavlink::telemetryLoggingEnabled() const
{
    return telemetryLoggingEnabledState;
}

bool Mavlink::telemetryLogActive() const
{
    return telemetryLogWriter->isOpen();
}

QString Mavlink::telemetryLogDirectory() const
{
    return telemetryLogDirectoryPath.isEmpty() ? defaultTelemetryLogDirectory() : telemetryLogDirectoryPath;
}

QString Mavlink::currentTelemetryLogPath() const
{
    return currentTelemetryLogFilePath;
}

bool Mavlink::flightControllerLoggingEnabled() const
{
    return flightControllerLoggingEnabledState;
}

bool Mavlink::flightControllerLogActive() const
{
    return flightControllerLogWriter->isOpen();
}

QString Mavlink::flightControllerLogDirectory() const
{
    return flightControllerLogDirectoryPath.isEmpty() ? defaultFlightControllerLogDirectory() : flightControllerLogDirectoryPath;
}

QString Mavlink::currentFlightControllerLogPath() const
{
    return currentFlightControllerLogFilePath;
}

QObject *Mavlink::inspector() const
{
    return inspectorController;
}

QObject *Mavlink::forwarder() const
{
    return forwarderObject;
}

bool Mavlink::connectToVehicle(QString ip, int port)
{
    return connectToVehicleProtocol(QStringLiteral("TCP"), ip, port);
}

bool Mavlink::connectToVehicleProtocol(QString protocol, QString hostOrPortName, int port, int baudRate)
{
    const QString normalizedProtocol = protocol.trimmed().toUpper();

    if (normalizedProtocol == QStringLiteral("SERIAL")) {
        refreshSerialPorts();
        if (!serialPortNames.contains(hostOrPortName)) {
            setConnected(false);
            qDebug() << "Serial port unavailable:" << hostOrPortName;
            return false;
        }
        setLink(new SerialLink(this));
        port = baudRate;
    } else if (normalizedProtocol == QStringLiteral("UDP")) {
        setLink(new UdpLink(this));
    } else {
        setLink(new TcpLink(this));
    }

    if (link->connectLink(hostOrPortName, port, 3000)) {
        qDebug() << "Connection started via" << normalizedProtocol << "to" << hostOrPortName << port;
        return true;
    } else {
        setConnected(false);
        qDebug() << "Connection failed:" << link->errorString();
        return false;
    }
}

void Mavlink::disconnectFromVehicle()
{
    link->disconnectLink();
    stopTelemetryLog();
    stopFlightControllerLog();
    setMissionUploaded(false);
    missionUploadActive = false;
    missionClearBeforeUploadActive = false;
    missionDownloadActive = false;
    missionDownloadRequestedOnConnect = false;
    missionSaveRequestedAfterDownload = false;
    missionDownloadExpectedCount = 0;
    downloadingMissionItems.clear();
    downloadedMissionItemSeqs.clear();
    clearPendingTerrainRequests();
    targetSystemId = 1;
    targetComponentId = MAV_COMP_ID_AUTOPILOT1;
    resetParameterCache();
    setConnected(false);
}

void Mavlink::refreshSerialPorts()
{
    if (serialPortRefreshActive) {
        return;
    }

    serialPortRefreshActive = true;
    serialPortRefreshWatcher->setFuture(QtConcurrent::run([]() {
        QStringList ports;
        const QList<QSerialPortInfo> availablePorts = QSerialPortInfo::availablePorts();
        ports.reserve(availablePorts.size());
        for (const QSerialPortInfo &portInfo : availablePorts) {
            ports.append(portInfo.portName());
        }
        return ports;
    }));
}

void Mavlink::armVehicle(bool arm)
{
    sendCommandLong(MAV_CMD_COMPONENT_ARM_DISARM, arm ? 1.0f : 0.0f);
}

void Mavlink::takeoff(double altitudeMeters)
{
    if (!connectedState) {
        qDebug() << "Cannot start takeoff while disconnected";
        return;
    }

    const double safeAltitudeMeters = qBound(1.0, altitudeMeters, 100.0);
    sendSetMode(4);

    QTimer::singleShot(1200, this, [this, safeAltitudeMeters]() {
        if (!connectedState) {
            return;
        }

        sendCommandLong(MAV_CMD_NAV_TAKEOFF, 0, 0, 0, 0, 0, 0, static_cast<float>(safeAltitudeMeters));
    });
}

void Mavlink::startMission()
{
    if (!connectedState) {
        qDebug() << "Cannot start mission while disconnected";
        return;
    }

    sendSetMode(3);
    QTimer::singleShot(900, this, [this]() {
        if (!connectedState) {
            return;
        }

        sendCommandLong(MAV_CMD_MISSION_START);
    });
}

void Mavlink::returnToLaunch()
{
    sendCommandLong(MAV_CMD_NAV_RETURN_TO_LAUNCH);
}

void Mavlink::land()
{
    sendCommandLong(MAV_CMD_NAV_LAND);
}

void Mavlink::emergencyStop()
{
    sendCommandLong(MAV_CMD_COMPONENT_ARM_DISARM, 0, 21196);
}

void Mavlink::setFlightMode(int customMode)
{
    if (customMode < 0) {
        return;
    }

    sendSetMode(static_cast<uint32_t>(customMode));
}

void Mavlink::motor()
{
    armVehicle(connectedState);
}

void Mavlink::testMotor(int motor, int throttlePercent, int durationSeconds)
{
    stopMotorTestSequence();
    sendMotorTestCommand(QStringLiteral("single"), motor, throttlePercent, durationSeconds);
}

void Mavlink::sendMotorTestCommand(const QString &mode, int motor, int throttlePercent, int durationSeconds, int motorCount)
{
    if (!connectedState || motor < 1) {
        return;
    }

    const int safeThrottle = qBound(1, throttlePercent, 20);
    const int safeDuration = qBound(1, durationSeconds, 30);
    const int safeMotorCount = qBound(0, motorCount, 12);

    qDebug() << "Motor test command:"
             << "motor" << motor
             << "throttlePercent" << safeThrottle
             << "durationSeconds" << safeDuration
             << "motorCount" << safeMotorCount;

    PendingMotorTestAck pending;
    pending.command = MAV_CMD_DO_MOTOR_TEST;
    pending.mode = mode;
    pending.firstMotor = motor;
    pending.motorCount = safeMotorCount > 0 ? safeMotorCount : 1;
    pending.durationSeconds = safeDuration;
    pending.sequenceId = nextMotorTestSequenceId++;
    pendingMotorTestAcks.append(pending);

    sendCommandLong(MAV_CMD_DO_MOTOR_TEST,
                    static_cast<float>(motor),
                    static_cast<float>(MOTOR_TEST_THROTTLE_PERCENT),
                    static_cast<float>(safeThrottle),
                    static_cast<float>(safeDuration),
                    static_cast<float>(safeMotorCount),
                    static_cast<float>(MOTOR_TEST_ORDER_DEFAULT));
}

void Mavlink::sendActuatorMotorTestCommand(const QString &mode, int motor, int throttlePercent, int durationSeconds)
{
    if (!connectedState || motor < 1) {
        return;
    }

    const int safeMotor = qBound(1, motor, 16);
    const int safeThrottle = qBound(1, throttlePercent, 20);
    const int safeDuration = qBound(1, durationSeconds, 30);
    const int outputFunction = ACTUATOR_OUTPUT_FUNCTION_MOTOR1 + safeMotor - 1;
    const float outputValue = static_cast<float>(safeThrottle) / 100.0f;

    qDebug() << "Actuator motor test command:"
             << "motor" << safeMotor
             << "throttlePercent" << safeThrottle
             << "durationSeconds" << safeDuration
             << "outputFunction" << outputFunction;

    PendingMotorTestAck pending;
    pending.command = MAV_CMD_ACTUATOR_TEST;
    pending.mode = mode;
    pending.firstMotor = safeMotor;
    pending.motorCount = 1;
    pending.durationSeconds = safeDuration;
    pending.sequenceId = nextMotorTestSequenceId++;
    pendingMotorTestAcks.append(pending);

    sendCommandLong(MAV_CMD_ACTUATOR_TEST,
                    outputValue,
                    static_cast<float>(safeDuration),
                    0.0f,
                    0.0f,
                    static_cast<float>(outputFunction),
                    0.0f,
                    0.0f);
}

void Mavlink::sendActuatorMotorStopCommand(int motor)
{
    if (!connectedState || motor < 1) {
        return;
    }

    const int safeMotor = qBound(1, motor, 16);
    const int outputFunction = ACTUATOR_OUTPUT_FUNCTION_MOTOR1 + safeMotor - 1;
    sendCommandLong(MAV_CMD_ACTUATOR_TEST,
                    std::numeric_limits<float>::quiet_NaN(),
                    0.0f,
                    0.0f,
                    0.0f,
                    static_cast<float>(outputFunction),
                    0.0f,
                    0.0f);
}

void Mavlink::runAllMotorTest(int throttlePercent, int durationSeconds, int motorCount)
{
    stopMotorTestSequence();

    const int safeDuration = qBound(1, durationSeconds, 30);
    const int safeMotorCount = qBound(1, qMax(motorCount, 4), 12);
    const int safeThrottle = qBound(1, throttlePercent, 20);
    const int commandGeneration = motorTestCommandGeneration;

    qDebug() << "Run all motor test:"
             << "throttlePercent" << safeThrottle
             << "durationSeconds" << safeDuration
             << "motorCount" << safeMotorCount;

    for (int motor = 1; motor <= safeMotorCount; ++motor) {
        QTimer::singleShot((motor - 1) * simultaneousMotorCommandDelayMs,
                           this,
                           [this, commandGeneration, motor, safeThrottle, safeDuration]() {
            if (!connectedState || commandGeneration != motorTestCommandGeneration) {
                return;
            }

            sendMotorTestCommand(QStringLiteral("all"), motor, safeThrottle, safeDuration);
        });
    }

    activeMotorTestStopCount = safeMotorCount;
    motorTestStopTimer->start((safeDuration * 1000)
                              + ((safeMotorCount - 1) * simultaneousMotorCommandDelayMs)
                              + 250);
}

void Mavlink::runSequentialMotorTest(int throttlePercent, int durationSeconds, int motorCount)
{
    stopMotorTestSequence();

    motorTestSequenceNextMotor = 1;
    motorTestSequenceMotorCount = qBound(1, motorCount, 12);
    motorTestSequenceThrottle = qBound(1, throttlePercent, 20);
    motorTestSequenceDuration = qBound(1, durationSeconds, 30);

    runNextSequentialMotorTest();
}

void Mavlink::runNextSequentialMotorTest()
{
    if (!connectedState || motorTestSequenceNextMotor > motorTestSequenceMotorCount) {
        stopMotorTestSequence();
        return;
    }

    qDebug() << "Run sequential motor test:"
             << "motor" << motorTestSequenceNextMotor
             << "of" << motorTestSequenceMotorCount
             << "throttlePercent" << motorTestSequenceThrottle
             << "durationSeconds" << motorTestSequenceDuration;

    sendMotorTestCommand(QStringLiteral("sequential"), motorTestSequenceNextMotor,
                         motorTestSequenceThrottle, motorTestSequenceDuration);
    ++motorTestSequenceNextMotor;

    if (motorTestSequenceNextMotor <= motorTestSequenceMotorCount) {
        motorTestSequenceTimer->start((motorTestSequenceDuration * 1000) + 250);
    }
}

void Mavlink::stopMotorTestSequence()
{
    ++motorTestCommandGeneration;

    if (motorTestSequenceTimer) {
        motorTestSequenceTimer->stop();
    }
    if (motorTestStopTimer) {
        motorTestStopTimer->stop();
    }

    motorTestSequenceNextMotor = 1;
    motorTestSequenceMotorCount = 0;
    activeMotorTestStopCount = 0;
    pendingMotorTestAcks.clear();
}

void Mavlink::stopAllMotorTests(int motorCount)
{
    stopMotorTestSequence();
    const int safeMotorCount = qBound(1, motorCount, 12);

    for (int motor = 1; motor <= safeMotorCount; ++motor) {
        sendActuatorMotorStopCommand(motor);

        PendingMotorTestAck pending;
        pending.command = MAV_CMD_DO_MOTOR_TEST;
        pending.mode = QStringLiteral("stop");
        pending.firstMotor = motor;
        pending.motorCount = 1;
        pending.durationSeconds = 0;
        pending.sequenceId = nextMotorTestSequenceId++;
        pendingMotorTestAcks.append(pending);

        sendCommandLong(MAV_CMD_DO_MOTOR_TEST,
                        static_cast<float>(motor),
                        static_cast<float>(MOTOR_TEST_THROTTLE_PERCENT),
                        0.0f,
                        0.0f,
                        1.0f,
                        static_cast<float>(MOTOR_TEST_ORDER_DEFAULT));
    }
}

bool Mavlink::servo(int servoNumber, int pwm)
{
    const int safeServoNumber = qBound(9, servoNumber, 14);
    const int safePwm = qBound(800, pwm, 2200);

    if (servoNumber != safeServoNumber) {
        emit servoCommandRejected(servoNumber, pwm, QStringLiteral("Servo must be between S9 and S14"));
        return false;
    }

    if (pwm != safePwm) {
        emit servoCommandRejected(servoNumber, pwm, QStringLiteral("PWM must be between 800 and 2200 us"));
        return false;
    }

    if (!connectedState) {
        emit servoCommandRejected(servoNumber, pwm, QStringLiteral("Vehicle is not connected"));
        return false;
    }

    PendingServoAck pending;
    pending.servoNumber = safeServoNumber;
    pending.pwm = safePwm;
    pendingServoAcks.append(pending);

    sendCommandLong(MAV_CMD_DO_SET_SERVO, static_cast<float>(safeServoNumber), static_cast<float>(safePwm));
    return true;
}

bool Mavlink::sendManualControl(int x, int y, int z, int r, int buttons, int buttons2)
{
    if (!connectedState) {
        return false;
    }

    mavlink_message_t msg;
    mavlink_msg_manual_control_pack(
        255,
        0,
        &msg,
        targetSystemId,
        static_cast<int16_t>(qBound(-1000, x, 1000)),
        static_cast<int16_t>(qBound(-1000, y, 1000)),
        static_cast<int16_t>(qBound(0, z, 1000)),
        static_cast<int16_t>(qBound(-1000, r, 1000)),
        static_cast<uint16_t>(qBound(0, buttons, 0xffff)),
        static_cast<uint16_t>(qBound(0, buttons2, 0xffff)),
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    );

    sendMavlinkMessage(msg);
    return true;
}

bool Mavlink::sendRcChannelsOverride(int rollPwm, int pitchPwm, int throttlePwm, int yawPwm)
{
    if (!connectedState) {
        return false;
    }

    const bool releaseOverride = rollPwm == 0 && pitchPwm == 0 && throttlePwm == 0 && yawPwm == 0;
    const uint16_t roll = releaseOverride ? 0 : static_cast<uint16_t>(qBound(1000, rollPwm, 2000));
    const uint16_t pitch = releaseOverride ? 0 : static_cast<uint16_t>(qBound(1000, pitchPwm, 2000));
    const uint16_t throttle = releaseOverride ? 0 : static_cast<uint16_t>(qBound(1000, throttlePwm, 2000));
    const uint16_t yaw = releaseOverride ? 0 : static_cast<uint16_t>(qBound(1000, yawPwm, 2000));
    constexpr uint16_t ignoreChannel = UINT16_MAX;

    mavlink_message_t msg;
    mavlink_msg_rc_channels_override_pack(
        255,
        0,
        &msg,
        targetSystemId,
        targetComponentId,
        roll,
        pitch,
        throttle,
        yaw,
        ignoreChannel,
        ignoreChannel,
        ignoreChannel,
        ignoreChannel,
        ignoreChannel,
        ignoreChannel,
        ignoreChannel,
        ignoreChannel,
        ignoreChannel,
        ignoreChannel,
        ignoreChannel,
        ignoreChannel,
        ignoreChannel,
        ignoreChannel
    );

    sendMavlinkMessage(msg);
    return true;
}

void Mavlink::refreshTelemetry()
{
    if (connectedState) {
        requestDataStreams();
    }
}

bool Mavlink::uploadMissionFile(const QUrl &fileUrl)
{
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QVector<MissionItem> parsedItems;
    QVariantList displayWaypoints;
    QVariantMap homeLocation;
    QString error;

    if (!parseMissionFile(path, parsedItems, displayWaypoints, homeLocation, error)) {
        setUploadedHomeLocation(QVariantMap());
        setMissionStatus(error);
        emit missionUploadFinished(false, error);
        return false;
    }

    pendingMissionItems = parsedItems;
    loadedMissionWaypoints = displayWaypoints;
    loadedMissionElevationWaypoints = buildElevationWaypoints(pendingMissionItems);
    loadedMissionTerrainProfile = buildMissionTerrainProfile(loadedMissionElevationWaypoints);
    missionDownloadActive = false;
    missionDownloadExpectedCount = 0;
    downloadingMissionItems.clear();
    downloadedMissionItemSeqs.clear();
    missionSaveRequestedAfterDownload = false;
    setUploadedHomeLocation(homeLocation);
    setMissionUploaded(false);
    emit missionWaypointsChanged();
    emit missionElevationWaypointsChanged();
    emit missionTerrainProfileChanged();

    if (!connectedState) {
        setMissionStatus(QStringLiteral("Mission loaded on map. Connect to a drone before uploading."));
        emit missionUploadFinished(false, missionStatusText);
        return false;
    }

    missionClearBeforeUploadActive = true;
    missionUploadActive = false;
    setMissionStatus(QStringLiteral("Clearing existing mission before upload."));
    sendMissionClearAll();

    QTimer::singleShot(750, this, [this]() {
        if (!connectedState || !missionClearBeforeUploadActive) {
            return;
        }

        missionClearBeforeUploadActive = false;
        missionUploadActive = true;
        setMissionStatus(QStringLiteral("Uploading mission: %1 waypoint items").arg(pendingMissionItems.size()));
        sendMissionCount();
    });

    return true;
}

bool Mavlink::resetHomeToUploadedLocation()
{
    if (loadedUploadedHomeLocation.isEmpty()) {
        setMissionStatus(QStringLiteral("No uploaded home location is available."));
        return false;
    }

    if (!connectedState) {
        setMissionStatus(QStringLiteral("Connect to a drone before resetting home."));
        return false;
    }

    const double latitude = loadedUploadedHomeLocation.value(QStringLiteral("lat")).toDouble();
    const double longitude = loadedUploadedHomeLocation.value(QStringLiteral("lon")).toDouble();
    const double altitude = loadedUploadedHomeLocation.value(QStringLiteral("alt")).toDouble();
    if (!hasValidCoordinate(latitude, longitude) || !qIsFinite(altitude)) {
        setMissionStatus(QStringLiteral("Uploaded home location is invalid."));
        return false;
    }

    sendCommandLong(MAV_CMD_DO_SET_HOME,
                    0.0f,
                    0.0f,
                    0.0f,
                    0.0f,
                    static_cast<float>(latitude),
                    static_cast<float>(longitude),
                    static_cast<float>(altitude));
    emit homeLocationResetRequested(latitude, longitude, altitude);
    setMissionStatus(QStringLiteral("Home reset requested to uploaded coordinates."));
    return true;
}

bool Mavlink::writeLoadedMissionToVehicle()
{
    if (pendingMissionItems.isEmpty()) {
        setMissionStatus(QStringLiteral("No mission plan loaded to write."));
        emit missionUploadFinished(false, missionStatusText);
        return false;
    }

    if (!connectedState) {
        setMissionStatus(QStringLiteral("Connect to a drone before writing mission."));
        emit missionUploadFinished(false, missionStatusText);
        return false;
    }

    if (missionUploadActive || missionClearBeforeUploadActive || missionDownloadActive) {
        setMissionStatus(QStringLiteral("Wait for the current mission transfer to finish."));
        emit missionUploadFinished(false, missionStatusText);
        return false;
    }

    missionClearBeforeUploadActive = true;
    missionUploadActive = false;
    missionDownloadActive = false;
    missionSaveRequestedAfterDownload = false;
    missionDownloadExpectedCount = 0;
    downloadingMissionItems.clear();
    downloadedMissionItemSeqs.clear();
    setMissionUploaded(false);
    setMissionStatus(QStringLiteral("Clearing existing mission before write."));
    sendMissionClearAll();

    QTimer::singleShot(750, this, [this]() {
        if (!connectedState || !missionClearBeforeUploadActive) {
            return;
        }

        missionClearBeforeUploadActive = false;
        missionUploadActive = true;
        setMissionStatus(QStringLiteral("Uploading mission: %1 waypoint items").arg(pendingMissionItems.size()));
        sendMissionCount();
    });

    return true;
}

void Mavlink::downloadMissionFromVehicle()
{
    if (!connectedState) {
        setMissionStatus(QStringLiteral("Connect to a drone before downloading mission."));
        return;
    }

    if (missionUploadActive || missionClearBeforeUploadActive) {
        return;
    }

    missionDownloadActive = true;
    missionSaveRequestedAfterDownload = false;
    missionDownloadExpectedCount = 0;
    downloadingMissionItems.clear();
    downloadedMissionItemSeqs.clear();
    setMissionStatus(QStringLiteral("Requesting mission from vehicle."));
    requestMissionList();
}

void Mavlink::saveMissionFromVehicle()
{
    if (!connectedState) {
        setMissionStatus(QStringLiteral("Connect to a drone before saving vehicle mission."));
        return;
    }

    if (missionUploadActive || missionClearBeforeUploadActive) {
        setMissionStatus(QStringLiteral("Wait for the current mission write to finish before saving."));
        return;
    }

    missionSaveRequestedAfterDownload = true;
    if (missionDownloadActive) {
        setMissionStatus(QStringLiteral("Saving mission after current download finishes."));
        return;
    }

    missionDownloadActive = true;
    missionDownloadExpectedCount = 0;
    downloadingMissionItems.clear();
    downloadedMissionItemSeqs.clear();
    setMissionStatus(QStringLiteral("Downloading vehicle mission to save."));
    requestMissionList();
}

void Mavlink::clearMission()
{
    pendingMissionItems.clear();
    loadedMissionWaypoints.clear();
    loadedMissionElevationWaypoints.clear();
    loadedMissionTerrainProfile.clear();
    setUploadedHomeLocation(QVariantMap());
    missionUploadActive = false;
    missionClearBeforeUploadActive = false;
    missionDownloadActive = false;
    missionSaveRequestedAfterDownload = false;
    missionDownloadExpectedCount = 0;
    downloadingMissionItems.clear();
    downloadedMissionItemSeqs.clear();
    setMissionUploaded(false);
    emit missionWaypointsChanged();
    emit missionElevationWaypointsChanged();
    emit missionTerrainProfileChanged();

    if (connectedState) {
        sendMissionClearAll();
        setMissionStatus(QStringLiteral("Mission cleared."));
    } else {
        setMissionStatus(QStringLiteral("Mission cleared from map."));
    }
}

void Mavlink::requestParameters()
{
    if (!connectedState) {
        qDebug() << "Cannot request parameters while disconnected";
        return;
    }

    parameterCache.clear();
    receivedParameterIndices.clear();
    expectedParameterCount = -1;
    parameterRetryCount = 0;
    parameterFetchActive = true;
    parameterRequestSent = true;
    emit parameterFetchStatusChanged();

    mavlink_message_t msg;
    mavlink_msg_param_request_list_pack(
        255,
        0,
        &msg,
        targetSystemId,
        targetComponentId
    );

    sendMavlinkMessage(msg);
    parameterFetchIdleTimer->start();
    missingParameterRetryTimer->start();

    qDebug() << "Requested full parameter list from target" << targetSystemId << targetComponentId;
}

QVariantMap Mavlink::parameterValue(const QString &name) const
{
    const QString key = name.trimmed();
    if (!parameterCache.contains(key)) {
        return {};
    }

    return parameterToVariantMap(parameterCache.value(key));
}

QVariantMap Mavlink::parameters() const
{
    QVariantMap allParameters;
    for (auto it = parameterCache.constBegin(); it != parameterCache.constEnd(); ++it) {
        allParameters.insert(it.key(), parameterToVariantMap(it.value()));
    }

    return allParameters;
}

bool Mavlink::setParameter(const QString &name, double value, int type)
{
    return sendParameterSet(name, static_cast<float>(value), static_cast<uint8_t>(type));
}

bool Mavlink::writeRcCalibration(const QVariantList &calibration)
{
    if (!connectedState) {
        qDebug() << "Cannot write RC calibration while disconnected";
        return false;
    }

    bool wroteAny = false;
    for (const QVariant &entryValue : calibration) {
        const QVariantMap entry = entryValue.toMap();
        const int channel = entry.value(QStringLiteral("channel")).toInt();
        if (channel < 1 || channel > 16) {
            continue;
        }

        const int minimum = qBound(800, entry.value(QStringLiteral("minimum")).toInt(), 2200);
        const int maximum = qBound(800, entry.value(QStringLiteral("maximum")).toInt(), 2200);
        const int trim = qBound(800, entry.value(QStringLiteral("trim")).toInt(), 2200);
        const bool reversed = entry.value(QStringLiteral("reversed")).toBool();
        const QString prefix = QStringLiteral("RC%1_").arg(channel);

        wroteAny = sendParameterSet(prefix + QStringLiteral("MIN"), minimum, MAV_PARAM_TYPE_INT16) || wroteAny;
        wroteAny = sendParameterSet(prefix + QStringLiteral("MAX"), maximum, MAV_PARAM_TYPE_INT16) || wroteAny;
        wroteAny = sendParameterSet(prefix + QStringLiteral("TRIM"), trim, MAV_PARAM_TYPE_INT16) || wroteAny;
        if (channel <= 4) {
            wroteAny = sendParameterSet(prefix + QStringLiteral("REVERSED"), reversed ? 1.0f : 0.0f, MAV_PARAM_TYPE_INT8) || wroteAny;
        }
    }

    if (wroteAny) {
        setMissionStatus(QStringLiteral("RC calibration parameters written."));
    }
    return wroteAny;
}

void Mavlink::setTelemetryLoggingEnabled(bool enabled)
{
    if (telemetryLoggingEnabledState == enabled) {
        return;
    }

    telemetryLoggingEnabledState = enabled;
    QSettings settings;
    settings.setValue(QStringLiteral("Log/autoTelemetryLoggingEnabled"), telemetryLoggingEnabledState);

    if (!telemetryLoggingEnabledState) {
        stopTelemetryLog();
    } else if (connectedState) {
        startTelemetryLog();
    }

    emit telemetryLoggingSettingsChanged();
}

void Mavlink::setTelemetryLogDirectory(const QString &directory)
{
    const QString cleanedDirectory = QDir::fromNativeSeparators(directory.trimmed());
    if (telemetryLogDirectoryPath == cleanedDirectory) {
        return;
    }

    const bool wasActive = telemetryLogActive();
    if (wasActive) {
        stopTelemetryLog();
    }

    telemetryLogDirectoryPath = cleanedDirectory;
    emit telemetryLogDirectoryChanged();

    if (wasActive && telemetryLoggingEnabledState && connectedState) {
        startTelemetryLog();
    }
}

bool Mavlink::startTelemetryLog()
{
    if (!telemetryLoggingEnabledState) {
        return false;
    }

    if (telemetryLogWriter->isOpen()) {
        return true;
    }

    const QString directory = telemetryLogDirectory();
    if (!QDir().mkpath(directory)) {
        qWarning() << "Could not create telemetry log directory:" << directory;
        return false;
    }

    const QString logPath = nextTelemetryLogPath();
    QString error;
    if (!telemetryLogWriter->start(logPath, QIODevice::WriteOnly, &error)) {
        qWarning() << "Could not open telemetry log:" << logPath << error;
        currentTelemetryLogFilePath.clear();
        emit telemetryLogStateChanged();
        return false;
    }

    currentTelemetryLogFilePath = QFileInfo(logPath).absoluteFilePath();
    telemetryLogEpochUsec = static_cast<quint64>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()) * 1000ULL;
    telemetryLogTimer.restart();

    qDebug() << "Telemetry logging started:" << currentTelemetryLogFilePath;
    emit telemetryLogStateChanged();
    return true;
}

void Mavlink::stopTelemetryLog()
{
    if (!telemetryLogWriter->isOpen()) {
        currentTelemetryLogFilePath.clear();
        return;
    }

    telemetryLogWriter->stop();
    qDebug() << "Telemetry logging stopped:" << currentTelemetryLogFilePath;
    currentTelemetryLogFilePath.clear();
    emit telemetryLogStateChanged();
}

QStringList Mavlink::telemetryLogFiles() const
{
    const QDir directory(telemetryLogDirectory());
    const QFileInfoList entries = directory.entryInfoList(
                { QStringLiteral("*.tlog") },
                QDir::Files | QDir::Readable,
                QDir::Time);

    QStringList files;
    files.reserve(entries.size());
    for (const QFileInfo &entry : entries) {
        files.append(entry.absoluteFilePath());
    }
    return files;
}

void Mavlink::setFlightControllerLoggingEnabled(bool enabled)
{
    if (flightControllerLoggingEnabledState == enabled) {
        return;
    }

    flightControllerLoggingEnabledState = enabled;
    QSettings settings;
    settings.setValue(QStringLiteral("General/flightControllerLoggingEnabled"), flightControllerLoggingEnabledState);

    if (!flightControllerLoggingEnabledState) {
        stopFlightControllerLog();
    }

    emit flightControllerLoggingSettingsChanged();
}

void Mavlink::setFlightControllerLogDirectory(const QString &directory)
{
    const QString cleanedDirectory = QDir::fromNativeSeparators(directory.trimmed());
    if (flightControllerLogDirectoryPath == cleanedDirectory) {
        return;
    }

    const bool wasActive = flightControllerLogActive();
    if (wasActive) {
        stopFlightControllerLog();
    }

    flightControllerLogDirectoryPath = cleanedDirectory;
    emit flightControllerLogDirectoryChanged();

    if (wasActive && flightControllerLoggingEnabledState && connectedState) {
        startFlightControllerLog();
    }
}

bool Mavlink::startFlightControllerLog()
{
    if (!flightControllerLoggingEnabledState) {
        return false;
    }

    if (flightControllerLogWriter->isOpen()) {
        return true;
    }

    const QString directory = flightControllerLogDirectory();
    if (!QDir().mkpath(directory)) {
        qWarning() << "Could not create flight controller log directory:" << directory;
        return false;
    }

    const QString logPath = nextFlightControllerLogPath();
    QString error;
    if (!flightControllerLogWriter->start(logPath, QIODevice::WriteOnly | QIODevice::Text, &error)) {
        qWarning() << "Could not open flight controller log:" << logPath << error;
        currentFlightControllerLogFilePath.clear();
        emit flightControllerLogStateChanged();
        return false;
    }

    currentFlightControllerLogFilePath = QFileInfo(logPath).absoluteFilePath();
    writeFlightControllerLogLine(QStringLiteral("# BonV GCS flight controller messages"));
    writeFlightControllerLogLine(QStringLiteral("# Started %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));

    qDebug() << "Flight controller logging started:" << currentFlightControllerLogFilePath;
    emit flightControllerLogStateChanged();
    return true;
}

void Mavlink::stopFlightControllerLog()
{
    if (!flightControllerLogWriter->isOpen()) {
        currentFlightControllerLogFilePath.clear();
        return;
    }

    writeFlightControllerLogLine(QStringLiteral("# Stopped %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
    flightControllerLogWriter->stop();
    qDebug() << "Flight controller logging stopped:" << currentFlightControllerLogFilePath;
    currentFlightControllerLogFilePath.clear();
    emit flightControllerLogStateChanged();
}

QStringList Mavlink::flightControllerLogFiles() const
{
    const QDir directory(flightControllerLogDirectory());
    const QFileInfoList entries = directory.entryInfoList(
                { QStringLiteral("flight_controller_*.log") },
                QDir::Files | QDir::Readable,
                QDir::Time);

    QStringList files;
    files.reserve(entries.size());
    for (const QFileInfo &entry : entries) {
        files.append(entry.absoluteFilePath());
    }
    return files;
}

void Mavlink::readData()
{
    const QByteArray data = link->readAll();
    mavlink_message_t msg;

    for (int i = 0; i < data.size(); i++) {
        const uint8_t byte = static_cast<uint8_t>(data[i]);
        // Keep only the trailing wire window required for one maximum-size
        // MAVLink frame.  Its bounds come from the parser's accepted message,
        // not a parallel guessed framing state: malformed data or parser
        // resynchronisation therefore cannot make us lose the next valid raw
        // packet (or retain unbounded input).
        inboundFrameBytes.append(static_cast<char>(byte));
        if (inboundFrameBytes.size() > MAVLINK_MAX_PACKET_LEN) {
            inboundFrameBytes.remove(0, inboundFrameBytes.size() - MAVLINK_MAX_PACKET_LEN);
        }

        const bool parsed = mavlink_parse_char(MAVLINK_COMM_0, byte, &msg, &mavlinkStatus);
        if (parsed) {
            const bool mavlink1 = msg.magic == MAVLINK_STX_MAVLINK1;
            const int frameLength = (mavlink1 ? 8 : 12) + msg.len
                    + ((!mavlink1 && (msg.incompat_flags & MAVLINK_IFLAG_SIGNED))
                           ? MAVLINK_SIGNATURE_BLOCK_LEN : 0);
            // This is a slice of bytes actually received immediately before
            // parser acceptance, never a serialization of mavlink_message_t.
            const QByteArray rawFrame = frameLength <= inboundFrameBytes.size()
                    ? inboundFrameBytes.sliced(inboundFrameBytes.size() - frameLength)
                    : QByteArray();
            logTelemetryMessage(msg);
            logFlightControllerStatusText(msg);

            if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
                mavlink_heartbeat_t heartbeat;
                mavlink_msg_heartbeat_decode(&msg, &heartbeat);
                if (heartbeat.autopilot != MAV_AUTOPILOT_INVALID) {
                    // A transport can be open without a vehicle at the other
                    // end (especially UDP).  A valid autopilot heartbeat is
                    // the first proof that this is an established vehicle
                    // connection.
                    setConnected(true);
                    targetSystemId = msg.sysid;
                    targetComponentId = msg.compid;
                    if (!parameterRequestSent) {
                        QTimer::singleShot(250, this, [this]() {
                            if (connectedState && !parameterRequestSent) {
                                requestParameters();
                            }
                        });
                    }
                    if (!missionDownloadRequestedOnConnect) {
                        missionDownloadRequestedOnConnect = true;
                        QTimer::singleShot(650, this, [this]() {
                            if (connectedState && !missionUploadActive && !missionClearBeforeUploadActive) {
                                downloadMissionFromVehicle();
                            }
                        });
                    }
                }
            }

            const QString linkIdentity = link
                    ? QStringLiteral("%1@%2")
                              .arg(QString::fromLatin1(link->metaObject()->className()))
                              .arg(reinterpret_cast<quintptr>(link), 0, 16)
                    : QString();
            emit frameObserved(MavlinkFrame(QDateTime::currentDateTimeUtc(), linkIdentity, msg, rawFrame));
            emit messageReceived(msg);
            updateMavlinkQuality(msg);
            handleLinkPing(msg);
            handleParameterValue(msg);
            handleRcChannels(msg);
            handleTerrainRequest(msg);
            handleMissionMessage(msg);
        }

    }
}

void Mavlink::setConnected(bool connected)
{
    if (connectedState == connected) {
        return;
    }

    connectedState = connected;
    if (!connectedState) {
        pendingServoAcks.clear();
        linkPingTimer->stop();
        resetLinkQuality();
    } else {
        linkPingTimer->start();
        startTelemetryLog();
        requestDataStreams();
        sendLinkPing();
    }
    emit connectionStatusChanged(connectedState);
}

void Mavlink::sendLinkPing()
{
    if (!connectedState) {
        return;
    }

    lastPingTimestampUsec = static_cast<quint64>(linkPingClock.nsecsElapsed() / 1000);
    mavlink_message_t msg = {};
    mavlink_msg_ping_pack(255, MAV_COMP_ID_MISSIONPLANNER, &msg,
                          lastPingTimestampUsec, nextPingSequence++, 0, 0);
    sendMavlinkMessage(msg);
}

void Mavlink::handleLinkPing(const mavlink_message_t &msg)
{
    if (msg.msgid != MAVLINK_MSG_ID_PING || lastPingTimestampUsec == 0) {
        return;
    }

    mavlink_ping_t ping = {};
    mavlink_msg_ping_decode(&msg, &ping);
    if (ping.time_usec != lastPingTimestampUsec || ping.target_system != 255) {
        return;
    }

    const qint64 elapsedUsec = linkPingClock.nsecsElapsed() / 1000;
    const qint64 roundTripMs = (elapsedUsec - static_cast<qint64>(ping.time_usec)) / 1000;
    if (roundTripMs < 0 || roundTripMs > 30000) {
        return;
    }

    const int measuredLatencyMs = static_cast<int>(roundTripMs);
    const int newLatencyMs = linkLatencyMsValue < 0
            ? measuredLatencyMs
            : qRound(linkLatencyMsValue * 0.7 + measuredLatencyMs * 0.3);
    if (newLatencyMs != linkLatencyMsValue) {
        linkLatencyMsValue = newLatencyMs;
        emit linkQualityChanged();
    }
}

void Mavlink::updateMavlinkQuality(const mavlink_message_t &msg)
{
    if (msg.sysid != targetSystemId || msg.msgid == MAVLINK_MSG_ID_PING) {
        return;
    }

    const quint16 source = static_cast<quint16>((static_cast<quint16>(msg.sysid) << 8) | msg.compid);
    const auto previous = lastMessageSequenceBySource.constFind(source);
    if (previous != lastMessageSequenceBySource.cend()) {
        const int gap = (static_cast<int>(msg.seq) - static_cast<int>(previous.value()) - 1 + 256) % 256;
        const double sampleQuality = gap == 0 ? 100.0 : 0.0;
        const double newQuality = mavlinkQualityValue < 0
                ? sampleQuality
                : mavlinkQualityValue * 0.9 + sampleQuality * 0.1;
        if (mavlinkQualityValue < 0 || qAbs(newQuality - mavlinkQualityValue) >= 0.5) {
            mavlinkQualityValue = newQuality;
            emit linkQualityChanged();
        }
    }
    lastMessageSequenceBySource.insert(source, msg.seq);
}

void Mavlink::resetLinkQuality()
{
    const bool hadQuality = linkLatencyMsValue >= 0 || mavlinkQualityValue >= 0 || !lastMessageSequenceBySource.isEmpty();
    linkLatencyMsValue = -1;
    mavlinkQualityValue = -1;
    lastPingTimestampUsec = 0;
    lastMessageSequenceBySource.clear();
    if (hadQuality) {
        emit linkQualityChanged();
    }
}

void Mavlink::setMissionStatus(const QString &status)
{
    if (missionStatusText == status) {
        return;
    }

    missionStatusText = status;
    emit missionStatusChanged(missionStatusText);
}

void Mavlink::setMissionUploaded(bool uploaded)
{
    if (missionUploadedState == uploaded) {
        return;
    }

    missionUploadedState = uploaded;
    emit missionUploadedChanged(missionUploadedState);
}

void Mavlink::setUploadedHomeLocation(const QVariantMap &location)
{
    loadedUploadedHomeLocation = location;
    emit uploadedHomeLocationChanged();
}

void Mavlink::resetParameterCache()
{
    if (parameterFetchIdleTimer) {
        parameterFetchIdleTimer->stop();
    }
    if (missingParameterRetryTimer) {
        missingParameterRetryTimer->stop();
    }

    const bool hadState = parameterFetchActive || parameterRequestSent || !parameterCache.isEmpty()
            || expectedParameterCount > 0 || !receivedParameterIndices.isEmpty();

    parameterCache.clear();
    receivedParameterIndices.clear();
    expectedParameterCount = -1;
    parameterRetryCount = 0;
    parameterFetchActive = false;
    parameterRequestSent = false;

    if (hadState) {
        emit parameterFetchStatusChanged();
    }
}

void Mavlink::requestMissingParameters()
{
    if (!connectedState || !parameterFetchActive || expectedParameterCount <= 0) {
        return;
    }

    if (parameterCache.size() >= expectedParameterCount) {
        finishParameterFetch(true);
        return;
    }

    if (parameterRetryCount >= 5) {
        finishParameterFetch(false);
        return;
    }

    ++parameterRetryCount;
    int requested = 0;
    for (int index = 0; index < expectedParameterCount; ++index) {
        if (receivedParameterIndices.contains(index)) {
            continue;
        }

        mavlink_message_t msg;
        char emptyName[16] = {};
        mavlink_msg_param_request_read_pack(
            255,
            0,
            &msg,
            targetSystemId,
            targetComponentId,
            emptyName,
            static_cast<int16_t>(index)
        );
        sendMavlinkMessage(msg);

        ++requested;
        if (requested >= 25) {
            break;
        }
    }

    qDebug() << "Requested missing parameters batch"
             << "retry" << parameterRetryCount
             << "requested" << requested
             << "loaded" << parameterCache.size()
             << "expected" << expectedParameterCount;

    missingParameterRetryTimer->start();
}

void Mavlink::finishParameterFetch(bool complete)
{
    if (!parameterFetchActive) {
        return;
    }

    parameterFetchActive = false;
    if (parameterFetchIdleTimer) {
        parameterFetchIdleTimer->stop();
    }
    if (missingParameterRetryTimer) {
        missingParameterRetryTimer->stop();
    }

    qDebug() << "Parameter fetch finished"
             << "complete" << complete
             << "loaded" << parameterCache.size()
             << "expected" << expectedParameterCount;

    emit parameterFetchStatusChanged();
    emit parameterFetchFinished(complete, parameterCache.size(), parameterCount());
}

void Mavlink::handleParameterValue(const mavlink_message_t &msg)
{
    if (msg.msgid != MAVLINK_MSG_ID_PARAM_VALUE) {
        return;
    }

    mavlink_param_value_t value;
    mavlink_msg_param_value_decode(&msg, &value);

    const QString name = mavlinkParamIdToString(value.param_id);
    if (name.isEmpty()) {
        return;
    }

    const int previousExpectedCount = expectedParameterCount;
    if (value.param_count > 0) {
        expectedParameterCount = value.param_count;
    }

    VehicleParameter parameter;
    parameter.name = name;
    parameter.value = value.param_value;
    parameter.type = value.param_type;
    parameter.index = value.param_index;

    const bool isNewParameter = !parameterCache.contains(name);
    parameterCache.insert(name, parameter);
    if (value.param_index != UINT16_MAX) {
        receivedParameterIndices.insert(value.param_index);
    }

    parameterFetchActive = parameterFetchActive || parameterRequestSent;
    parameterFetchIdleTimer->start();

    emit parameterUpdated(name, parameterToVariantMap(parameter));
    if (isNewParameter || previousExpectedCount != expectedParameterCount) {
        emit parameterFetchStatusChanged();
    }

    if (expectedParameterCount > 0 && parameterCache.size() >= expectedParameterCount) {
        finishParameterFetch(true);
    } else if (expectedParameterCount > 0 && !missingParameterRetryTimer->isActive()) {
        missingParameterRetryTimer->start();
    }
}

void Mavlink::handleRcChannels(const mavlink_message_t &msg)
{
    QVariantList channels;

    if (msg.msgid == MAVLINK_MSG_ID_RC_CHANNELS) {
        mavlink_rc_channels_t rc;
        mavlink_msg_rc_channels_decode(&msg, &rc);
        const std::array<uint16_t, 18> values = {
            rc.chan1_raw, rc.chan2_raw, rc.chan3_raw, rc.chan4_raw,
            rc.chan5_raw, rc.chan6_raw, rc.chan7_raw, rc.chan8_raw,
            rc.chan9_raw, rc.chan10_raw, rc.chan11_raw, rc.chan12_raw,
            rc.chan13_raw, rc.chan14_raw, rc.chan15_raw, rc.chan16_raw,
            rc.chan17_raw, rc.chan18_raw
        };
        const int count = qBound(0, int(rc.chancount), int(values.size()));
        for (int index = 0; index < count; ++index) {
            channels.append(makeRcChannel(index + 1, values[index]));
        }
    } else if (msg.msgid == MAVLINK_MSG_ID_RC_CHANNELS_RAW) {
        mavlink_rc_channels_raw_t rc;
        mavlink_msg_rc_channels_raw_decode(&msg, &rc);
        const std::array<uint16_t, 8> values = {
            rc.chan1_raw, rc.chan2_raw, rc.chan3_raw, rc.chan4_raw,
            rc.chan5_raw, rc.chan6_raw, rc.chan7_raw, rc.chan8_raw
        };
        for (int index = 0; index < int(values.size()); ++index) {
            channels.append(makeRcChannel(index + 1, values[index]));
        }
    } else {
        return;
    }

    if (latestRcChannels == channels) {
        return;
    }

    latestRcChannels = channels;
    emit rcChannelsChanged();
}

QString Mavlink::mavlinkParamIdToString(const char paramId[16]) const
{
    const QByteArray raw(paramId, 16);
    const int terminatorIndex = raw.indexOf('\0');
    const int length = terminatorIndex >= 0 ? terminatorIndex : raw.size();
    return QString::fromLatin1(raw.constData(), length).trimmed();
}

QVariantMap Mavlink::parameterToVariantMap(const VehicleParameter &parameter) const
{
    QVariantMap value;
    value.insert(QStringLiteral("name"), parameter.name);
    value.insert(QStringLiteral("value"), parameter.value);
    value.insert(QStringLiteral("type"), parameter.type);
    value.insert(QStringLiteral("index"), parameter.index);
    return value;
}

void Mavlink::requestDataStreams()
{
    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_request_data_stream_pack(
        255,
        0,
        &msg,
        targetSystemId,
        targetComponentId,
        MAV_DATA_STREAM_ALL,
        10,
        1
    );

    sendMavlinkMessage(msg);

    mavlink_msg_request_data_stream_pack(
        255,
        0,
        &msg,
        targetSystemId,
        targetComponentId,
        MAV_DATA_STREAM_RC_CHANNELS,
        50,
        1
    );

    sendMavlinkMessage(msg);

    // Modern flight stacks use this command instead of REQUEST_DATA_STREAM.
    mavlink_msg_command_long_pack(
        255,
        0,
        &msg,
        targetSystemId,
        targetComponentId,
        MAV_CMD_SET_MESSAGE_INTERVAL,
        0,
        static_cast<float>(MAVLINK_MSG_ID_RC_CHANNELS),
        20000.0f,
        0,
        0,
        0,
        0,
        0
    );

    sendMavlinkMessage(msg);

    qDebug() << "Requested all data streams at 10Hz and RC channels at 50Hz";
}

void Mavlink::sendCommandLong(uint16_t command, float param1, float param2, float param3,
                              float param4, float param5, float param6, float param7)
{
    if (!connectedState) {
        qDebug() << "Cannot send MAVLink command while disconnected:" << command;
        return;
    }

    mavlink_message_t msg;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];

    mavlink_msg_command_long_pack(
        255,
        0,
        &msg,
        targetSystemId,
        targetComponentId,
        command,
        0,
        param1,
        param2,
        param3,
        param4,
        param5,
        param6,
        param7
    );

    sendMavlinkMessage(msg);
    qDebug() << "Sent MAVLink command:" << command
             << "target" << targetSystemId << targetComponentId
             << "params" << param1 << param2 << param3 << param4 << param5 << param6 << param7;
}

void Mavlink::sendSetMode(uint32_t customMode)
{
    if (!connectedState) {
        qDebug() << "Cannot set MAVLink mode while disconnected:" << customMode;
        return;
    }

    mavlink_message_t msg;
    mavlink_msg_set_mode_pack(
        255,
        0,
        &msg,
        targetSystemId,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
        customMode
    );

    sendMavlinkMessage(msg);
    qDebug() << "Sent MAVLink set mode:" << customMode << "target" << targetSystemId << targetComponentId;
}

bool Mavlink::sendParameterSet(const QString &name, float value, uint8_t type)
{
    if (!connectedState) {
        return false;
    }

    const QByteArray encodedName = name.trimmed().toLatin1();
    if (encodedName.isEmpty()) {
        return false;
    }

    char paramId[16] = {};
    const int copyLength = qMin(encodedName.size(), int(sizeof(paramId)));
    memcpy(paramId, encodedName.constData(), copyLength);

    mavlink_message_t msg;
    mavlink_msg_param_set_pack(
        255,
        0,
        &msg,
        targetSystemId,
        targetComponentId,
        paramId,
        value,
        type
    );

    sendMavlinkMessage(msg);

    VehicleParameter parameter;
    parameter.name = QString::fromLatin1(paramId, copyLength).trimmed();
    parameter.value = value;
    parameter.type = type;
    parameter.index = parameterCache.value(parameter.name).index;
    parameterCache.insert(parameter.name, parameter);
    emit parameterUpdated(parameter.name, parameterToVariantMap(parameter));

    qDebug() << "Sent MAVLink parameter set:" << parameter.name << value << type;
    return true;
}

void Mavlink::sendMavlinkMessage(mavlink_message_t &msg)
{
    if (!link) {
        return;
    }

    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    const int len = mavlink_msg_to_send_buffer(buf, &msg);
    const qint64 bytesWritten = link->writeData(reinterpret_cast<const char*>(buf), len);
    if (bytesWritten == len && shouldLogTelemetryMessage(msg)) {
        writeTelemetryLogRecord(reinterpret_cast<const char*>(buf), len);
    }
    link->flush();
}

bool Mavlink::shouldLogTelemetryMessage(const mavlink_message_t &msg) const
{
#ifdef MAVLINK_MSG_ID_SETUP_SIGNING
    if (msg.msgid == MAVLINK_MSG_ID_SETUP_SIGNING) {
        return false;
    }
#endif

    Q_UNUSED(msg)
    return telemetryLogWriter->isOpen();
}

void Mavlink::logTelemetryMessage(const mavlink_message_t &msg)
{
    if (!shouldLogTelemetryMessage(msg)) {
        return;
    }

    uint8_t buffer[MAVLINK_MAX_PACKET_LEN] = {};
    const int length = mavlink_msg_to_send_buffer(buffer, &msg);
    writeTelemetryLogRecord(reinterpret_cast<const char*>(buffer), length);
}

void Mavlink::writeTelemetryLogRecord(const char *packet, int packetLength)
{
    if (!telemetryLogWriter->isOpen() || !packet || packetLength <= 0) {
        return;
    }

    // QGC/mission-planner compatible .tlog record: big-endian epoch usec, then one MAVLink wire frame.
    QByteArray record;
    record.resize(8 + packetLength);
    quint64 timestamp = currentTelemetryLogTimestampUsec();
    for (int i = 7; i >= 0; --i) {
        record[i] = static_cast<char>(timestamp & 0xff);
        timestamp >>= 8;
    }
    std::memcpy(record.data() + 8, packet, static_cast<size_t>(packetLength));
    telemetryLogWriter->append(record);
}

quint64 Mavlink::currentTelemetryLogTimestampUsec() const
{
    if (!telemetryLogTimer.isValid()) {
        return static_cast<quint64>(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()) * 1000ULL;
    }

    return telemetryLogEpochUsec + static_cast<quint64>(telemetryLogTimer.nsecsElapsed() / 1000);
}

QString Mavlink::defaultTelemetryLogDirectory() const
{
    return AppDataPaths::telemetryLogsDirectory();
}

QString Mavlink::nextTelemetryLogPath() const
{
    const QDir directory(telemetryLogDirectory());
    const QString baseName = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss"));
    QString candidate = directory.filePath(baseName + QStringLiteral(".tlog"));
    int suffix = 1;

    while (QFileInfo::exists(candidate)) {
        candidate = directory.filePath(QStringLiteral("%1_%2.tlog").arg(baseName).arg(suffix++));
    }

    return candidate;
}

void Mavlink::logFlightControllerStatusText(const mavlink_message_t &msg)
{
    if (msg.msgid != MAVLINK_MSG_ID_STATUSTEXT || !flightControllerLoggingEnabledState) {
        return;
    }

    mavlink_statustext_t statusText = {};
    mavlink_msg_statustext_decode(&msg, &statusText);

    const QByteArray rawText(statusText.text, MAVLINK_MSG_STATUSTEXT_FIELD_TEXT_LEN);
    const int terminatorIndex = rawText.indexOf('\0');
    const int textLength = terminatorIndex >= 0 ? terminatorIndex : rawText.size();
    const QString text = QString::fromLatin1(rawText.constData(), textLength).trimmed();
    if (text.isEmpty()) {
        return;
    }

    appendFlightControllerLogEntry(mavSeverityText(statusText.severity), msg.sysid, msg.compid, text);
}

void Mavlink::appendFlightControllerLogEntry(const QString &level, uint8_t systemId, uint8_t componentId, const QString &text)
{
    if (!flightControllerLoggingEnabledState || text.trimmed().isEmpty()) {
        return;
    }

    if (!flightControllerLogWriter->isOpen() && !startFlightControllerLog()) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const QString line = QStringLiteral("%1 [%2] sys=%3 comp=%4 %5")
            .arg(timestamp,
                 level,
                 QString::number(systemId),
                 QString::number(componentId),
                 text.trimmed());
    writeFlightControllerLogLine(line);
}

bool Mavlink::writeFlightControllerLogLine(const QString &line)
{
    if (!flightControllerLogWriter->isOpen()) {
        return false;
    }

    flightControllerLogWriter->append(line.toUtf8() + '\n');
    return true;
}

QString Mavlink::defaultFlightControllerLogDirectory() const
{
    return AppDataPaths::ardupilotLogsDirectory();
}

QString Mavlink::nextFlightControllerLogPath() const
{
    const QDir directory(flightControllerLogDirectory());
    const QString baseName = QStringLiteral("flight_controller_%1")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss")));
    QString candidate = directory.filePath(baseName + QStringLiteral(".log"));
    int suffix = 1;

    while (QFileInfo::exists(candidate)) {
        candidate = directory.filePath(QStringLiteral("%1_%2.log").arg(baseName).arg(suffix++));
    }

    return candidate;
}

QString Mavlink::mavSeverityText(uint8_t severity) const
{
    switch (severity) {
    case MAV_SEVERITY_EMERGENCY:
        return QStringLiteral("EMERGENCY");
    case MAV_SEVERITY_ALERT:
        return QStringLiteral("ALERT");
    case MAV_SEVERITY_CRITICAL:
        return QStringLiteral("CRITICAL");
    case MAV_SEVERITY_ERROR:
        return QStringLiteral("ERROR");
    case MAV_SEVERITY_WARNING:
        return QStringLiteral("WARNING");
    case MAV_SEVERITY_NOTICE:
        return QStringLiteral("NOTICE");
    case MAV_SEVERITY_INFO:
        return QStringLiteral("INFO");
    case MAV_SEVERITY_DEBUG:
        return QStringLiteral("DEBUG");
    default:
        return QStringLiteral("SEVERITY_%1").arg(severity);
    }
}

void Mavlink::handleTerrainRequest(const mavlink_message_t &msg)
{
    if (msg.msgid != MAVLINK_MSG_ID_TERRAIN_REQUEST) {
        return;
    }

    mavlink_terrain_request_t request;
    mavlink_msg_terrain_request_decode(&msg, &request);

    qDebug() << "Terrain request:"
             << "lat" << request.lat
             << "lon" << request.lon
             << "gridSpacing" << request.grid_spacing
             << "mask" << QStringLiteral("0x%1").arg(request.mask, 0, 16);

    if (request.grid_spacing == 0 || request.mask == 0) {
        return;
    }

    for (uint8_t gridbit = 0; gridbit < 56; ++gridbit) {
        if ((request.mask & (1ULL << gridbit)) == 0) {
            continue;
        }

        if (!sendTerrainDataBlock(request, gridbit)) {
            qWarning() << "Unable to satisfy terrain request block" << gridbit
                       << "from directories" << terrainProvider.terrainDirectories();
            queuePendingTerrainBlock(request, gridbit);
        }
    }
}

bool Mavlink::buildTerrainDataBlock(const mavlink_terrain_request_t &request, uint8_t gridbit, int16_t data[16])
{
    if (gridbit >= 56 || request.grid_spacing == 0) {
        return false;
    }

    const int blockColumn = gridbit % 8;
    const int blockRow = gridbit / 8;
    const double originLatitude = request.lat / 10000000.0;
    const double originLongitude = request.lon / 10000000.0;
    const double spacing = static_cast<double>(request.grid_spacing);

    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            const int sampleColumn = (blockColumn * 4) + column;
            const int sampleRow = (blockRow * 4) + row;
            const double northMeters = sampleRow * spacing;
            const double eastMeters = sampleColumn * spacing;
            const double latitude = originLatitude + metersToLatitudeDegrees(northMeters);
            const double longitude = originLongitude + metersToLongitudeDegrees(eastMeters, latitude);

            double terrainHeight = 0.0;
            if (!terrainProvider.heightMeters(latitude, longitude, terrainHeight)) {
                return false;
            }

            data[(row * 4) + column] = static_cast<int16_t>(
                        qBound(-32767, static_cast<int>(qRound(terrainHeight)), 32767));
        }
    }

    return true;
}

bool Mavlink::sendTerrainDataBlock(const mavlink_terrain_request_t &request, uint8_t gridbit)
{
    int16_t data[16] = {};
    if (!buildTerrainDataBlock(request, gridbit, data)) {
        return false;
    }

    mavlink_message_t response;
    mavlink_msg_terrain_data_pack(
        255,
        0,
        &response,
        request.lat,
        request.lon,
        request.grid_spacing,
        gridbit,
        data
    );
    sendMavlinkMessage(response);
    qDebug() << "Sent terrain data block" << gridbit;
    return true;
}

void Mavlink::queuePendingTerrainBlock(const mavlink_terrain_request_t &request, uint8_t gridbit)
{
    for (const PendingTerrainBlock &pending : pendingTerrainBlocks) {
        if (pending.gridbit == gridbit
                && pending.request.lat == request.lat
                && pending.request.lon == request.lon
                && pending.request.grid_spacing == request.grid_spacing) {
            return;
        }
    }

    PendingTerrainBlock pending;
    pending.request = request;
    pending.gridbit = gridbit;
    pendingTerrainBlocks.append(pending);

    qWarning() << "Terrain block queued. Add the matching .hgt file to"
               << AppDataPaths::terrainDirectory()
               << "or set BONV_TERRAIN_DIR; upload will continue when the tile is available.";

    if (terrainRetryTimer && !terrainRetryTimer->isActive()) {
        terrainRetryTimer->start();
    }
}

void Mavlink::retryPendingTerrainRequests()
{
    if (!connectedState || pendingTerrainBlocks.isEmpty()) {
        clearPendingTerrainRequests();
        return;
    }

    QVector<PendingTerrainBlock> stillPending;
    stillPending.reserve(pendingTerrainBlocks.size());

    for (const PendingTerrainBlock &pending : std::as_const(pendingTerrainBlocks)) {
        if (!sendTerrainDataBlock(pending.request, pending.gridbit)) {
            stillPending.append(pending);
        }
    }

    const int sentCount = pendingTerrainBlocks.size() - stillPending.size();
    pendingTerrainBlocks = stillPending;
    if (sentCount > 0) {
        qDebug() << "Sent" << sentCount << "previously pending terrain data block(s).";
    }

    if (pendingTerrainBlocks.isEmpty() && terrainRetryTimer) {
        terrainRetryTimer->stop();
    }
}

void Mavlink::clearPendingTerrainRequests()
{
    pendingTerrainBlocks.clear();
    if (terrainRetryTimer) {
        terrainRetryTimer->stop();
    }
}

void Mavlink::sendMissionClearAll()
{
    mavlink_message_t msg;
    mavlink_msg_mission_clear_all_pack(
        255,
        0,
        &msg,
        targetSystemId,
        targetComponentId,
        MAV_MISSION_TYPE_MISSION
    );
    sendMavlinkMessage(msg);
}

void Mavlink::sendMissionCount()
{
    if (!connectedState || pendingMissionItems.isEmpty()) {
        return;
    }

    mavlink_message_t msg;
    mavlink_msg_mission_count_pack(
        255,
        0,
        &msg,
        targetSystemId,
        targetComponentId,
        static_cast<uint16_t>(pendingMissionItems.size()),
        MAV_MISSION_TYPE_MISSION,
        0
    );
    sendMavlinkMessage(msg);
}

void Mavlink::sendMissionItem(uint16_t seq, bool useInt)
{
    if (seq >= static_cast<uint16_t>(pendingMissionItems.size())) {
        return;
    }

    const MissionItem &item = pendingMissionItems[seq];
    mavlink_message_t msg;

    if (useInt) {
        mavlink_msg_mission_item_int_pack(
            255,
            0,
            &msg,
            targetSystemId,
            targetComponentId,
            item.seq,
            item.frame,
            item.command,
            item.current,
            item.autocontinue,
            item.param1,
            item.param2,
            item.param3,
            item.param4,
            static_cast<int32_t>(qRound64(item.x * 10000000.0)),
            static_cast<int32_t>(qRound64(item.y * 10000000.0)),
            item.z,
            MAV_MISSION_TYPE_MISSION
        );
    } else {
        mavlink_msg_mission_item_pack(
            255,
            0,
            &msg,
            targetSystemId,
            targetComponentId,
            item.seq,
            item.frame,
            item.command,
            item.current,
            item.autocontinue,
            item.param1,
            item.param2,
            item.param3,
            item.param4,
            static_cast<float>(item.x),
            static_cast<float>(item.y),
            item.z,
            MAV_MISSION_TYPE_MISSION
        );
    }

    sendMavlinkMessage(msg);
    setMissionStatus(QStringLiteral("Uploading mission item %1 of %2").arg(seq + 1).arg(pendingMissionItems.size()));
}

void Mavlink::requestMissionList()
{
    mavlink_message_t msg;
    mavlink_msg_mission_request_list_pack(
        255,
        0,
        &msg,
        targetSystemId,
        targetComponentId,
        MAV_MISSION_TYPE_MISSION
    );
    sendMavlinkMessage(msg);
}

void Mavlink::requestMissionItem(uint16_t seq)
{
    mavlink_message_t msg;
    mavlink_msg_mission_request_int_pack(
        255,
        0,
        &msg,
        targetSystemId,
        targetComponentId,
        seq,
        MAV_MISSION_TYPE_MISSION
    );
    sendMavlinkMessage(msg);
    setMissionStatus(QStringLiteral("Downloading mission item %1 of %2").arg(seq + 1).arg(missionDownloadExpectedCount));
}

void Mavlink::sendMissionAck(uint8_t type)
{
    mavlink_message_t msg;
    mavlink_msg_mission_ack_pack(
        255,
        0,
        &msg,
        targetSystemId,
        targetComponentId,
        type,
        MAV_MISSION_TYPE_MISSION,
        0
    );
    sendMavlinkMessage(msg);
}

void Mavlink::handleMissionMessage(const mavlink_message_t &msg)
{
    if (msg.msgid == MAVLINK_MSG_ID_MISSION_COUNT) {
        if (!missionDownloadActive || missionUploadActive || missionClearBeforeUploadActive) {
            return;
        }

        mavlink_mission_count_t count;
        mavlink_msg_mission_count_decode(&msg, &count);
        if (count.mission_type != MAV_MISSION_TYPE_MISSION && count.mission_type != MAV_MISSION_TYPE_ALL) {
            return;
        }

        missionDownloadExpectedCount = count.count;
        downloadingMissionItems.clear();
        downloadedMissionItemSeqs.clear();

        if (missionDownloadExpectedCount == 0) {
            missionDownloadActive = false;
            missionSaveRequestedAfterDownload = false;
            pendingMissionItems.clear();
            loadedMissionWaypoints.clear();
            loadedMissionElevationWaypoints.clear();
            loadedMissionTerrainProfile.clear();
            setUploadedHomeLocation(QVariantMap());
            setMissionUploaded(false);
            emit missionWaypointsChanged();
            emit missionElevationWaypointsChanged();
            emit missionTerrainProfileChanged();
            setMissionStatus(QStringLiteral("Vehicle has no mission to save or load."));
            sendMissionAck(MAV_MISSION_ACCEPTED);
            return;
        }

        downloadingMissionItems.resize(missionDownloadExpectedCount);
        setMissionStatus(QStringLiteral("Vehicle mission found: %1 waypoint items.").arg(missionDownloadExpectedCount));
        requestMissionItem(0);
        return;
    }

    if (msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM_INT || msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM) {
        if (!missionDownloadActive || missionDownloadExpectedCount == 0) {
            return;
        }

        MissionItem item;
        if (msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM_INT) {
            mavlink_mission_item_int_t downloadedItem;
            mavlink_msg_mission_item_int_decode(&msg, &downloadedItem);
            if (downloadedItem.mission_type != MAV_MISSION_TYPE_MISSION && downloadedItem.mission_type != MAV_MISSION_TYPE_ALL) {
                return;
            }
            item.seq = downloadedItem.seq;
            item.current = downloadedItem.current;
            item.frame = downloadedItem.frame;
            item.command = downloadedItem.command;
            item.param1 = downloadedItem.param1;
            item.param2 = downloadedItem.param2;
            item.param3 = downloadedItem.param3;
            item.param4 = downloadedItem.param4;
            item.x = downloadedItem.x / 10000000.0;
            item.y = downloadedItem.y / 10000000.0;
            item.z = downloadedItem.z;
            item.autocontinue = downloadedItem.autocontinue;
        } else {
            mavlink_mission_item_t downloadedItem;
            mavlink_msg_mission_item_decode(&msg, &downloadedItem);
            if (downloadedItem.mission_type != MAV_MISSION_TYPE_MISSION && downloadedItem.mission_type != MAV_MISSION_TYPE_ALL) {
                return;
            }
            item.seq = downloadedItem.seq;
            item.current = downloadedItem.current;
            item.frame = downloadedItem.frame;
            item.command = downloadedItem.command;
            item.param1 = downloadedItem.param1;
            item.param2 = downloadedItem.param2;
            item.param3 = downloadedItem.param3;
            item.param4 = downloadedItem.param4;
            item.x = downloadedItem.x;
            item.y = downloadedItem.y;
            item.z = downloadedItem.z;
            item.autocontinue = downloadedItem.autocontinue;
        }

        if (item.seq >= missionDownloadExpectedCount) {
            sendMissionAck(MAV_MISSION_INVALID_SEQUENCE);
            missionDownloadActive = false;
            missionSaveRequestedAfterDownload = false;
            setMissionStatus(QStringLiteral("Mission download failed: invalid item sequence %1.").arg(item.seq));
            return;
        }

        downloadingMissionItems[item.seq] = item;
        downloadedMissionItemSeqs.insert(item.seq);

        if (downloadedMissionItemSeqs.size() < missionDownloadExpectedCount) {
            for (uint16_t seq = 0; seq < missionDownloadExpectedCount; ++seq) {
                if (!downloadedMissionItemSeqs.contains(seq)) {
                    requestMissionItem(seq);
                    break;
                }
            }
            return;
        }

        missionDownloadActive = false;
        pendingMissionItems = downloadingMissionItems;
        loadedMissionWaypoints = buildDisplayWaypoints(pendingMissionItems);
        loadedMissionElevationWaypoints = buildElevationWaypoints(pendingMissionItems);
        loadedMissionTerrainProfile = buildMissionTerrainProfile(loadedMissionElevationWaypoints);
        setUploadedHomeLocation(missionHomeLocation(pendingMissionItems));
        downloadingMissionItems.clear();
        downloadedMissionItemSeqs.clear();
        missionDownloadExpectedCount = 0;

        setMissionUploaded(!pendingMissionItems.isEmpty());
        emit missionWaypointsChanged();
        emit missionElevationWaypointsChanged();
        emit missionTerrainProfileChanged();
        sendMissionAck(MAV_MISSION_ACCEPTED);

        if (missionSaveRequestedAfterDownload) {
            missionSaveRequestedAfterDownload = false;
            const QString outputPath = saveLoadedMissionToMissionsFolder();
            if (outputPath.isEmpty()) {
                setMissionStatus(QStringLiteral("Vehicle mission downloaded, but saving failed."));
            } else {
                setMissionStatus(QStringLiteral("Saved vehicle mission: %1").arg(QFileInfo(outputPath).fileName()));
            }
        } else {
            setMissionStatus(QStringLiteral("Downloaded vehicle mission: %1 waypoint items.").arg(pendingMissionItems.size()));
        }
        return;
    }

    if (msg.msgid == MAVLINK_MSG_ID_COMMAND_ACK) {
        mavlink_command_ack_t ack;
        mavlink_msg_command_ack_decode(&msg, &ack);
        qDebug() << "Command ACK command:" << ack.command << "result:" << ack.result;

        // Older firmware can reject the modern stream-rate request while still
        // accepting the legacy REQUEST_DATA_STREAM fallback sent above.
        if (ack.command == MAV_CMD_SET_MESSAGE_INTERVAL) {
            return;
        }

        PendingMotorTestAck motorAck;
        bool hasMotorAck = false;
        if (ack.command == MAV_CMD_DO_MOTOR_TEST || ack.command == MAV_CMD_ACTUATOR_TEST) {
            for (int i = 0; i < pendingMotorTestAcks.size(); ++i) {
                if (pendingMotorTestAcks[i].command == ack.command) {
                    motorAck = pendingMotorTestAcks.takeAt(i);
                    hasMotorAck = true;
                    qDebug() << "Matched motor test ACK:"
                             << "mode" << motorAck.mode
                             << "motor" << motorAck.firstMotor
                             << "count" << motorAck.motorCount
                             << "durationSeconds" << motorAck.durationSeconds
                             << "sequenceId" << motorAck.sequenceId;
                    break;
                }
            }
        }

        PendingServoAck servoAck;
        bool hasServoAck = false;
        if (ack.command == MAV_CMD_DO_SET_SERVO && !pendingServoAcks.isEmpty()) {
            servoAck = pendingServoAcks.takeAt(0);
            hasServoAck = true;
            qDebug() << "Matched servo ACK:"
                     << "servo" << servoAck.servoNumber
                     << "pwm" << servoAck.pwm;
        }

        if (ack.result != MAV_RESULT_ACCEPTED && ack.result != MAV_RESULT_IN_PROGRESS) {
            QString resultText;
            switch (ack.result) {
            case MAV_RESULT_TEMPORARILY_REJECTED:
                resultText = QStringLiteral("temporarily rejected");
                break;
            case MAV_RESULT_DENIED:
                resultText = QStringLiteral("denied");
                break;
            case MAV_RESULT_UNSUPPORTED:
                resultText = QStringLiteral("unsupported");
                break;
            case MAV_RESULT_FAILED:
                resultText = QStringLiteral("failed");
                break;
            default:
                resultText = QStringLiteral("rejected");
                break;
            }
            const QString rejectionMessage = QStringLiteral("Command %1 %2 by autopilot.").arg(ack.command).arg(resultText);
            appendFlightControllerLogEntry(QStringLiteral("COMMAND_ACK"), msg.sysid, msg.compid, rejectionMessage);
            setMissionStatus(rejectionMessage);
            emit commandRejected(static_cast<int>(ack.command), resultText);
            if (hasMotorAck && motorAck.mode != QStringLiteral("stop")) {
                emit motorTestCommandRejected(motorAck.mode, motorAck.firstMotor, motorAck.motorCount, resultText);
            }
            if (hasServoAck) {
                emit servoCommandRejected(servoAck.servoNumber, servoAck.pwm, resultText);
            }
        } else if (hasMotorAck && motorAck.mode != QStringLiteral("stop")) {
            emit motorTestCommandAccepted(motorAck.mode, motorAck.firstMotor, motorAck.motorCount, motorAck.durationSeconds);
        } else if (hasServoAck) {
            emit servoCommandAccepted(servoAck.servoNumber, servoAck.pwm);
        }
        return;
    }

    if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_INT) {
        mavlink_mission_request_int_t request;
        mavlink_msg_mission_request_int_decode(&msg, &request);
        if (missionUploadActive) {
            sendMissionItem(request.seq, true);
        }
        return;
    }

    if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST) {
        mavlink_mission_request_t request;
        mavlink_msg_mission_request_decode(&msg, &request);
        if (missionUploadActive) {
            sendMissionItem(request.seq, false);
        }
        return;
    }

    if (msg.msgid == MAVLINK_MSG_ID_MISSION_ACK) {
        mavlink_mission_ack_t ack;
        mavlink_msg_mission_ack_decode(&msg, &ack);

        if (missionClearBeforeUploadActive) {
            missionClearBeforeUploadActive = false;
            missionUploadActive = true;
            setMissionStatus(QStringLiteral("Uploading mission: %1 waypoint items").arg(pendingMissionItems.size()));
            sendMissionCount();
            return;
        }

        if (!missionUploadActive) {
            return;
        }

        missionUploadActive = false;
        const bool ok = ack.type == MAV_MISSION_ACCEPTED;
        setMissionUploaded(ok);
        const QString message = ok
                ? QStringLiteral("Mission uploaded successfully.")
                : QStringLiteral("Mission upload failed. ACK type: %1").arg(ack.type);
        setMissionStatus(message);
        emit missionUploadFinished(ok, message);
    }
}

void Mavlink::setLink(ILink *newLink)
{
    if (!newLink || link == newLink) {
        return;
    }

    if (link) {
        if (link->isConnected()) {
            link->disconnectLink();
        }
        stopTelemetryLog();
        link->deleteLater();
    }

    link = newLink;
    inboundFrameBytes.clear();
    connect(link, &ILink::connected, this, [this]() {
        // Keep the public connection state false until readData receives a
        // valid autopilot heartbeat.  Opening a serial/TCP transport or a UDP
        // socket alone does not confirm a flight-controller connection.
        missionDownloadRequestedOnConnect = false;
    });
    connect(link, &ILink::readyRead, this, &Mavlink::readData);
    connect(link, &ILink::disconnected, this, [this]() {
        inboundFrameBytes.clear();
        stopTelemetryLog();
        setMissionUploaded(false);
        missionUploadActive = false;
        missionClearBeforeUploadActive = false;
        missionDownloadActive = false;
        missionDownloadRequestedOnConnect = false;
        missionSaveRequestedAfterDownload = false;
        missionDownloadExpectedCount = 0;
        downloadingMissionItems.clear();
        downloadedMissionItemSeqs.clear();
        clearPendingTerrainRequests();
        targetSystemId = 1;
        targetComponentId = MAV_COMP_ID_AUTOPILOT1;
        resetParameterCache();
        setConnected(false);
    });
}

QString Mavlink::saveLoadedMissionToMissionsFolder() const
{
    if (pendingMissionItems.isEmpty()) {
        return QString();
    }

    const QString outputFolder = AppDataPaths::missionsDirectory();
    if (!AppDataPaths::ensureDirectory(outputFolder)) {
        return QString();
    }

    const QString fileName = QStringLiteral("vehicle_mission_%1.waypoints")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString outputPath = QDir(outputFolder).filePath(fileName);

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qDebug() << "Could not save vehicle mission:" << file.errorString();
        return QString();
    }

    QTextStream stream(&file);
    stream.setRealNumberPrecision(10);
    stream << "QGC WPL 110\n";
    for (int i = 0; i < pendingMissionItems.size(); ++i) {
        const MissionItem &item = pendingMissionItems.at(i);
        stream << i << '\t'
               << int(item.current) << '\t'
               << int(item.frame) << '\t'
               << int(item.command) << '\t'
               << item.param1 << '\t'
               << item.param2 << '\t'
               << item.param3 << '\t'
               << item.param4 << '\t'
               << item.x << '\t'
               << item.y << '\t'
               << item.z << '\t'
               << int(item.autocontinue) << '\n';
    }

    if (stream.status() != QTextStream::Ok) {
        qDebug() << "Could not write vehicle mission file:" << outputPath;
        return QString();
    }

    return QFileInfo(file).absoluteFilePath();
}

bool Mavlink::parseMissionFile(const QString &path, QVector<MissionItem> &items, QVariantList &displayWaypoints, QVariantMap &homeLocation, QString &error) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QStringLiteral("Could not open mission file: %1").arg(file.errorString());
        return false;
    }

    QTextStream stream(&file);
    const QString header = stream.readLine().trimmed();
    if (header != QStringLiteral("QGC WPL 110")) {
        error = QStringLiteral("Unsupported mission file. Expected QGC WPL 110.");
        return false;
    }

    int lineNumber = 1;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty() || line.startsWith("#")) {
            continue;
        }

        const QStringList columns = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (columns.size() < 12) {
            error = QStringLiteral("Invalid waypoint line %1.").arg(lineNumber);
            return false;
        }

        MissionItem item;
        bool ok = true;
        const uint16_t fileSeq = static_cast<uint16_t>(columns[0].toUShort(&ok));
        if (!ok) { error = QStringLiteral("Invalid numeric value on waypoint line %1.").arg(lineNumber); return false; }
        item.current = static_cast<uint8_t>(columns[1].toUInt(&ok));
        if (!ok) { error = QStringLiteral("Invalid numeric value on waypoint line %1.").arg(lineNumber); return false; }
        item.frame = static_cast<uint8_t>(columns[2].toUInt(&ok));
        if (!ok) { error = QStringLiteral("Invalid numeric value on waypoint line %1.").arg(lineNumber); return false; }
        item.command = static_cast<uint16_t>(columns[3].toUShort(&ok));
        if (!ok) { error = QStringLiteral("Invalid numeric value on waypoint line %1.").arg(lineNumber); return false; }
        item.param1 = columns[4].toFloat(&ok);
        if (!ok) { error = QStringLiteral("Invalid numeric value on waypoint line %1.").arg(lineNumber); return false; }
        item.param2 = columns[5].toFloat(&ok);
        if (!ok) { error = QStringLiteral("Invalid numeric value on waypoint line %1.").arg(lineNumber); return false; }
        item.param3 = columns[6].toFloat(&ok);
        if (!ok) { error = QStringLiteral("Invalid numeric value on waypoint line %1.").arg(lineNumber); return false; }
        item.param4 = columns[7].toFloat(&ok);
        if (!ok) { error = QStringLiteral("Invalid numeric value on waypoint line %1.").arg(lineNumber); return false; }
        item.x = columns[8].toDouble(&ok);
        if (!ok) { error = QStringLiteral("Invalid numeric value on waypoint line %1.").arg(lineNumber); return false; }
        item.y = columns[9].toDouble(&ok);
        if (!ok) { error = QStringLiteral("Invalid numeric value on waypoint line %1.").arg(lineNumber); return false; }
        item.z = columns[10].toFloat(&ok);
        if (!ok) { error = QStringLiteral("Invalid numeric value on waypoint line %1.").arg(lineNumber); return false; }
        item.autocontinue = static_cast<uint8_t>(columns[11].toUInt(&ok));
        if (!ok) { error = QStringLiteral("Invalid numeric value on waypoint line %1.").arg(lineNumber); return false; }

        const bool isHomeRow = fileSeq == 0 && item.current == 1 && item.command == MAV_CMD_NAV_WAYPOINT;
        if (isHomeRow) {
            if (hasValidCoordinate(item.x, item.y) && qIsFinite(item.z)) {
                homeLocation[QStringLiteral("lat")] = item.x;
                homeLocation[QStringLiteral("lon")] = item.y;
                homeLocation[QStringLiteral("alt")] = item.z;
            }
            continue;
        }

        item.seq = static_cast<uint16_t>(items.size());
        item.current = items.isEmpty() ? 1 : 0;
        items.append(item);

        const bool hasCoordinate = hasValidCoordinate(item.x, item.y);
        if (!hasCoordinate || !isGlobalFrame(item.frame) || !isDisplayableNavigationCommand(item.command)) {
            continue;
        }

        bool repeatsPreviousDisplayPoint = false;
        if (!displayWaypoints.isEmpty()) {
            QVariantMap waypoint = displayWaypoints.last().toMap();
            const double waypointLat = waypoint.value(QStringLiteral("lat")).toDouble();
            const double waypointLon = waypoint.value(QStringLiteral("lon")).toDouble();
            repeatsPreviousDisplayPoint = distanceMeters(waypointLat, waypointLon, item.x, item.y) <= 15.0;
            const uint16_t previousCommand = static_cast<uint16_t>(waypoint.value(QStringLiteral("command")).toUInt());
            if (repeatsPreviousDisplayPoint && navigationDisplayPriority(item.command) > navigationDisplayPriority(previousCommand)) {
                displayWaypoints[displayWaypoints.size() - 1] = makeDisplayWaypoint(item.seq, item.command, item.frame, item.x, item.y, item.z);
            }
        }

        if (!repeatsPreviousDisplayPoint) {
            displayWaypoints.append(makeDisplayWaypoint(item.seq, item.command, item.frame, item.x, item.y, item.z));
        }
    }

    if (items.isEmpty()) {
        error = QStringLiteral("Mission file has no waypoint items.");
        return false;
    }

    if (displayWaypoints.isEmpty()) {
        error = QStringLiteral("Mission file has no map-displayable waypoint coordinates.");
        return false;
    }

    qDebug() << "Parsed mission file" << QFileInfo(path).fileName() << "items" << items.size();
    return true;
}

QVariantMap Mavlink::missionHomeLocation(const QVector<MissionItem> &items) const
{
    for (const MissionItem &item : items) {
        const bool isHomeRow = item.seq == 0 && item.current == 1 && item.command == MAV_CMD_NAV_WAYPOINT;
        if (!isHomeRow || !hasValidCoordinate(item.x, item.y) || !qIsFinite(item.z)) {
            continue;
        }

        QVariantMap homeLocation;
        homeLocation[QStringLiteral("lat")] = item.x;
        homeLocation[QStringLiteral("lon")] = item.y;
        homeLocation[QStringLiteral("alt")] = item.z;
        return homeLocation;
    }

    return QVariantMap();
}

QVariantList Mavlink::buildDisplayWaypoints(const QVector<MissionItem> &items) const
{
    QVariantList displayWaypoints;

    for (const MissionItem &item : items) {
        const bool hasCoordinate = hasValidCoordinate(item.x, item.y);
        if (!hasCoordinate || !isGlobalFrame(item.frame) || !isDisplayableNavigationCommand(item.command)) {
            continue;
        }

        bool repeatsPreviousDisplayPoint = false;
        if (!displayWaypoints.isEmpty()) {
            QVariantMap waypoint = displayWaypoints.last().toMap();
            const double waypointLat = waypoint.value(QStringLiteral("lat")).toDouble();
            const double waypointLon = waypoint.value(QStringLiteral("lon")).toDouble();
            repeatsPreviousDisplayPoint = distanceMeters(waypointLat, waypointLon, item.x, item.y) <= 15.0;
            const uint16_t previousCommand = static_cast<uint16_t>(waypoint.value(QStringLiteral("command")).toUInt());
            if (repeatsPreviousDisplayPoint && navigationDisplayPriority(item.command) > navigationDisplayPriority(previousCommand)) {
                displayWaypoints[displayWaypoints.size() - 1] = makeDisplayWaypoint(item.seq, item.command, item.frame, item.x, item.y, item.z);
            }
        }

        if (!repeatsPreviousDisplayPoint) {
            displayWaypoints.append(makeDisplayWaypoint(item.seq, item.command, item.frame, item.x, item.y, item.z));
        }
    }

    return displayWaypoints;
}

QVariantList Mavlink::buildElevationWaypoints(const QVector<MissionItem> &items) const
{
    QVariantList elevationWaypoints;

    for (const MissionItem &item : items) {
        const bool hasCoordinate = hasValidCoordinate(item.x, item.y);
        if (!hasCoordinate || !isGlobalFrame(item.frame) || !isDisplayableNavigationCommand(item.command)) {
            continue;
        }

        elevationWaypoints.append(makeDisplayWaypoint(item.seq, item.command, item.frame, item.x, item.y, item.z));
    }

    return elevationWaypoints;
}

QVariantList Mavlink::buildMissionTerrainProfile(const QVariantList &profileWaypoints)
{
    QVariantList profile;
    if (profileWaypoints.isEmpty()) {
        return profile;
    }

    struct ProfileWaypoint {
        double latitude = 0.0;
        double longitude = 0.0;
        double distance = 0.0;
    };

    QVector<ProfileWaypoint> waypoints;
    waypoints.reserve(profileWaypoints.size());

    double totalDistance = 0.0;
    bool hasPrevious = false;
    ProfileWaypoint previous;

    for (const QVariant &waypointValue : profileWaypoints) {
        const QVariantMap waypoint = waypointValue.toMap();
        const double latitude = waypoint.value(QStringLiteral("lat")).toDouble();
        const double longitude = waypoint.value(QStringLiteral("lon")).toDouble();
        if (!qIsFinite(latitude) || !qIsFinite(longitude)
                || latitude < -90.0 || latitude > 90.0
                || longitude < -180.0 || longitude > 180.0) {
            continue;
        }

        ProfileWaypoint point;
        point.latitude = latitude;
        point.longitude = longitude;
        if (hasPrevious) {
            totalDistance += distanceMeters(previous.latitude, previous.longitude, latitude, longitude);
        }
        point.distance = totalDistance;
        waypoints.append(point);
        previous = point;
        hasPrevious = true;
    }

    if (waypoints.isEmpty()) {
        return profile;
    }

    QVector<TerrainProvider::Coordinate> path;
    path.reserve(waypoints.size());
    for (const ProfileWaypoint &waypoint : std::as_const(waypoints)) {
        TerrainProvider::Coordinate coordinate;
        coordinate.latitude = waypoint.latitude;
        coordinate.longitude = waypoint.longitude;
        path.append(coordinate);
    }

    const QVector<TerrainProvider::TerrainSample> terrainSamples = terrainProvider.queryPath(path, 60.0);
    profile.reserve(terrainSamples.size());
    for (const TerrainProvider::TerrainSample &terrainSample : terrainSamples) {
        if (!terrainSample.hasTerrain) {
            continue;
        }

        QVariantMap sample;
        sample[QStringLiteral("distance")] = terrainSample.distanceMeters;
        sample[QStringLiteral("terrain")] = terrainSample.terrainHeightMeters;
        sample[QStringLiteral("lat")] = terrainSample.latitude;
        sample[QStringLiteral("lon")] = terrainSample.longitude;
        profile.append(sample);
    }

    return profile;
}
