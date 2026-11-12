#include <QtTest/QtTest>
#include <QSignalSpy>
#include <cstring>
#include <limits>
#include <QElapsedTimer>
#include <memory>
#include "parameters/ParameterManager.h"
#include "common/mavlink.h"

namespace {
ParameterManager::Sample sample(const QByteArray &name, int index, int count = 3,
                                float value = 1, quint8 type = MAV_PARAM_TYPE_REAL32)
{
    auto id = name;
    id.resize(16, '\0');
    return {id, value, type, index, count};
}
void start(ParameterManager &manager, ParameterManager::Encoding encoding = ParameterManager::NumericCast)
{
    manager.connectSource(42, 1, encoding);
    manager.refresh();
}
void deliver(ParameterManager &manager, const ParameterManager::Sample &value)
{
    manager.receive(42, 1, manager.generation(), value);
}
void complete(ParameterManager &manager)
{
    deliver(manager, sample("A", 0));
    deliver(manager, sample("B", 1));
    deliver(manager, sample("C", 2));
}
struct RecoveryHarness {
    qint64 now = 0;
    ParameterManager manager{nullptr, [this] { return now; }};
    ParameterManager::RecoveryConfig config;
    RecoveryHarness() {
        config.initialResponseTimeoutMs = 100;
        config.streamInactivityTimeoutMs = 50;
        config.requestSpacingMs = 10;
        config.indexResponseTimeoutMs = 30;
        config.maxListAttempts = 3;
        config.maxIndexAttempts = 2;
        config.maxInFlight = 2;
        config.maxRequestsPerCycle = 2;
        config.recoveryCycleIntervalMs = 40;
        config.recoveryTimeoutMs = 1000;
        configure();
    }
    void configure() {
        if (!manager.setRecoveryConfiguration(config)) qFatal("Invalid test recovery configuration");
    }
    void tick(qint64 milliseconds) {
        now += milliseconds;
        manager.processRecovery(manager.generation());
    }
    void beginMissing(int count = 3) {
        start(manager);
        deliver(manager, sample("A", 0, count));
        tick(config.streamInactivityTimeoutMs);
    }
};
}

class ParameterManagerTest : public QObject
{
    Q_OBJECT
private slots:
    void liveUpdateObserverInvalidation_data()
    {
        QTest::addColumn<bool>("refresh");
        QTest::newRow("snapshot-observer-refresh") << true;
        QTest::newRow("snapshot-observer-newer-value") << false;
    }
    void liveUpdateObserverInvalidation()
    {
        QFETCH(bool, refresh);
        ParameterManager manager; start(manager); complete(manager);
        QSignalSpy updates(&manager, &ParameterManager::parameterUpdated);
        bool observed = false;
        connect(&manager, &ParameterManager::snapshotChanged, this, [&] {
            if (observed) return;
            observed = true;
            if (refresh) manager.refresh();
            else deliver(manager, sample("A", 0, 3, 7));
        });
        deliver(manager, sample("A", 0, 3, 2));
        QCOMPARE(updates.size(), refresh ? 0 : 1);
        if (!refresh) {
            QCOMPARE(updates.last()[1].toMap().value("value").toDouble(), 7.);
            QCOMPARE(manager.parameterValue("A").value("value").toDouble(), 7.);
        } else QCOMPARE(manager.syncState(), ParameterManager::Loading);
    }

