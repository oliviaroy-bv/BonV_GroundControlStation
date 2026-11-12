#pragma once

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QPointer>
#include <functional>

#include <memory>

#include "terrain/TerrainProvider.h"
#include "transport/ILink.h"
#include "mavlink/MavlinkFrame.h"

class MissionPlanModel;
class ParameterManager;

extern "C" {
#include "common/mavlink.h"
}

// Value-copy representation intentionally exposed for conversion only.  It
// does not expose Mavlink's transfer queues for mutation.
struct MavlinkMissionItem {
    uint16_t seq = 0;
    uint8_t current = 0;
    uint8_t frame = 0;
    uint16_t command = 0;
    float param1 = 0;
    float param2 = 0;
    float param3 = 0;
    float param4 = 0;
    double x = 0;
    double y = 0;
    float z = 0;
    uint8_t autocontinue = 1;
};

class Mavlink : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap motorTestPrerequisites READ motorTestPrerequisites NOTIFY motorTestPrerequisitesChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionStatusChanged)
    Q_PROPERTY(ConnectionState connectionState READ connectionState NOTIFY connectionLifecycleChanged)
    Q_PROPERTY(quint64 connectionGeneration READ connectionGeneration NOTIFY connectionLifecycleChanged)
    Q_PROPERTY(qint64 lastHeartbeatTimestampMs READ lastHeartbeatTimestampMs NOTIFY heartbeatObserved)
    Q_PROPERTY(int heartbeatTimeoutMs READ heartbeatTimeoutMs NOTIFY connectionLifecycleChanged)
    Q_PROPERTY(quint64 heartbeatExpiryCount READ heartbeatExpiryCount NOTIFY connectionLifecycleChanged)
    Q_PROPERTY(QString lastDisconnectReason READ lastDisconnectReason NOTIFY connectionLifecycleChanged)
    Q_PROPERTY(bool vpsConnected READ vpsConnected NOTIFY vpsStatusChanged)
    Q_PROPERTY(QVariantMap vpsStatus READ vpsStatus NOTIFY vpsStatusChanged)
    Q_PROPERTY(QVariantMap vpsConfig READ vpsConfig NOTIFY vpsConfigChanged)
    Q_PROPERTY(QVariantMap transportDiagnostics READ transportDiagnostics NOTIFY linkQualityChanged)
    Q_PROPERTY(QString diagnosticEndpoint READ diagnosticEndpoint NOTIFY linkQualityChanged)
    Q_PROPERTY(int estimatedLinkQuality READ estimatedLinkQuality NOTIFY linkQualityChanged)
    Q_PROPERTY(QString linkQualityCategory READ linkQualityCategory NOTIFY linkQualityChanged)
    Q_PROPERTY(QVariantMap linkQualityDiagnostics READ linkQualityDiagnostics NOTIFY linkQualityChanged)
    Q_PROPERTY(int rawRadioRssi READ rawRadioRssi NOTIFY linkQualityChanged)
    Q_PROPERTY(int linkLatencyMs READ linkLatencyMs NOTIFY linkQualityChanged)
    Q_PROPERTY(int mavlinkQuality READ mavlinkQuality NOTIFY linkQualityChanged)
    Q_PROPERTY(QVariantList missionWaypoints READ missionWaypoints NOTIFY missionWaypointsChanged)
    Q_PROPERTY(QVariantList missionElevationWaypoints READ missionElevationWaypoints NOTIFY missionElevationWaypointsChanged)
    Q_PROPERTY(QVariantList missionTerrainProfile READ missionTerrainProfile NOTIFY missionTerrainProfileChanged)
    Q_PROPERTY(QVariantMap uploadedHomeLocation READ uploadedHomeLocation NOTIFY uploadedHomeLocationChanged)
    Q_PROPERTY(bool uploadedHomeLocationAvailable READ uploadedHomeLocationAvailable NOTIFY uploadedHomeLocationChanged)
    Q_PROPERTY(QString missionStatus READ missionStatus NOTIFY missionStatusChanged)
    Q_PROPERTY(bool missionUploaded READ missionUploaded NOTIFY missionUploadedChanged)
    Q_PROPERTY(TransferState missionTransferState READ missionTransferState NOTIFY missionTransferChanged)
    Q_PROPERTY(quint64 missionTransferId READ missionTransferId NOTIFY missionTransferChanged)
    Q_PROPERTY(int missionTransferProgress READ missionTransferProgress NOTIFY missionTransferChanged)
    Q_PROPERTY(int missionTransferTotal READ missionTransferTotal NOTIFY missionTransferChanged)
    Q_PROPERTY(int missionTransferPercent READ missionTransferPercent NOTIFY missionTransferChanged)
    Q_PROPERTY(QString missionTransferOperation READ missionTransferOperation NOTIFY missionTransferChanged)
    Q_PROPERTY(QString missionTransferErrorCode READ missionTransferErrorCode NOTIFY missionTransferChanged)
    Q_PROPERTY(QStringList serialPorts READ serialPorts NOTIFY serialPortsChanged)
    Q_PROPERTY(bool parameterFetchInProgress READ parameterFetchInProgress NOTIFY parameterFetchStatusChanged)
    Q_PROPERTY(int parameterFetchProgress READ parameterFetchProgress NOTIFY parameterFetchStatusChanged)
    Q_PROPERTY(int parameterCount READ parameterCount NOTIFY parameterFetchStatusChanged)
    Q_PROPERTY(int loadedParameterCount READ loadedParameterCount NOTIFY parameterFetchStatusChanged)
    Q_PROPERTY(QVariantList rcChannels READ rcChannels NOTIFY rcChannelsChanged)
    Q_PROPERTY(bool telemetryLoggingEnabled READ telemetryLoggingEnabled WRITE setTelemetryLoggingEnabled NOTIFY telemetryLoggingSettingsChanged)
    Q_PROPERTY(bool telemetryLogActive READ telemetryLogActive NOTIFY telemetryLogStateChanged)
    Q_PROPERTY(QString telemetryLogDirectory READ telemetryLogDirectory WRITE setTelemetryLogDirectory NOTIFY telemetryLogDirectoryChanged)
    Q_PROPERTY(QString currentTelemetryLogPath READ currentTelemetryLogPath NOTIFY telemetryLogStateChanged)
    Q_PROPERTY(bool flightControllerLoggingEnabled READ flightControllerLoggingEnabled WRITE setFlightControllerLoggingEnabled NOTIFY flightControllerLoggingSettingsChanged)
    Q_PROPERTY(bool flightControllerLogActive READ flightControllerLogActive NOTIFY flightControllerLogStateChanged)
    Q_PROPERTY(QString flightControllerLogDirectory READ flightControllerLogDirectory WRITE setFlightControllerLogDirectory NOTIFY flightControllerLogDirectoryChanged)
    Q_PROPERTY(QString currentFlightControllerLogPath READ currentFlightControllerLogPath NOTIFY flightControllerLogStateChanged)
    Q_PROPERTY(QObject *console READ console CONSTANT)
    Q_PROPERTY(QObject *inspector READ inspector CONSTANT)
    Q_PROPERTY(QObject *forwarder READ forwarder CONSTANT)

