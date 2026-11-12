#include "ParameterManager.h"
#include "common/mavlink.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <chrono>
#include <algorithm>

ParameterManager::ParameterManager(QObject *parent) : ParameterManager(parent, {}) {}

ParameterManager::ParameterManager(QObject *parent, std::function<qint64()> monotonicClock)
    : QObject(parent), clock(std::move(monotonicClock))
{
    if (!clock) clock = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
    };
    schedulerTimer.setSingleShot(true);
    schedulerTimer.setTimerType(Qt::PreciseTimer);
    writeTimer.setSingleShot(true);
    writeTimer.setTimerType(Qt::PreciseTimer);
}

ParameterManager::~ParameterManager() { connected = false; cancelScheduler(); cancelWrites(QStringLiteral("Parameter manager destroyed.")); }

bool ParameterManager::setRecoveryConfiguration(const RecoveryConfig &config)
{
    if (state == Loading || state == Recovering) return false;
    if (config.initialResponseTimeoutMs <= 0 || config.streamInactivityTimeoutMs <= 0
            || config.requestSpacingMs <= 0 || config.indexResponseTimeoutMs <= 0
            || config.maxListAttempts <= 0 || config.maxIndexAttempts <= 0
            || config.maxInFlight <= 0 || config.maxRequestsPerCycle <= 0
            || config.recoveryCycleIntervalMs <= 0 || config.recoveryTimeoutMs <= 0) return false;
    recoveryConfig = config;
    return true;
}

int ParameterManager::indexRequestAttempts(int index) const
{
    return index >= 0 && index < indexRetries.size() ? indexRetries[index].attempts : 0;
}

QList<int> ParameterManager::exhaustedParameterIndices() const
{
    QList<int> result;
    for (int index = 0; index < indexRetries.size(); ++index)
        if (indexRetries[index].exhausted && !receivedIndices.contains(index)) result.append(index);
    return result;
}

bool ParameterManager::parametersReady() const
{
    return connected && state == Ready && snapshotValid
            && snapshotConnection == connectionGeneration && snapshotTransfer == transferGeneration;
}

int ParameterManager::progress() const
{
    if (parametersReady()) return 100;
    return expectedCount > 0 ? qMin(99, int(100LL * receivedIndices.size() / expectedCount)) : 0;
}

QVariantMap ParameterManager::snapshotProvenance() const
{
    if (!snapshotValid) return {};
    return {{"systemId", snapshotSystem}, {"componentId", snapshotComponent},
            {"connectionGeneration", QVariant::fromValue(snapshotConnection)},
            {"transferGeneration", QVariant::fromValue(snapshotTransfer)},
            {"encoding", int(snapshotEncoding)}, {"current", parametersReady()},
            {"sourceConnectionGeneration", QVariant::fromValue(snapshotSourceConnectionGeneration)}};
}

void ParameterManager::clearStaging()
{
    expectedCount = 0;
    receivedIndices.clear();
    indexToName.clear();
    nameToIndex.clear();
    staging.clear();
    listAttempts = recoveryRequests = requestsThisCycle = recoveryCursor = 0;
    initialDeadline = streamDeadline = recoveryDeadline = nextRequestAt = cycleStart = 0;
    indexRetries.clear();
}

void ParameterManager::connectSource(quint8 system, quint8 component, Encoding mode, quint64 sourceGeneration)
{
    disconnectSource();
    if (!system || !component) return;
    connected = true;
    systemId = system;
    componentId = component;
    encoding = mode;
    sourceConnectionGeneration = sourceGeneration;
    emit statusChanged();
}

void ParameterManager::disconnectSource()
{
    if (!connected && state == Idle && writeTransactions.isEmpty() && staging.isEmpty()) return;
    cancelScheduler();
    ++connectionGeneration;
    ++transferGeneration;
    connected = false;
    systemId = componentId = 0;
    encoding = UnknownEncoding;
    state = Idle;
    error.clear();
    clearStaging();
    cancelWrites(QStringLiteral("Vehicle disconnected or target changed."));
    emit statusChanged();
    emit snapshotChanged();
    emit recoveryDiagnosticsChanged();
}

