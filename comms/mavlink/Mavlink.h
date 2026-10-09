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

#include <memory>

#include "terrain/TerrainProvider.h"
#include "transport/ILink.h"
#include "mavlink/MavlinkFrame.h"

extern "C" {
#include "common/mavlink.h"
}

class Mavlink : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectionStatusChanged)
    Q_PROPERTY(int linkLatencyMs READ linkLatencyMs NOTIFY linkQualityChanged)
    Q_PROPERTY(int mavlinkQuality READ mavlinkQuality NOTIFY linkQualityChanged)
    Q_PROPERTY(QVariantList missionWaypoints READ missionWaypoints NOTIFY missionWaypointsChanged)
    Q_PROPERTY(QVariantList missionElevationWaypoints READ missionElevationWaypoints NOTIFY missionElevationWaypointsChanged)
    Q_PROPERTY(QVariantList missionTerrainProfile READ missionTerrainProfile NOTIFY missionTerrainProfileChanged)
    Q_PROPERTY(QVariantMap uploadedHomeLocation READ uploadedHomeLocation NOTIFY uploadedHomeLocationChanged)
    Q_PROPERTY(bool uploadedHomeLocationAvailable READ uploadedHomeLocationAvailable NOTIFY uploadedHomeLocationChanged)
    Q_PROPERTY(QString missionStatus READ missionStatus NOTIFY missionStatusChanged)
    Q_PROPERTY(bool missionUploaded READ missionUploaded NOTIFY missionUploadedChanged)
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
    Q_PROPERTY(QObject *inspector READ inspector CONSTANT)
    Q_PROPERTY(QObject *forwarder READ forwarder CONSTANT)

