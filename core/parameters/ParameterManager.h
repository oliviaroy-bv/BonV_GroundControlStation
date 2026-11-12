#pragma once

#include <QObject>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>
#include <QVector>
#include <functional>

// Owns synchronization and confirmed data. The transport decodes MAVLink messages
// into Sample and tags delivery with the current local generation. MAVLink has no
// wire transfer identifier: same-source delayed packets cannot be distinguished.
class ParameterManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SyncState syncState READ syncState NOTIFY statusChanged)
    Q_PROPERTY(bool parametersReady READ parametersReady NOTIFY statusChanged)
    Q_PROPERTY(bool hasCompleteSnapshot READ hasCompleteSnapshot NOTIFY statusChanged)
    Q_PROPERTY(int receivedParameterCount READ receivedParameterCount NOTIFY statusChanged)
    Q_PROPERTY(int expectedParameterCount READ expectedParameterCount NOTIFY statusChanged)
    Q_PROPERTY(QString syncError READ syncError NOTIFY statusChanged)
    Q_PROPERTY(int progress READ progress NOTIFY statusChanged)
    Q_PROPERTY(quint64 snapshotRevision READ snapshotRevision NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantMap snapshotProvenance READ snapshotProvenance NOTIFY snapshotChanged)
public:
    enum SyncState { Idle, Loading, Recovering, Ready, Failed };
    Q_ENUM(SyncState)
    enum Encoding { UnknownEncoding, NumericCast, Bytewise };
    Q_ENUM(Encoding)
    struct Sample {
        QByteArray identifier; // Exactly the 16-byte wire field.
        float wireValue = 0;
        quint8 type = 0;
        int index = -1;
        int count = 0;
    };
    struct RecoveryConfig {
        int initialResponseTimeoutMs = 3000;
        int streamInactivityTimeoutMs = 1800;
        int requestSpacingMs = 40;
        int indexResponseTimeoutMs = 1000;
        int maxListAttempts = 3; // Includes the first transmission.
        int maxIndexAttempts = 4;
        int maxInFlight = 8;
        int maxRequestsPerCycle = 8;
        int recoveryCycleIntervalMs = 200;
        int recoveryTimeoutMs = 180000;
    };

    explicit ParameterManager(QObject *parent = nullptr);
    // Optional monotonic clock enables deterministic scheduler tests without sleeps.
    ParameterManager(QObject *parent, std::function<qint64()> monotonicClock);
    ~ParameterManager() override;
    SyncState syncState() const { return state; }
    bool parametersReady() const;
    bool hasCompleteSnapshot() const { return snapshotValid; }
    int receivedParameterCount() const { return receivedIndices.size(); }
    int expectedParameterCount() const { return expectedCount; }
    QString syncError() const { return error; }
    int progress() const;
    quint64 generation() const { return transferGeneration; }
    quint64 snapshotRevision() const { return revision; }
    QVariantMap snapshotProvenance() const;
    QVariantMap parameterValue(const QString &name) const;
    QVariantMap parameters() const;
    int pendingWriteCount() const { return writeTransactions.size(); }
    enum WriteOutcome { Confirmed, Rejected, TimedOut, Cancelled, InvalidRequest, TransportError };
    Q_ENUM(WriteOutcome)
    struct WriteConfig {
        int responseTimeoutMs = 1000;
        int requestSpacingMs = 40;
        int maxAttempts = 3;
        int maxActive = 4;
        int maxPending = 128;
        int queueTimeoutMs = 60000;
    };
    const WriteConfig &writeConfiguration() const { return writeConfig; }
    bool setWriteConfiguration(const WriteConfig &config);
    Q_INVOKABLE QVariantMap requestWrite(const QString &name, double value, int type);
    Q_INVOKABLE QVariantMap requestWriteBatch(const QVariantList &requests);
    void processWrites(quint64 generation);
    void writeTransportResult(quint64 id, quint64 generation, int attempt, bool accepted);
    bool writeTransmissionCurrent(quint64 id, quint64 generation, int attempt) const;
    const RecoveryConfig &recoveryConfiguration() const { return recoveryConfig; }
    bool setRecoveryConfiguration(const RecoveryConfig &config);
    int listRequestAttempts() const { return listAttempts; }
    int recoveryRequestCount() const { return recoveryRequests; }
    int missingParameterCount() const { return expectedCount - receivedParameterCount(); }
    bool parameterIndexMissing(int index) const {
        return index >= 0 && index < expectedCount && !receivedIndices.contains(index);
    }
    int indexRequestAttempts(int index) const;
    QList<int> exhaustedParameterIndices() const;
    int inFlightRequestCount() const { return inFlight.size(); }
    // Drives at most one paced transmission. Normal execution uses the owned timer.
    // Calls tagged with a previous local generation have no effect.
    void processRecovery(quint64 generation);

    void connectSource(quint8 system, quint8 component, Encoding encoding, quint64 sourceConnectionGeneration = 0);
    void disconnectSource();
    void setEncoding(Encoding encoding);
    Q_INVOKABLE void refresh();
    void receive(quint8 system, quint8 component, quint64 generation, const Sample &sample);
    void markRecovering(quint64 generation);
    void fail(quint64 generation, const QString &reason);
    // Preparation validates and encodes without modifying confirmed data.
    bool prepareWrite(const QString &name, double value, quint8 type, float &wireValue) const;