void ParameterManager::setEncoding(Encoding mode)
{
    if (encoding == mode) return;
    encoding = mode;
    if (connected && state != Idle) refresh();
}

bool ParameterManager::synchronizationActive(quint64 attempt) const
{
    return connected && attempt == transferGeneration && (state == Loading || state == Recovering);
}

void ParameterManager::cancelScheduler()
{
    schedulerTimer.stop();
    QObject::disconnect(schedulerConnection);
    ++wakeSerial;
    inFlight.clear();
}

int ParameterManager::nextMissingIndex() const
{
    for (int offset = 0; offset < expectedCount; ++offset) {
        const int index = (recoveryCursor + offset) % expectedCount;
        if (!receivedIndices.contains(index) && !inFlight.contains(index) && !indexRetries[index].exhausted)
            return index;
    }
    return -1;
}

void ParameterManager::scheduleWake()
{
    schedulerTimer.stop();
    QObject::disconnect(schedulerConnection);
    const auto serial = ++wakeSerial;
    const quint64 attempt = transferGeneration;
    if (!synchronizationActive(attempt)) return;
    const auto now = clock();
    qint64 due = expectedCount ? streamDeadline : initialDeadline;
    if (state == Recovering) {
        due = recoveryDeadline;
        for (int index : inFlight) due = qMin(due, indexRetries[index].deadline);
        if (inFlight.size() < recoveryConfig.maxInFlight && nextMissingIndex() >= 0) {
            auto requestDue = nextRequestAt;
            if (requestsThisCycle >= recoveryConfig.maxRequestsPerCycle)
                requestDue = qMax(requestDue, cycleStart + recoveryConfig.recoveryCycleIntervalMs);
            due = qMin(due, requestDue);
        }
    }
    schedulerConnection = connect(&schedulerTimer, &QTimer::timeout, this, [this, attempt, serial]() {
        if (serial == wakeSerial) processRecovery(attempt);
    });
    schedulerTimer.start(int(std::clamp<qint64>(due - now, 0, std::numeric_limits<int>::max())));
}

void ParameterManager::sendListRequest()
{
    const auto attempt = transferGeneration;
    ++listAttempts;
    initialDeadline = clock() + recoveryConfig.initialResponseTimeoutMs;
    scheduleWake();
    emit requestList(systemId, componentId, attempt);
    if (attempt == transferGeneration) emit recoveryDiagnosticsChanged();
}

void ParameterManager::processRecovery(quint64 attempt)
{
    if (!synchronizationActive(attempt)) return;
    const auto now = clock();
    if (state == Loading) {
        if (expectedCount) {
            if (now >= streamDeadline) { markRecovering(attempt); return; }
        } else if (now >= initialDeadline) {
            if (listAttempts >= recoveryConfig.maxListAttempts) {
                fail(attempt, QStringLiteral("No valid parameter response after %1 list requests.").arg(listAttempts));
            } else sendListRequest();
            return;
        }
        scheduleWake();
        return;
    }
    if (now >= recoveryDeadline) {
        fail(attempt, QStringLiteral("Parameter recovery deadline exceeded with %1 missing indices after %2 read requests.")
             .arg(missingParameterCount()).arg(recoveryRequests));
        return;
    }
    // Expire only outstanding requests. Each index owns its own attempt budget.
    const auto outstanding = inFlight.values();
    for (int index : outstanding) {
        if (now < indexRetries[index].deadline) continue;
        inFlight.remove(index);
        if (indexRetries[index].attempts >= recoveryConfig.maxIndexAttempts) {
            indexRetries[index].exhausted = true;
            emit indexRetryExhausted(index, indexRetries[index].attempts, attempt);
            if (!synchronizationActive(attempt)) return;
        }
    }
    const int candidate = nextMissingIndex();
    if (candidate < 0 && inFlight.isEmpty()) {
        const auto exhausted = exhaustedParameterIndices();
        fail(attempt, QStringLiteral("Parameter index retries exhausted: %1 missing indices; first exhausted index %2 (%3 attempts).")
             .arg(missingParameterCount()).arg(exhausted.isEmpty() ? -1 : exhausted.first())
             .arg(recoveryConfig.maxIndexAttempts));
        return;
    }
    if (now >= cycleStart + recoveryConfig.recoveryCycleIntervalMs) {
        cycleStart = now;
        requestsThisCycle = 0;
    }
    if (candidate >= 0 && inFlight.size() < recoveryConfig.maxInFlight && now >= nextRequestAt
            && requestsThisCycle < recoveryConfig.maxRequestsPerCycle) {
        auto &retry = indexRetries[candidate];
        ++retry.attempts;
        retry.deadline = now + recoveryConfig.indexResponseTimeoutMs;
        inFlight.insert(candidate);
        ++recoveryRequests;
        ++requestsThisCycle;
        recoveryCursor = (candidate + 1) % expectedCount;
        nextRequestAt = now + recoveryConfig.requestSpacingMs;
        scheduleWake(); // Install deadlines before observers can synchronously reply or refresh.
        emit requestRead(systemId, componentId, candidate, attempt);
        if (attempt == transferGeneration) emit recoveryDiagnosticsChanged();
        return; // Never catch up with a tight loop after a delayed event-loop wake.
    }
    scheduleWake();
}

