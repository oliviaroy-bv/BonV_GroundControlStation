#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QDir>
#include "parameters/ParameterConfigurationController.h"
#include "parameters/ParameterManager.h"
#include "mavlink/Mavlink.h"

using Configuration = ParameterConfigurationController;
namespace {
QVariantMap record(QString name, double value = 1, int type = 9, int index = 0) {
    return {{"name",name},{"value",value},{"type",type},{"index",index}};
}
QUrl save(QTemporaryDir &directory, const QByteArray &bytes, QString name = "test.param") {
    const auto path = directory.filePath(name); QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) qFatal("Cannot create test fixture");
    file.close(); return QUrl::fromLocalFile(path);
}
bool waitFor(const std::function<bool()> &predicate) {
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed() < 3000) { QCoreApplication::processEvents(); QTest::qWait(1); }
    return predicate();
}
struct Harness {
    qint64 now = 0;
    ParameterManager manager;
    Mavlink transport{nullptr,&manager,[this] { return now; }};
    QTcpServer server;
    QTcpSocket *peer = nullptr;
    Configuration *configuration = qobject_cast<Configuration *>(transport.parameterConfiguration());
    QSignalSpy sets{&manager,&ParameterManager::requestSet};
    Harness() { transport.setTelemetryLoggingEnabled(false); transport.setFlightControllerLoggingEnabled(false); }
    bool open(uint8_t family = MAV_AUTOPILOT_ARDUPILOTMEGA) {
        if (!server.listen(QHostAddress::LocalHost,0) || !transport.connectToVehicle("127.0.0.1",server.serverPort())) return false;
        if (!waitFor([this] { return server.hasPendingConnections(); })) return false;
        peer = server.nextPendingConnection();
        mavlink_message_t heartbeat{};
        mavlink_msg_heartbeat_pack(42,1,&heartbeat,MAV_TYPE_QUADROTOR,family,0,4,MAV_STATE_ACTIVE);
        uint8_t bytes[MAVLINK_MAX_PACKET_LEN]; const int size = mavlink_msg_to_send_buffer(bytes,&heartbeat);
        peer->write(reinterpret_cast<char *>(bytes),size); peer->flush();
        if (!waitFor([this] { return transport.connected(); })) return false;
        transport.requestParameters();
        if (family == MAV_AUTOPILOT_GENERIC) manager.setEncoding(ParameterManager::NumericCast);
        echo("A",1,0); echo("B",1,1); echo("C",1,2);
        return manager.parametersReady();
    }
    void echo(const char *name, float value, int index) {
        QByteArray id(name); id.resize(16,'\0');
        manager.receive(42,1,manager.generation(),{id,value,MAV_PARAM_TYPE_REAL32,index,3});
    }
};
}