    void confirmationAtTimeoutBoundary_data()
    {
        QTest::addColumn<bool>("responseFirst");
        QTest::newRow("response-before-timeout") << true;
        QTest::newRow("timeout-before-response") << false;
    }
    void confirmationAtTimeoutBoundary()
    {
        QFETCH(bool, responseFirst);
        RecoveryHarness h; start(h.manager); complete(h.manager);
        auto config = h.manager.writeConfiguration(); config.responseTimeoutMs = 30; config.maxAttempts = 1;
        QVERIFY(h.manager.setWriteConfiguration(config));
        QSignalSpy results(&h.manager, &ParameterManager::writeFinished);
        QSignalSpy transmissions(&h.manager, &ParameterManager::requestSet);
        QVERIFY(h.manager.requestWrite("A", 2, MAV_PARAM_TYPE_REAL32).value("accepted").toBool());
        h.manager.processWrites(h.manager.generation());
        QCOMPARE(h.manager.parameterValue("A").value("value").toDouble(), 1.);
        h.now = 30;
        if (responseFirst) deliver(h.manager, sample("A", 0, 3, 2));
        h.manager.processWrites(h.manager.generation());
        if (!responseFirst) deliver(h.manager, sample("A", 0, 3, 2));
        deliver(h.manager, sample("A", 0, 3, 2)); // Duplicate/late echo never completes twice.
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0][0].toMap().value("outcome").toInt(), int(responseFirst ? ParameterManager::Confirmed : ParameterManager::TimedOut));
        QCOMPARE(transmissions.size(), 1);
        QCOMPARE(h.manager.receivedParameterCount(), 3);
        QCOMPARE(h.manager.parameterValue("A").value("value").toDouble(), 2.);
    }
    void unrelatedReentrantUpdateStillConfirmsWrite()
    {
        ParameterManager manager; start(manager); complete(manager);
        QSignalSpy results(&manager, &ParameterManager::writeFinished);
        QVERIFY(manager.requestWrite("A", 2, MAV_PARAM_TYPE_REAL32).value("accepted").toBool());
        manager.processWrites(manager.generation());
        bool nested = false;
        connect(&manager, &ParameterManager::snapshotChanged, this, [&] {
            if (!nested) { nested = true; deliver(manager, sample("B", 1, 3, 7)); }
        });
        deliver(manager, sample("A", 0, 3, 2));
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0][0].toMap().value("outcome").toInt(), int(ParameterManager::Confirmed));
        QCOMPARE(manager.parameterValue("B").value("value").toDouble(), 7.);
    }

    void download_data()
    {
        QTest::addColumn<QList<int>>("order");
        QTest::newRow("ordered") << QList<int>{0, 1, 2};
        QTest::newRow("out-of-order") << QList<int>{2, 0, 1};
        QTest::newRow("duplicates") << QList<int>{1, 1, 0, 0, 2};
    }
    void download()
    {
        QFETCH(QList<int>, order);
        ParameterManager manager;
        start(manager);
        QSignalSpy published(&manager, &ParameterManager::snapshotPublished);
        QSignalSpy finished(&manager, &ParameterManager::fetchFinished);
        for (int index : order) {
            deliver(manager, sample(QByteArray(1, char('A' + index)), index));
            if (manager.receivedParameterCount() < 3) {
                QVERIFY(!manager.parametersReady());
                QVERIFY(manager.parameters().isEmpty());
                QVERIFY(!manager.hasCompleteSnapshot());
                QVERIFY(manager.progress() < 100);
            }
        }
        QCOMPARE(manager.syncState(), ParameterManager::Ready);
        QCOMPARE(manager.progress(), 100);
        QCOMPARE(manager.parameters().size(), 3);
        QCOMPARE(published.size(), 1);
        QCOMPARE(finished.size(), 1);
        QVERIFY(finished[0][0].toBool());
        QVERIFY(manager.snapshotProvenance()["current"].toBool());
    }
    void invalidTable_data()
    {
        QTest::addColumn<QByteArray>("name");
        QTest::addColumn<int>("index");
        QTest::addColumn<int>("count");
        QTest::newRow("index-conflict") << QByteArray("B") << 0 << 3;
        QTest::newRow("name-conflict") << QByteArray("A") << 1 << 3;
        QTest::newRow("out-of-range") << QByteArray("B") << 3 << 3;
        QTest::newRow("negative") << QByteArray("B") << -1 << 3;
        QTest::newRow("count-change") << QByteArray("B") << 1 << 2;
        QTest::newRow("zero-count") << QByteArray("B") << 1 << 0;
        QTest::newRow("unsupported-count") << QByteArray("B") << 1 << 32769;
        QTest::newRow("empty-id") << QByteArray() << 1 << 3;
        QTest::newRow("space-not-renamed") << QByteArray(" B") << 1 << 3;
    }
    void invalidTable()
    {
        QFETCH(QByteArray, name); QFETCH(int, index); QFETCH(int, count);
        ParameterManager manager;
        start(manager);
        deliver(manager, sample("A", 0));
        deliver(manager, sample(name, index, count));
        QCOMPARE(manager.syncState(), ParameterManager::Failed);
        QVERIFY(!manager.syncError().isEmpty());
        QVERIFY(!manager.hasCompleteSnapshot());
        QVERIFY(manager.parameters().isEmpty());
    }
    void unknownIndexAndHash()
    {
        ParameterManager manager;
        start(manager);
        deliver(manager, sample("A", UINT16_MAX));
        deliver(manager, sample("_HASH_CHECK", INT16_MAX, 3, std::numeric_limits<float>::quiet_NaN(), MAV_PARAM_TYPE_UINT32));
        QCOMPARE(manager.receivedParameterCount(), 0);
        QCOMPARE(manager.expectedParameterCount(), 0);
        QCOMPARE(manager.syncState(), ParameterManager::Loading);
        complete(manager);
        deliver(manager, sample("A", UINT16_MAX, 3, 9));
        QCOMPARE(manager.syncState(), ParameterManager::Ready);
        QCOMPARE(manager.parameterValue("A")["value"].toDouble(), 9.);
    }
    void wrongSourceAndGeneration()
    {
        ParameterManager manager;
        start(manager);
        const auto old = manager.generation();
        manager.receive(43, 1, old, sample("A", 0));
        manager.receive(42, 2, old, sample("A", 0));
        QCOMPARE(manager.receivedParameterCount(), 0);
        manager.refresh();
        manager.receive(42, 1, old, sample("A", 0));
        manager.markRecovering(old);
        manager.fail(old, "stale callback");
        QCOMPARE(manager.syncState(), ParameterManager::Loading);
        QCOMPARE(manager.receivedParameterCount(), 0);
        QVERIFY(manager.syncError().isEmpty());
    }
    void failedRefreshAndReconnect()
    {
        ParameterManager manager;
        start(manager); complete(manager);
        const auto before = manager.parameters();
        const auto revision = manager.snapshotRevision();
        manager.refresh();
        deliver(manager, sample("A", 0, 3, 99));
        QCOMPARE(manager.parameters(), before);
        QVERIFY(!manager.parametersReady());
        manager.fail(manager.generation(), "lost response");
        QVERIFY(manager.hasCompleteSnapshot());
        QCOMPARE(manager.parameters(), before);
        QCOMPARE(manager.snapshotRevision(), revision);
        QVERIFY(!manager.snapshotProvenance()["current"].toBool());
        manager.disconnectSource();
        start(manager);
        QVERIFY(manager.hasCompleteSnapshot());
        QVERIFY(!manager.parametersReady());
        QCOMPARE(manager.parameters(), before);
        complete(manager);
        QVERIFY(manager.parametersReady());
        QVERIFY(manager.snapshotRevision() > revision);
    }
    void writesDoNotPublish()
    {
        ParameterManager manager;
        start(manager);
        deliver(manager, sample("A", 0));
        deliver(manager, sample("B", 1));
        float wire = 0;
        QVERIFY(manager.prepareWrite("C", 10, MAV_PARAM_TYPE_REAL32, wire));
        QVERIFY(!manager.requestWrite("C", 10, MAV_PARAM_TYPE_REAL32).value("accepted").toBool());
        QCOMPARE(manager.pendingWriteCount(), 0);
        QCOMPARE(manager.receivedParameterCount(), 2);
        QVERIFY(!manager.parametersReady());
        QVERIFY(manager.parameters().isEmpty());
        deliver(manager, sample("C", 2, 3, 8)); // Genuine vehicle response may fill coverage.
        QVERIFY(manager.parametersReady());
        QCOMPARE(manager.parameterValue("C")["value"].toDouble(), 8.);
        QVERIFY(manager.requestWrite("C", 10, MAV_PARAM_TYPE_REAL32).value("accepted").toBool());
        manager.processWrites(manager.generation());
        QCOMPARE(manager.parameterValue("C")["value"].toDouble(), 8.);
        deliver(manager, sample("C", 2, 3, 10));
        QCOMPARE(manager.parameterValue("C")["value"].toDouble(), 10.);
        QCOMPARE(manager.pendingWriteCount(), 0);
    }
    void liveUpdatesAndChangedTable()
    {
        ParameterManager manager;
        start(manager); complete(manager);
        QSignalSpy published(&manager, &ParameterManager::snapshotPublished);
        QSignalSpy finished(&manager, &ParameterManager::fetchFinished);
        const auto generation = manager.generation();
        deliver(manager, sample("B", 1, 3, 22));
        QCOMPARE(manager.generation(), generation);
        QCOMPARE(manager.syncState(), ParameterManager::Ready);
        QCOMPARE(manager.parameterValue("B")["value"].toDouble(), 22.);
        QCOMPARE(published.size(), 0);
        QCOMPARE(finished.size(), 0);
        deliver(manager, sample("B", 1, 4));
        QCOMPARE(manager.syncState(), ParameterManager::Failed);
        QVERIFY(manager.hasCompleteSnapshot());
        QVERIFY(!manager.parametersReady());
    }
    void encodingAndIdentifiers()
    {
        ParameterManager manager;
        start(manager, ParameterManager::Bytewise);
        float wire;
        QVERIFY(manager.prepareWrite("INTEGER", 2147483647., MAV_PARAM_TYPE_INT32, wire));
        deliver(manager, sample("INTEGER", 0, 1, wire, MAV_PARAM_TYPE_INT32));
        QCOMPARE(manager.parameterValue("INTEGER")["value"].toDouble(), 2147483647.);
        QVERIFY(manager.prepareWrite("NEGATIVE", -123, MAV_PARAM_TYPE_INT16, wire));
        QVERIFY(!manager.prepareWrite("OVERFLOW", 256, MAV_PARAM_TYPE_UINT8, wire));
        QVERIFY(!manager.prepareWrite("12345678901234567", 1, MAV_PARAM_TYPE_REAL32, wire));
        QVERIFY(!manager.prepareWrite(" A", 1, MAV_PARAM_TYPE_REAL32, wire));
        QVERIFY(manager.prepareWrite("1234567890123456", 1, MAV_PARAM_TYPE_REAL32, wire));
        manager.setEncoding(ParameterManager::NumericCast);
        QVERIFY(!manager.parametersReady());
        QVERIFY(!manager.prepareWrite("INTEGER", 16777217., MAV_PARAM_TYPE_INT32, wire));
        QVERIFY(!manager.prepareWrite("INTEGER", 1, MAV_PARAM_TYPE_INT64, wire));
        deliver(manager, sample("1234567890123456", 0, 1, -123, MAV_PARAM_TYPE_INT16));
        QVERIFY(manager.parametersReady());
        QCOMPARE(manager.parameterValue("1234567890123456")["value"].toDouble(), -123.);
        ParameterManager unknown;
        start(unknown, ParameterManager::UnknownEncoding);
        QCOMPARE(unknown.syncState(), ParameterManager::Failed);
        unknown.setEncoding(ParameterManager::Bytewise);
        QCOMPARE(unknown.syncState(), ParameterManager::Loading);
    }
    void invalidTypeAndProgress()
    {
        ParameterManager manager;
        start(manager);
        for (int i = 0; i < 199; ++i) deliver(manager, sample(QByteArray::number(i), i, 200));
        QCOMPARE(manager.progress(), 99);
        deliver(manager, sample("LAST", 199, 200, 1, MAV_PARAM_TYPE_UINT64));
        QCOMPARE(manager.syncState(), ParameterManager::Failed);
        QVERIFY(!manager.hasCompleteSnapshot());
    }
    void recoveryInterfaceAndAtomicObservation()
    {
        ParameterManager manager;
        start(manager);
        deliver(manager, sample("A", 0));
        QSignalSpy recovery(&manager, &ParameterManager::recoveryRequired);
        manager.markRecovering(manager.generation());
        QCOMPARE(manager.syncState(), ParameterManager::Recovering);
        QCOMPARE(recovery.size(), 1);
        bool observed = false;
        connect(&manager, &ParameterManager::snapshotPublished, this, [&] {
            observed = manager.parametersReady() && manager.parameters().size() == 3;
        });
        complete(manager);
        QVERIFY(observed);
        manager.disconnectSource();
        deliver(manager, sample("A", 0));
        QCOMPARE(manager.syncState(), ParameterManager::Idle);
        QVERIFY(!manager.parametersReady());
    }
    void refreshFromPublicationObserver_data()
    {
        QTest::addColumn<bool>("recovering");
        QTest::newRow("Loading-completion") << false;
        QTest::newRow("Recovering-completion") << true;
    }
    void refreshFromPublicationObserver()
    {
        QFETCH(bool, recovering);
        ParameterManager manager;
        start(manager);
        QSignalSpy finished(&manager, &ParameterManager::fetchFinished);
        connect(&manager, &ParameterManager::snapshotPublished, this, [&] { manager.refresh(); });
        if (recovering) {
            deliver(manager, sample("A", 0));
            manager.markRecovering(manager.generation());
            QCOMPARE(manager.syncState(), ParameterManager::Recovering);
        }
        complete(manager);
        QCOMPARE(manager.syncState(), ParameterManager::Loading);
        QVERIFY(!manager.parametersReady());
        QVERIFY(manager.hasCompleteSnapshot());
        QCOMPARE(finished.size(), 0); // No stale success after observer invalidates the attempt.
    }
    void bytewiseNaNBitsAreIntegers()
    {
        ParameterManager manager;
        start(manager, ParameterManager::Bytewise);
        float wire;
        QVERIFY(manager.prepareWrite("NEGATIVE", -1, MAV_PARAM_TYPE_INT32, wire));
        deliver(manager, sample("NEGATIVE", 0, 1, wire, MAV_PARAM_TYPE_INT32));
        QVERIFY(manager.parametersReady());
        QCOMPARE(manager.parameterValue("NEGATIVE")["value"].toDouble(), -1.);
    }
    void initialListRetryAndExhaustion()
    {
        RecoveryHarness h;
        QSignalSpy lists(&h.manager, &ParameterManager::requestList);
        start(h.manager);
        QCOMPARE(lists.size(), 1);
        QCOMPARE(h.manager.listRequestAttempts(), 1);
        QCOMPARE(lists[0][0].toUInt(), 42u);
        QCOMPARE(lists[0][1].toUInt(), 1u);
        h.tick(99);
        QCOMPARE(lists.size(), 1);
        h.tick(1);
        QCOMPARE(lists.size(), 2);
        h.tick(100);
        QCOMPARE(lists.size(), 3);
        h.tick(100);
        QCOMPARE(h.manager.syncState(), ParameterManager::Failed);
        QCOMPARE(lists.size(), 3);
        QVERIFY(h.manager.syncError().contains("3 list requests"));
        QVERIFY(!h.manager.parametersReady());
        h.manager.refresh();
        QCOMPARE(h.manager.listRequestAttempts(), 1);
        QCOMPARE(lists.size(), 4);
    }
    void duplicateCoverageDoesNotResetBudgetsOrInactivity()
    {
        RecoveryHarness h;
        start(h.manager);
        h.tick(100);
        QCOMPARE(h.manager.listRequestAttempts(), 2);
        deliver(h.manager, sample("A", 0));
        h.tick(49);
        deliver(h.manager, sample("A", 0));
        QCOMPARE(h.manager.receivedParameterCount(), 1);
        QCOMPARE(h.manager.listRequestAttempts(), 2);
        h.tick(1);
        QCOMPARE(h.manager.syncState(), ParameterManager::Recovering);
        h.tick(10);
        QCOMPARE(h.manager.indexRequestAttempts(1), 1);
        deliver(h.manager, sample("A", 0));
        QCOMPARE(h.manager.indexRequestAttempts(1), 1);
        QCOMPARE(h.manager.listRequestAttempts(), 2);
    }
    void invalidSourcesAndSentinelsDoNotAffectRetries()
    {
        RecoveryHarness h;
        start(h.manager);
        h.tick(99);
        const auto generation = h.manager.generation();
        h.manager.receive(43, 1, generation, sample("BAD", 0));
        h.manager.receive(42, 2, generation, sample("BAD", 0));
        deliver(h.manager, sample("UNKNOWN", UINT16_MAX));
        QCOMPARE(h.manager.expectedParameterCount(), 0);
        h.tick(1);
        QCOMPARE(h.manager.listRequestAttempts(), 2);
        deliver(h.manager, sample("A", 0));
        h.tick(49);
        h.manager.receive(43, 1, generation, sample("B", 1));
        h.tick(1);
        QCOMPARE(h.manager.syncState(), ParameterManager::Recovering);
        h.tick(10);
        h.manager.receive(42, 2, generation, sample("B", 1));
        QCOMPARE(h.manager.inFlightRequestCount(), 1);
        QCOMPARE(h.manager.missingParameterCount(), 2);
    }
    void onlyMissingIndicesAndCycleLimits()
    {
        RecoveryHarness h;
        start(h.manager);
        deliver(h.manager, sample("A", 0, 5));
        deliver(h.manager, sample("C", 2, 5));
        QSignalSpy reads(&h.manager, &ParameterManager::requestRead);
        h.tick(49);
        QCOMPARE(h.manager.syncState(), ParameterManager::Loading);
        QCOMPARE(reads.size(), 0);
        h.tick(1);
        QCOMPARE(h.manager.syncState(), ParameterManager::Recovering);
        QCOMPARE(reads.size(), 0);
        h.tick(10);
        QCOMPARE(reads.size(), 1);
        QCOMPARE(reads[0][2].toInt(), 1);
        h.tick(10);
        QCOMPARE(reads.size(), 2);
        QCOMPARE(reads[1][2].toInt(), 3);
        deliver(h.manager, sample("B", 1, 5));
        deliver(h.manager, sample("D", 3, 5));
        h.tick(10);
        QCOMPARE(reads.size(), 2); // Capacity is free, but this cycle's budget is spent.
        h.tick(10);
        QCOMPARE(reads.size(), 3);
        QCOMPARE(reads[2][2].toInt(), 4);
        for (const auto &request : reads) {
            QVERIFY(request[2].toInt() >= 0 && request[2].toInt() < 5);
            QVERIFY(request[2].toInt() != 0 && request[2].toInt() != 2);
        }
    }
    void largeMissingSetDoesNotBurstOrCatchUp()
    {
        RecoveryHarness h;
        h.config.maxInFlight = h.config.maxRequestsPerCycle = 8;
        h.configure();
        QSignalSpy reads(&h.manager, &ParameterManager::requestRead);
        h.beginMissing(2000);
        h.tick(200); // A delayed event loop must still emit only one request.
        QCOMPARE(reads.size(), 1);
        h.tick(0);
        h.tick(9);
        QCOMPARE(reads.size(), 1);
        h.tick(1);
        QCOMPARE(reads.size(), 2);
        QCOMPARE(h.manager.missingParameterCount(), 1999);
        QVERIFY(h.manager.parameters().isEmpty());
    }
    void inFlightWindowAndResponseDeadline()
    {
        RecoveryHarness h;
        h.config.maxInFlight = 1;
        h.configure();
        QSignalSpy reads(&h.manager, &ParameterManager::requestRead);
        h.beginMissing(4);
        h.tick(10);
        QCOMPARE(reads.size(), 1);
        h.tick(29);
        QCOMPARE(reads.size(), 1);
        h.tick(1);
        QCOMPARE(reads.size(), 2);
        QCOMPARE(reads[1][2].toInt(), 2); // Round-robin advances before retrying index 1.
    }
    void hundredsOfMissingIndicesRecoverWithDroppedReplies()
    {
        RecoveryHarness h;
        h.config.maxInFlight = h.config.maxRequestsPerCycle = 4;
        h.config.recoveryTimeoutMs = 10000;
        h.configure();
        QSignalSpy publications(&h.manager, &ParameterManager::snapshotPublished);
        connect(&h.manager, &ParameterManager::requestRead, this, [&](quint8, quint8, int index, quint64) {
            if (index % 7 == 0 && h.manager.indexRequestAttempts(index) == 1) return;
            deliver(h.manager, sample("P" + QByteArray::number(index), index, 301));
        });
        h.beginMissing(301);
        for (int tick = 0; tick < 1000 && h.manager.syncState() == ParameterManager::Recovering; ++tick) h.tick(10);
        QVERIFY(h.manager.parametersReady());
        QCOMPARE(h.manager.receivedParameterCount(), 301);
        QCOMPARE(h.manager.missingParameterCount(), 0);
        QVERIFY(h.manager.recoveryRequestCount() > 300);
        QCOMPARE(h.manager.indexRequestAttempts(7), 2);
        QCOMPARE(h.manager.indexRequestAttempts(1), 1);
        QCOMPARE(publications.size(), 1);
    }
    void validRepliesCancelRetriesAndCompleteRecovery()
    {
        RecoveryHarness h;
        QSignalSpy reads(&h.manager, &ParameterManager::requestRead);
        QSignalSpy publications(&h.manager, &ParameterManager::snapshotPublished);
        h.beginMissing(4);
        h.tick(10);
        deliver(h.manager, sample("B", 1, 4));
        QCOMPARE(h.manager.inFlightRequestCount(), 0);
        QCOMPARE(h.manager.missingParameterCount(), 2);
        h.tick(10);
        deliver(h.manager, sample("D", 3, 4)); // An unsolicited valid response also fills coverage.
        deliver(h.manager, sample("B", 1, 4)); // Idempotent duplicate during recovery.
        QCOMPARE(h.manager.syncState(), ParameterManager::Recovering);
        QCOMPARE(publications.size(), 0);
        deliver(h.manager, sample("C", 2, 4));
        QCOMPARE(h.manager.syncState(), ParameterManager::Ready);
        QCOMPARE(publications.size(), 1);
        const auto requests = reads.size();
        h.tick(10000);
        QCOMPARE(reads.size(), requests);
        QCOMPARE(h.manager.inFlightRequestCount(), 0);
    }
    void exhaustedIndexDoesNotConsumeOtherBudgets()
    {
        RecoveryHarness h;
        h.config.maxInFlight = 1;
        h.config.maxRequestsPerCycle = 1;
        h.config.recoveryCycleIntervalMs = 10;
        h.config.indexResponseTimeoutMs = 15;
        h.configure();
        QSignalSpy exhausted(&h.manager, &ParameterManager::indexRetryExhausted);
        h.beginMissing(4);
        h.tick(10); // 1, attempt 1
        h.tick(15); // 2, attempt 1
        h.tick(15); // 3, attempt 1
        h.tick(15); // 1, attempt 2
        h.tick(15); // 1 exhausted; 2, attempt 2
        QCOMPARE(exhausted.size(), 1);
        QCOMPARE(exhausted[0][0].toInt(), 1);
        QCOMPARE(h.manager.syncState(), ParameterManager::Recovering);
        QCOMPARE(h.manager.indexRequestAttempts(1), 2);
        QCOMPARE(h.manager.indexRequestAttempts(2), 2);
        QCOMPARE(h.manager.indexRequestAttempts(3), 1);
        deliver(h.manager, sample("B", 1, 4)); // Late response to an exhausted index remains valid.
        QVERIFY(h.manager.exhaustedParameterIndices().isEmpty());
        deliver(h.manager, sample("C", 2, 4));
        deliver(h.manager, sample("D", 3, 4));
        QVERIFY(h.manager.parametersReady());
    }
    void everyIndexGetsIndependentAttemptsBeforeFailure()
    {
        RecoveryHarness h;
        h.config.maxInFlight = 1;
        h.config.maxRequestsPerCycle = 1;
        h.config.recoveryCycleIntervalMs = 10;
        h.config.indexResponseTimeoutMs = 15;
        h.configure();
        h.beginMissing(4);
        h.tick(10);
        for (int i = 0; i < 6; ++i) h.tick(15);
        QCOMPARE(h.manager.syncState(), ParameterManager::Failed);
        QCOMPARE(h.manager.recoveryRequestCount(), 6);
        QCOMPARE(h.manager.exhaustedParameterIndices(), QList<int>({1, 2, 3}));
        for (int index = 1; index < 4; ++index) QCOMPARE(h.manager.indexRequestAttempts(index), 2);
        QVERIFY(h.manager.syncError().contains("retries exhausted"));
        QVERIFY(!h.manager.hasCompleteSnapshot());
        QVERIFY(!h.manager.parametersReady());
    }
    void recoveryDeadlinePreservesPreviousSnapshot()
    {
        RecoveryHarness h;
        h.config.recoveryTimeoutMs = 50;
        h.configure();
        start(h.manager); complete(h.manager);
        const auto snapshot = h.manager.parameters();
        h.manager.refresh();
        deliver(h.manager, sample("A", 0, 3, 99));
        h.tick(50);
        h.tick(10);
        h.tick(39);
        deliver(h.manager, sample("A", 0));
        h.tick(1);
        QCOMPARE(h.manager.syncState(), ParameterManager::Failed);
        QVERIFY(h.manager.syncError().contains("deadline exceeded"));
        QVERIFY(h.manager.hasCompleteSnapshot());
        QVERIFY(!h.manager.parametersReady());
        QCOMPARE(h.manager.parameters(), snapshot);
    }
    void disconnectAndRefreshCancelOldRecovery()
    {
        RecoveryHarness h;
        QSignalSpy reads(&h.manager, &ParameterManager::requestRead);
        h.beginMissing();
        h.tick(10);
        const auto old = h.manager.generation();
        const auto sent = reads.size();
        h.manager.disconnectSource();
        h.tick(10000);
        h.manager.processRecovery(old);
        QCOMPARE(h.manager.syncState(), ParameterManager::Idle);
        QCOMPARE(h.manager.inFlightRequestCount(), 0);
        QCOMPARE(reads.size(), sent);
        start(h.manager);
        h.manager.processRecovery(old);
        QCOMPARE(reads.size(), sent);
        QCOMPARE(h.manager.listRequestAttempts(), 1);
        deliver(h.manager, sample("A", 0));
        h.tick(50); h.tick(10);
        const auto recoveryGeneration = h.manager.generation();
        const auto recoverySent = reads.size();
        QTimer::singleShot(0, &h.manager, [&h, recoveryGeneration] { h.manager.processRecovery(recoveryGeneration); });
        h.manager.refresh();
        QCoreApplication::processEvents();
        QCOMPARE(reads.size(), recoverySent);
        QCOMPARE(h.manager.syncState(), ParameterManager::Loading);
        QCOMPARE(h.manager.recoveryRequestCount(), 0);
        QCOMPARE(h.manager.listRequestAttempts(), 1);
        QCOMPARE(h.manager.inFlightRequestCount(), 0);
    }
    void inconsistentRecoveryReplyFailsWithoutMoreRequests()
    {
        RecoveryHarness h;
        QSignalSpy reads(&h.manager, &ParameterManager::requestRead);
        h.beginMissing(); h.tick(10);
        deliver(h.manager, sample("WRONG", 0));
        QCOMPARE(h.manager.syncState(), ParameterManager::Failed);
        const auto requests = reads.size();
        h.tick(10000);
        QCOMPARE(reads.size(), requests);
        QVERIFY(!h.manager.hasCompleteSnapshot());
    }
    void refreshInsideRequestObserverCannotContinueOldCycle()
    {
        RecoveryHarness h;
        connect(&h.manager, &ParameterManager::requestRead, this, [&](quint8, quint8, int, quint64) { h.manager.refresh(); });
        h.beginMissing();
        const auto old = h.manager.generation();
        h.tick(10);
        QVERIFY(h.manager.generation() > old);
        QCOMPARE(h.manager.syncState(), ParameterManager::Loading);
        QCOMPARE(h.manager.recoveryRequestCount(), 0);
        h.manager.processRecovery(old);
        QCOMPARE(h.manager.recoveryRequestCount(), 0);
    }
    void realTimerPacingAndDestruction()
    {
        ParameterManager manager;
        auto config = manager.recoveryConfiguration();
        config.streamInactivityTimeoutMs = 20;
        config.requestSpacingMs = 25;
        config.indexResponseTimeoutMs = 500;
        QVERIFY(manager.setRecoveryConfiguration(config));
        QList<qint64> sendTimes;
        QElapsedTimer elapsed;
        elapsed.start();
        connect(&manager, &ParameterManager::requestRead, this, [&](quint8, quint8, int, quint64) { sendTimes.append(elapsed.elapsed()); });
        start(manager);
        deliver(manager, sample("A", 0, 10));
        QTRY_VERIFY_WITH_TIMEOUT(sendTimes.size() >= 3, 2000);
        for (int i = 1; i < sendTimes.size(); ++i) QVERIFY(sendTimes[i] - sendTimes[i - 1] >= 24);
        manager.disconnectSource();
        int lists = 0;
        auto doomed = std::make_unique<ParameterManager>();
        config.initialResponseTimeoutMs = 10;
        QVERIFY(doomed->setRecoveryConfiguration(config));
        connect(doomed.get(), &ParameterManager::requestList, this, [&](quint8, quint8, quint64) { ++lists; });
        start(*doomed);
        doomed.reset();
        QTest::qWait(25);
        QCOMPARE(lists, 1);
    }
    void recoveryConfigurationValidation()
    {
        ParameterManager manager;
        auto config = manager.recoveryConfiguration();
        config.requestSpacingMs = 0;
        QVERIFY(!manager.setRecoveryConfiguration(config));
        config.requestSpacingMs = 1;
        QVERIFY(manager.setRecoveryConfiguration(config));
        start(manager);
        QVERIFY(!manager.setRecoveryConfiguration(config));
    }
    void managedWriteEncoding_data()
    {
        QTest::addColumn<int>("encoding"); QTest::addColumn<int>("type"); QTest::addColumn<double>("value");
        QTest::newRow("real32-numeric") << int(ParameterManager::NumericCast) << int(MAV_PARAM_TYPE_REAL32) << 0.1;
        QTest::newRow("signed-numeric") << int(ParameterManager::NumericCast) << int(MAV_PARAM_TYPE_INT16) << -123.;
        QTest::newRow("unsigned-numeric") << int(ParameterManager::NumericCast) << int(MAV_PARAM_TYPE_UINT16) << 65535.;
        QTest::newRow("signed-bytewise") << int(ParameterManager::Bytewise) << int(MAV_PARAM_TYPE_INT32) << -2147483648.;
        QTest::newRow("unsigned-bytewise") << int(ParameterManager::Bytewise) << int(MAV_PARAM_TYPE_UINT32) << 4294967295.;
        QTest::newRow("real32-bytewise") << int(ParameterManager::Bytewise) << int(MAV_PARAM_TYPE_REAL32) << 12.5;
    }
    void managedWriteEncoding()
    {
        QFETCH(int, encoding); QFETCH(int, type); QFETCH(double, value);
        ParameterManager manager;
        start(manager, ParameterManager::Encoding(encoding));
        deliver(manager, sample("VALUE", 0, 1, 0, quint8(type)));
        QVERIFY(manager.parametersReady());
        QSignalSpy results(&manager, &ParameterManager::writeFinished);
        QSignalSpy sets(&manager, &ParameterManager::requestSet);
        const auto generation = manager.generation();
        const auto request = manager.requestWrite("VALUE", value, type);
        QVERIFY(request.value("accepted").toBool());
        QCOMPARE(sets.size(), 0);
        QCOMPARE(manager.parameterValue("VALUE")["value"].toDouble(), 0.);
        manager.processWrites(generation);
        QCOMPARE(sets.size(), 1);
        QCOMPARE(manager.pendingWriteCount(), 1);
        const auto wire = sets[0][4].toFloat();
        deliver(manager, sample("VALUE", 0, 1, wire, quint8(type)));
        QCOMPARE(results.size(), 1);
        const auto result = results[0][0].toMap();
        QCOMPARE(result.value("id"), request.value("id"));
        QCOMPARE(result.value("outcome").toInt(), int(ParameterManager::Confirmed));
        QCOMPARE(manager.generation(), generation);
        QCOMPARE(manager.receivedParameterCount(), 1);
        QVERIFY(manager.parametersReady());
        QCOMPARE(manager.parameterValue("VALUE")["value"].toDouble(), type == MAV_PARAM_TYPE_REAL32 ? double(float(value)) : value);
        deliver(manager, sample("VALUE", 0, 1, wire, quint8(type)));
        QCOMPARE(results.size(), 1);
    }
    void invalidManagedWrites_data()
    {
        QTest::addColumn<QString>("name"); QTest::addColumn<int>("type"); QTest::addColumn<double>("value");
        QTest::newRow("long-id") << QString("12345678901234567") << int(MAV_PARAM_TYPE_INT16) << 1.;
        QTest::newRow("non-ascii") << QString::fromUtf8("Pé") << int(MAV_PARAM_TYPE_INT16) << 1.;
        QTest::newRow("type64") << QString("A") << int(MAV_PARAM_TYPE_INT64) << 1.;
        QTest::newRow("type-mismatch") << QString("A") << int(MAV_PARAM_TYPE_REAL32) << 1.;
        QTest::newRow("out-of-range") << QString("A") << int(MAV_PARAM_TYPE_INT16) << 32768.;
        QTest::newRow("fractional-integer") << QString("A") << int(MAV_PARAM_TYPE_INT16) << 1.5;
        QTest::newRow("nan") << QString("A") << int(MAV_PARAM_TYPE_INT16) << std::numeric_limits<double>::quiet_NaN();
        QTest::newRow("infinity") << QString("A") << int(MAV_PARAM_TYPE_INT16) << std::numeric_limits<double>::infinity();
        QTest::newRow("unknown-name") << QString("UNKNOWN") << int(MAV_PARAM_TYPE_INT16) << 1.;
    }
    void invalidManagedWrites()
    {
        QFETCH(QString, name); QFETCH(int, type); QFETCH(double, value);
        ParameterManager manager;
        start(manager); deliver(manager, sample("A", 0, 1, 1, MAV_PARAM_TYPE_INT16));
        QSignalSpy results(&manager, &ParameterManager::writeFinished);
        QSignalSpy sets(&manager, &ParameterManager::requestSet);
        QVERIFY(!manager.requestWrite(name, value, type).value("accepted").toBool());
        QTRY_COMPARE(results.size(), 1);
        QCOMPARE(results[0][0].toMap().value("outcome").toInt(), int(ParameterManager::InvalidRequest));
        QCOMPARE(sets.size(), 0);
        QCOMPARE(manager.pendingWriteCount(), 0);
    }
    void writeSourcesIdentityAndAuthoritativeRejection()
    {
        ParameterManager manager;
        start(manager); complete(manager);
        QSignalSpy results(&manager, &ParameterManager::writeFinished);
        manager.requestWrite("A", 10, MAV_PARAM_TYPE_REAL32);
        manager.processWrites(manager.generation());
        manager.receive(43, 1, manager.generation(), sample("A", 0, 3, 10));
        manager.receive(42, 2, manager.generation(), sample("A", 0, 3, 10));
        deliver(manager, sample("B", 1, 3, 10));
        QCOMPARE(results.size(), 0);
        QCOMPARE(manager.parameterValue("A")["value"].toDouble(), 1.);
        deliver(manager, sample("A", 0, 3, 7));
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0][0].toMap().value("outcome").toInt(), int(ParameterManager::Rejected));
        QCOMPARE(manager.parameterValue("A")["value"].toDouble(), 7.);
        QVERIFY(manager.parametersReady());
        QCOMPARE(manager.receivedParameterCount(), 3);
        deliver(manager, sample("A", 0, 3, 10));
        QCOMPARE(results.size(), 1);
    }
    void wrongWriteResponseTypeCannotConfirm()
    {
        ParameterManager manager;
        start(manager); complete(manager);
        QSignalSpy results(&manager, &ParameterManager::writeFinished);
        manager.requestWrite("A", 10, MAV_PARAM_TYPE_REAL32);
        manager.processWrites(manager.generation());
        deliver(manager, sample("A", 0, 3, 10, MAV_PARAM_TYPE_INT16));
        QCOMPARE(results.size(), 1);
        QVERIFY(results[0][0].toMap().value("outcome").toInt() != ParameterManager::Confirmed);
        QCOMPARE(manager.parameterValue("A")["value"].toDouble(), 1.);
    }
    void writeRetryTimeoutAndTransportError()
    {
        RecoveryHarness h;
        auto config = h.manager.writeConfiguration(); config.responseTimeoutMs = 30; config.requestSpacingMs = 10; config.maxAttempts = 2;
        QVERIFY(h.manager.setWriteConfiguration(config));
        start(h.manager); complete(h.manager);
        QSignalSpy sets(&h.manager, &ParameterManager::requestSet);
        QSignalSpy results(&h.manager, &ParameterManager::writeFinished);
        const auto accepted = h.manager.requestWrite("A", 5, MAV_PARAM_TYPE_REAL32);
        h.manager.processWrites(h.manager.generation());
        QCOMPARE(sets.size(), 1);
        h.now = 29; h.manager.processWrites(h.manager.generation()); QCOMPARE(sets.size(), 1);
        h.now = 30; h.manager.processWrites(h.manager.generation()); QCOMPARE(sets.size(), 2);
        h.manager.writeTransportResult(accepted.value("id").toULongLong(), h.manager.generation(), 1, false); // stale attempt
        QCOMPARE(results.size(), 0);
        h.now = 60; h.manager.processWrites(h.manager.generation());
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0][0].toMap().value("outcome").toInt(), int(ParameterManager::TimedOut));
        QCOMPARE(h.manager.parameterValue("A")["value"].toDouble(), 1.);
        auto error = h.manager.requestWrite("B", 3, MAV_PARAM_TYPE_REAL32);
        h.manager.processWrites(h.manager.generation());
        h.manager.writeTransportResult(error.value("id").toULongLong(), h.manager.generation(), 1, false);
        QCOMPARE(results.size(), 2);
        QCOMPARE(results[1][0].toMap().value("outcome").toInt(), int(ParameterManager::TransportError));
    }
    void sameNameSerializationAndOutOfOrderDifferentNames()
    {
        RecoveryHarness h;
        auto config = h.manager.writeConfiguration(); config.requestSpacingMs = 10; config.responseTimeoutMs = 30;
        QVERIFY(h.manager.setWriteConfiguration(config));
        start(h.manager); complete(h.manager);
        QSignalSpy sets(&h.manager, &ParameterManager::requestSet);
        QSignalSpy results(&h.manager, &ParameterManager::writeFinished);
        h.manager.requestWrite("A", 2, MAV_PARAM_TYPE_REAL32);
        h.manager.requestWrite("A", 3, MAV_PARAM_TYPE_REAL32);
        h.manager.requestWrite("B", 4, MAV_PARAM_TYPE_REAL32);
        h.manager.processWrites(h.manager.generation());
        h.now = 10; h.manager.processWrites(h.manager.generation());
        QCOMPARE(sets.size(), 2);
        QCOMPARE(sets[1][3].toString(), QString("B"));
        deliver(h.manager, sample("B", 1, 3, 4));
        deliver(h.manager, sample("A", 0, 3, 2));
        QCOMPARE(results.size(), 2);
        deliver(h.manager, sample("A", 0, 3, 2)); // Duplicate cannot confirm a queued same-name write.
        QCOMPARE(results.size(), 2);
        h.now = 39; h.manager.processWrites(h.manager.generation()); QCOMPARE(sets.size(), 2);
        h.now = 40; h.manager.processWrites(h.manager.generation()); QCOMPARE(sets.size(), 3);
        deliver(h.manager, sample("A", 0, 3, 3));
        QCOMPARE(results.size(), 3);
        QCOMPARE(h.manager.pendingWriteCount(), 0);
    }
    void writesCancelOnRefreshDisconnectAndStaleCallbacks()
    {
        RecoveryHarness h; start(h.manager); complete(h.manager);
        QSignalSpy results(&h.manager, &ParameterManager::writeFinished);
        QSignalSpy sets(&h.manager, &ParameterManager::requestSet);
        h.manager.requestWrite("A", 3, MAV_PARAM_TYPE_REAL32);
        h.manager.requestWrite("A", 4, MAV_PARAM_TYPE_REAL32);
        h.manager.processWrites(h.manager.generation());
        const auto old = h.manager.generation();
        h.manager.refresh();
        QCOMPARE(results.size(), 2);
        for (const auto &row : results) QCOMPARE(row[0].toMap().value("outcome").toInt(), int(ParameterManager::Cancelled));
        complete(h.manager);
        h.now = 10000; h.manager.processWrites(old);
        QCOMPARE(sets.size(), 1);
        h.manager.requestWrite("A", 5, MAV_PARAM_TYPE_REAL32);
        h.manager.disconnectSource();
        QCOMPARE(results.size(), 3);
        h.manager.processWrites(h.manager.generation());
        QCOMPARE(sets.size(), 1);
        start(h.manager); complete(h.manager); // A new connection has its own write generation.
        QVERIFY(h.manager.requestWrite("B", 6, MAV_PARAM_TYPE_REAL32).value("accepted").toBool());
        h.manager.processWrites(old);
        QCOMPARE(sets.size(), 1);
        h.manager.processWrites(h.manager.generation());
        QCOMPARE(sets.size(), 2);
        deliver(h.manager, sample("B", 1, 3, 6));
        QCOMPARE(results.size(), 4);
        QCOMPARE(results.last()[0].toMap().value("outcome").toInt(), int(ParameterManager::Confirmed));
    }
    void managedBatchResults_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("all-confirmed") << 0; QTest::newRow("partial-rejection") << 1; QTest::newRow("cancelled") << 2;
        QTest::newRow("partial-timeout") << 3;
    }
    void managedBatchResults()
    {
        QFETCH(int, mode);
        RecoveryHarness h; start(h.manager); complete(h.manager);
        QSignalSpy batches(&h.manager, &ParameterManager::writeBatchFinished);
        const auto accepted = h.manager.requestWriteBatch({QVariantMap{{"name", "A"}, {"value", 2}, {"type", MAV_PARAM_TYPE_REAL32}},
                                                           QVariantMap{{"name", "B"}, {"value", 3}, {"type", MAV_PARAM_TYPE_REAL32}}});
        QVERIFY(accepted.value("accepted").toBool());
        h.manager.processWrites(h.manager.generation());
        if (mode == 2) h.manager.disconnectSource();
        else {
            deliver(h.manager, sample("A", 0, 3, 2));
            QCOMPARE(batches.size(), 0);
            h.now = 40; h.manager.processWrites(h.manager.generation());
            if (mode == 3) {
                for (int i = 1; i <= 3; ++i) { h.now = 40 + i * 1000; h.manager.processWrites(h.manager.generation()); }
            } else deliver(h.manager, sample("B", 1, 3, mode == 1 ? 7 : 3));
        }
        QCOMPARE(batches.size(), 1);
        const auto result = batches[0][0].toMap();
        QCOMPARE(result.value("success").toBool(), mode == 0);
        QCOMPARE(result.value("results").toList().size(), 2);
        if (mode == 1) {
            QVERIFY(result.value("reason").toString().contains("B: Rejected"));
            QCOMPARE(h.manager.parameterValue("A")["value"].toDouble(), 2.);
        }
        if (mode == 2) QCOMPARE(result.value("status").toString(), QString("Cancelled"));
    }
    void boundedWriteQueueAndQueueDeadline()
    {
        RecoveryHarness h;
        auto config = h.manager.writeConfiguration(); config.maxPending = 1; config.queueTimeoutMs = 10;
        QVERIFY(h.manager.setWriteConfiguration(config));
        start(h.manager); complete(h.manager);
        QVERIFY(h.manager.requestWrite("A", 2, MAV_PARAM_TYPE_REAL32).value("accepted").toBool());
        QVERIFY(!h.manager.requestWrite("B", 3, MAV_PARAM_TYPE_REAL32).value("accepted").toBool());
        QSignalSpy results(&h.manager, &ParameterManager::writeFinished);
        h.now = 10; h.manager.processWrites(h.manager.generation());
        QCOMPARE(h.manager.pendingWriteCount(), 0);
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0][0].toMap().value("outcome").toInt(), int(ParameterManager::TimedOut));
    }
    void immediateWriteResponseIsRegisteredBeforeTransmission()
    {
        ParameterManager manager;
        start(manager); complete(manager);
        QSignalSpy results(&manager, &ParameterManager::writeFinished);
        connect(&manager, &ParameterManager::requestSet, this,
                [&](quint64 id, quint8, quint8, const QString &name, float wire, quint8 type, quint64 generation, int attempt) {
            QVERIFY(manager.writeTransmissionCurrent(id, generation, attempt));
            manager.writeTransportResult(id, generation, attempt, true);
            deliver(manager, sample(name.toLatin1(), 0, 3, wire, type));
        });
        const auto request = manager.requestWrite("A", 9, MAV_PARAM_TYPE_REAL32);
        QCOMPARE(results.size(), 0);
        deliver(manager, sample("A", 0, 3, 9)); // An update before transmission cannot confirm a queued write.
        QCOMPARE(results.size(), 0);
        QCOMPARE(manager.pendingWriteCount(), 1);
        manager.processWrites(manager.generation());
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0][0].toMap().value("id"), request.value("id"));
        QCOMPARE(results[0][0].toMap().value("outcome").toInt(), int(ParameterManager::Confirmed));
        QCOMPARE(manager.pendingWriteCount(), 0);
    }
    void cancellationObserverRefreshCannotRestartAnOldAttempt()
    {
        ParameterManager manager;
        start(manager); complete(manager);
        manager.requestWrite("A", 2, MAV_PARAM_TYPE_REAL32);
        QSignalSpy lists(&manager, &ParameterManager::requestList);
        connect(&manager, &ParameterManager::writeFinished, this, [&](const QVariantMap &result) {
            if (result.value("outcome").toInt() == ParameterManager::Cancelled) manager.refresh();
        });
        manager.refresh();
        QCOMPARE(lists.size(), 1);
        QCOMPARE(manager.listRequestAttempts(), 1);
        QCOMPARE(manager.syncState(), ParameterManager::Loading);
    }
    void real32RangeAndBatchPreflight()
    {
        ParameterManager manager;
        start(manager); complete(manager);
        QVERIFY(!manager.requestWrite("A", 1e-300, MAV_PARAM_TYPE_REAL32).value("accepted").toBool());
        QVERIFY(!manager.requestWrite("A", 1e300, MAV_PARAM_TYPE_REAL32).value("accepted").toBool());
        QSignalSpy batches(&manager, &ParameterManager::writeBatchFinished);
        QSignalSpy sets(&manager, &ParameterManager::requestSet);
        const auto accepted = manager.requestWriteBatch({QVariantMap{{"name", "A"}, {"value", 2}, {"type", MAV_PARAM_TYPE_REAL32}},
                                                         QVariantMap{{"name", "B"}, {"value", 3}, {"type", 9.5}}});
        QVERIFY(!accepted.value("accepted").toBool());
        QTRY_COMPARE(batches.size(), 1);
        QCOMPARE(batches[0][0].toMap().value("results").toList().size(), 2);
        QCOMPARE(sets.size(), 0);
        QCOMPARE(manager.pendingWriteCount(), 0);
        QVERIFY(manager.parametersReady());
    }
};

QTEST_GUILESS_MAIN(ParameterManagerTest)
#include "ParameterManagerTest.moc"