void ParameterManager::refresh()
{
    if (!connected) return;
    ++transferGeneration;
    const auto attempt = transferGeneration;
    cancelScheduler();
    clearStaging();
    error.clear();
    state = Loading;
    cancelWrites(QStringLiteral("Parameter refresh invalidated the write generation."));
    if (!synchronizationActive(attempt)) return;
    emit statusChanged();
    if (!synchronizationActive(attempt)) return;
    emit snapshotChanged();
    if (!synchronizationActive(attempt)) return;
    if (encoding == UnknownEncoding) {
        fail(transferGeneration, QStringLiteral("Parameter encoding is unknown or unsupported."));
        return;
    }
    sendListRequest();
}

void ParameterManager::markRecovering(quint64 attempt)
{
    if (!connected || attempt != transferGeneration || state != Loading || expectedCount <= 0) return;
    state = Recovering;
    indexRetries.resize(expectedCount);
    const auto now = clock();
    recoveryDeadline = now + recoveryConfig.recoveryTimeoutMs;
    nextRequestAt = now + recoveryConfig.requestSpacingMs;
    cycleStart = now;
    scheduleWake();
    emit statusChanged();
    if (!synchronizationActive(attempt)) return;
    emit recoveryRequired(attempt);
    if (attempt == transferGeneration) emit recoveryDiagnosticsChanged();
}

void ParameterManager::fail(quint64 attempt, const QString &reason)
{
    if (!connected || attempt != transferGeneration || state == Idle || state == Failed) return;
    cancelScheduler();
    state = Failed;
    error = reason;
    cancelWrites(QStringLiteral("Parameter synchronization is no longer valid."));
    if (attempt != transferGeneration) return;
    const auto received = receivedParameterCount();
    const auto expected = expectedCount;
    emit statusChanged();
    if (attempt != transferGeneration) return;
    emit snapshotChanged();
    if (attempt != transferGeneration) return;
    emit recoveryDiagnosticsChanged();
    if (attempt == transferGeneration) emit fetchFinished(false, received, expected);
}

bool ParameterManager::validName(const QByteArray &bytes)
{
    if (bytes.isEmpty() || bytes.size() > 16) return false;
    for (unsigned char c : bytes) if (c < 33 || c > 126) return false;
    return true;
}

QString ParameterManager::identifier(const QByteArray &field)
{
    if (field.size() != 16) return {};
    const auto end = field.indexOf('\0');
    const QByteArray bytes = end < 0 ? field : field.left(end);
    return validName(bytes) ? QString::fromLatin1(bytes) : QString();
}