class ParameterConfigurationTest : public QObject {
    Q_OBJECT
private slots:
    void parseFormats_data() {
        QTest::addColumn<QByteArray>("text"); QTest::addColumn<QString>("extension"); QTest::addColumn<int>("total"); QTest::addColumn<QString>("kind"); QTest::addColumn<bool>("error");
        QTest::newRow("pairs") << QByteArray("A,1.25\nB -2\n") << QString("param") << 2 << QString("") << false;
        QTest::newRow("typed") << QByteArray("# Stack: PX4\n1\t1\tA\t1.25\t9\n") << QString("params") << 1 << QString("") << false;
        QTest::newRow("duplicate") << QByteArray("A 1\nA 2\n") << QString("param") << 2 << QString("Invalid") << false;
        QTest::newRow("nonfinite") << QByteArray("A NaN\n") << QString("param") << 1 << QString("Invalid") << false;
        QTest::newRow("infinity") << QByteArray("A inf\n") << QString("param") << 1 << QString("Invalid") << false;
        QTest::newRow("overflow") << QByteArray("A 1e999\n") << QString("param") << 1 << QString("Invalid") << false;
        QTest::newRow("64bit") << QByteArray("1 1 A 1 7\n") << QString("params") << 1 << QString("Unsupported") << false;
        QTest::newRow("invalid-name") << QByteArray("12345678901234567 1\n") << QString("param") << 1 << QString("Invalid") << false;
        QTest::newRow("empty") << QByteArray("# none\n") << QString("param") << 0 << QString("") << true;
        QTest::newRow("extension") << QByteArray("A 1\n") << QString("json") << 0 << QString("") << true;
        QTest::newRow("bad-comma") << QByteArray("A,,1\n") << QString("param") << 1 << QString("Invalid") << false;
        QTest::newRow("components") << QByteArray("1 1 A 1 9\n1 2 B 1 9\n") << QString("params") << 2 << QString("Unsupported") << false;
        QTest::newRow("conflicting-family") << QByteArray("# Stack: PX4\n# BonV firmware: ardupilot\nA 1\n") << QString("param") << 1 << QString("") << true;
        QTest::newRow("invalid-utf8") << QByteArray::fromHex("ff") << QString("param") << 0 << QString("") << true;
        QTest::newRow("binary") << QByteArray("A 1\0",4) << QString("param") << 0 << QString("") << true;
        QTest::newRow("too-large") << QByteArray(17*1024*1024,'x') << QString("param") << 0 << QString("") << true;
        QTest::newRow("integer-range") << QByteArray("1 1 A 256 1\n") << QString("params") << 1 << QString("Invalid") << false;
        QTest::newRow("integer-fraction") << QByteArray("1 1 A 1.5 6\n") << QString("params") << 1 << QString("Invalid") << false;
        QTest::newRow("invalid-source") << QByteArray("0 1 A 2 9\n") << QString("params") << 1 << QString("Invalid") << false;
    }
    void parseFormats() {
        QFETCH(QByteArray,text); QFETCH(QString,extension); QFETCH(int,total); QFETCH(QString,kind); QFETCH(bool,error);
        const auto parsed = Configuration::parse(text,extension);
        QCOMPARE(parsed.entries.size(),total); QCOMPARE(!parsed.error.isEmpty(),error);
        if (total) QCOMPARE(parsed.entries[0].toMap().value("kind").toString(),kind);
        if (QTest::currentDataTag() == QByteArray("duplicate")) { QCOMPARE(parsed.duplicates,1); QCOMPARE(parsed.entries[1].toMap().value("kind").toString(),QString("Invalid")); }
    }
    void exportPrecisionAndCompatibility() {
        const QVariantMap snapshot{{"A",record("A",double(0.1f))},{"B",record("B",4294967295.,5,1)}};
        const QVariantMap provenance{{"systemId",42},{"componentId",1}};
        for (const auto &family : {QString("px4"),QString("ardupilot"),QString("unknown")}) {
            const auto bytes = Configuration::serialize(snapshot,provenance,family,true);
            QVERIFY(bytes.contains("retained/stale"));
            auto document = Configuration::parse(bytes, family == "ardupilot" ? "param" : "params");
            QVERIFY(document.error.isEmpty()); QCOMPARE(document.family,family);
            QCOMPARE(document.entries[0].toMap().value("value").toDouble(),double(0.1f));
            QCOMPARE(document.entries[1].toMap().value("value").toDouble(),4294967295.);
            QCOMPARE(document.entries[1].toMap().value("type").toInt(),family == "ardupilot" ? -1 : 5);
        }
    }
    void comparisonCategoriesAndLargeFiles() {
        const auto document = Configuration::parse("A 1\nB 2\nG 7\nD NaN\n1 1 E 1 7\n", "param");
        // Mixed layout is invalid, but remains in the report rather than being dropped.
        const QVariantMap snapshot{{"A",record("A")},{"B",record("B")},{"C",record("C")},{"D",record("D")}};
        const auto rows = Configuration::compareEntries(document,snapshot);
        QHash<QString,QString> statuses;
        for (const auto &item : rows) { const auto row = item.toMap(); statuses[row.value("name").toString()] = row.value("status").toString(); }
        QCOMPARE(statuses["A"],QString("Identical")); QCOMPARE(statuses["B"],QString("Different"));
        QCOMPARE(statuses["C"],QString("Missing from file")); QCOMPARE(statuses["D"],QString("Invalid")); QCOMPARE(statuses["G"],QString("Missing from vehicle"));
        const auto mismatch = Configuration::parse("1 1 A 2 6\n","params");
        QCOMPARE(Configuration::compareEntries(mismatch,snapshot)[0].toMap().value("status").toString(),QString("Type mismatch"));
        const auto unsupported = Configuration::parse("1 1 A 2 7\n","params");
        QCOMPARE(Configuration::compareEntries(unsupported,snapshot)[0].toMap().value("status").toString(),QString("Unsupported"));
        const auto px4 = Configuration::parse("# Stack: PX4\n1 1 A 2 9\n","params");
        QCOMPARE(Configuration::compareEntries(px4,snapshot,1,"ardupilot")[0].toMap().value("status").toString(),QString("Unsupported"));
        QByteArray large; QVariantMap many;
        for (int i = 0; i < 1500; ++i) { const auto name = QString("P_%1").arg(i,4,10,QChar('0')); large += name.toUtf8() + " 2\n"; many[name] = record(name); }
        auto parsed = Configuration::parse(large,"param"); QCOMPARE(parsed.entries.size(),1500);
        const auto comparison = Configuration::compareEntries(parsed,many); QCOMPARE(comparison.size(),1500);
        QCOMPARE(comparison[0].toMap().value("name").toString(),QString("P_0000"));
    }
    void exportAsyncErrorsAndStale() {
        Harness h; QVERIFY(h.open()); QTemporaryDir directory;
        auto *c = h.configuration; QVERIFY(c);
        h.transport.requestParameterWrite("A",2,9); // A pending proposal must not appear in the export.
        c->exportFile(QUrl::fromLocalFile(directory.filePath("missing/output.param")),false,false);
        QTRY_VERIFY(!c->busy()); QVERIFY(!c->message().contains("successfully"));
        const auto path = directory.filePath("output.param");
        c->exportFile(QUrl::fromLocalFile(path),false,false); QTRY_VERIFY(!c->busy());
        QVERIFY(c->message().contains("successfully")); QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); QVERIFY(file.readAll().contains("A\t1")); file.close();
        h.transport.disconnectFromVehicle();
        c->exportFile(QUrl::fromLocalFile(path),false,false); QVERIFY(c->message().contains("acknowledgement"));
        c->exportFile(QUrl::fromLocalFile(path),true,false); QTRY_VERIFY(!c->busy());
        QVERIFY(file.open(QIODevice::ReadOnly)); QVERIFY(file.readAll().contains("retained/stale"));
        Mavlink empty; auto *none = qobject_cast<Configuration *>(empty.parameterConfiguration());
        none->exportFile(QUrl::fromLocalFile(directory.filePath("empty.params")),true,true);
        QVERIFY(!QFile::exists(directory.filePath("empty.params")));
    }
    void sequentialApplicationAndPartialResults() {
        Harness h; QVERIFY(h.open()); QTemporaryDir directory; auto *c = h.configuration;
        c->importFile(save(directory,"# BonV firmware: ardupilot\nA 2\nB 3\n")); QTRY_VERIFY(!c->busy());
        QCOMPARE(h.manager.parameterValue("A").value("value").toDouble(),1.); QCOMPARE(h.sets.size(),0);
        c->compare(); QTRY_VERIFY(c->comparisonCurrent()); c->selectAll(true); QCOMPARE(c->selectedCount(),2);
        c->applySelected({"A"},false,false); QVERIFY(!c->applying()); QVERIFY(c->message().contains("drafts"));
        c->applySelected({"A"},false,true); QTRY_COMPARE(h.sets.size(),1);
        QTest::qWait(80); QCOMPARE(h.sets.size(),1); QCOMPARE(h.manager.pendingWriteCount(),1);
        h.echo("A",2,0); QTRY_COMPARE(h.sets.size(),2);
        h.echo("B",4,1); QTRY_VERIFY(!c->applying());
        QCOMPARE(c->progress().value("Confirmed").toInt(),1); QCOMPARE(c->progress().value("Rejected").toInt(),1);
        QVERIFY(c->message().contains("Partial success")); QVERIFY(!c->comparisonCurrent());
        QVERIFY(!c->data(c->index(0),Configuration::IdRole).toString().isEmpty());
        QVERIFY(c->data(c->index(0),Configuration::AttemptsRole).toInt() >= 1);
        QCOMPARE(c->data(c->index(0),Configuration::CurrentRole).toDouble(),2.);
        QCOMPARE(c->data(c->index(1),Configuration::CurrentRole).toDouble(),4.);
    }
    void transportFailuresAreNotSuccess() {
        Harness h; QVERIFY(h.open()); QTemporaryDir directory; auto *c = h.configuration;
        connect(&h.manager,&ParameterManager::requestSet,&h.manager,[&h](quint64 id, quint8, quint8, QString, float, quint8, quint64 generation, int attempt) {
            h.manager.writeTransportResult(id,generation,attempt,false);
        });
        c->importFile(save(directory,"# Stack: ArduPilot\nA 2\nB 3\n")); QTRY_VERIFY(!c->busy()); c->compare(); QTRY_VERIFY(c->comparisonCurrent()); c->selectAll(true);
        c->applySelected({},false,false);
        QTRY_COMPARE(c->progress().value("TransportError").toInt(),2);
        QVERIFY(!c->applying()); QVERIFY(c->message().contains("No writes confirmed"));
        QCOMPARE(c->progress().value("Confirmed").toInt(),0);
    }
    void unknownVehicleRequiresAcknowledgement() {
        Harness h; QVERIFY(h.open(MAV_AUTOPILOT_GENERIC)); QTemporaryDir directory; auto *c = h.configuration;
        QCOMPARE(c->firmware(),QString("unknown"));
        const auto output = QUrl::fromLocalFile(directory.filePath("unknown.params"));
        c->exportFile(output,false,false); QVERIFY(!QFile::exists(output.toLocalFile()));
        c->exportFile(output,false,true); QTRY_VERIFY(!c->busy()); QVERIFY(QFile::exists(output.toLocalFile()));
        c->importFile(save(directory,"# Stack: ArduPilot\nA 2\n")); QTRY_VERIFY(!c->busy());
        c->compare(); QTRY_VERIFY(c->comparisonCurrent()); c->selectAll(true);
        c->applySelected({},false,false); QVERIFY(!c->applying());
        c->applySelected({},true,false); QTRY_COMPARE(h.sets.size(),1); h.echo("A",2,0); QTRY_VERIFY(!c->applying());
        QCOMPARE(c->progress().value("Confirmed").toInt(),1);
    }
    void interruptedApplication_data() {
        QTest::addColumn<QString>("mode");
        for (const auto &mode : {"disconnect","heartbeat","refresh","stop","revalidate","other-write"}) QTest::newRow(mode) << QString(mode);
    }
    void interruptedApplication() {
        QFETCH(QString,mode); Harness h; QVERIFY(h.open()); QTemporaryDir directory; auto *c = h.configuration;
        c->importFile(save(directory,"# BonV firmware: ardupilot\nA 2\nB 3\n")); QTRY_VERIFY(!c->busy());
        c->compare(); QTRY_VERIFY(c->comparisonCurrent()); c->selectAll(true); c->applySelected({},false,false);
        QTRY_COMPARE(h.sets.size(),1);
        if (mode == "disconnect") h.transport.disconnectFromVehicle();
        else if (mode == "heartbeat") { h.now += h.transport.heartbeatTimeoutMs() + 1; h.transport.processConnectionTimeout(h.transport.connectionGeneration()); }
        else if (mode == "refresh") h.transport.requestParameters();
        else {
            if (mode == "stop") c->stop();
            if (mode == "revalidate") h.echo("B",8,1);
            if (mode == "other-write") h.transport.requestParameterWrite("C",7,9);
            h.echo("A",2,0);
        }
        QTRY_VERIFY(!c->applying());
        QVERIFY(!c->comparisonCurrent()); QCOMPARE(c->progress().value("total").toInt(),2);
        if (mode == "disconnect" || mode == "heartbeat" || mode == "refresh") QCOMPARE(c->progress().value("Cancelled").toInt(),1);
        else QCOMPARE(c->progress().value("Confirmed").toInt(),1);
        if (mode != "other-write") QCOMPARE(h.sets.size(),1);
        // None of these cases is allowed to resume just because time passes.
        QTest::qWait(80); QVERIFY(!c->applying());
    }
    void unknownFirmwareAndInvalidSelection() {
        Harness h; QVERIFY(h.open()); QTemporaryDir directory; auto *c = h.configuration;
        c->importFile(save(directory,"A 2\nX 3\n")); QTRY_VERIFY(!c->busy());
        c->compare(); QTRY_VERIFY(c->comparisonCurrent());
        c->applySelected({},true,false); QVERIFY(!c->applying()); // no selections
        c->selectRow(99,true); c->selectAll(true); QCOMPARE(c->selectedCount(),1);
        c->applySelected({},false,false); QVERIFY(!c->applying());
        c->applySelected({},true,false); QTRY_COMPARE(h.sets.size(),1); h.echo("A",2,0); QTRY_VERIFY(!c->applying());
        QVERIFY(c->message().contains("All selected writes confirmed"));
        c->importFile(save(directory,"# Stack: PX4\n1 1 A 3 9\n","other.params")); QTRY_VERIFY(!c->busy());
        c->compare(); QTRY_VERIFY(c->comparisonCurrent()); c->selectAll(true); QCOMPARE(c->selectedCount(),0);
        QVERIFY(c->compatibility().contains("Incompatible"));
    }
    void queueAndProxySelections() {
        Harness h; QVERIFY(h.open()); QTemporaryDir directory; auto *c = h.configuration;
        h.transport.requestParameterWrite("C",7,9);
        c->importFile(save(directory,"# Stack: ArduPilot\nA 2\nB 3\n")); QTRY_VERIFY(!c->busy()); c->compare(); QTRY_VERIFY(c->comparisonCurrent()); c->selectAll(true);
        c->applySelected({},false,false); QVERIFY(!c->applying());
        QVERIFY(c->message().contains("other parameter writes"));
        h.echo("C",7,2); QVERIFY(!c->comparisonCurrent());
        c->compare(); QTRY_VERIFY(c->comparisonCurrent());
        c->setFilter("Different"); QCOMPARE(c->rows()->rowCount(),2);
        const auto proxyIndex = c->rows()->index(0,0);
        c->selectRow(c->rows()->data(proxyIndex,Configuration::SourceRowRole).toInt(),true); QCOMPARE(c->selectedCount(),1);
        c->setFilter("Missing"); QCOMPARE(c->rows()->rowCount(),1);
        c->selectRow(c->rows()->data(c->rows()->index(0,0),Configuration::SourceRowRole).toInt(),true); QCOMPARE(c->selectedCount(),1);
        c->selectAll(false); QCOMPARE(c->selectedCount(),0);
    }
    void largeAsyncComparisonAndSelection() {
        Harness h; QVERIFY(h.open()); QTemporaryDir directory; auto *c = h.configuration;
        h.transport.requestParameters(); QByteArray file("# Stack: ArduPilot\n");
        for (int i = 0; i < 1500; ++i) {
            const auto name = QString("P_%1").arg(i,4,10,QChar('0')).toLatin1(); file += name + " 2\n";
            auto id = name; id.resize(16,'\0'); h.manager.receive(42,1,h.manager.generation(),{id,1.f,MAV_PARAM_TYPE_REAL32,i,1500});
        }
        bool eventProcessed = false;
        QTimer::singleShot(0,c,[&eventProcessed] { eventProcessed = true; });
        c->importFile(save(directory,file)); QVERIFY(c->busy()); QTRY_VERIFY(eventProcessed); QTRY_VERIFY(!c->busy());
        QCOMPARE(c->importSummary().value("total").toInt(),1500);
        c->compare(); QVERIFY(c->busy()); QTRY_VERIFY(c->comparisonCurrent());
        QCOMPARE(c->rowCount(),1500); c->selectAll(true); QCOMPARE(c->selectedCount(),1500);
        c->selectAll(false); QCOMPARE(c->selectedCount(),0); QCOMPARE(h.sets.size(),0);
    }
    void comparisonPanelUsesActualModel() {
        Harness h; QVERIFY(h.open()); QTemporaryDir directory; auto *c = h.configuration;
        c->importFile(save(directory,"# Stack: ArduPilot\nA 2\nB 3\n")); QTRY_VERIFY(!c->busy()); c->compare(); QTRY_VERIFY(c->comparisonCurrent());
        QQmlEngine engine; QStringList warnings;
        connect(&engine,&QQmlEngine::warnings,this,[&warnings](const QList<QQmlError> &errors) { for (const auto &error : errors) warnings.append(error.description()); });
        QQmlEngine::setObjectOwnership(&h.transport,QQmlEngine::CppOwnership); QQmlEngine::setObjectOwnership(&h.manager,QQmlEngine::CppOwnership);
        QQmlComponent wrapper(&engine);
        wrapper.setData("import QtQml\nQtObject { property var mavlink; property var parameterManager }",QUrl());
        QScopedPointer<QObject> vehicle(wrapper.createWithInitialProperties({{"mavlink",QVariant::fromValue(static_cast<QObject *>(&h.transport))}, {"parameterManager",QVariant::fromValue(static_cast<QObject *>(&h.manager))}}));
        QVERIFY2(vehicle,qPrintable(wrapper.errorString()));
        QQmlComponent component(&engine,QUrl::fromLocalFile(QFINDTESTDATA("../ui/qml/settings/FullParameterListPage.qml")));
        QTRY_VERIFY(component.status() != QQmlComponent::Loading); QVERIFY2(component.isReady(),qPrintable(component.errorString()));
        QScopedPointer<QObject> page(component.createWithInitialProperties({{"vehicle",QVariant::fromValue(vehicle.data())},{"width",1200},{"height",800},{"configurationVisible",true}}));
        QVERIFY2(page,qPrintable(component.errorString()));
        page->setProperty("configurationVisible",true);
        QQuickWindow window; window.resize(1200,800);
        auto *item = qobject_cast<QQuickItem *>(page.data()); QVERIFY(item); item->setParentItem(window.contentItem()); window.show();
        auto *list = page->findChild<QObject *>("parameterConfigurationRows"); QVERIFY(list);
        QTRY_COMPARE(list->property("count").toInt(),3); QTest::qWait(50);
        window.grabWindow().save(QDir(QFINDTESTDATA("../build")).filePath("phase6-comparison.png"));
        item->setParentItem(nullptr); window.hide();
        for (const auto &warning : warnings) QVERIFY2(!warning.contains("Unable to assign") && !warning.contains("ReferenceError"),qPrintable(warning));
    }
    void timeoutAndLateResults() {
        Harness h; QVERIFY(h.open()); auto config = h.manager.writeConfiguration(); config.maxAttempts = 1; config.responseTimeoutMs = 30; config.requestSpacingMs = 1;
        QVERIFY(h.manager.setWriteConfiguration(config)); QTemporaryDir directory; auto *c = h.configuration;
        c->importFile(save(directory,"# Stack: ArduPilot\nA 2\nB 3\n")); QTRY_VERIFY(!c->busy()); c->compare(); QTRY_VERIFY(c->comparisonCurrent()); c->selectAll(true);
        c->applySelected({},false,false); QTRY_VERIFY(c->progress().value("TimedOut").toInt() == 2);
        QVERIFY(!c->applying()); QCOMPARE(c->progress().value("Confirmed").toInt(),0);
        const auto before = c->progress();
        emit h.transport.parameterWriteFinished({{"id",999},{"parameter","A"},{"outcomeName","Confirmed"}});
        QCOMPARE(c->progress(),before); QVERIFY(c->message().contains("No writes confirmed"));
    }
};
QTEST_MAIN(ParameterConfigurationTest)
#include "ParameterConfigurationTest.moc"
