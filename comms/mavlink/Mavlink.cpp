#include "Mavlink.h"
#include "parameters/ParameterManager.h"
#include "mavlink/MavlinkConsoleController.h"

#include "mavlink/MavlinkInspectorController.h"
#include "mavlink/MavlinkForwarder.h"

#include "transport/serial/SerialLink.h"
#include "transport/tcp/TcpLink.h"
#include "transport/udp/UdpLink.h"
#include "storage/AppDataPaths.h"
#include "mission/MissionPlanConverter.h"
#include "../../integrations/VPS/VpsProtocol.h"
#include <cmath>
#include <chrono>
#include <algorithm>

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

Mavlink::Mavlink(QObject *parent, ParameterManager *parameters, std::function<qint64()> monotonicClock)
    : QObject(parent)
{
    vpsTimeoutTimer = new QTimer(this);
    vpsTimeoutTimer->setSingleShot(true);
    vpsTimeoutTimer->setInterval(5000);
    connect(vpsTimeoutTimer, &QTimer::timeout, this, &Mavlink::clearVpsStatus);
    missionTransferTimer = new QTimer(this);
    missionTransferTimer->setSingleShot(true);

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

    connectionClock = std::move(monotonicClock);
    if (!connectionClock) connectionClock = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    };
    heartbeatTimer.setSingleShot(true);
    heartbeatTimer.setTimerType(Qt::PreciseTimer);
    parameterStartupTimer.setSingleShot(true);
    consoleController = new MavlinkConsoleController(this);
    setLink(new TcpLink(this));
    refreshSerialPorts();

    QTimer *serialPortRefreshTimer = new QTimer(this);
    serialPortRefreshTimer->setInterval(1500);
    connect(serialPortRefreshTimer, &QTimer::timeout, this, &Mavlink::refreshSerialPorts);
    serialPortRefreshTimer->start();

    motorTestSequenceTimer = new QTimer(this);
    motorTestSequenceTimer->setSingleShot(true);


    motorTestStopTimer = new QTimer(this);
    motorTestStopTimer->setSingleShot(true);
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

    parameterManagerObject = parameters ? parameters : new ParameterManager(this);
    connect(parameterManagerObject, &ParameterManager::statusChanged, this, &Mavlink::parameterFetchStatusChanged);
    connect(parameterManagerObject, &ParameterManager::parameterUpdated, this, &Mavlink::parameterUpdated);
    // Presentation adapter only: use the same validator as command admission.
    connect(this, &Mavlink::connectionLifecycleChanged, this, &Mavlink::motorTestPrerequisitesChanged);
    connect(this, &Mavlink::heartbeatObserved, this, &Mavlink::motorTestPrerequisitesChanged);
    connect(parameterManagerObject, &ParameterManager::statusChanged, this, &Mavlink::motorTestPrerequisitesChanged);
    connect(parameterManagerObject, &ParameterManager::parameterUpdated, this, [this](const QString &name, const QVariantMap &) {
        if (name == "FRAME_CLASS" || name == "FRAME_TYPE") emit motorTestPrerequisitesChanged();
    });
    connect(parameterManagerObject, &ParameterManager::statusChanged, this, &Mavlink::revalidateMotorSchedule);
    connect(parameterManagerObject, &ParameterManager::parameterUpdated, this, [this](const QString &name, const QVariantMap &) {
        if (name == QLatin1String("FRAME_CLASS") || name == QLatin1String("FRAME_TYPE")) revalidateMotorSchedule();
    });
    connect(parameterManagerObject, &ParameterManager::fetchFinished, this, &Mavlink::parameterFetchFinished);
    connect(parameterManagerObject, &ParameterManager::requestList, this,
            [this](quint8 system, quint8 component, quint64 generation) {
        if (!parameterManagerObject || !connected() || system != targetSystemId || component != targetComponentId
                || generation != parameterManagerObject->generation()
                || parameterManagerObject->syncState() != ParameterManager::Loading) return;
        mavlink_message_t msg = {};
        mavlink_msg_param_request_list_pack(255, 0, &msg, system, component);
        sendMavlinkMessage(msg);
    });

    connect(parameterManagerObject, &ParameterManager::requestRead, this,
            [this](quint8 system, quint8 component, int index, quint64 generation) {
        if (!parameterManagerObject || !connected() || system != targetSystemId || component != targetComponentId
                || generation != parameterManagerObject->generation()
                || parameterManagerObject->syncState() != ParameterManager::Recovering
                || !parameterManagerObject->parameterIndexMissing(index)
                || index > INT16_MAX) return;
        mavlink_message_t msg = {};
        const char unusedId[16] = {}; // Index selects the parameter; param_id is ignored by the protocol.
        mavlink_msg_param_request_read_pack(255, 0, &msg, system, component, unusedId, int16_t(index));
        sendMavlinkMessage(msg);
    });

    connect(parameterManagerObject, &ParameterManager::writeFinished, this, &Mavlink::parameterWriteFinished);
    connect(parameterManagerObject, &ParameterManager::writeBatchFinished, this, [this](const QVariantMap &result) {
        if (rcCalibrationBatches.remove(result.value("id").toULongLong())) emit rcCalibrationWriteFinished(result);
    });
    connect(parameterManagerObject, &ParameterManager::requestSet, this,
            [this](quint64 id, quint8 system, quint8 component, const QString &name,
                   float value, quint8 type, quint64 generation, int attempt) {
        if (!parameterManagerObject || !parameterManagerObject->writeTransmissionCurrent(id, generation, attempt)) return;
        bool sent = false;
        if (connected() && system == targetSystemId && component == targetComponentId) {
            char paramId[16] = {};
            const auto bytes = name.toLatin1();
            if (!bytes.isEmpty() && bytes.size() <= 16) {
                memcpy(paramId, bytes.constData(), size_t(bytes.size()));
                mavlink_message_t message = {};
                mavlink_msg_param_set_pack(255, 0, &message, system, component, paramId, value, type);
                sent = sendMavlinkMessage(message);
            }
        }
        parameterManagerObject->writeTransportResult(id, generation, attempt, sent);
    });

    qualityTimer.setInterval(qualityConfig.tickMs);
    connect(&qualityTimer, &QTimer::timeout, this, &Mavlink::processLinkQuality);
    connect(this, &Mavlink::connectionLifecycleChanged, this, [this] {
        if (qualityGeneration != connectionGenerationValue) resetLinkQuality();
        if (connected()) qualityTimer.start(qualityConfig.tickMs);
        else qualityTimer.stop();
    });
    linkPingTimer = new QTimer(this);
    linkPingTimer->setInterval(1000);
    connect(linkPingTimer, &QTimer::timeout, this, &Mavlink::sendLinkPing);
}

bool Mavlink::connected() const
{
    return connectionStateValue == Connected;
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

QVector<MavlinkMissionItem> Mavlink::missionItems() const
{
    return pendingMissionItems;
}

Mavlink::~Mavlink()
{
    shuttingDown = true;
    if (link) QObject::disconnect(link, nullptr, this, nullptr);
    endConnection(QStringLiteral("MAVLink object destroyed"), false);
    stopLifecycleTimers();
}

ParameterManager *Mavlink::parameterManager() const { return parameterManagerObject.data(); }

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

Mavlink::TransferState Mavlink::missionTransferState() const { return transferStateValue; }
quint64 Mavlink::missionTransferId() const { return transferIdValue; }
int Mavlink::missionTransferProgress() const { return transferProgressValue; }
int Mavlink::missionTransferTotal() const { return transferTotalValue; }
int Mavlink::missionTransferPercent() const { return transferTotalValue > 0 ? (100 * transferProgressValue) / transferTotalValue : 0; }
QString Mavlink::missionTransferOperation() const
{
    switch (transferStateValue) {
    case ClearingBeforeUpload: case Uploading: return QStringLiteral("upload");
    case Downloading: return QStringLiteral("download");
    case Clearing: return QStringLiteral("clear");
    default: return QString();
    }
}
QString Mavlink::missionTransferErrorCode() const { return transferErrorCodeValue; }
void Mavlink::setMissionPlanModel(MissionPlanModel *model) { missionPlanModel = model; }

QStringList Mavlink::serialPorts() const
{
    return serialPortNames;
}

bool Mavlink::parameterFetchInProgress() const
{
    return parameterManagerObject->syncState() == ParameterManager::Loading
            || parameterManagerObject->syncState() == ParameterManager::Recovering;
}

int Mavlink::parameterFetchProgress() const { return parameterManagerObject->progress(); }
int Mavlink::parameterCount() const { return parameterManagerObject->expectedParameterCount(); }
int Mavlink::loadedParameterCount() const { return parameterManagerObject->receivedParameterCount(); }
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
    if (shuttingDown || transitioningConnection) return false;
    const QString normalizedProtocol = protocol.trimmed().toUpper();

    if (normalizedProtocol == QStringLiteral("SERIAL")) {
        refreshSerialPorts();
        if (!serialPortNames.contains(hostOrPortName)) {
            endConnection(QStringLiteral("Transport connection failed"), false);
            if (link) link->disconnectLink();
            qDebug() << "Serial port unavailable:" << hostOrPortName;
            return false;
        }
        if (!setLink(new SerialLink(this))) return false;
        port = baudRate;
    } else if (normalizedProtocol == QStringLiteral("UDP")) {
        if (!setLink(new UdpLink(this))) return false;
    } else {
        if (!setLink(new TcpLink(this))) return false;
    }

    diagnosticEndpointValue = normalizedProtocol + ":" + hostOrPortName + ":" + QString::number(port);
    receivedPackets = transmittedPackets = detectedSequenceGaps = 0;
    transportDuplicates = transportReordered = 0;
    lastPacketAt = -1;
    packetRateStartedAt = connectionClock();
    packetTimes.clear();
    beginConnection();
    if (link->connectLink(hostOrPortName, port, 3000)) {
        qDebug() << "Connection started via" << normalizedProtocol << "to" << hostOrPortName << port;
        return true;
    } else {
        endConnection(QStringLiteral("Transport connection failed"), false);
        qDebug() << "Connection failed:" << link->errorString();
        return false;
    }
}

void Mavlink::disconnectFromVehicle()
{
    auto *disconnectingLink = link;
    const auto transport = transportGeneration;
    endConnection(QStringLiteral("Explicit disconnect"), false);
    if (link && link == disconnectingLink && transport == transportGeneration) link->disconnectLink();
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
    if (!connected()) {
        rejectCommand("takeoff", "Vehicle is not connected.");
        qDebug() << "Cannot start takeoff while disconnected";
        return;
    }

    const double safeAltitudeMeters = qBound(1.0, altitudeMeters, 100.0);
    sendSetMode(4);

    const auto generation = connectionGenerationValue;
    QTimer::singleShot(1200, this, [this, safeAltitudeMeters, generation]() {
        if (!connected() || generation != connectionGenerationValue) {
            return;
        }

        sendCommandLong(MAV_CMD_NAV_TAKEOFF, 0, 0, 0, 0, 0, 0, static_cast<float>(safeAltitudeMeters));
    });
}