bool ParameterManager::decode(const Sample &sample, QVariant &value) const
{
    if (encoding == UnknownEncoding) return false;
    if (sample.type == MAV_PARAM_TYPE_REAL32) {
        if (!std::isfinite(sample.wireValue)) return false;
        value = double(sample.wireValue);
        return true;
    }
    double low = 0, high = 0;
    switch (sample.type) {
    case MAV_PARAM_TYPE_UINT8: high = 255; break;
    case MAV_PARAM_TYPE_INT8: low = -128; high = 127; break;
    case MAV_PARAM_TYPE_UINT16: high = 65535; break;
    case MAV_PARAM_TYPE_INT16: low = -32768; high = 32767; break;
    case MAV_PARAM_TYPE_UINT32: high = 4294967295.; break;
    case MAV_PARAM_TYPE_INT32: low = -2147483648.; high = 2147483647.; break;
    default: return false; // 64-bit values cannot fit the classic protocol's 4-byte payload.
    }
    if (encoding == NumericCast) {
        const double number = sample.wireValue;
        if (!std::isfinite(number) || std::trunc(number) != number || number < low || number > high) return false;
        value = number;
        return true;
    }
    quint32 bits;
    std::memcpy(&bits, &sample.wireValue, sizeof(bits));
    switch (sample.type) {
    case MAV_PARAM_TYPE_UINT8: value = double(quint8(bits)); break;
    case MAV_PARAM_TYPE_INT8: value = double(qint8(bits)); break;
    case MAV_PARAM_TYPE_UINT16: value = double(quint16(bits)); break;
    case MAV_PARAM_TYPE_INT16: value = double(qint16(bits)); break;
    case MAV_PARAM_TYPE_UINT32: value = double(bits); break;
    case MAV_PARAM_TYPE_INT32: value = double(qint32(bits)); break;
    default: return false;
    }
    return true;
}

QVariantMap ParameterManager::asMap(const Record &record)
{
    return {{"name", record.name}, {"value", record.value}, {"type", record.type}, {"index", record.index}};
}

QVariantMap ParameterManager::parameterValue(const QString &name) const
{
    const auto it = snapshot.constFind(name);
    return it == snapshot.cend() ? QVariantMap() : asMap(it.value());
}

QVariantMap ParameterManager::parameters() const
{
    QVariantMap result;
    for (const auto &record : snapshot) result.insert(record.name, asMap(record));
    return result;
}

void ParameterManager::receive(quint8 system, quint8 component, quint64 attempt, const Sample &sample)
{
    if (!connected || system != systemId || component != componentId || attempt != transferGeneration
            || (state != Loading && state != Recovering && state != Ready)) return;
    const QString name = identifier(sample.identifier);
    // PX4's optional hash is not a parameter value and its float bits may be NaN.
    if (sample.index == INT16_MAX && name == QLatin1String("_HASH_CHECK")) return;
    QVariant value;
    if (name.isEmpty() || !decode(sample, value)) {
        fail(attempt, QStringLiteral("Invalid parameter identifier, type or encoded value."));
        return;
    }
    if (sample.count <= 0 || sample.count > 32768 || (expectedCount && sample.count != expectedCount)) {
        fail(attempt, QStringLiteral("Invalid or changed parameter count; refresh required."));
        return;
    }
    // Unknown indices and PX4's optional hash response do not establish coverage.
    if (sample.index == UINT16_MAX) {
        if (state != Ready || !snapshot.contains(name) || snapshot.value(name).type != sample.type) return;
    } else {
        if (sample.index < 0 || sample.index >= sample.count) {
            fail(attempt, QStringLiteral("Parameter index is outside the advertised table."));
            return;
        }
        if ((indexToName.contains(sample.index) && indexToName.value(sample.index) != name)
                || (nameToIndex.contains(name) && nameToIndex.value(name) != sample.index)) {
            fail(attempt, QStringLiteral("Conflicting parameter index/name mapping; refresh required."));
            return;
        }
    }
    if (state == Ready) {
        auto it = snapshot.find(name);
        if (it == snapshot.end() || it->type != sample.type) {
            fail(attempt, QStringLiteral("Parameter table changed after synchronization; refresh required."));
            return;
        }
        it->value = value;
        const auto confirmed = asMap(it.value());
        const auto writeId = activeWriteNames.value(name);
        ++revision;
        // Direct Qt observers may refresh/disconnect or deliver a newer sample.
        // Re-read the record after each notification rather than retaining an
        // iterator across reentrant calls or publishing an obsolete value.
        const auto updateStillCurrent = [&] {
            const auto current = snapshot.constFind(name);
            return parametersReady() && attempt == transferGeneration
                    && current != snapshot.cend() && current->type == sample.type && current->value == value;
        };
        emit snapshotChanged();
        if (!updateStillCurrent()) return;
        emit parameterUpdated(name, confirmed);
        if (!updateStillCurrent()) return;
        if (writeId && writeTransactions.contains(writeId)) {
            const auto transaction = writeTransactions.value(writeId);
            if (transaction.generation == attempt && transaction.type == sample.type
                    && transaction.state == AwaitingResponse) {
                const bool matches = transaction.expected.toDouble() == value.toDouble();
                finishWrite(writeId, matches ? Confirmed : Rejected,
                            matches ? QString() : QStringLiteral("Vehicle reported a different value."), value);
            }
        }
        return;
    }
    if (staging.contains(name) && staging.value(name).type != sample.type) {
        fail(attempt, QStringLiteral("Parameter type changed during synchronization."));
        return;
    }
    expectedCount = sample.count;
    const bool newIndex = !receivedIndices.contains(sample.index);
    staging.insert(name, {name, value, sample.type, sample.index});
    receivedIndices.insert(sample.index);
    indexToName.insert(sample.index, name);
    nameToIndex.insert(name, sample.index);
    if (newIndex) {
        inFlight.remove(sample.index);
        if (state == Loading) streamDeadline = clock() + recoveryConfig.streamInactivityTimeoutMs;
        scheduleWake();
    }
    // Explicit coverage, independent of cache size and total messages received.
    bool complete = receivedIndices.size() == expectedCount;
    if (complete) {
        for (int index = 0; index < expectedCount; ++index)
            if (!receivedIndices.contains(index)) { complete = false; break; }
    }
    if (complete) publish();
    else if (newIndex) {
        emit statusChanged();
        if (attempt == transferGeneration) emit recoveryDiagnosticsChanged();
    }
}