public:
    enum ConnectionState { Disconnected, AwaitingHeartbeat, Connected };
    Q_ENUM(ConnectionState)
    struct ConnectionConfig {
        int heartbeatTimeoutMs = 5000; // Tolerate several missed nominal 1 Hz heartbeats.
        int initialHeartbeatTimeoutMs = 10000;
        int parameterStartupDelayMs = 250; // Preserve the existing startup pacing.
    };
    struct QualityConfig {
        int tickMs = 250;
        int sequenceWindowMs = 5000;
        int sequenceFreshMs = 3000;
        int radioFreshMs = 5000;
        int pingFreshMs = 5000;
        int pingResponseMs = 3000;
        int minimumSequenceExpected = 20;
        int maximumForwardDelta = 63;
        int sustainedCategoryMs = 2000;
        int minimumCategoryDwellMs = 3000;
        double timeConstantSeconds = 3.0;
        double goodEntry = 85, goodExit = 75, poorEntry = 45, poorExit = 60;
    };
    const QualityConfig &qualityConfiguration() const { return qualityConfig; }
    bool setQualityConfiguration(const QualityConfig &config);
    void processLinkQuality();
    int estimatedLinkQuality() const { return qualityScore; }
    QString linkQualityCategory() const { return qualityCategory; }
    QVariantMap linkQualityDiagnostics() const;
    QVariantMap transportDiagnostics() const;
    Q_INVOKABLE QVariantMap transportDiagnosticSnapshot() const { return transportDiagnostics(); }
    QString diagnosticEndpoint() const { return link ? diagnosticEndpointValue : QString(); }
    int rawRadioRssi() const;
    QVariantMap motorTestPrerequisites() const;
    ConnectionState connectionState() const { return connectionStateValue; }
    quint64 connectionGeneration() const { return connectionGenerationValue; }
    qint64 lastHeartbeatTimestampMs() const { return lastHeartbeatUtcMs; }
    qint64 lastHeartbeatMonotonicMs() const { return lastHeartbeatMs; }
    int heartbeatTimeoutMs() const { return connectionConfig.heartbeatTimeoutMs; }
    quint64 heartbeatExpiryCount() const { return heartbeatExpiries; }
    QString lastDisconnectReason() const { return disconnectReason; }
    const ConnectionConfig &connectionConfiguration() const { return connectionConfig; }
    bool setConnectionConfiguration(const ConnectionConfig &config);
    void processConnectionTimeout(quint64 generation);
    enum TransferState { TransferIdle, ClearingBeforeUpload, Uploading, Downloading, Clearing, Completed, Failed, Cancelled };
    Q_ENUM(TransferState)
    // The first autopilot heartbeat selects the source until disconnect/reconnect.
    int selectedSystemId() const { return selectedVehicleValid ? targetSystemId : 0; }
    int selectedComponentId() const { return selectedVehicleValid ? targetComponentId : 0; }
    explicit Mavlink(QObject *parent = nullptr, ParameterManager *parameters = nullptr,
                     std::function<qint64()> monotonicClock = {});
    ParameterManager *parameterManager() const;
    ~Mavlink() override;

    bool connected() const;
    bool vpsConnected() const;
    QVariantMap vpsStatus() const;
    QVariantMap vpsConfig() const;
    int linkLatencyMs() const;
    int mavlinkQuality() const;
    QVariantList missionWaypoints() const;
    QVariantList missionElevationWaypoints() const;
    QVariantList missionTerrainProfile() const;
    QVector<MavlinkMissionItem> missionItems() const;
    QVariantMap uploadedHomeLocation() const;
    bool uploadedHomeLocationAvailable() const;
    QString missionStatus() const;
    bool missionUploaded() const;
    TransferState missionTransferState() const;
    quint64 missionTransferId() const;
    int missionTransferProgress() const;
    int missionTransferTotal() const;
    int missionTransferPercent() const;
    QString missionTransferOperation() const;
    QString missionTransferErrorCode() const;
    void setMissionPlanModel(MissionPlanModel *model);
    QStringList serialPorts() const;
    bool parameterFetchInProgress() const;
    int parameterFetchProgress() const;
    int parameterCount() const;
    int loadedParameterCount() const;
    QVariantList rcChannels() const;
    bool telemetryLoggingEnabled() const;
    bool telemetryLogActive() const;
    QString telemetryLogDirectory() const;
    QString currentTelemetryLogPath() const;
    bool flightControllerLoggingEnabled() const;
    bool flightControllerLogActive() const;
    QString flightControllerLogDirectory() const;
    QString currentFlightControllerLogPath() const;
    QObject *console() const;
    QObject *inspector() const;
    QObject *forwarder() const;

    Q_INVOKABLE bool connectToVehicle(QString ip, int port);
    Q_INVOKABLE bool connectToVehicleProtocol(QString protocol, QString hostOrPortName, int port, int baudRate = 57600);
    Q_INVOKABLE void disconnectFromVehicle();
    Q_INVOKABLE void refreshSerialPorts();
    Q_INVOKABLE void armVehicle(bool arm = true);
    Q_INVOKABLE void takeoff(double altitudeMeters = 10.0);
    Q_INVOKABLE void startMission();
    Q_INVOKABLE void returnToLaunch();
    Q_INVOKABLE void land();
    Q_INVOKABLE void emergencyStop();
    Q_INVOKABLE void setFlightMode(int customMode);
    Q_INVOKABLE void motor();
    Q_INVOKABLE void testMotor(int motor, int throttlePercent = 10, int durationSeconds = 2);
    Q_INVOKABLE void runAllMotorTest(int throttlePercent = 10, int durationSeconds = 2, int motorCount = 4);
    Q_INVOKABLE void runSequentialMotorTest(int throttlePercent = 10, int durationSeconds = 2, int motorCount = 4);
    Q_INVOKABLE void stopAllMotorTests(int motorCount = 4);
    Q_INVOKABLE bool servo(int servoNumber = 9, int pwm = 1500);
    Q_INVOKABLE bool sendPayloadCommand(int commandId, int watchdogMinutes = 0);
    Q_INVOKABLE bool startVpsAction(int action);
    Q_INVOKABLE bool refreshVpsConfig();
    Q_INVOKABLE bool setVpsHome(double latitude, double longitude);
    Q_INVOKABLE bool setVpsLogging(bool enabled);
    Q_INVOKABLE bool setVpsSensorSpeed(int speed);
    Q_INVOKABLE bool setVpsGpsCheck(const QVariantMap &values);
    Q_INVOKABLE bool setVpsGpsJamming(const QVariantMap &values);
    Q_INVOKABLE void refreshTelemetry();
    Q_INVOKABLE bool uploadMissionFile(const QUrl &fileUrl);
    Q_INVOKABLE bool writeLoadedMissionToVehicle();
    Q_INVOKABLE bool uploadMissionPlan(QObject *model);
    Q_INVOKABLE void cancelMissionTransfer();
    Q_INVOKABLE bool replaceDownloadedMission(QObject *model);
    Q_INVOKABLE bool resetHomeToUploadedLocation();
    Q_INVOKABLE bool sendManualControl(int x, int y, int z, int r, int buttons = 0, int buttons2 = 0);
    Q_INVOKABLE bool sendRcChannelsOverride(int rollPwm, int pitchPwm, int throttlePwm, int yawPwm);
    // replaceDirtyPlan is only set by an explicit operator decision in the UI.
    // It affects canonical-model commit after a successful protocol download,
    // never the MAVLink request/retry exchange itself.
    Q_INVOKABLE void downloadMissionFromVehicle(bool replaceDirtyPlan = false);
    Q_INVOKABLE void saveMissionFromVehicle();
    Q_INVOKABLE void clearMission();
    Q_INVOKABLE void requestParameters();
    Q_INVOKABLE bool requestParameterRead(const QString &name);
    Q_INVOKABLE QVariantMap parameterValue(const QString &name) const;
    Q_INVOKABLE QVariantMap parameters() const;
    Q_INVOKABLE bool setParameter(const QString &name, double value, int type = MAV_PARAM_TYPE_REAL32);
    // Legacy booleans mean request acceptance, never vehicle confirmation.
    Q_INVOKABLE bool writeRcCalibration(const QVariantList &calibration);
    Q_INVOKABLE QVariantMap requestParameterWrite(const QString &name, double value, int type);
    Q_INVOKABLE QVariantMap requestRcCalibrationWrite(const QVariantList &calibration);
    Q_INVOKABLE void setTelemetryLoggingEnabled(bool enabled);
    Q_INVOKABLE void setTelemetryLogDirectory(const QString &directory);
    Q_INVOKABLE bool startTelemetryLog();
    Q_INVOKABLE void stopTelemetryLog();
    Q_INVOKABLE QStringList telemetryLogFiles() const;
    Q_INVOKABLE void setFlightControllerLoggingEnabled(bool enabled);
    Q_INVOKABLE void setFlightControllerLogDirectory(const QString &directory);
    Q_INVOKABLE bool startFlightControllerLog();
    Q_INVOKABLE void stopFlightControllerLog();
    Q_INVOKABLE QStringList flightControllerLogFiles() const;