void Mavlink::startMission()
{
    if (!connected()) {
        rejectCommand("missionStart", "Vehicle is not connected.");
        qDebug() << "Cannot start mission while disconnected";
        return;
    }

    sendSetMode(3);
    const auto generation = connectionGenerationValue;
    QTimer::singleShot(900, this, [this, generation]() {
        if (!connected() || generation != connectionGenerationValue) {
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
        rejectCommand("setFlightMode", "Flight mode must be non-negative.");
        return;
    }

    sendSetMode(static_cast<uint32_t>(customMode));
}

void Mavlink::motor()
{
    armVehicle(connected());
}

bool Mavlink::vpsConnected() const
{
    return vpsConnectedState;
}

QVariantMap Mavlink::vpsStatus() const
{
    return vpsStatusValues;
}

QVariantMap Mavlink::vpsConfig() const { return vpsConfigValues; }

void Mavlink::clearVpsStatus()
{
    vpsTimeoutTimer->stop();
    if (!vpsConnectedState && vpsStatusValues.isEmpty())
        return;
    vpsConnectedState = false;
    vpsSystemId = 0;
    vpsComponentId = 0;
    vpsStatusValues.clear();
    vpsConfigValues.clear();
    emit vpsConfigChanged();
    emit vpsStatusChanged();
}

void Mavlink::handleVpsMessage(const mavlink_message_t &message)
{
    // The supplied VPS integration identifies status packets from sys 200,
    // component 14. Learn the reply target only from a valid status packet.
    if (!connected() || message.msgid != MAVLINK_MSG_ID_TUNNEL
            || message.sysid != 200 || message.compid != 14)
        return;

    mavlink_tunnel_t tunnel = {};
    mavlink_msg_tunnel_decode(&message, &tunnel);
    if (tunnel.payload_length > sizeof(tunnel.payload))
        return;

    const QByteArray payload(reinterpret_cast<const char *>(tunnel.payload), tunnel.payload_length);
    if (tunnel.payload_type == Vps::RX_CONFIG) {
        Vps::Config config = {};
        if (!vpsConnectedState || !Vps::parseConfig(payload, config)) return;
        vpsConfigValues = {
            {"latitude", config.initLat}, {"longitude", config.initLon},
            {"logging", bool(config.logAutoStart)}, {"sensorSpeed", config.sensorSpeed},
            {"fixType", config.gps.fixType}, {"satCount", config.gps.satCount},
            {"hdop", config.gps.hdop}, {"hAcc", config.gps.hAccMm}, {"vAcc", config.gps.vAccMmS},
            {"healthy", config.gps.healthySec}, {"unhealthy", config.gps.unhealthySec},
            {"jamEnabled", config.jam.enable}, {"jamTime", config.jam.timeSampleSec},
            {"jamHAcc", config.jam.deltaHAccMm}, {"jamVAcc", config.jam.deltaVAccMmS},
            {"jamUnhealthy", config.jam.unhealthySec}
        };
        emit vpsConfigChanged();
        return;
    }
    if (tunnel.payload_type != Vps::RX_STATUS) return;
    Vps::Status status = {};
    if (!Vps::parseStatus(payload, status))
        return;

    vpsSystemId = message.sysid;
    vpsComponentId = message.compid;
    vpsConnectedState = true;
    vpsStatusValues = {
        {QStringLiteral("sensors"), Vps::processStatusText(status.sensorStatus)},
        {QStringLiteral("vio"), Vps::processStatusText(status.vioStatus)},
        {QStringLiteral("vpr"), Vps::processStatusText(status.vprStatus)},
        {QStringLiteral("logging"), Vps::processStatusText(status.logStatus)},
        {QStringLiteral("autoAid"), status.aidFlag ? QStringLiteral("On") : QStringLiteral("Off")},
        {"vioLat", status.vioLat}, {"vioLon", status.vioLon},
        {"vprLat", status.vprLat}, {"vprLon", status.vprLon},
        {"fusedLat", status.fusedLat}, {"fusedLon", status.fusedLon}
    };
    vpsTimeoutTimer->start();
    emit vpsStatusChanged();
}

bool Mavlink::writeVpsMessage(uint16_t type, const QByteArray &payload)
{
    if (!connected() || !vpsConnectedState || !link || !link->isConnected()
            || payload.size() > 128)
        return false;

    uint8_t data[128] = {};
    std::memcpy(data, payload.constData(), static_cast<size_t>(payload.size()));
    mavlink_message_t message = {};
    mavlink_msg_tunnel_pack(255, MAV_COMP_ID_MISSIONPLANNER, &message,
                            vpsSystemId, vpsComponentId, type,
                            static_cast<uint8_t>(payload.size()), data);
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN] = {};
    const int length = mavlink_msg_to_send_buffer(buffer, &message);
    const qint64 written = link->writeData(reinterpret_cast<const char *>(buffer), length);
    if (written != length)
        return false;
    ++transmittedPackets;
    if (shouldLogTelemetryMessage(message))
        writeTelemetryLogRecord(reinterpret_cast<const char *>(buffer), length);
    link->flush();
    return true;
}

bool Mavlink::startVpsAction(int action)
{
    if (action >= 0 && action <= 3)
        return writeVpsMessage(Vps::TX_PROCESS, Vps::buildProcess(static_cast<Vps::Process>(action), true));
    if (action == 4)
        return writeVpsMessage(Vps::TX_AIDING, Vps::buildByte(1));
    if (action == 5)
        return writeVpsMessage(Vps::TX_AUTO_AIDING, Vps::buildByte(1));
    return false;
}

bool Mavlink::refreshVpsConfig() { return writeVpsMessage(Vps::TX_REFRESH_CONFIG, Vps::buildByte(1)); }
bool Mavlink::setVpsHome(double lat, double lon)
{
    return std::isfinite(lat) && std::isfinite(lon) && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180
            && writeVpsMessage(Vps::TX_SET_HOME, Vps::buildHome(lat, lon));
}
bool Mavlink::setVpsLogging(bool enabled) { return writeVpsMessage(Vps::TX_LOG_AUTOSTART, Vps::buildByte(enabled)); }
bool Mavlink::setVpsSensorSpeed(int speed)
{
    return speed >= 0 && speed <= 1 && writeVpsMessage(Vps::TX_SENSOR_SPEED, Vps::buildByte(speed));
}
bool Mavlink::setVpsGpsCheck(const QVariantMap &v)
{
    const auto valid = [&v](const char *key, double max) {
        bool ok = false; const double n = v.value(QLatin1String(key)).toDouble(&ok);
        return ok && std::isfinite(n) && n >= 0 && n <= max;
    };
    if (!valid("fixType", 6) || v.value("fixType").toInt() < 3 || !valid("satCount", 255)
            || !valid("hdop", 100) || !valid("hAcc", 1000000) || !valid("vAcc", 1000000)
            || !valid("healthy", 3600) || !valid("unhealthy", 3600)) return false;
    Vps::GpsCheck g;
    g.fixType = v.value("fixType").toInt(); g.satCount = v.value("satCount").toInt();
    g.hdop = v.value("hdop").toFloat(); g.hAccMm = v.value("hAcc").toInt(); g.vAccMmS = v.value("vAcc").toInt();
    g.healthySec = v.value("healthy").toFloat(); g.unhealthySec = v.value("unhealthy").toFloat();
    return writeVpsMessage(Vps::TX_GPS_CHECK, Vps::buildGpsCheck(g));
}
bool Mavlink::setVpsGpsJamming(const QVariantMap &v)
{
    const auto valid = [&v](const char *key, double max) {
        bool ok = false; const double n = v.value(QLatin1String(key)).toDouble(&ok);
        return ok && std::isfinite(n) && n >= 0 && n <= max;
    };
    if (!v.contains("enabled") || !valid("time", 3600) || !valid("hAcc", 1000000)
            || !valid("vAcc", 1000000) || !valid("unhealthy", 3600)) return false;
    Vps::GpsJamming j;
    j.enable = v.value("enabled").toBool(); j.timeSampleSec = v.value("time").toFloat();
    j.deltaHAccMm = v.value("hAcc").toInt(); j.deltaVAccMmS = v.value("vAcc").toInt();
    j.unhealthySec = v.value("unhealthy").toFloat();
    return writeVpsMessage(Vps::TX_GPS_JAMMING, Vps::buildGpsJamming(j));
}

bool Mavlink::sendPayloadCommand(int commandId, int watchdogMinutes)
{
    if (!connected()) {
        qDebug() << "Cannot send payload command while disconnected:" << commandId;
        return false;
    }

    if (commandId < 0 || commandId > 255 || watchdogMinutes < 0 || watchdogMinutes > 255) {
        qDebug() << "Rejected invalid payload command:" << commandId << watchdogMinutes;
        return false;
    }

    // Payload-control commands use MAV_CMD_USER_2 and are handled by the
    // companion computer rather than the autopilot component.
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(
        255,
        0,
        &msg,
        targetSystemId,
        MAV_COMP_ID_ONBOARD_COMPUTER,
        MAV_CMD_USER_2,
        0,
        static_cast<float>(commandId),
        static_cast<float>(watchdogMinutes),
        0,
        0,
        0,
        0,
        0
    );

    sendMavlinkMessage(msg);
    qDebug() << "Sent payload command:" << commandId
             << "watchdog minutes:" << watchdogMinutes
             << "target system:" << targetSystemId
             << "target component:" << MAV_COMP_ID_ONBOARD_COMPUTER;
    return true;
}

bool Mavlink::rejectCommand(const QString &operation, const QString &reason)
{
    const QVariantMap context{{"accepted", false}, {"connected", connected()}, {"connectionState", int(connectionStateValue)},
                             {"connectionGeneration", QVariant::fromValue(connectionGenerationValue)},
                             {"parameterState", parameterManagerObject ? int(parameterManagerObject->syncState()) : -1},
                             {"parametersReady", parameterManagerObject && parameterManagerObject->parametersReady()}};
    emit commandPrerequisiteRejected(operation, reason, context);
    return false;
}

QString Mavlink::liveConnectionError() const
{
    return connected() && link && link->isConnected() ? QString() : QStringLiteral("Vehicle is not connected.");
}

QString Mavlink::parameterPrerequisiteError(const QStringList &required) const
{
    const auto connectionError = liveConnectionError();
    if (!connectionError.isEmpty()) return connectionError;
    if (!parameterManagerObject) return QStringLiteral("Parameter manager is unavailable.");
    if (!parameterManagerObject->parametersReady()) {
        switch (parameterManagerObject->syncState()) {
        case ParameterManager::Loading: return QStringLiteral("Parameters are loading.");
        case ParameterManager::Recovering: return QStringLiteral("Parameters are recovering.");
        case ParameterManager::Failed: return QStringLiteral("Parameter synchronization failed: %1").arg(parameterManagerObject->syncError());
        default: return QStringLiteral("Current validated parameters are not available.");
        }
    }
    if (parameterManagerObject->snapshotProvenance().value("sourceConnectionGeneration").toULongLong() != connectionGenerationValue)
        return QStringLiteral("Parameter snapshot belongs to a stale connection.");
    for (const auto &name : required) {
        const auto record = parameterManagerObject->parameterValue(name);
        if (record.isEmpty()) return QStringLiteral("Required parameter %1 is missing.").arg(name);
        const int type = record.value("type").toInt();
        bool numeric = false;
        const double value = record.value("value").toDouble(&numeric);
        const int index = record.value("index", -1).toInt();
        if (!numeric || !std::isfinite(value) || index < 0 || index >= parameterManagerObject->expectedParameterCount()
                || !((type >= MAV_PARAM_TYPE_UINT8 && type <= MAV_PARAM_TYPE_INT32) || type == MAV_PARAM_TYPE_REAL32))
            return QStringLiteral("Required parameter %1 has invalid metadata.").arg(name);
    }
    return {};
}

QString Mavlink::motorConfigurationError(int &motorCount, QString *signature) const
{
    auto error = parameterPrerequisiteError({QStringLiteral("FRAME_CLASS")});
    if (!error.isEmpty()) return error;
    if (selectedAutopilot != MAV_AUTOPILOT_ARDUPILOTMEGA) return QStringLiteral("Motor frame configuration is unsupported for this autopilot.");
    const auto frame = parameterValue(QStringLiteral("FRAME_CLASS"));
    const auto value = frame.value("value").toDouble();
    if (std::trunc(value) != value || value < 1 || value > 14) return QStringLiteral("Unsupported FRAME_CLASS value.");
    const int frameClass = int(value);
    // Static ArduCopter classes; scripting and helicopter layouts cannot supply
    // a trustworthy motor count through this existing FRAME_CLASS API.
    static const QHash<int, int> counts{{1,4}, {2,6}, {3,8}, {4,8}, {5,6}, {7,3}, {9,2}, {10,2}, {12,12}, {14,10}};
    if (!counts.contains(frameClass)) return QStringLiteral("Vehicle frame configuration is unsupported for motor testing.");
    motorCount = counts.value(frameClass);
    int frameType = -1;
    if (frameClass != 5 && frameClass != 7 && frameClass != 9 && frameClass != 10) {
        error = parameterPrerequisiteError({QStringLiteral("FRAME_TYPE")});
        if (!error.isEmpty()) return error;
        const double type = parameterValue(QStringLiteral("FRAME_TYPE")).value("value").toDouble();
        static const QSet<int> types{0,1,2,3,4,5,6,12,13,14,15,16,17,18,19};
        if (std::trunc(type) != type || type < 0 || type > 19 || !types.contains(int(type)))
            return QStringLiteral("Vehicle FRAME_TYPE is unsupported for motor testing.");
        frameType = int(type);
    }
    if (selectedHeartbeatArmed) return QStringLiteral("Motor testing requires a disarmed vehicle.");
    if (signature) *signature = QStringLiteral("%1:%2:%3").arg(frameClass).arg(frameType).arg(motorCount);
    return {};
}

QVariantMap Mavlink::motorTestPrerequisites() const
{
    int count = 0;
    const auto reason = motorConfigurationError(count);
    return {{"available", reason.isEmpty()}, {"reason", reason}, {"motorCount", reason.isEmpty() ? count : 0}};
}

bool Mavlink::validateMotorStart(const QString &mode, int motor, int throttle, int duration, int count)
{
    int confirmedCount = 0;
    auto reason = motorConfigurationError(confirmedCount);
    if (reason.isEmpty() && (motor < 1 || motor > confirmedCount)) reason = QStringLiteral("Motor index is outside the confirmed motor count (%1).").arg(confirmedCount);
    if (reason.isEmpty() && count && count != confirmedCount) reason = QStringLiteral("Requested motor count differs from confirmed frame count (%1).").arg(confirmedCount);
    if (reason.isEmpty() && (throttle < 1 || throttle > 20 || duration < 1 || duration > 30))
        reason = QStringLiteral("Motor test requires throttle 1-20 percent and duration 1-30 seconds.");
    if (!reason.isEmpty()) {
        rejectCommand(QStringLiteral("motorTestStart"), reason);
        emit motorTestCommandRejected(mode, motor, count > 0 ? count : 1, reason);
        return false;
    }
    lastConfirmedMotorCount = confirmedCount;
    return true;
}

void Mavlink::cancelPendingMotorStarts(const QString &reason)
{
    if (pendingMotorStartMode.isEmpty()) return;
    const auto mode = pendingMotorStartMode;
    const int motor = motorTestSequenceNextMotor;
    const int count = motorTestSequenceMotorCount;
    ++motorTestCommandGeneration;
    motorTestSequenceTimer->stop();
    pendingMotorStartMode.clear();
    motorTestSequenceMotorCount = 0;
    // Keep any automatic stop timer and sent-command acknowledgements alive.
    rejectCommand(QStringLiteral("motorTestStart"), reason);
    emit motorTestCommandRejected(mode, motor, qMax(1, count), reason);
}

void Mavlink::revalidateMotorSchedule()
{
    if (pendingMotorStartMode.isEmpty()) return;
    int count = 0;
    QString signature;
    auto reason = motorConfigurationError(count, &signature);
    if (reason.isEmpty() && (signature != scheduledMotorConfiguration
            || motorStartConnectionGeneration != connectionGenerationValue
            || motorStartParameterGeneration != parameterManagerObject->generation())) reason = QStringLiteral("Required motor configuration changed while a start was scheduled.");
    if (!reason.isEmpty()) cancelPendingMotorStarts(reason);
}

bool Mavlink::missionTransferAvailable(const QString &operation)
{
    auto reason = liveConnectionError();
    if (reason.isEmpty() && (missionUploadActive || missionClearBeforeUploadActive || missionDownloadActive
            || transferStateValue == Uploading || transferStateValue == Downloading
            || transferStateValue == ClearingBeforeUpload || transferStateValue == Clearing))
        reason = QStringLiteral("Wait for the current mission transfer to finish.");
    if (reason.isEmpty()) return true;
    setMissionStatus(reason);
    return rejectCommand(operation, reason);
}

void Mavlink::armMotorSequenceTimer(int delay)
{
    QObject::disconnect(motorSequenceConnection);
    const auto connection = connectionGenerationValue;
    const auto command = motorTestCommandGeneration;
    motorSequenceConnection = connect(motorTestSequenceTimer, &QTimer::timeout, this, [this, connection, command] {
        if (connection != connectionGenerationValue || command != motorTestCommandGeneration) return;
        revalidateMotorSchedule();
        if (command == motorTestCommandGeneration) runNextSequentialMotorTest();
    });
    motorTestSequenceTimer->start(delay);
}

void Mavlink::armMotorStopTimer(int delay)
{
    QObject::disconnect(motorStopConnection);
    const auto connection = connectionGenerationValue;
    const auto serial = ++motorStopSerial;
    motorStopConnection = connect(motorTestStopTimer, &QTimer::timeout, this, [this, connection, serial] {
        if (!connected() || connection != connectionGenerationValue || serial != motorStopSerial) return;
        stopAllMotorTests(qBound(1, activeMotorTestStopCount, 12));
    });
    motorTestStopTimer->start(delay);
}

void Mavlink::testMotor(int motor, int throttlePercent, int durationSeconds)
{
    if (!validateMotorStart("single", motor, throttlePercent, durationSeconds)) return;
    stopMotorTestSequence();
    sendMotorTestCommand(QStringLiteral("single"), motor, throttlePercent, durationSeconds);
}

void Mavlink::sendMotorTestCommand(const QString &mode, int motor, int throttlePercent, int durationSeconds, int motorCount)
{
    if (!validateMotorStart(mode, motor, throttlePercent, durationSeconds, motorCount)) {
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
    if (!validateMotorStart(mode, motor, throttlePercent, durationSeconds)) {
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
    if (!connected() || motor < 1) {
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
    if (motorCount < 1) { rejectCommand("motorTestStart", "Motor count must be positive."); emit motorTestCommandRejected("all", 1, motorCount, "Motor count must be positive."); return; }
    if (!validateMotorStart("all", 1, throttlePercent, durationSeconds, motorCount)) return;
    stopMotorTestSequence();

    const int safeDuration = qBound(1, durationSeconds, 30);
    const int safeMotorCount = motorCount;
    const int safeThrottle = qBound(1, throttlePercent, 20);
    const int commandGeneration = motorTestCommandGeneration;
    pendingMotorStartMode = QStringLiteral("all");
    motorStartConnectionGeneration = connectionGenerationValue;
    motorStartParameterGeneration = parameterManagerObject->generation();
    int confirmedCount = 0;
    motorConfigurationError(confirmedCount, &scheduledMotorConfiguration);
    const auto connection = motorStartConnectionGeneration;
    const auto parameters = motorStartParameterGeneration;

    qDebug() << "Run all motor test:"
             << "throttlePercent" << safeThrottle
             << "durationSeconds" << safeDuration
             << "motorCount" << safeMotorCount;

    for (int motor = 1; motor <= safeMotorCount; ++motor) {
        QTimer::singleShot((motor - 1) * simultaneousMotorCommandDelayMs,
                           this,
                           [this, commandGeneration, connection, parameters, motor, safeThrottle, safeDuration, safeMotorCount]() {
            if (!connected() || commandGeneration != motorTestCommandGeneration || connection != connectionGenerationValue
                    || !parameterManagerObject || parameters != parameterManagerObject->generation()) {
                return;
            }

            revalidateMotorSchedule();
            if (commandGeneration != motorTestCommandGeneration) return;
            sendMotorTestCommand(QStringLiteral("all"), motor, safeThrottle, safeDuration);
            if (motor == safeMotorCount) pendingMotorStartMode.clear();
        });
    }

    activeMotorTestStopCount = safeMotorCount;
    armMotorStopTimer((safeDuration * 1000)
                              + ((safeMotorCount - 1) * simultaneousMotorCommandDelayMs)
                              + 250);
}

void Mavlink::runSequentialMotorTest(int throttlePercent, int durationSeconds, int motorCount)
{
    if (motorCount < 1) { rejectCommand("motorTestStart", "Motor count must be positive."); emit motorTestCommandRejected("sequential", 1, motorCount, "Motor count must be positive."); return; }
    if (!validateMotorStart("sequential", 1, throttlePercent, durationSeconds, motorCount)) return;
    stopMotorTestSequence();

    pendingMotorStartMode = QStringLiteral("sequential");
    motorStartConnectionGeneration = connectionGenerationValue;
    motorStartParameterGeneration = parameterManagerObject->generation();
    int confirmedCount = 0;
    motorConfigurationError(confirmedCount, &scheduledMotorConfiguration);
    motorTestSequenceNextMotor = 1;
    motorTestSequenceMotorCount = motorCount;
    motorTestSequenceThrottle = qBound(1, throttlePercent, 20);
    motorTestSequenceDuration = qBound(1, durationSeconds, 30);

    runNextSequentialMotorTest();
}

void Mavlink::runNextSequentialMotorTest()
{
    if (!connected() || motorStartConnectionGeneration != connectionGenerationValue || !parameterManagerObject
            || motorStartParameterGeneration != parameterManagerObject->generation()
            || motorTestSequenceNextMotor > motorTestSequenceMotorCount) {
        stopMotorTestSequence();
        return;
    }

    if (!validateMotorStart("sequential", motorTestSequenceNextMotor, motorTestSequenceThrottle, motorTestSequenceDuration)) {
        cancelPendingMotorStarts("Motor prerequisites changed."); return;
    }
    qDebug() << "Run sequential motor test:"
             << "motor" << motorTestSequenceNextMotor
             << "of" << motorTestSequenceMotorCount
             << "throttlePercent" << motorTestSequenceThrottle
             << "durationSeconds" << motorTestSequenceDuration;

    sendMotorTestCommand(QStringLiteral("sequential"), motorTestSequenceNextMotor,
                         motorTestSequenceThrottle, motorTestSequenceDuration);
    ++motorTestSequenceNextMotor;
    if (motorTestSequenceNextMotor > motorTestSequenceMotorCount) pendingMotorStartMode.clear();

    if (motorTestSequenceNextMotor <= motorTestSequenceMotorCount) {
        armMotorSequenceTimer((motorTestSequenceDuration * 1000) + 250);
    }
}

void Mavlink::stopMotorTestSequence()
{
    ++motorTestCommandGeneration;
    ++motorStopSerial;
    pendingMotorStartMode.clear();

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
    const int safeMotorCount = qBound(1, qMax(motorCount, lastConfirmedMotorCount), 12);
    stopMotorTestSequence();
    if (!connected()) { rejectCommand("stopAllMotorTests", "Vehicle is not connected."); return; }

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

    if (!connected()) {
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
    if (!connected()) {
        return rejectCommand("manualControl", "Vehicle is not connected.");
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
    if (!connected()) {
        return rejectCommand("rcChannelsOverride", "Vehicle is not connected.");
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
    if (connected()) {
        requestDataStreams();
    }
}

bool Mavlink::uploadMissionFile(const QUrl &fileUrl)
{
    if (connected() && !missionTransferAvailable("missionUpload")) { emit missionUploadFinished(false, missionStatusText); return false; }
    canonicalUploadPending = false;
    transferUsesCanonicalSnapshot = false;
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QVector<MissionItem> parsedItems;
    QVariantList displayWaypoints;
    QVariantMap homeLocation;
    QString error;

    if (!parseMissionFile(path, parsedItems, displayWaypoints, homeLocation, error)) {
        setUploadedHomeLocation(QVariantMap());
        rejectCommand("missionUpload", error);
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

    if (!connected()) {
        rejectCommand("missionUpload", "Vehicle is not connected; mission retained locally.");
        setMissionStatus(QStringLiteral("Mission loaded on map. Connect to a drone before uploading."));
        emit missionUploadFinished(false, missionStatusText);
        return false;
    }

    missionClearBeforeUploadActive = true;
    missionUploadActive = false;
    startMissionTransfer(ClearingBeforeUpload, pendingMissionItems.size());
    setMissionStatus(QStringLiteral("Clearing existing mission before upload."));
    sendMissionClearAll();

    return true;
}

bool Mavlink::resetHomeToUploadedLocation()
{
    if (loadedUploadedHomeLocation.isEmpty()) {
        setMissionStatus(QStringLiteral("No uploaded home location is available."));
        return false;
    }

    if (!connected()) {
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
    if (!missionTransferAvailable("missionUpload")) { emit missionUploadFinished(false, missionStatusText); return false; }
    transferUsesCanonicalSnapshot = canonicalUploadPending;
    canonicalUploadPending = false;
    if (pendingMissionItems.isEmpty()) {
        rejectCommand("missionUpload", "No mission plan loaded to write.");
        setMissionStatus(QStringLiteral("No mission plan loaded to write."));
        emit missionUploadFinished(false, missionStatusText);
        return false;
    }

    if (!connected()) {
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
    startMissionTransfer(ClearingBeforeUpload, pendingMissionItems.size());
    setMissionUploaded(false);
    setMissionStatus(QStringLiteral("Clearing existing mission before write."));
    sendMissionClearAll();

    return true;
}

bool Mavlink::uploadMissionPlan(QObject *object)
{
    if (!missionTransferAvailable("missionUpload")) { emit missionUploadFinished(false, missionStatusText); return false; }
    auto *model = qobject_cast<MissionPlanModel *>(object);
    if (!model || model->rowCount() == 0) {
        rejectCommand("missionUpload", "No canonical mission plan is available to upload.");
        setMissionStatus(QStringLiteral("No canonical mission plan is available to upload."));
        return false;
    }
    QVector<MavlinkMissionItem> snapshot;
    const auto conversion = MissionPlanConverter::exportVehicleMission(*model, &snapshot);
    if (!conversion.success) { setMissionStatus(conversion.error); return rejectCommand("missionUpload", conversion.error); }
    missionPlanModel = model;
    transferSnapshotRevision = model->revision();
    pendingMissionItems = std::move(snapshot);
    canonicalUploadPending = true;
    return writeLoadedMissionToVehicle();
}

bool Mavlink::replaceDownloadedMission(QObject *object)
{
    auto *model = qobject_cast<MissionPlanModel *>(object);
    if (!model || pendingMissionItems.isEmpty()) return false;
    const auto result = MissionPlanConverter::importVehicleMission(pendingMissionItems, model, true);
    if (!result.success) setMissionStatus(result.error);
    return result.success;
}

void Mavlink::downloadMissionFromVehicle(bool replaceDirtyPlan)
{
    if (!missionTransferAvailable("missionDownload")) return;

    missionDownloadActive = true;
    replaceDirtyPlanOnDownloadCompletion = replaceDirtyPlan;
    missionSaveRequestedAfterDownload = false;
    missionDownloadExpectedCount = 0;
    downloadingMissionItems.clear();
    downloadedMissionItemSeqs.clear();
    startMissionTransfer(Downloading);
    setMissionStatus(QStringLiteral("Requesting mission from vehicle."));
    requestMissionList();
}

void Mavlink::saveMissionFromVehicle()
{
    if (!missionDownloadActive && !missionTransferAvailable("missionSave")) return;
    if (!connected()) {
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
    startMissionTransfer(Downloading);
    setMissionStatus(QStringLiteral("Downloading vehicle mission to save."));
    requestMissionList();
}

void Mavlink::clearMission()
{
    if (!missionTransferAvailable("missionClear")) return;
    startMissionTransfer(Clearing);
    sendMissionClearAll();
    setMissionStatus(QStringLiteral("Clearing vehicle mission."));
}

void Mavlink::requestParameters()
{
    const auto error = liveConnectionError();
    if (!error.isEmpty()) { rejectCommand("parameterRefresh", error); return; }
    parameterManagerObject->refresh();
}

bool Mavlink::requestParameterRead(const QString &name)
{
    auto error = liveConnectionError();
    const auto bytes = name.toLatin1();
    if (error.isEmpty() && (name.isEmpty() || bytes.size() > 16 || QString::fromLatin1(bytes) != name))
        error = "Parameter identifier must contain 1-16 printable ASCII bytes.";
    for (const auto byte : bytes) if (byte < 33 || byte > 126) error = "Invalid parameter identifier.";
    if (!error.isEmpty()) return rejectCommand("parameterRead", error);
    char id[16]{};
    std::memcpy(id, bytes.constData(), size_t(bytes.size()));
    mavlink_message_t message{};
    mavlink_msg_param_request_read_pack(255, 0, &message, targetSystemId, targetComponentId, id, -1);
    if (!sendMavlinkMessage(message)) return rejectCommand("parameterRead", "Transport rejected parameter read.");
    return true;
}

QVariantMap Mavlink::parameterValue(const QString &name) const
{
    return parameterManagerObject->parameterValue(name);
}

QVariantMap Mavlink::parameters() const
{
    return parameterManagerObject->parameters();
}

QVariantMap Mavlink::requestParameterWrite(const QString &name, double value, int type)
{
    const auto result = parameterManagerObject->requestWrite(name, value, type);
    if (!result.value("accepted").toBool()) rejectCommand("parameterWrite", result.value("reason").toString());
    return result;
}

bool Mavlink::setParameter(const QString &name, double value, int type)
{
    return requestParameterWrite(name, value, type).value("accepted").toBool();
}

bool Mavlink::writeRcCalibration(const QVariantList &calibration)
{
    return requestRcCalibrationWrite(calibration).value("accepted").toBool();
}

QVariantMap Mavlink::requestRcCalibrationWrite(const QVariantList &calibration)
{
    QVariantList requests;
    QString error = parameterPrerequisiteError();
    if (!error.isEmpty()) {
        rejectCommand("rcCalibration", error);
        return {{"id", qulonglong(0)}, {"accepted", false}, {"reason", error}};
    }
    QSet<int> channels;
    for (const auto &entryValue : calibration) {
        const auto entry = entryValue.toMap();
        bool channelOk = false, minOk = false, maxOk = false, trimOk = false;
        const auto channelNumber = entry.value("channel").toDouble(&channelOk);
        const auto minimum = entry.value("minimum").toDouble(&minOk);
        const auto maximum = entry.value("maximum").toDouble(&maxOk);
        const auto trim = entry.value("trim").toDouble(&trimOk);
        if (!channelOk || !std::isfinite(channelNumber) || std::trunc(channelNumber) != channelNumber
                || channelNumber < 1 || channelNumber > 16) { error = "Invalid RC channel (supported range 1-16)."; break; }
        const int channel = int(channelNumber);
        if (channels.contains(channel)) { error = "Duplicate RC channel."; break; }
        channels.insert(channel);
        const auto prefix = QStringLiteral("RC%1_").arg(channel);
        // The existing workflow offers 16 channels. Only channels present in the
        // complete firmware table participate; partial channel metadata is an error.
        const QStringList suffixes{"MIN", "MAX", "TRIM"};
        int supported = 0;
        for (const auto &suffix : suffixes) supported += !parameterValue(prefix + suffix).isEmpty();
        if (!supported) continue;
        if (supported != 3) { error = prefix + "has incomplete calibration metadata."; break; }
        if (!minOk || !maxOk || !trimOk || !std::isfinite(minimum) || !std::isfinite(maximum) || !std::isfinite(trim)
                || minimum < 800 || maximum > 2200 || minimum >= maximum || trim < minimum || trim > maximum
                || std::trunc(minimum) != minimum || std::trunc(maximum) != maximum || std::trunc(trim) != trim) {
            error = prefix + "requires integral 800-2200 PWM values with MIN <= TRIM <= MAX and MIN < MAX."; break;
        }
        const double values[] = {minimum, maximum, trim};
        for (int i = 0; i < suffixes.size(); ++i) {
            const auto name = prefix + suffixes[i];
            requests.append(QVariantMap{{"name", name}, {"value", values[i]}, {"type", parameterValue(name).value("type")}});
        }
        const auto reversedName = prefix + "REVERSED";
        if (channel <= 4 && !parameterValue(reversedName).isEmpty())
            requests.append(QVariantMap{{"name", reversedName}, {"value", entry.value("reversed").toBool() ? 1 : 0},
                                        {"type", parameterValue(reversedName).value("type")}});
    }
    if (error.isEmpty() && requests.isEmpty()) error = "No supported RC calibration parameters were requested.";
    for (const auto &request : requests) {
        const auto row = request.toMap();
        float wire = 0;
        if (error.isEmpty()) error = parameterPrerequisiteError({row.value("name").toString()});
        if (error.isEmpty() && !parameterManagerObject->prepareWrite(row.value("name").toString(), row.value("value").toDouble(), row.value("type").toUInt(), wire))
            error = row.value("name").toString() + ": invalid calibration value or metadata.";
    }
    if (!error.isEmpty()) {
        rejectCommand("rcCalibration", error);
        return {{"id", qulonglong(0)}, {"accepted", false}, {"reason", error}};
    }
    const auto accepted = parameterManagerObject->requestWriteBatch(requests);
    rcCalibrationBatches.insert(accepted.value("id").toULongLong());
    return accepted;
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
    } else if (connected()) {
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

    if (wasActive && telemetryLoggingEnabledState && connected()) {
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

    if (wasActive && flightControllerLoggingEnabledState && connected()) {
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
    if (!link || !acceptTransportMessages) return;
    processConnectionTimeout(connectionGenerationValue);
    if (!link || !acceptTransportMessages) return;
    const auto transport = transportGeneration;
    const QByteArray data = link->readAll();
    mavlink_message_t msg;

    for (int i = 0; i < data.size(); i++) {
        if (transport != transportGeneration || !acceptTransportMessages) return;
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
            ++receivedPackets;
            lastPacketAt = connectionClock();
            packetTimes.append(lastPacketAt);
            while (!packetTimes.isEmpty() && (lastPacketAt - packetTimes.first() >= 1000 || packetTimes.size() > 10000)) packetTimes.removeFirst();
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

            handleHeartbeat(msg);
            if (transport != transportGeneration || !acceptTransportMessages) return;
            const QString linkIdentity = link
                    ? QStringLiteral("%1@%2")
                              .arg(QString::fromLatin1(link->metaObject()->className()))
                              .arg(reinterpret_cast<quintptr>(link), 0, 16)
                    : QString();
            if (!parameterManagerObject) return;
            const auto lifecycleGeneration = connectionGenerationValue;
            const auto parameterGeneration = parameterManagerObject->generation();
            emit frameObserved(MavlinkFrame(QDateTime::currentDateTimeUtc(), linkIdentity, msg, rawFrame));
            emit messageReceived(msg);
            if (transport != transportGeneration || lifecycleGeneration != connectionGenerationValue || !acceptTransportMessages) return;
            updateMavlinkQuality(msg);
            handleLinkPing(msg);
            handleParameterValue(msg, parameterGeneration);
            handleRcChannels(msg);
            handleVpsMessage(msg);
            handleTerrainRequest(msg);
            handleMissionMessage(msg);
        }

    }
}

bool Mavlink::setConnectionConfiguration(const ConnectionConfig &config)
{
    if (connectionStateValue != Disconnected || acceptTransportMessages || config.heartbeatTimeoutMs <= 0
            || config.initialHeartbeatTimeoutMs <= 0 || config.parameterStartupDelayMs < 0) return false;
    connectionConfig = config;
    emit connectionLifecycleChanged();
    return true;
}

void Mavlink::stopLifecycleTimers()
{
    heartbeatTimer.stop();
    parameterStartupTimer.stop();
    QObject::disconnect(heartbeatTimerConnection);
    QObject::disconnect(parameterStartupConnection);
    ++heartbeatTimerSerial;
}

void Mavlink::beginConnection()
{
    ++connectionGenerationValue;
    acceptTransportMessages = true;
    connectionStateValue = AwaitingHeartbeat;
    lastHeartbeatMs = -1;
    lastHeartbeatUtcMs = 0;
    awaitingHeartbeatDeadline = connectionClock() + connectionConfig.initialHeartbeatTimeoutMs;
    armConnectionDeadline();
    emit connectionLifecycleChanged();
}

void Mavlink::armConnectionDeadline()
{
    heartbeatTimer.stop();
    QObject::disconnect(heartbeatTimerConnection);
    const auto serial = ++heartbeatTimerSerial;
    const auto generation = connectionGenerationValue;
    if (connectionStateValue == Disconnected) return;
    const auto deadline = connected() ? lastHeartbeatMs + connectionConfig.heartbeatTimeoutMs : awaitingHeartbeatDeadline;
    heartbeatTimerConnection = connect(&heartbeatTimer, &QTimer::timeout, this, [this, generation, serial] {
        if (serial == heartbeatTimerSerial) processConnectionTimeout(generation);
    });
    heartbeatTimer.start(int(std::clamp<qint64>(deadline - connectionClock(), 0, std::numeric_limits<int>::max())));
}

void Mavlink::processConnectionTimeout(quint64 generation)
{
    if (generation != connectionGenerationValue || connectionStateValue == Disconnected) return;
    const auto deadline = connected() ? lastHeartbeatMs + connectionConfig.heartbeatTimeoutMs : awaitingHeartbeatDeadline;
    if (connectionClock() < deadline) { armConnectionDeadline(); return; }
    if (connected()) {
        ++heartbeatExpiries;
        endConnection(QStringLiteral("Selected autopilot heartbeat timed out"), true);
    } else {
        auto *disconnectingLink = link;
        const auto transport = transportGeneration;
        endConnection(QStringLiteral("Initial autopilot heartbeat timed out"), false);
        if (link && link == disconnectingLink && transport == transportGeneration) link->disconnectLink();
    }
}

void Mavlink::scheduleInitialParameters()
{
    parameterStartupTimer.stop();
    QObject::disconnect(parameterStartupConnection);
    const auto generation = connectionGenerationValue;
    const auto parameterGeneration = parameterManagerObject->generation();
    parameterStartupConnection = connect(&parameterStartupTimer, &QTimer::timeout, this, [this, generation, parameterGeneration] {
        if (connected() && generation == connectionGenerationValue && parameterManagerObject
                && parameterGeneration == parameterManagerObject->generation()
                && parameterManagerObject->syncState() == ParameterManager::Idle) requestParameters();
    });
    parameterStartupTimer.start(connectionConfig.parameterStartupDelayMs);
}

void Mavlink::handleHeartbeat(const mavlink_message_t &msg)
{
    if (shuttingDown || transitioningConnection || !acceptTransportMessages || !link || !link->isConnected() || !parameterManagerObject
            || msg.msgid != MAVLINK_MSG_ID_HEARTBEAT || msg.len < MAVLINK_MSG_ID_HEARTBEAT_MIN_LEN
            || msg.sysid == 0 || msg.compid == 0) return;
    mavlink_heartbeat_t heartbeat = {};
    mavlink_msg_heartbeat_decode(&msg, &heartbeat);
    if (heartbeat.type == MAV_TYPE_GCS || heartbeat.autopilot == MAV_AUTOPILOT_INVALID
            || (selectedVehicleValid && (msg.sysid != targetSystemId || msg.compid != targetComponentId))) return;
    if (connectionStateValue == Disconnected) ++connectionGenerationValue;
    const auto generation = connectionGenerationValue;
    const bool establishing = !connected();
    if (!selectedVehicleValid) {
        targetSystemId = msg.sysid;
        targetComponentId = msg.compid;
        selectedAutopilot = heartbeat.autopilot;
        selectedVehicleValid = true;
        emit selectedVehicleChanged();
        if (generation != connectionGenerationValue || !acceptTransportMessages) return;
    }
    selectedHeartbeatArmed = (heartbeat.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
    revalidateMotorSchedule();
    lastHeartbeatMs = connectionClock();
    lastHeartbeatUtcMs = QDateTime::currentMSecsSinceEpoch();
    if (establishing) {
        connectionStateValue = Connected;
        const auto encoding = selectedAutopilot == MAV_AUTOPILOT_ARDUPILOTMEGA ? ParameterManager::NumericCast
                : selectedAutopilot == MAV_AUTOPILOT_PX4 ? ParameterManager::Bytewise : ParameterManager::UnknownEncoding;
        parameterManagerObject->connectSource(targetSystemId, targetComponentId, encoding, generation);
        if (generation != connectionGenerationValue || !connected()) return;
        scheduleInitialParameters();
        missionDownloadRequestedOnConnect = true;
        QTimer::singleShot(650, this, [this, generation] {
            if (connected() && generation == connectionGenerationValue && !missionUploadActive && !missionClearBeforeUploadActive && !missionDownloadActive
                    && transferStateValue != Clearing)
                downloadMissionFromVehicle();
        });
        linkPingTimer->start();
        startTelemetryLog();
        requestDataStreams();
        sendLinkPing();
    }
    armConnectionDeadline();
    emit heartbeatObserved();
    if (generation != connectionGenerationValue || !connected()) return;
    if (establishing) {
        emit connectionLifecycleChanged();
        if (generation == connectionGenerationValue && connected()) emit connectionStatusChanged(true);
    }
}

void Mavlink::endConnection(const QString &reason, bool permitHeartbeatReconnect)
{
    const bool wasConnected = connected();
    const bool active = connectionStateValue != Disconnected;
    acceptTransportMessages = permitHeartbeatReconnect && link && link->isConnected();
    stopLifecycleTimers();
    if (!active) {
        if (!acceptTransportMessages && selectedVehicleValid) {
            selectedVehicleValid = false;
            emit selectedVehicleChanged();
        }
        return;
    }
    connectionStateValue = Disconnected;
    selectedHeartbeatArmed = false;
    lastConfirmedMotorCount = 0;
    transitioningConnection = true;
    const auto generation = ++connectionGenerationValue;
    disconnectReason = reason;
    // Revoke readiness and cancel parameter work before notifying connection consumers.
    if (parameterManagerObject) parameterManagerObject->disconnectSource();
    if (generation != connectionGenerationValue) { transitioningConnection = false; return; }
    cancelMissionTransfer();
    stopTelemetryLog();
    stopFlightControllerLog();
    setMissionUploaded(false);
    missionUploadActive = missionClearBeforeUploadActive = missionDownloadActive = false;
    missionDownloadRequestedOnConnect = missionSaveRequestedAfterDownload = false;
    missionDownloadExpectedCount = 0;
    downloadingMissionItems.clear();
    downloadedMissionItemSeqs.clear();
    clearPendingTerrainRequests();
    stopMotorTestSequence(); // Cancel local timers, without sending new motor commands.
    clearVpsStatus();
    pendingServoAcks.clear();
    linkPingTimer->stop();
    resetLinkQuality();
    inboundFrameBytes.clear();
    if (!acceptTransportMessages && selectedVehicleValid) {
        selectedVehicleValid = false;
        emit selectedVehicleChanged();
    }
    transitioningConnection = false;
    if (generation != connectionGenerationValue) return;
    qInfo() << "MAVLink disconnected:" << reason << "generation" << generation;
    if (wasConnected) emit connectionStatusChanged(false);
    if (generation == connectionGenerationValue) emit connectionLifecycleChanged();
}
bool Mavlink::setQualityConfiguration(const QualityConfig &config)
{
    if (connected() || config.tickMs < 50 || config.sequenceWindowMs <= 0 || config.sequenceFreshMs <= 0
            || config.radioFreshMs <= 0 || config.pingFreshMs <= 0 || config.pingResponseMs <= 0
            || config.minimumSequenceExpected < 1 || config.maximumForwardDelta < 2 || config.maximumForwardDelta > 127
            || config.sustainedCategoryMs < 0 || config.minimumCategoryDwellMs < 0
            || !std::isfinite(config.timeConstantSeconds) || config.timeConstantSeconds <= 0
            || !(0 <= config.poorEntry && config.poorEntry < config.poorExit && config.poorExit < config.goodExit
                 && config.goodExit < config.goodEntry && config.goodEntry <= 100)) return false;
    qualityConfig = config;
    qualityTimer.setInterval(config.tickMs);
    resetLinkQuality();
    return true;
}

int Mavlink::rawRadioRssi() const
{
    const auto now = connectionClock();
    return connected() && lastRssiAt >= 0 && now >= lastRssiAt && now - lastRssiAt < qualityConfig.radioFreshMs ? rawRssi : -1;
}

void Mavlink::sendLinkPing()
{
    if (!connected()) return;
    const auto now = connectionClock();
    const auto stamp = quint64(qMax<qint64>(1, now * 1000));
    if (pendingQualityPings.size() >= 8) {
        auto oldest = pendingQualityPings.begin();
        for (auto it = pendingQualityPings.begin(); it != pendingQualityPings.end(); ++it)
            if (it->at < oldest->at) oldest = it;
        pendingQualityPings.erase(oldest);
    }
    const auto seq = nextPingSequence++;
    pendingQualityPings.insert(seq, {now, stamp});
    mavlink_message_t message{};
    mavlink_msg_ping_pack(255, MAV_COMP_ID_MISSIONPLANNER, &message, stamp, seq, 0, 0);
    if (!sendMavlinkMessage(message)) pendingQualityPings.remove(seq);
}

void Mavlink::handleLinkPing(const mavlink_message_t &message)
{
    if (!connected() || message.msgid != MAVLINK_MSG_ID_PING || message.sysid != targetSystemId
            || message.compid != targetComponentId) return;
    mavlink_ping_t ping{};
    mavlink_msg_ping_decode(&message, &ping);
    if (ping.target_system != 255 || ping.target_component != MAV_COMP_ID_MISSIONPLANNER) return;
    const auto request = pendingQualityPings.constFind(ping.seq);
    if (request == pendingQualityPings.cend() || request->stamp != ping.time_usec) return;
    const auto now = connectionClock(), elapsed = now - request->at;
    pendingQualityPings.remove(ping.seq);
    if (elapsed < 0 || elapsed > qualityConfig.pingResponseMs) return;
    lastValidPingAt = now;
    latencyWindow.append({now, int(elapsed)});
    if (latencyWindow.size() > 5) latencyWindow.removeFirst();
}

void Mavlink::updateMavlinkQuality(const mavlink_message_t &message)
{
    if (!connected() || qualityGeneration != connectionGenerationValue || message.sysid != targetSystemId) return;
    const auto now = connectionClock();
    if (message.msgid == MAVLINK_MSG_ID_RADIO_STATUS
            && (message.compid == targetComponentId || message.compid == MAV_COMP_ID_TELEMETRY_RADIO)) {
        if (radioSourceComponent != message.compid) {
            radioSourceComponent = message.compid; radioBaseline = false; radioWindow.clear();
        }
        mavlink_radio_status_t radio{};
        mavlink_msg_radio_status_decode(&message, &radio);
        diagnosticRadio = radio;
        const bool continuous = radioBaseline && now >= lastRadioAt && now - lastRadioAt < qualityConfig.radioFreshMs;
        if (continuous) {
            const auto delta = [](quint16 oldValue, quint16 newValue) {
                if (newValue >= oldValue) return int(newValue - oldValue);
                // Only an edge-to-edge transition is a defensible 16-bit wrap.
                if (oldValue >= 0xf000 && newValue <= 0x0fff) return int(65536u + newValue - oldValue);
                return -1;
            };
            const int errors = delta(previousRadioErrors, radio.rxerrors);
            const int corrections = delta(previousRadioCorrections, radio.fixed);
            if (errors < 0 || corrections < 0) {
                ++radioResets; radioWindow.clear(); radioCounterStatus = "reset/rebaseline";
            } else {
                radioWindow.append({now, errors, corrections});
                if (radioWindow.size() > 256) radioWindow.removeFirst();
                radioCounterStatus = errors || corrections ? "recent counter deltas (not packet loss)" : "no new error/correction events";
            }
        } else { radioWindow.clear(); radioCounterStatus = "baseline only"; }
        radioBaseline = true;
        previousRadioErrors = radio.rxerrors; previousRadioCorrections = radio.fixed;
        lastRadioAt = now;
        rawRssi = radio.rssi == UINT8_MAX ? -1 : radio.rssi;
        lastRssiAt = rawRssi < 0 ? -1 : now;
    }
    // Track every selected-autopilot frame, including PING, before message-type
    // filtering. Other components have independent sequence spaces and are ignored.
    if (message.compid != targetComponentId) return;
    if (!sequenceBaseline || now < lastSequenceAt || now - lastSequenceAt >= qualityConfig.sequenceFreshMs) {
        sequenceWindow.clear(); sequenceBaseline = true; qualityLastSequence = message.seq; lastSequenceAt = now; return;
    }
    const int delta = quint8(message.seq - qualityLastSequence);
    if (!delta) { ++sequenceDuplicates; ++transportDuplicates; return; }
    if (delta >= 128) { ++sequenceReordered; ++transportReordered; return; }
    qualityLastSequence = message.seq; lastSequenceAt = now;
    if (delta > qualityConfig.maximumForwardDelta) { ++sequenceResyncs; sequenceWindow.clear(); return; }
    detectedSequenceGaps += quint64(delta - 1);
    sequenceWindow.append({now, delta});
    if (sequenceWindow.size() > 4096) sequenceWindow.removeFirst();
}

void Mavlink::processLinkQuality()
{
    if (!connected() || qualityGeneration != connectionGenerationValue) return;
    const auto now = connectionClock();
    const auto fresh = [now](qint64 stamp, int limit) { return stamp >= 0 && now >= stamp && now - stamp < limit; };
    while (!sequenceWindow.isEmpty() && !fresh(sequenceWindow.first().at, qualityConfig.sequenceWindowMs)) sequenceWindow.removeFirst();
    while (!latencyWindow.isEmpty() && !fresh(latencyWindow.first().at, qualityConfig.pingFreshMs)) latencyWindow.removeFirst();
    while (!radioWindow.isEmpty() && !fresh(radioWindow.first().at, qualityConfig.radioFreshMs)) radioWindow.removeFirst();
    for (auto it = pendingQualityPings.begin(); it != pendingQualityPings.end();) {
        if (!fresh(it->at, qualityConfig.pingResponseMs + 1)) it = pendingQualityPings.erase(it); else ++it;
    }
    int expected = 0;
    for (const auto &sample : sequenceWindow) expected += sample.expected;
    sequenceEstimate = fresh(lastSequenceAt, qualityConfig.sequenceFreshMs) && expected >= qualityConfig.minimumSequenceExpected
            ? std::clamp(100.0 * sequenceWindow.size() / expected, 0.0, 100.0) : -1;
    QVector<int> latency;
    for (const auto &sample : latencyWindow) latency.append(sample.value);
    std::sort(latency.begin(), latency.end());
    linkLatencyMsValue = latency.isEmpty() ? -1 : latency.size() % 2 ? latency[latency.size()/2]
        : (latency[latency.size()/2 - 1] + latency[latency.size()/2]) / 2;
    double measured = sequenceEstimate;
    if (measured >= 0 && linkLatencyMsValue >= 0) {
        const double latencyScore = std::clamp(100.0 - (linkLatencyMsValue - 50) * 100.0 / 450.0, 0.0, 100.0);
        // Continuity remains primary; sustained high RTT must still be able
        // to downgrade a perfectly continuous stream from GOOD to FAIR.
        measured = 0.70 * measured + 0.30 * latencyScore;
    }
    mavlinkQualityValue = sequenceEstimate;
    if (measured < 0) {
        filteredQuality = -1; qualityScore = -1; filterAt = -1;
        qualityCategory.clear(); candidateCategory.clear(); candidateAt = categoryAt = -1;
    } else {
        if (filteredQuality < 0 || filterAt < 0 || now < filterAt || now - filterAt > qMax(qualityConfig.sequenceWindowMs, qualityConfig.pingFreshMs)) {
            filteredQuality = measured;
            qualityCategory.clear(); candidateCategory.clear(); candidateAt = categoryAt = -1;
        } else {
            const double seconds = double(now - filterAt) / 1000.;
            filteredQuality += (1.0 - std::exp(-seconds / qualityConfig.timeConstantSeconds)) * (measured - filteredQuality);
        }
        filterAt = now;
        qualityScore = qRound(std::clamp(filteredQuality, 0.0, 100.0));
        QString desired;
        if (qualityCategory == "GOOD") desired = filteredQuality < qualityConfig.poorEntry ? "POOR" : filteredQuality < qualityConfig.goodExit ? "FAIR" : "GOOD";
        else if (qualityCategory == "POOR") desired = filteredQuality >= qualityConfig.goodEntry ? "GOOD" : filteredQuality >= qualityConfig.poorExit ? "FAIR" : "POOR";
        else desired = filteredQuality >= qualityConfig.goodEntry ? "GOOD" : filteredQuality < qualityConfig.poorEntry ? "POOR" : "FAIR";
        if (desired == qualityCategory) { candidateCategory.clear(); candidateAt = -1; }
        else {
            if (candidateCategory != desired || candidateAt < 0 || now < candidateAt) { candidateCategory = desired; candidateAt = now; }
            if (now - candidateAt >= qualityConfig.sustainedCategoryMs
                    && (categoryAt < 0 || now - categoryAt >= qualityConfig.minimumCategoryDwellMs)) {
                qualityCategory = desired; categoryAt = now; candidateCategory.clear(); candidateAt = -1;
            }
        }
    }
    const auto diagnostics = linkQualityDiagnostics();
    if (diagnostics != publishedQualityDiagnostics) { publishedQualityDiagnostics = diagnostics; emit linkQualityChanged(); }
}

QVariantMap Mavlink::linkQualityDiagnostics() const
{
    const auto now = connectionClock();
    const auto fresh = [now](qint64 at, int limit) { return at >= 0 && now >= at && now - at < limit; };
    QStringList available, stale;
    if (sequenceEstimate >= 0 && fresh(lastSequenceAt, qualityConfig.sequenceFreshMs)) available << "sequence continuity estimate";
    else if (lastSequenceAt >= 0) stale << "sequence (stale or insufficient evidence)";
    if (linkLatencyMsValue >= 0 && fresh(lastValidPingAt, qualityConfig.pingFreshMs)) available << "PING median RTT";
    else if (lastValidPingAt >= 0) stale << "PING";
    if (rawRadioRssi() >= 0) available << "raw RSSI (diagnostic only)";
    else if (lastRssiAt >= 0) stale << "RSSI";
    const bool radioFresh = fresh(lastRadioAt, qualityConfig.radioFreshMs);
    if (radioFresh && !radioWindow.isEmpty()) available << "radio counter deltas (diagnostic only)";
    if (!radioFresh && lastRadioAt >= 0) stale << "radio counters";
    int errors = 0, corrections = 0, expected = 0;
    for (const auto &sample : radioWindow) if (fresh(sample.at, qualityConfig.radioFreshMs)) { errors += sample.errors; corrections += sample.corrections; }
    for (const auto &sample : sequenceWindow) if (fresh(sample.at, qualityConfig.sequenceWindowMs)) expected += sample.expected;
    return {{"connected", connected()}, {"connectionState", int(connectionStateValue)},
        {"connectionGeneration", QVariant::fromValue(connectionGenerationValue)}, {"systemId", targetSystemId},
        {"componentId", targetComponentId}, {"radioComponentId", radioSourceComponent}, {"qualityEstimate", qualityScore},
        {"category", qualityCategory}, {"availableMetrics", available}, {"staleMetrics", stale},
        {"lastRadioMonotonicMs", lastRadioAt}, {"lastPingMonotonicMs", lastValidPingAt}, {"lastSequenceMonotonicMs", lastSequenceAt},
        {"latencyMs", linkLatencyMsValue}, {"rawRssi", rawRadioRssi()}, {"rssiInterpretation", "uncalibrated device-dependent raw RSSI; not dBm or percent"},
        {"radioCountersFresh", radioFresh}, {"radioCounterStatus", radioFresh ? radioCounterStatus : "unavailable/stale"},
        {"radioErrorsDelta", errors}, {"radioCorrectionsDelta", corrections}, {"radioCounterResets", radioResets},
        {"sequenceExpected", expected}, {"sequenceReceived", sequenceWindow.size()}, {"sequenceScore", sequenceEstimate},
        {"duplicates", sequenceDuplicates}, {"reordered", sequenceReordered}, {"resynchronizations", sequenceResyncs},
        {"confidence", qualityScore < 0 ? "unavailable" : linkLatencyMsValue < 0 ? "limited: sequence only" : "moderate: sequence plus latency"},
        {"unknownReason", !connected() ? "No live heartbeat connection" : qualityScore < 0 ? "Insufficient fresh sequence evidence" : qualityCategory.isEmpty() ? "Assessing sustained quality" : ""}};
}

QVariantMap Mavlink::transportDiagnostics() const
{
    if (!link) return {};
    const auto now = connectionClock();
    const bool radioFresh = connected() && lastRadioAt >= 0 && now >= lastRadioAt && now - lastRadioAt < qualityConfig.radioFreshMs;
    const bool pingFresh = connected() && lastValidPingAt >= 0 && now >= lastValidPingAt && now - lastValidPingAt < qualityConfig.pingFreshMs;
    int recent = 0;
    for (auto at : packetTimes) if (now >= at && now - at < 1000) ++recent;
    const auto raw = [radioFresh](int value) { return radioFresh && value != UINT8_MAX ? value : -1; };
    return {{"endpoint", diagnosticEndpointValue}, {"connectionGeneration", QVariant::fromValue(connectionGenerationValue)},
        {"received", QVariant::fromValue(receivedPackets)}, {"transmitted", QVariant::fromValue(transmittedPackets)},
        {"sequenceGaps", QVariant::fromValue(detectedSequenceGaps)}, {"duplicates", QVariant::fromValue(transportDuplicates)}, {"reordered", QVariant::fromValue(transportReordered)},
        {"rate", packetRateStartedAt >= 0 && now - packetRateStartedAt >= 1000 && recent < 10000 ? recent : -1},
        {"lastPacketAgeMs", lastPacketAt >= 0 && now >= lastPacketAt ? now - lastPacketAt : -1},
        {"radioState", lastRadioAt < 0 ? "Unavailable" : radioFresh ? "Available" : "Stale"},
        {"radioAgeMs", lastRadioAt >= 0 && now >= lastRadioAt ? now - lastRadioAt : -1},
        {"radioSystem", targetSystemId}, {"radioComponent", radioSourceComponent},
        {"rssi", raw(diagnosticRadio.rssi)}, {"remoteRssi", raw(diagnosticRadio.remrssi)},
        {"noise", raw(diagnosticRadio.noise)}, {"remoteNoise", raw(diagnosticRadio.remnoise)},
        {"rxerrors", radioFresh ? int(diagnosticRadio.rxerrors) : -1}, {"fixed", radioFresh ? int(diagnosticRadio.fixed) : -1},
        {"txbuf", radioFresh && diagnosticRadio.txbuf <= 100 ? int(diagnosticRadio.txbuf) : -1},
        {"systemId", targetSystemId}, {"componentId", targetComponentId},
        {"latencyState", lastValidPingAt < 0 ? "Unavailable" : pingFresh ? "Available" : "Stale"},
        {"latencyAgeMs", lastValidPingAt >= 0 && now >= lastValidPingAt ? now - lastValidPingAt : -1},
        {"latencyMs", pingFresh ? linkLatencyMsValue : -1},
        {"quality", connected() ? qualityScore : -1}, {"category", connected() ? qualityCategory : QString()}};
}

void Mavlink::resetLinkQuality()
{
    qualityTimer.stop();
    qualityGeneration = connectionGenerationValue;
    sequenceWindow.clear(); latencyWindow.clear(); pendingQualityPings.clear(); radioWindow.clear();
    sequenceBaseline = radioBaseline = false; radioSourceComponent = -1;
    lastSequenceAt = lastRadioAt = lastRssiAt = lastValidPingAt = filterAt = candidateAt = categoryAt = -1;
    qualityScore = rawRssi = linkLatencyMsValue = -1; filteredQuality = sequenceEstimate = mavlinkQualityValue = -1;
    sequenceDuplicates = sequenceReordered = sequenceResyncs = radioResets = 0;
    qualityCategory.clear(); candidateCategory.clear(); radioCounterStatus = "unavailable";
    publishedQualityDiagnostics.clear();
    emit linkQualityChanged();
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

void Mavlink::handleParameterValue(const mavlink_message_t &msg, quint64 generation)
{
    if (!connected() || msg.sysid != targetSystemId || msg.compid != targetComponentId
            || generation != parameterManagerObject->generation()) return;
    if (msg.msgid == MAVLINK_MSG_ID_AUTOPILOT_VERSION) {
        mavlink_autopilot_version_t version = {};
        mavlink_msg_autopilot_version_decode(&msg, &version);
        const bool bytewise = version.capabilities & MAV_PROTOCOL_CAPABILITY_PARAM_ENCODE_BYTEWISE;
        const bool numeric = version.capabilities & MAV_PROTOCOL_CAPABILITY_PARAM_ENCODE_C_CAST;
        if (bytewise || numeric)
            parameterManagerObject->setEncoding(bytewise && numeric ? ParameterManager::UnknownEncoding
                                                : bytewise ? ParameterManager::Bytewise : ParameterManager::NumericCast);
        return;
    }
    if (msg.msgid != MAVLINK_MSG_ID_PARAM_VALUE) return;
    mavlink_param_value_t value = {};
    mavlink_msg_param_value_decode(&msg, &value);
    ParameterManager::Sample sample;
    sample.identifier = QByteArray(value.param_id, 16);
    sample.wireValue = value.param_value;
    sample.type = value.param_type;
    sample.index = value.param_index;
    sample.count = value.param_count;
    parameterManagerObject->receive(msg.sysid, msg.compid, generation, sample);
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

    // ArduPilot 4.0+ and PX4 support per-message intervals. This is a request,
    // not evidence of support/delivery: Vehicle remains unknown until telemetry
    // arrives. Keep legacy stream requests and RC configuration unchanged.
    mavlink_msg_command_long_pack(255, 0, &msg, targetSystemId, targetComponentId,
                                 MAV_CMD_SET_MESSAGE_INTERVAL, 0,
                                 MAVLINK_MSG_ID_EXTENDED_SYS_STATE, 1000000.0f,
                                 0, 0, 0, 0, 0);
    sendMavlinkMessage(msg);
    qDebug() << "Requested all data streams at 10Hz, RC channels at 50Hz, and landed state at 1Hz";
}

void Mavlink::sendCommandLong(uint16_t command, float param1, float param2, float param3,
                              float param4, float param5, float param6, float param7)
{
    if (!connected()) {
        rejectCommand(QStringLiteral("commandLong:%1").arg(command), "Vehicle is not connected.");
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
    if (!connected()) {
        rejectCommand("setFlightMode", "Vehicle is not connected.");
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

bool Mavlink::sendParameterSet(const QString &name, double value, uint8_t type)
{
    return setParameter(name, value, type);
}

bool Mavlink::sendMavlinkMessage(mavlink_message_t &msg)
{
    if (!link) {
        return false;
    }

    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    const int len = mavlink_msg_to_send_buffer(buf, &msg);
    const qint64 bytesWritten = link->writeData(reinterpret_cast<const char*>(buf), len);
    if (bytesWritten == len && shouldLogTelemetryMessage(msg)) {
        writeTelemetryLogRecord(reinterpret_cast<const char*>(buf), len);
    }
    link->flush();
    if (bytesWritten == len) ++transmittedPackets;
    return bytesWritten == len;
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
    if (!connected() || pendingTerrainBlocks.isEmpty()) {
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
    if (!connected() || pendingMissionItems.isEmpty()) {
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

bool Mavlink::missionMessageMatchesTransfer(const mavlink_message_t &msg) const
{
    return transferStateValue != TransferIdle && transferStateValue != Completed
            && transferStateValue != Failed && transferStateValue != Cancelled
            && connected() && missionTransferConnectionGeneration == connectionGenerationValue
            && msg.sysid == transferSystemId && msg.compid == transferComponentId;
}

void Mavlink::startMissionTransfer(TransferState state, int total)
{
    if (transferStateValue != TransferIdle && transferStateValue != Completed
            && transferStateValue != Failed && transferStateValue != Cancelled) {
        finishMissionTransfer(Cancelled, QStringLiteral("superseded"), QStringLiteral("Mission transfer superseded by a new request."));
    }
    ++transferIdValue;
    transferStateValue = state;
    missionTransferConnectionGeneration = connectionGenerationValue;
    transferSystemId = targetSystemId;
    transferComponentId = targetComponentId;
    transferTotalValue = total;
    transferProgressValue = 0;
    transferRetryCount = 0;
    transferExpectedSequence = 0;
    transferErrorCodeValue.clear();
    emit missionTransferChanged();
    restartMissionTransferTimer();
}

void Mavlink::finishMissionTransfer(TransferState state, const QString &code, const QString &message)
{
    if (transferStateValue == Completed || transferStateValue == Failed || transferStateValue == Cancelled) return;
    if (missionTransferTimer) missionTransferTimer->stop();
    if (state == Completed || state == Failed || state == Cancelled) {
        replaceDirtyPlanOnDownloadCompletion = false;
    }
    transferStateValue = state;
    transferErrorCodeValue = code;
    emit missionTransferChanged();
    setMissionStatus(message);
    emit missionTransferFinished(transferIdValue, int(state), code, message);
}

void Mavlink::restartMissionTransferTimer()
{
    if (!missionTransferTimer) return;
    QObject::disconnect(missionTimerConnection);
    const auto serial = ++missionTimerSerial;
    const auto generation = connectionGenerationValue;
    const auto transfer = transferIdValue;
    missionTimerConnection = connect(missionTransferTimer, &QTimer::timeout, this, [this, serial, generation, transfer] {
        if (serial == missionTimerSerial && generation == connectionGenerationValue && transfer == transferIdValue)
            onMissionTransferTimeout();
    });
    missionTransferTimer->start(3000);
}

void Mavlink::onMissionTransferTimeout()
{
    if (!connected() || missionTransferConnectionGeneration != connectionGenerationValue) return;
    if (transferStateValue == TransferIdle || transferStateValue == Completed || transferStateValue == Failed || transferStateValue == Cancelled) return;
    if (++transferRetryCount > 3) {
        const bool upload = transferStateValue == ClearingBeforeUpload || transferStateValue == Uploading;
        missionUploadActive = false; missionClearBeforeUploadActive = false; missionDownloadActive = false;
        finishMissionTransfer(Failed, QStringLiteral("timeout"), QStringLiteral("Mission transfer timed out."));
        if (upload) emit missionUploadFinished(false, missionStatusText);
        return;
    }
    if (transferStateValue == Downloading) {
        if (missionDownloadExpectedCount == 0) requestMissionList(); else requestMissionItem(transferExpectedSequence);
    } else if (transferStateValue == ClearingBeforeUpload || transferStateValue == Clearing) {
        sendMissionClearAll();
    } else if (transferStateValue == Uploading) {
        sendMissionCount();
    }
    restartMissionTransferTimer();
}

void Mavlink::cancelMissionTransfer()
{
    if (transferStateValue == TransferIdle || transferStateValue == Completed || transferStateValue == Failed || transferStateValue == Cancelled) return;
    missionUploadActive = false; missionClearBeforeUploadActive = false; missionDownloadActive = false;
    replaceDirtyPlanOnDownloadCompletion = false;
    finishMissionTransfer(Cancelled, QStringLiteral("cancelled"), QStringLiteral("Mission transfer cancelled."));
}

void Mavlink::handleMissionMessage(const mavlink_message_t &msg)
{
    if ((msg.msgid == MAVLINK_MSG_ID_MISSION_COUNT || msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM
            || msg.msgid == MAVLINK_MSG_ID_MISSION_ITEM_INT || msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST
            || msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_INT || msg.msgid == MAVLINK_MSG_ID_MISSION_ACK)
            && !missionMessageMatchesTransfer(msg)) {
        return;
    }
    if (msg.msgid == MAVLINK_MSG_ID_MISSION_COUNT) {
        if (!missionDownloadActive || missionUploadActive || missionClearBeforeUploadActive) {
            return;
        }

        mavlink_mission_count_t count;
        mavlink_msg_mission_count_decode(&msg, &count);
        if (count.mission_type != MAV_MISSION_TYPE_MISSION) {
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
            finishMissionTransfer(Completed, {}, missionStatusText);
            return;
        }

        downloadingMissionItems.resize(missionDownloadExpectedCount);
        transferTotalValue = missionDownloadExpectedCount;
        transferExpectedSequence = 0;
        emit missionTransferChanged();
        setMissionStatus(QStringLiteral("Vehicle mission found: %1 waypoint items.").arg(missionDownloadExpectedCount));
        requestMissionItem(0);
        restartMissionTransferTimer();
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
            if (downloadedItem.mission_type != MAV_MISSION_TYPE_MISSION) {
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
            if (downloadedItem.mission_type != MAV_MISSION_TYPE_MISSION) {
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
            finishMissionTransfer(Failed, QStringLiteral("invalid-sequence"), missionStatusText);
            return;
        }

        if (downloadedMissionItemSeqs.contains(item.seq)) {
            sendMissionAck(MAV_MISSION_INVALID_SEQUENCE);
            missionDownloadActive = false;
            finishMissionTransfer(Failed, QStringLiteral("duplicate-sequence"), QStringLiteral("Mission download failed: duplicate item sequence."));
            return;
        }

        if (item.seq != transferExpectedSequence) {
            sendMissionAck(MAV_MISSION_INVALID_SEQUENCE);
            missionDownloadActive = false;
            finishMissionTransfer(Failed, QStringLiteral("unexpected-sequence"), QStringLiteral("Mission download received an out-of-order item."));
            return;
        }

        downloadingMissionItems[item.seq] = item;
        downloadedMissionItemSeqs.insert(item.seq);
        transferProgressValue = downloadedMissionItemSeqs.size();
        emit missionTransferChanged();

        if (downloadedMissionItemSeqs.size() < missionDownloadExpectedCount) {
            for (uint16_t seq = 0; seq < missionDownloadExpectedCount; ++seq) {
                if (!downloadedMissionItemSeqs.contains(seq)) {
                    transferExpectedSequence = seq;
                    requestMissionItem(seq);
                    restartMissionTransferTimer();
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
        if (missionPlanModel) {
            const auto converted = MissionPlanConverter::importVehicleMission(
                    pendingMissionItems, missionPlanModel, replaceDirtyPlanOnDownloadCompletion);
            if (!converted.success) {
                replaceDirtyPlanOnDownloadCompletion = false;
                finishMissionTransfer(Failed, QStringLiteral("model-import"), converted.error);
                return;
            }
        }
        replaceDirtyPlanOnDownloadCompletion = false;

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
        finishMissionTransfer(Completed, {}, missionStatusText);
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
        if (!missionUploadActive) {
            return;
        }
        // Fence/rally and other mission transactions can share the selected
        // autopilot link. They are unrelated to this normal mission upload.
        if (request.mission_type != MAV_MISSION_TYPE_MISSION) {
            return;
        }
        if (request.seq < static_cast<uint16_t>(pendingMissionItems.size())) {
            sendMissionItem(request.seq, true);
            transferProgressValue = qMax(transferProgressValue, int(request.seq) + 1);
            emit missionTransferChanged();
            restartMissionTransferTimer();
        } else {
            missionUploadActive = false;
            finishMissionTransfer(Failed, QStringLiteral("invalid-request"), QStringLiteral("Mission upload received an invalid item request."));
            emit missionUploadFinished(false, missionStatusText);
        }
        return;
    }

    if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST) {
        mavlink_mission_request_t request;
        mavlink_msg_mission_request_decode(&msg, &request);
        if (!missionUploadActive) {
            return;
        }
        if (request.mission_type != MAV_MISSION_TYPE_MISSION) {
            return;
        }
        if (request.seq < static_cast<uint16_t>(pendingMissionItems.size())) {
            sendMissionItem(request.seq, false);
            transferProgressValue = qMax(transferProgressValue, int(request.seq) + 1);
            emit missionTransferChanged();
            restartMissionTransferTimer();
        } else {
            missionUploadActive = false;
            finishMissionTransfer(Failed, QStringLiteral("invalid-request"), QStringLiteral("Mission upload received an invalid item request."));
            emit missionUploadFinished(false, missionStatusText);
        }
        return;
    }

    if (msg.msgid == MAVLINK_MSG_ID_MISSION_ACK) {
        mavlink_mission_ack_t ack;
        mavlink_msg_mission_ack_decode(&msg, &ack);

        if (ack.mission_type != MAV_MISSION_TYPE_MISSION) return;
        if (missionClearBeforeUploadActive) {
            if (ack.type != MAV_MISSION_ACCEPTED) {
                missionClearBeforeUploadActive = false;
                finishMissionTransfer(Failed, QStringLiteral("clear-rejected"), QStringLiteral("Vehicle rejected mission clear before upload."));
                emit missionUploadFinished(false, missionStatusText);
                return;
            }
            missionClearBeforeUploadActive = false;
            missionUploadActive = true;
            setMissionStatus(QStringLiteral("Uploading mission: %1 waypoint items").arg(pendingMissionItems.size()));
            sendMissionCount();
            transferStateValue = Uploading;
            emit missionTransferChanged();
            restartMissionTransferTimer();
            return;
        }

        if (transferStateValue == Clearing) {
            if (ack.type == MAV_MISSION_ACCEPTED) {
                // The clear acknowledgement is the authoritative point at
                // which the legacy render cache may be discarded.  This keeps
                // a failed clear from incorrectly removing the mission map.
                pendingMissionItems.clear();
                loadedMissionWaypoints.clear();
                loadedMissionElevationWaypoints.clear();
                loadedMissionTerrainProfile.clear();
                setUploadedHomeLocation(QVariantMap());
                setMissionUploaded(false);
                emit missionWaypointsChanged();
                emit missionElevationWaypointsChanged();
                emit missionTerrainProfileChanged();
                finishMissionTransfer(Completed, {}, QStringLiteral("Vehicle mission cleared."));
            } else {
                finishMissionTransfer(Failed, QStringLiteral("clear-rejected"), QStringLiteral("Vehicle rejected mission clear."));
            }
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
        if (ok && transferUsesCanonicalSnapshot && missionPlanModel
                && missionPlanModel->revision() == transferSnapshotRevision) {
            missionPlanModel->markCurrentRevisionSaved();
        }
        finishMissionTransfer(ok ? Completed : Failed, ok ? QString() : QStringLiteral("mission-ack"), message);
    }
}

bool Mavlink::setLink(ILink *newLink)
{
    if (!newLink) return false;
    if (link == newLink) return true;
    if (link) {
        auto *oldLink = link;
        const auto previousTransport = transportGeneration;
        QObject::disconnect(oldLink, nullptr, this, nullptr);
        endConnection(QStringLiteral("Transport replaced"), false);
        if (link != oldLink || transportGeneration != previousTransport) {
            newLink->deleteLater();
            return false; // A disconnect observer already initiated a newer connection.
        }
        oldLink->disconnectLink();
        oldLink->deleteLater();
    }
    link = newLink;
    const auto transport = ++transportGeneration;
    inboundFrameBytes.clear();
    mavlinkStatus = {};
    mavlink_reset_channel_status(MAVLINK_COMM_0);
    connect(newLink, &ILink::readyRead, this, [this, newLink, transport] {
        if (link == newLink && transport == transportGeneration && acceptTransportMessages) readData();
    });
    connect(newLink, &ILink::disconnected, this, [this, newLink, transport] {
        if (link == newLink && transport == transportGeneration) endConnection(QStringLiteral("Transport disconnected or failed"), false);
    });
    connect(newLink, &QObject::destroyed, this, [this, transport] {
        if (transport != transportGeneration) return;
        link = nullptr;
        ++transportGeneration;
        endConnection(QStringLiteral("Transport destroyed"), false);
    });
    return true;
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

QObject *Mavlink::console() const { return consoleController; }