void ParameterManager::publish()
{
    cancelScheduler();
    snapshot = std::move(staging);
    snapshotValid = true;
    snapshotSystem = systemId;
    snapshotComponent = componentId;
    snapshotConnection = connectionGeneration;
    snapshotSourceConnectionGeneration = sourceConnectionGeneration;
    snapshotTransfer = transferGeneration;
    snapshotEncoding = encoding;
    ++revision;
    state = Ready;
    error.clear();
    const auto published = snapshot;
    const auto attempt = transferGeneration;
    // The complete map and readiness are installed before any observer runs.
    emit statusChanged();
    if (attempt != transferGeneration || !parametersReady()) return;
    emit snapshotChanged();
    if (attempt != transferGeneration || !parametersReady()) return;
    emit snapshotPublished(revision);
    for (const auto &record : published) {
        if (attempt != transferGeneration || !parametersReady()) return;
        emit parameterUpdated(record.name, asMap(record));
    }
    if (attempt == transferGeneration && parametersReady()) emit fetchFinished(true, receivedParameterCount(), expectedCount);
}

bool ParameterManager::prepareWrite(const QString &name, double value, quint8 type, float &wireValue) const
{
    if (!connected || name.toLatin1() != name.toUtf8() || !validName(name.toLatin1()) || !std::isfinite(value)) return false;
    Sample sample;
    sample.type = type;
    if (encoding == NumericCast || type == MAV_PARAM_TYPE_REAL32) {
        wireValue = float(value);
        if (!std::isfinite(wireValue)) return false;
        if (type == MAV_PARAM_TYPE_REAL32 && value != 0 && wireValue == 0) return false;
        if (type != MAV_PARAM_TYPE_REAL32 && double(wireValue) != value) return false;
    } else if (encoding == Bytewise) {
        if (std::trunc(value) != value || value < -2147483648. || value > 4294967295.) return false;
        const quint32 bits = value < 0 ? quint32(qint32(value)) : quint32(value);
        std::memcpy(&wireValue, &bits, sizeof(bits));
    } else return false;
    sample.wireValue = wireValue;
    QVariant decoded;
    return decode(sample, decoded) && (type == MAV_PARAM_TYPE_REAL32 || decoded.toDouble() == value);
}