signals:
    // Acceptance is local admission, never proof of vehicle execution.
    void motorTestPrerequisitesChanged();
    void commandPrerequisiteRejected(const QString &operation, const QString &reason, const QVariantMap &context);
    void connectionLifecycleChanged();
    void heartbeatObserved();
    void selectedVehicleChanged();
    void connectionStatusChanged(bool connected);
    void vpsStatusChanged();
    void vpsConfigChanged();
    void linkQualityChanged();
    void messageReceived(const mavlink_message_t &msg);
    void frameObserved(const MavlinkFrame &frame);
    void missionWaypointsChanged();
    void missionElevationWaypointsChanged();
    void missionTerrainProfileChanged();
    void uploadedHomeLocationChanged();
    void missionStatusChanged(const QString &status);
    void missionUploadedChanged(bool uploaded);
    void missionUploadFinished(bool success, const QString &message);
    void missionTransferChanged();
    void missionTransferFinished(quint64 transferId, int terminalState, const QString &errorCode, const QString &message);
    void serialPortsChanged();
    void commandRejected(int command, const QString &reason);
    void motorTestCommandAccepted(const QString &mode, int firstMotor, int motorCount, int durationSeconds);
    void motorTestCommandRejected(const QString &mode, int firstMotor, int motorCount, const QString &reason);
    void servoCommandAccepted(int servoNumber, int pwm);
    void servoCommandRejected(int servoNumber, int pwm, const QString &reason);
    void parameterWriteFinished(const QVariantMap &result);
    void rcCalibrationWriteFinished(const QVariantMap &result);
    void parameterFetchStatusChanged();
    void parameterUpdated(const QString &name, const QVariantMap &parameter);
    void parameterFetchFinished(bool complete, int loadedCount, int expectedCount);
    void rcChannelsChanged();
    void telemetryLoggingSettingsChanged();
    void telemetryLogDirectoryChanged();
    void telemetryLogStateChanged();
    void flightControllerLoggingSettingsChanged();
    void flightControllerLogDirectoryChanged();
    void flightControllerLogStateChanged();
    void homeLocationResetRequested(double latitude, double longitude, double altitude);