signals:
    void statusChanged();
    void snapshotChanged();
    void snapshotPublished(quint64 revision);
    void parameterUpdated(const QString &name, const QVariantMap &parameter);
    void fetchFinished(bool complete, int received, int expected);
    void requestList(quint8 system, quint8 component, quint64 generation);
    void requestRead(quint8 system, quint8 component, int index, quint64 generation);
    void recoveryRequired(quint64 generation);
    void recoveryDiagnosticsChanged();
    void indexRetryExhausted(int index, int attempts, quint64 generation);
    void requestSet(quint64 id, quint8 system, quint8 component, QString name,
                    float wireValue, quint8 type, quint64 generation, int attempt);
    void writeFinished(const QVariantMap &result);
    void writeBatchFinished(const QVariantMap &result);

private:
    struct Record { QString name; QVariant value; quint8 type = 0; int index = -1; };
    static bool validName(const QByteArray &bytes);
    static QString identifier(const QByteArray &field);
    bool decode(const Sample &sample, QVariant &value) const;
    static QVariantMap asMap(const Record &record);
    void cancelScheduler();
    void scheduleWake();
    void sendListRequest();
    bool synchronizationActive(quint64 generation) const;
    int nextMissingIndex() const;
    void publish();
    void clearStaging();

    SyncState state = Idle;
    Encoding encoding = UnknownEncoding;
    bool connected = false;
    quint8 systemId = 0, componentId = 0;
    quint64 connectionGeneration = 0, transferGeneration = 0;
    quint64 sourceConnectionGeneration = 0, snapshotSourceConnectionGeneration = 0;
    int expectedCount = 0;
    QSet<int> receivedIndices;
    QHash<int, QString> indexToName;
    QHash<QString, int> nameToIndex;
    QHash<QString, Record> staging;
    QHash<QString, Record> snapshot;
    enum WriteState { Queued, AwaitingResponse };
    struct WriteTransaction {
        quint64 id = 0, connection = 0, generation = 0, batch = 0;
        quint8 system = 0, component = 0, type = 0;
        QString name;
        double requested = 0;
        QVariant expected;
        float wireValue = 0;
        int attempts = 0;
        WriteState state = Queued;
        qint64 deadline = 0, queuedDeadline = 0;
    };
    struct WriteBatch { QList<quint64> ids; QHash<quint64, QVariantMap> results; };
    QString validateWrite(const QString &name, double value, int type, float &wireValue) const;
    WriteTransaction makeWrite(quint64 id, const QString &name, double value, int type, float wireValue, quint64 batch = 0) const;
    QVariantMap writeResult(const WriteTransaction &transaction, WriteOutcome outcome, const QString &reason,
                            const QVariant &confirmed = {}) const;
    void finishWrite(quint64 id, WriteOutcome outcome, const QString &reason, const QVariant &confirmed = {});
    void cancelWrites(const QString &reason);
    void scheduleWrites();
    void stopWriteTimer();
    QHash<quint64, WriteTransaction> writeTransactions;
    QList<quint64> writeOrder;
    QHash<QString, quint64> activeWriteNames;
    QHash<QString, qint64> nameAvailableAt;
    QHash<quint64, WriteBatch> writeBatches;
    WriteConfig writeConfig;
    quint64 nextWriteId = 0, nextBatchId = 0, writeWakeSerial = 0;
    qint64 nextWriteAt = 0;
    QTimer writeTimer;
    QMetaObject::Connection writeTimerConnection;
    bool snapshotValid = false;
    quint64 revision = 0, snapshotConnection = 0, snapshotTransfer = 0;
    quint8 snapshotSystem = 0, snapshotComponent = 0;
    Encoding snapshotEncoding = UnknownEncoding;
    QString error;
    struct IndexRetry { int attempts = 0; qint64 deadline = 0; bool exhausted = false; };
    RecoveryConfig recoveryConfig;
    std::function<qint64()> clock;
    QTimer schedulerTimer;
    QMetaObject::Connection schedulerConnection;
    quint64 wakeSerial = 0;
    qint64 initialDeadline = 0, streamDeadline = 0, recoveryDeadline = 0;
    qint64 nextRequestAt = 0, cycleStart = 0;
    int listAttempts = 0, recoveryRequests = 0, requestsThisCycle = 0, recoveryCursor = 0;
    QVector<IndexRetry> indexRetries;
    QSet<int> inFlight;
};