bool ParameterManager::setWriteConfiguration(const WriteConfig &config)
{
    if (!writeTransactions.isEmpty() || config.responseTimeoutMs <= 0 || config.requestSpacingMs <= 0
            || config.maxAttempts <= 0 || config.maxActive <= 0 || config.maxPending <= 0
            || config.queueTimeoutMs <= 0) return false;
    writeConfig = config;
    return true;
}

QString ParameterManager::validateWrite(const QString &name, double value, int type, float &wireValue) const
{
    if (!parametersReady()) return QStringLiteral("A current complete parameter snapshot is required for configuration writes.");
    if (type < 0 || type > 255 || !prepareWrite(name, value, quint8(type), wireValue))
        return QStringLiteral("Invalid parameter identifier, type, range or representability.");
    const auto record = snapshot.constFind(name);
    if (record == snapshot.cend()) return QStringLiteral("Parameter is absent from the confirmed table.");
    if (record->type != type) return QStringLiteral("Requested type differs from confirmed parameter metadata.");
    return {};
}

ParameterManager::WriteTransaction ParameterManager::makeWrite(quint64 id, const QString &name, double value,
                                                              int type, float wireValue, quint64 batch) const
{
    WriteTransaction transaction;
    transaction.id = id;
    transaction.connection = connectionGeneration;
    transaction.generation = transferGeneration;
    transaction.batch = batch;
    transaction.system = systemId;
    transaction.component = componentId;
    transaction.name = name;
    transaction.type = quint8(type);
    transaction.requested = value;
    transaction.wireValue = wireValue;
    transaction.queuedDeadline = clock() + writeConfig.queueTimeoutMs;
    Sample encoded;
    encoded.type = transaction.type;
    encoded.wireValue = wireValue;
    decode(encoded, transaction.expected);
    return transaction;
}

QVariantMap ParameterManager::writeResult(const WriteTransaction &t, WriteOutcome outcome, const QString &reason,
                                         const QVariant &confirmed) const
{
    static const char *names[] = {"Confirmed", "Rejected", "TimedOut", "Cancelled", "InvalidRequest", "TransportError"};
    return {{"id", QVariant::fromValue(t.id)}, {"parameter", t.name}, {"outcome", int(outcome)},
            {"outcomeName", QString::fromLatin1(names[outcome])}, {"reason", reason}, {"requestedValue", t.requested},
            {"confirmedValue", confirmed}, {"hasConfirmedValue", confirmed.isValid()}, {"attempts", t.attempts},
            {"generation", QVariant::fromValue(t.generation)}, {"connectionGeneration", QVariant::fromValue(t.connection)},
            {"systemId", t.system}, {"componentId", t.component}, {"type", t.type}};
}

QVariantMap ParameterManager::requestWrite(const QString &name, double value, int type)
{
    const auto id = ++nextWriteId;
    float wire = 0;
    auto reason = validateWrite(name, value, type, wire);
    if (reason.isEmpty() && writeTransactions.size() >= writeConfig.maxPending) reason = QStringLiteral("Parameter write queue is full.");
    const auto transaction = makeWrite(id, name, value, type, wire);
    if (!reason.isEmpty()) {
        const auto result = writeResult(transaction, InvalidRequest, reason);
        QTimer::singleShot(0, this, [this, result] { emit writeFinished(result); });
    } else {
        writeTransactions.insert(id, transaction);
        writeOrder.append(id);
        scheduleWrites(); // Initial send is deferred until callers can retain the local ID.
    }
    return {{"id", QVariant::fromValue(id)}, {"accepted", reason.isEmpty()}, {"reason", reason}};
}