private slots:
    void readData();

private:
    struct SequenceSample { qint64 at; int expected; };
    struct PingRequest { qint64 at; quint64 stamp; };
    struct LatencySample { qint64 at; int value; };
    struct RadioSample { qint64 at; int errors; int corrections; };
    QualityConfig qualityConfig;
    QTimer qualityTimer;
    quint64 qualityGeneration = 0;
    QVector<SequenceSample> sequenceWindow;
    QHash<quint32, PingRequest> pendingQualityPings;
    QVector<LatencySample> latencyWindow;
    QVector<RadioSample> radioWindow;
    bool sequenceBaseline = false, radioBaseline = false;
    quint8 qualityLastSequence = 0;
    int radioSourceComponent = -1;
    quint16 previousRadioErrors = 0, previousRadioCorrections = 0;
    qint64 lastSequenceAt = -1, lastRadioAt = -1, lastRssiAt = -1, lastValidPingAt = -1;
    qint64 filterAt = -1, candidateAt = -1, categoryAt = -1;
    int qualityScore = -1, rawRssi = -1;
    int sequenceDuplicates = 0, sequenceReordered = 0, sequenceResyncs = 0, radioResets = 0;
    double filteredQuality = -1, sequenceEstimate = -1;
    QString qualityCategory, candidateCategory, radioCounterStatus = "unavailable";
    QVariantMap publishedQualityDiagnostics;
    friend class MavlinkTest; // Deterministic protocol-metric tests use the existing receive hooks.
    bool rejectCommand(const QString &operation, const QString &reason);
    QString liveConnectionError() const;
    QString parameterPrerequisiteError(const QStringList &required = {}) const;
    QString motorConfigurationError(int &motorCount, QString *signature = nullptr) const;
    bool validateMotorStart(const QString &mode, int motor, int throttle, int duration, int count = 0);
    void cancelPendingMotorStarts(const QString &reason);
    void revalidateMotorSchedule();
    bool missionTransferAvailable(const QString &operation);
    bool selectedHeartbeatArmed = false;
    QString pendingMotorStartMode, scheduledMotorConfiguration;
    quint64 motorStartConnectionGeneration = 0, motorStartParameterGeneration = 0;
    int lastConfirmedMotorCount = 0;
    quint64 missionTransferConnectionGeneration = 0, missionTimerSerial = 0;
    QMetaObject::Connection missionTimerConnection, motorSequenceConnection, motorStopConnection;
    quint64 motorStopSerial = 0;
    void armMotorSequenceTimer(int delay);
    void armMotorStopTimer(int delay);
    void beginConnection();
    void endConnection(const QString &reason, bool permitHeartbeatReconnect);
    void handleHeartbeat(const mavlink_message_t &message);
    void armConnectionDeadline();
    void scheduleInitialParameters();
    void stopLifecycleTimers();
    ConnectionState connectionStateValue = Disconnected;
    ConnectionConfig connectionConfig;
    std::function<qint64()> connectionClock;
    quint64 connectionGenerationValue = 0, transportGeneration = 0, heartbeatTimerSerial = 0;
    quint64 heartbeatExpiries = 0;
    qint64 lastHeartbeatMs = -1, lastHeartbeatUtcMs = 0, awaitingHeartbeatDeadline = 0;
    bool acceptTransportMessages = false, transitioningConnection = false, shuttingDown = false;
    QString disconnectReason;
    QTimer heartbeatTimer, parameterStartupTimer;
    QMetaObject::Connection heartbeatTimerConnection, parameterStartupConnection;
    friend class MavlinkConsoleController;
    class MavlinkConsoleController *consoleController = nullptr;
    bool selectedVehicleValid = false;
    void handleVpsMessage(const mavlink_message_t &message);
    void clearVpsStatus();
    bool writeVpsMessage(uint16_t type, const QByteArray &payload);
    QTimer *vpsTimeoutTimer = nullptr;
    bool vpsConnectedState = false;
    uint8_t vpsSystemId = 0;
    uint8_t vpsComponentId = 0;
    QVariantMap vpsStatusValues;
    QVariantMap vpsConfigValues;
    using MissionItem = MavlinkMissionItem;
    struct PendingTerrainBlock { mavlink_terrain_request_t request = {}; uint8_t gridbit = 0; };

    bool setLink(ILink *newLink); void setMissionStatus(const QString &status); void setMissionUploaded(bool uploaded); void setUploadedHomeLocation(const QVariantMap &location); void handleParameterValue(const mavlink_message_t &msg, quint64 generation); void handleRcChannels(const mavlink_message_t &msg); void requestDataStreams(); void sendCommandLong(uint16_t command, float param1 = 0, float param2 = 0, float param3 = 0, float param4 = 0, float param5 = 0, float param6 = 0, float param7 = 0); void sendSetMode(uint32_t customMode); bool sendParameterSet(const QString &name, double value, uint8_t type); bool sendMavlinkMessage(mavlink_message_t &msg); void sendLinkPing(); void handleLinkPing(const mavlink_message_t &msg); void updateMavlinkQuality(const mavlink_message_t &msg); void resetLinkQuality(); bool shouldLogTelemetryMessage(const mavlink_message_t &msg) const; void logTelemetryMessage(const mavlink_message_t &msg); void writeTelemetryLogRecord(const char *packet, int packetLength); quint64 currentTelemetryLogTimestampUsec() const; QString defaultTelemetryLogDirectory() const; QString nextTelemetryLogPath() const; void logFlightControllerStatusText(const mavlink_message_t &msg); void appendFlightControllerLogEntry(const QString &level, uint8_t systemId, uint8_t componentId, const QString &text); bool writeFlightControllerLogLine(const QString &line); QString defaultFlightControllerLogDirectory() const; QString nextFlightControllerLogPath() const; QString mavSeverityText(uint8_t severity) const; void sendMotorTestCommand(const QString &mode, int motor, int throttlePercent, int durationSeconds, int motorCount = 0); void sendActuatorMotorTestCommand(const QString &mode, int motor, int throttlePercent, int durationSeconds); void sendActuatorMotorStopCommand(int motor); void handleTerrainRequest(const mavlink_message_t &msg); bool buildTerrainDataBlock(const mavlink_terrain_request_t &request, uint8_t gridbit, int16_t data[16]); bool sendTerrainDataBlock(const mavlink_terrain_request_t &request, uint8_t gridbit); void queuePendingTerrainBlock(const mavlink_terrain_request_t &request, uint8_t gridbit); void retryPendingTerrainRequests(); void clearPendingTerrainRequests(); void stopMotorTestSequence(); void runNextSequentialMotorTest(); void sendMissionClearAll(); void sendMissionCount(); void sendMissionItem(uint16_t seq, bool useInt); void requestMissionList(); void requestMissionItem(uint16_t seq); void sendMissionAck(uint8_t type); void handleMissionMessage(const mavlink_message_t &msg); QString saveLoadedMissionToMissionsFolder() const; bool parseMissionFile(const QString &path, QVector<MissionItem> &items, QVariantList &displayWaypoints, QVariantMap &homeLocation, QString &error) const; QVariantMap missionHomeLocation(const QVector<MissionItem> &items) const; QVariantList buildDisplayWaypoints(const QVector<MissionItem> &items) const; QVariantList buildElevationWaypoints(const QVector<MissionItem> &items) const; QVariantList buildMissionTerrainProfile(const QVariantList &profileWaypoints);

    struct PendingMotorTestAck { uint16_t command = 0; QString mode; int firstMotor = 0; int motorCount = 0; int durationSeconds = 0; int sequenceId = 0; };
    struct PendingServoAck { int servoNumber = 0; int pwm = 0; };
    bool missionMessageMatchesTransfer(const mavlink_message_t &msg) const;
    void startMissionTransfer(TransferState state, int total = 0);
    void finishMissionTransfer(TransferState state, const QString &code, const QString &message);
    void restartMissionTransferTimer();
    void onMissionTransferTimeout();

    QString linkIdentityValue;
    QString diagnosticEndpointValue;
    quint64 receivedPackets = 0, transmittedPackets = 0, detectedSequenceGaps = 0;
    quint64 transportDuplicates = 0, transportReordered = 0;
    qint64 lastPacketAt = -1, packetRateStartedAt = -1;
    QVector<qint64> packetTimes;
    mavlink_radio_status_t diagnosticRadio{};
    QTimer *missionTransferTimer = nullptr;
    MissionPlanModel *missionPlanModel = nullptr;
    TransferState transferStateValue = TransferIdle;
    quint64 transferIdValue = 0;
    int transferProgressValue = 0;
    int transferTotalValue = 0;
    int transferRetryCount = 0;
    uint16_t transferExpectedSequence = 0;
    uint8_t transferSystemId = 0;
    uint8_t transferComponentId = 0;
    quint64 transferSnapshotRevision = 0;
    bool canonicalUploadPending = false;
    bool transferUsesCanonicalSnapshot = false;
    bool replaceDirtyPlanOnDownloadCompletion = false;
    QString transferErrorCodeValue;
    ILink *link = nullptr; class MavlinkInspectorController *inspectorController = nullptr; class MavlinkForwarder *forwarderObject = nullptr; QByteArray inboundFrameBytes; QStringList serialPortNames; mavlink_status_t mavlinkStatus = {}; QTimer *linkPingTimer = nullptr; quint32 nextPingSequence = 0; int linkLatencyMsValue = -1; double mavlinkQualityValue = -1; QVector<MissionItem> pendingMissionItems; QVariantList loadedMissionWaypoints; QVariantList loadedMissionElevationWaypoints; QVariantList loadedMissionTerrainProfile; QVariantMap loadedUploadedHomeLocation; QString missionStatusText; bool missionUploadedState = false; bool missionUploadActive = false; bool missionClearBeforeUploadActive = false; bool missionDownloadActive = false; bool missionDownloadRequestedOnConnect = false; bool missionSaveRequestedAfterDownload = false; uint16_t missionDownloadExpectedCount = 0; QVector<MissionItem> downloadingMissionItems; QSet<int> downloadedMissionItemSeqs; uint8_t targetSystemId = 1; uint8_t targetComponentId = MAV_COMP_ID_AUTOPILOT1; QTimer *motorTestSequenceTimer = nullptr; QTimer *motorTestStopTimer = nullptr; QVector<PendingMotorTestAck> pendingMotorTestAcks; QVector<PendingServoAck> pendingServoAcks; int motorTestSequenceNextMotor = 1; int motorTestSequenceMotorCount = 0; int motorTestSequenceThrottle = 10; int motorTestSequenceDuration = 2; int activeMotorTestStopCount = 0; int motorTestCommandGeneration = 0; int nextMotorTestSequenceId = 1; TerrainProvider terrainProvider; QVector<PendingTerrainBlock> pendingTerrainBlocks; QTimer *terrainRetryTimer = nullptr; QFutureWatcher<QStringList> *serialPortRefreshWatcher = nullptr; bool serialPortRefreshActive = false; QSet<quint64> rcCalibrationBatches; QPointer<ParameterManager> parameterManagerObject; uint8_t selectedAutopilot = MAV_AUTOPILOT_INVALID; QVariantList latestRcChannels; bool telemetryLoggingEnabledState = true; class BufferedLogWriter; std::unique_ptr<BufferedLogWriter> telemetryLogWriter; QString telemetryLogDirectoryPath; QString currentTelemetryLogFilePath; QElapsedTimer telemetryLogTimer; quint64 telemetryLogEpochUsec = 0; bool flightControllerLoggingEnabledState = true; std::unique_ptr<BufferedLogWriter> flightControllerLogWriter; QString flightControllerLogDirectoryPath; QString currentFlightControllerLogFilePath;
};