public:
    explicit Mavlink(QObject *parent = nullptr);
    ~Mavlink() override;

    bool connected() const;
    int linkLatencyMs() const;
    int mavlinkQuality() const;
    QVariantList missionWaypoints() const;
    QVariantList missionElevationWaypoints() const;
    QVariantList missionTerrainProfile() const;
    QVariantMap uploadedHomeLocation() const;
    bool uploadedHomeLocationAvailable() const;
    QString missionStatus() const;
    bool missionUploaded() const;
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
    Q_INVOKABLE void refreshTelemetry();
    Q_INVOKABLE bool uploadMissionFile(const QUrl &fileUrl);
    Q_INVOKABLE bool writeLoadedMissionToVehicle();
    Q_INVOKABLE bool resetHomeToUploadedLocation();
    Q_INVOKABLE bool sendManualControl(int x, int y, int z, int r, int buttons = 0, int buttons2 = 0);
    Q_INVOKABLE bool sendRcChannelsOverride(int rollPwm, int pitchPwm, int throttlePwm, int yawPwm);
    Q_INVOKABLE void downloadMissionFromVehicle();
    Q_INVOKABLE void saveMissionFromVehicle();
    Q_INVOKABLE void clearMission();
    Q_INVOKABLE void requestParameters();
    Q_INVOKABLE QVariantMap parameterValue(const QString &name) const;
    Q_INVOKABLE QVariantMap parameters() const;
    Q_INVOKABLE bool setParameter(const QString &name, double value, int type = MAV_PARAM_TYPE_REAL32);
    Q_INVOKABLE bool writeRcCalibration(const QVariantList &calibration);
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
    void connectionStatusChanged(bool connected);
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
    void serialPortsChanged();
    void commandRejected(int command, const QString &reason);
    void motorTestCommandAccepted(const QString &mode, int firstMotor, int motorCount, int durationSeconds);
    void motorTestCommandRejected(const QString &mode, int firstMotor, int motorCount, const QString &reason);
    void servoCommandAccepted(int servoNumber, int pwm);
    void servoCommandRejected(int servoNumber, int pwm, const QString &reason);
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
    struct MissionItem { uint16_t seq = 0; uint8_t current = 0; uint8_t frame = 0; uint16_t command = 0; float param1 = 0; float param2 = 0; float param3 = 0; float param4 = 0; double x = 0; double y = 0; float z = 0; uint8_t autocontinue = 1; };
    struct VehicleParameter { QString name; float value = 0.0f; uint8_t type = MAV_PARAM_TYPE_REAL32; int index = -1; };
    struct PendingTerrainBlock { mavlink_terrain_request_t request = {}; uint8_t gridbit = 0; };

    void setConnected(bool connected); void setLink(ILink *newLink); void setMissionStatus(const QString &status); void setMissionUploaded(bool uploaded); void setUploadedHomeLocation(const QVariantMap &location); void resetParameterCache(); void requestMissingParameters(); void finishParameterFetch(bool complete); void handleParameterValue(const mavlink_message_t &msg); void handleRcChannels(const mavlink_message_t &msg); QString mavlinkParamIdToString(const char paramId[16]) const; QVariantMap parameterToVariantMap(const VehicleParameter &parameter) const; void requestDataStreams(); void sendCommandLong(uint16_t command, float param1 = 0, float param2 = 0, float param3 = 0, float param4 = 0, float param5 = 0, float param6 = 0, float param7 = 0); void sendSetMode(uint32_t customMode); bool sendParameterSet(const QString &name, float value, uint8_t type); void sendMavlinkMessage(mavlink_message_t &msg); void sendLinkPing(); void handleLinkPing(const mavlink_message_t &msg); void updateMavlinkQuality(const mavlink_message_t &msg); void resetLinkQuality(); bool shouldLogTelemetryMessage(const mavlink_message_t &msg) const; void logTelemetryMessage(const mavlink_message_t &msg); void writeTelemetryLogRecord(const char *packet, int packetLength); quint64 currentTelemetryLogTimestampUsec() const; QString defaultTelemetryLogDirectory() const; QString nextTelemetryLogPath() const; void logFlightControllerStatusText(const mavlink_message_t &msg); void appendFlightControllerLogEntry(const QString &level, uint8_t systemId, uint8_t componentId, const QString &text); bool writeFlightControllerLogLine(const QString &line); QString defaultFlightControllerLogDirectory() const; QString nextFlightControllerLogPath() const; QString mavSeverityText(uint8_t severity) const; void sendMotorTestCommand(const QString &mode, int motor, int throttlePercent, int durationSeconds, int motorCount = 0); void sendActuatorMotorTestCommand(const QString &mode, int motor, int throttlePercent, int durationSeconds); void sendActuatorMotorStopCommand(int motor); void handleTerrainRequest(const mavlink_message_t &msg); bool buildTerrainDataBlock(const mavlink_terrain_request_t &request, uint8_t gridbit, int16_t data[16]); bool sendTerrainDataBlock(const mavlink_terrain_request_t &request, uint8_t gridbit); void queuePendingTerrainBlock(const mavlink_terrain_request_t &request, uint8_t gridbit); void retryPendingTerrainRequests(); void clearPendingTerrainRequests(); void stopMotorTestSequence(); void runNextSequentialMotorTest(); void sendMissionClearAll(); void sendMissionCount(); void sendMissionItem(uint16_t seq, bool useInt); void requestMissionList(); void requestMissionItem(uint16_t seq); void sendMissionAck(uint8_t type); void handleMissionMessage(const mavlink_message_t &msg); QString saveLoadedMissionToMissionsFolder() const; bool parseMissionFile(const QString &path, QVector<MissionItem> &items, QVariantList &displayWaypoints, QVariantMap &homeLocation, QString &error) const; QVariantMap missionHomeLocation(const QVector<MissionItem> &items) const; QVariantList buildDisplayWaypoints(const QVector<MissionItem> &items) const; QVariantList buildElevationWaypoints(const QVector<MissionItem> &items) const; QVariantList buildMissionTerrainProfile(const QVariantList &profileWaypoints);

    struct PendingMotorTestAck { uint16_t command = 0; QString mode; int firstMotor = 0; int motorCount = 0; int durationSeconds = 0; int sequenceId = 0; };
    struct PendingServoAck { int servoNumber = 0; int pwm = 0; };

    QString linkIdentityValue;
    ILink *link = nullptr; class MavlinkInspectorController *inspectorController = nullptr; class MavlinkForwarder *forwarderObject = nullptr; QByteArray inboundFrameBytes; QStringList serialPortNames; mavlink_status_t mavlinkStatus = {}; bool connectedState = false; QTimer *linkPingTimer = nullptr; QElapsedTimer linkPingClock; quint64 lastPingTimestampUsec = 0; quint32 nextPingSequence = 0; int linkLatencyMsValue = -1; double mavlinkQualityValue = -1; QHash<quint16, uint8_t> lastMessageSequenceBySource; QVector<MissionItem> pendingMissionItems; QVariantList loadedMissionWaypoints; QVariantList loadedMissionElevationWaypoints; QVariantList loadedMissionTerrainProfile; QVariantMap loadedUploadedHomeLocation; QString missionStatusText; bool missionUploadedState = false; bool missionUploadActive = false; bool missionClearBeforeUploadActive = false; bool missionDownloadActive = false; bool missionDownloadRequestedOnConnect = false; bool missionSaveRequestedAfterDownload = false; uint16_t missionDownloadExpectedCount = 0; QVector<MissionItem> downloadingMissionItems; QSet<int> downloadedMissionItemSeqs; uint8_t targetSystemId = 1; uint8_t targetComponentId = MAV_COMP_ID_AUTOPILOT1; QTimer *motorTestSequenceTimer = nullptr; QTimer *motorTestStopTimer = nullptr; QVector<PendingMotorTestAck> pendingMotorTestAcks; QVector<PendingServoAck> pendingServoAcks; int motorTestSequenceNextMotor = 1; int motorTestSequenceMotorCount = 0; int motorTestSequenceThrottle = 10; int motorTestSequenceDuration = 2; int activeMotorTestStopCount = 0; int motorTestCommandGeneration = 0; int nextMotorTestSequenceId = 1; TerrainProvider terrainProvider; QVector<PendingTerrainBlock> pendingTerrainBlocks; QTimer *terrainRetryTimer = nullptr; QTimer *parameterFetchIdleTimer = nullptr; QTimer *missingParameterRetryTimer = nullptr; QFutureWatcher<QStringList> *serialPortRefreshWatcher = nullptr; bool serialPortRefreshActive = false; QHash<QString, VehicleParameter> parameterCache; QSet<int> receivedParameterIndices; int expectedParameterCount = -1; int parameterRetryCount = 0; bool parameterFetchActive = false; bool parameterRequestSent = false; QVariantList latestRcChannels; bool telemetryLoggingEnabledState = true; class BufferedLogWriter; std::unique_ptr<BufferedLogWriter> telemetryLogWriter; QString telemetryLogDirectoryPath; QString currentTelemetryLogFilePath; QElapsedTimer telemetryLogTimer; quint64 telemetryLogEpochUsec = 0; bool flightControllerLoggingEnabledState = true; std::unique_ptr<BufferedLogWriter> flightControllerLogWriter; QString flightControllerLogDirectoryPath; QString currentFlightControllerLogFilePath;
};