QVariantMap ParameterManager::requestWriteBatch(const QVariantList &requests)
{
    const auto batchId = ++nextBatchId;
    QString reason;
    QList<WriteTransaction> transactions;
    QVariantList invalidResults;
    QHash<quint64, QString> validationErrors;
    QSet<QString> names;
    if (requests.isEmpty() || requests.size() > writeConfig.maxPending
            || requests.size() + writeTransactions.size() > writeConfig.maxPending)
        reason = QStringLiteral("Empty parameter batch or insufficient write queue capacity.");
    if (reason.isEmpty()) for (const auto &request : requests) {
        const auto map = request.toMap();
        const auto name = map.value("name").toString();
        bool valueOk = false, typeOk = false;
        const auto value = map.value("value").toDouble(&valueOk);
        const auto type = map.value("type").toInt(&typeOk);
        if (map.value("type").toDouble() != type) typeOk = false;
        float wire = 0;
        auto validation = map.value("validationError").toString();
        if (validation.isEmpty()) validation = validateWrite(name, value, type, wire);
        if (!valueOk || !typeOk || names.contains(name)) validation = QStringLiteral("Invalid or duplicate batch entry.");
        names.insert(name);
        auto transaction = makeWrite(++nextWriteId, name, value, type, wire, batchId);
        transactions.append(transaction);
        if (!validation.isEmpty()) {
            reason = QStringLiteral("%1: %2").arg(name, validation);
            invalidResults.append(writeResult(transaction, InvalidRequest, validation));
            validationErrors.insert(transaction.id, validation);
        }
    }
    if (!reason.isEmpty()) {
        invalidResults.clear();
        for (const auto &transaction : transactions) {
            const auto validation = validationErrors.value(transaction.id);
            invalidResults.append(writeResult(transaction, validation.isEmpty() ? Cancelled : InvalidRequest,
                validation.isEmpty() ? QStringLiteral("Batch rejected before any transmission.") : validation));
        }
        const QVariantMap result{{"id", QVariant::fromValue(batchId)}, {"success", false}, {"status", "InvalidRequest"},
                                 {"reason", reason}, {"results", invalidResults}, {"confirmedCount", 0}, {"totalCount", requests.size()}};
        QTimer::singleShot(0, this, [this, result] { emit writeBatchFinished(result); });
    } else {
        WriteBatch batch;
        for (const auto &transaction : transactions) {
            writeTransactions.insert(transaction.id, transaction);
            writeOrder.append(transaction.id);
            batch.ids.append(transaction.id);
        }
        writeBatches.insert(batchId, batch);
        scheduleWrites();
    }
    return {{"id", QVariant::fromValue(batchId)}, {"accepted", reason.isEmpty()}, {"reason", reason}};
}

void ParameterManager::stopWriteTimer()
{
    writeTimer.stop();
    QObject::disconnect(writeTimerConnection);
    ++writeWakeSerial;
}

void ParameterManager::cancelWrites(const QString &reason)
{
    stopWriteTimer();
    const auto ids = writeOrder;
    for (auto id : ids) finishWrite(id, Cancelled, reason);
    nameAvailableAt.clear();
    nextWriteAt = 0;
}

void ParameterManager::finishWrite(quint64 id, WriteOutcome outcome, const QString &reason, const QVariant &confirmed)
{
    const auto it = writeTransactions.find(id);
    if (it == writeTransactions.end()) return;
    const auto transaction = it.value();
    writeTransactions.erase(it);
    writeOrder.removeOne(id);
    if (activeWriteNames.value(transaction.name) == id) activeWriteNames.remove(transaction.name);
    // A quiet interval reduces duplicate-echo confusion between same-name writes.
    // There is no wire transaction ID, so this cannot prove causality of an echo.
    if (transaction.attempts) nameAvailableAt.insert(transaction.name, clock() + writeConfig.responseTimeoutMs);
    const auto result = writeResult(transaction, outcome, reason, confirmed);
    QVariantMap aggregate;
    auto batch = writeBatches.find(transaction.batch);
    if (batch != writeBatches.end()) {
        batch->results.insert(id, result);
        if (batch->results.size() == batch->ids.size()) {
            QVariantList results;
            int confirmations = 0, cancellations = 0;
            QStringList failures;
            for (auto child : batch->ids) {
                const auto item = batch->results.value(child);
                results.append(item);
                confirmations += item.value("outcome").toInt() == Confirmed;
                cancellations += item.value("outcome").toInt() == Cancelled;
                if (item.value("outcome").toInt() != Confirmed)
                    failures.append(item.value("parameter").toString() + ": " + item.value("outcomeName").toString() + " (" + item.value("reason").toString() + ")");
            }
            aggregate = {{"id", QVariant::fromValue(transaction.batch)}, {"success", confirmations == results.size()},
                         {"status", confirmations == results.size() ? "Confirmed" : cancellations == results.size() ? "Cancelled" : "PartialFailure"},
                         {"confirmedCount", confirmations}, {"totalCount", results.size()}, {"results", results}, {"reason", failures.join("; ")}};
            writeBatches.erase(batch);
        }
    }
    scheduleWrites();
    emit writeFinished(result);
    if (!aggregate.isEmpty()) emit writeBatchFinished(aggregate);
}

void ParameterManager::scheduleWrites()
{
    stopWriteTimer();
    if (!parametersReady() || writeTransactions.isEmpty()) return;
    const auto now = clock();
    qint64 due = std::numeric_limits<qint64>::max();
    QSet<QString> queuedNames;
    for (auto id : writeOrder) {
        const auto &transaction = writeTransactions[id];
        if (transaction.state == AwaitingResponse)
            due = qMin(due, transaction.attempts < writeConfig.maxAttempts ? qMax(transaction.deadline, nextWriteAt) : transaction.deadline);
        else {
            due = qMin(due, transaction.queuedDeadline);
            if (!queuedNames.contains(transaction.name) && !activeWriteNames.contains(transaction.name)
                    && activeWriteNames.size() < writeConfig.maxActive)
                due = qMin(due, qMax(nextWriteAt, nameAvailableAt.value(transaction.name, now)));
            queuedNames.insert(transaction.name);
        }
    }
    const auto attempt = transferGeneration, serial = writeWakeSerial;
    writeTimerConnection = connect(&writeTimer, &QTimer::timeout, this, [this, attempt, serial] {
        if (serial == writeWakeSerial) processWrites(attempt);
    });
    writeTimer.start(int(std::clamp<qint64>(due - now, 0, std::numeric_limits<int>::max())));
}

bool ParameterManager::writeTransmissionCurrent(quint64 id, quint64 attempt, int number) const
{
    const auto it = writeTransactions.constFind(id);
    return parametersReady() && attempt == transferGeneration && it != writeTransactions.cend()
            && it->generation == attempt && it->connection == connectionGeneration
            && it->state == AwaitingResponse && it->attempts == number;
}

void ParameterManager::writeTransportResult(quint64 id, quint64 attempt, int number, bool accepted)
{
    if (!writeTransmissionCurrent(id, attempt, number)) return;
    if (!accepted) finishWrite(id, TransportError, QStringLiteral("Transport did not accept the complete PARAM_SET frame."));
}

void ParameterManager::processWrites(quint64 attempt)
{
    if (!parametersReady() || attempt != transferGeneration) return;
    const auto now = clock();
    const auto ids = writeOrder;
    for (auto id : ids) {
        if (!writeTransactions.contains(id)) continue;
        auto &transaction = writeTransactions[id];
        if (transaction.state == Queued && now >= transaction.queuedDeadline) {
            finishWrite(id, TimedOut, QStringLiteral("Parameter write queue deadline exceeded."));
        } else if (transaction.state == AwaitingResponse && now >= transaction.deadline
                   && transaction.attempts >= writeConfig.maxAttempts) {
            finishWrite(id, TimedOut, QStringLiteral("No matching response after %1 attempts.").arg(transaction.attempts));
        }
        if (!parametersReady() || attempt != transferGeneration) return;
    }
    if (now < nextWriteAt) { scheduleWrites(); return; }
    for (auto id : writeOrder) {
        auto &transaction = writeTransactions[id];
        if (transaction.state == AwaitingResponse && now < transaction.deadline) continue;
        if (transaction.state == Queued && (activeWriteNames.contains(transaction.name)
                || activeWriteNames.size() >= writeConfig.maxActive || now < nameAvailableAt.value(transaction.name, now))) continue;
        transaction.state = AwaitingResponse;
        ++transaction.attempts;
        transaction.deadline = now + writeConfig.responseTimeoutMs;
        activeWriteNames.insert(transaction.name, id);
        nextWriteAt = now + writeConfig.requestSpacingMs;
        const auto sending = transaction;
        scheduleWrites();
        emit requestSet(id, sending.system, sending.component, sending.name, sending.wireValue, sending.type, attempt, sending.attempts);
        return;
    }
    scheduleWrites();
}
