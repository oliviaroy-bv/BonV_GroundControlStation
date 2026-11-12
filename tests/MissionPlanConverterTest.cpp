#include <QtTest/QtTest>

#include "mission/MissionPlanConverter.h"

#include <QFile>
#include <QTemporaryDir>

namespace {
QString writeWpl(QTemporaryDir &directory, const QByteArray &content, const QString &name = QStringLiteral("plan.waypoints"))
{
    const QString path = directory.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return {};
    if (file.write(content) != content.size()) return {};
    return path;
}

QByteArray representativeWpl()
{
    return QByteArrayLiteral(
        "QGC WPL 110\n"
        "9\t1\t0\t16\t0\t0\t0\t0\t10.0000001\t20.0000001\t100.25\t1\n"
        "10\t0\t2\t178\t1\t2\t3\t4\t0\t0\t0\t1\n"
        "11\t0\t3\t22\t5\t6\t7\t8\t10.0000002\t20.0000002\t50.5\t0\n");
}
}

class MissionPlanConverterTest : public QObject
{
    Q_OBJECT
private slots:
    void importsAndExportsWplWithoutLosingRawFields();
    void invalidWplReportsLineAndPreservesModel();
    void importedRowsReceiveIdsAndNormalizedSequences();
    void convertsDownloadedRawMission();
    void generatedPlannerFileImportsAsAutomaticResult();
    void projectionPreservesNearbyItemsAndDoesNotMutateModel();
    void projectionExcludesQgcHomeItem();
    void exportRejectsUnrepresentableItems();
};

void MissionPlanConverterTest::importsAndExportsWplWithoutLosingRawFields()
{
    QTemporaryDir directory; QVERIFY(directory.isValid());
    const QString input = writeWpl(directory, representativeWpl()); QVERIFY(!input.isEmpty());
    MissionPlanModel model;
    const auto imported = MissionPlanConverter::importWplFile(input, &model, MissionPlanModel::ImportedMission);
    QVERIFY2(imported.success, qPrintable(imported.error));
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.sourceState(), MissionPlanModel::ImportedMission);
    QVERIFY(!model.dirty());
    const auto nonCoordinate = model.missionItemAt(1);
    QVERIFY(nonCoordinate); QCOMPARE(nonCoordinate->command, quint16(178));
    QCOMPARE(nonCoordinate->frame, quint8(2)); QVERIFY(!nonCoordinate->hasCoordinate);
    QCOMPARE(nonCoordinate->param4, 4.0f);
    const auto landing = model.missionItemAt(2);
    QVERIFY(landing); QCOMPARE(landing->autocontinue, false); QCOMPARE(landing->altitude, 50.5f);

    const QString output = directory.filePath(QStringLiteral("export.waypoints"));
    const auto exported = MissionPlanConverter::exportWplFile(output, model);
    QVERIFY2(exported.success, qPrintable(exported.error));
    MissionPlanModel roundTrip;
    QVERIFY(MissionPlanConverter::importWplFile(output, &roundTrip, MissionPlanModel::ImportedMission).success);
    QCOMPARE(roundTrip.rowCount(), 3);
    QCOMPARE(roundTrip.missionItemAt(1)->command, quint16(178));
    QCOMPARE(roundTrip.missionItemAt(1)->param3, 3.0f);
    QCOMPARE(roundTrip.missionItemAt(2)->frame, quint8(3));
    QCOMPARE(roundTrip.missionItemAt(2)->autocontinue, false);
}

void MissionPlanConverterTest::invalidWplReportsLineAndPreservesModel()
{
    MissionPlanModel model;
    MissionPlanItem original; original.id = QUuid::createUuid(); original.latitude = 1; original.longitude = 2;
    QVERIFY(model.insertMissionItem(original));
    const quint64 revision = model.revision();
    QTemporaryDir directory; QVERIFY(directory.isValid());
    const QString input = writeWpl(directory, QByteArrayLiteral("QGC WPL 110\n0 1 3 16 0 0 0 0 91 10 20 1\n"));
    const auto result = MissionPlanConverter::importWplFile(input, &model, MissionPlanModel::ImportedMission);
    QVERIFY(!result.success);
    QCOMPARE(result.line, 2);
    QVERIFY(result.error.contains(QStringLiteral("Line 2")));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.revision(), revision);
    QCOMPARE(model.missionItemAt(0)->id, original.id);
}

void MissionPlanConverterTest::importedRowsReceiveIdsAndNormalizedSequences()
{
    QTemporaryDir directory; QVERIFY(directory.isValid());
    const QString input = writeWpl(directory, representativeWpl());
    MissionPlanModel model;
    QVERIFY(MissionPlanConverter::importWplFile(input, &model, MissionPlanModel::ImportedMission).success);
    const auto first = model.missionItemAt(0); const auto second = model.missionItemAt(1);
    QVERIFY(first && second); QVERIFY(!first->id.isNull()); QVERIFY(first->id != second->id);
    QCOMPARE(first->sequence, quint16(0)); QCOMPARE(second->sequence, quint16(1));
}

void MissionPlanConverterTest::convertsDownloadedRawMission()
{
    QVector<MavlinkMissionItem> raw;
    MavlinkMissionItem waypoint;
    waypoint.seq = 7; waypoint.current = 1; waypoint.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    waypoint.command = MAV_CMD_NAV_WAYPOINT; waypoint.x = 30.1; waypoint.y = 70.2; waypoint.z = 80.0f;
    waypoint.param1 = 1.0f; waypoint.param4 = 4.0f; raw.append(waypoint);
    MavlinkMissionItem command;
    command.seq = 8; command.frame = MAV_FRAME_MISSION; command.command = MAV_CMD_DO_JUMP;
    command.param1 = 2.0f; command.param2 = 3.0f; command.autocontinue = 0; raw.append(command);
    MissionPlanModel model;
    const auto result = MissionPlanConverter::importVehicleMission(raw, &model);
    QVERIFY2(result.success, qPrintable(result.error));
    QCOMPARE(model.sourceState(), MissionPlanModel::DownloadedVehicleMission);
    QVERIFY(!model.dirty());
    QCOMPARE(model.missionItemAt(0)->sequence, quint16(0));
    QCOMPARE(model.missionItemAt(0)->param4, 4.0f);
    QCOMPARE(model.missionItemAt(1)->command, quint16(MAV_CMD_DO_JUMP));
    QVERIFY(!model.missionItemAt(1)->hasCoordinate);
    QVERIFY(!model.missionItemAt(1)->autocontinue);

    MissionPlanItem local; local.id = QUuid::createUuid(); local.latitude = 1; local.longitude = 2;
    QVERIFY(model.insertMissionItem(local));
    const quint64 revision = model.revision();
    const auto protectedImport = MissionPlanConverter::importVehicleMission(raw, &model);
    QVERIFY(!protectedImport.success);
    QCOMPARE(model.revision(), revision);
}

void MissionPlanConverterTest::generatedPlannerFileImportsAsAutomaticResult()
{
    QTemporaryDir directory; QVERIFY(directory.isValid());
    // SkyRouteX produces QGC WPL 110; conversion is intentionally file based
    // so its planner and writer stay unchanged.
    const QString generated = writeWpl(directory, representativeWpl(), QStringLiteral("skyroutex.waypoints"));
    MissionPlanModel model;
    const auto result = MissionPlanConverter::importWplFile(generated, &model, MissionPlanModel::AutomaticPlannerResult);
    QVERIFY2(result.success, qPrintable(result.error));
    QCOMPARE(model.sourceState(), MissionPlanModel::AutomaticPlannerResult);
    QVERIFY(model.dirty());
}

void MissionPlanConverterTest::projectionPreservesNearbyItemsAndDoesNotMutateModel()
{
    MissionPlanModel model;
    MissionPlanItem first; first.id = QUuid::createUuid(); first.latitude = 10.0; first.longitude = 20.0;
    MissionPlanItem second = first; second.id = QUuid::createUuid(); second.latitude += 0.000001; // about 11 cm
    MissionPlanItem nonCoordinate = first; nonCoordinate.id = QUuid::createUuid(); nonCoordinate.command = MAV_CMD_DO_JUMP;
    nonCoordinate.frame = MAV_FRAME_MISSION; nonCoordinate.hasCoordinate = false;
    QVERIFY(model.replaceMissionItems({first, second, nonCoordinate}, MissionPlanModel::ManualDraft));
    const quint64 revision = model.revision();
    const QVariantList projection = MissionPlanConverter::mapProjection(model);
    QCOMPARE(projection.size(), 2);
    QCOMPARE(projection.at(0).toMap().value(QStringLiteral("id")).toString(), first.id.toString(QUuid::WithoutBraces));
    QCOMPARE(projection.at(1).toMap().value(QStringLiteral("seq")).toUInt(), 1U);
    QCOMPARE(projection.at(0).toMap().value(QStringLiteral("displaySequence")).toUInt(), 1U);
    QCOMPARE(projection.at(0).toMap().value(QStringLiteral("command")).toUInt(), uint(MAV_CMD_NAV_WAYPOINT));
    QCOMPARE(model.revision(), revision);
    QCOMPARE(model.rowCount(), 3);
}

void MissionPlanConverterTest::projectionExcludesQgcHomeItem()
{
    MissionPlanModel model;
    MissionPlanItem home; home.id = QUuid::createUuid(); home.current = true;
    home.command = MAV_CMD_NAV_WAYPOINT; home.frame = MAV_FRAME_GLOBAL;
    home.latitude = 10.0; home.longitude = 20.0;
    MissionPlanItem waypoint = home; waypoint.id = QUuid::createUuid(); waypoint.current = false;
    waypoint.latitude = 10.1; waypoint.longitude = 20.1;
    QVERIFY(model.replaceMissionItems({home, waypoint}, MissionPlanModel::ImportedMission));

    const QVariantList projection = MissionPlanConverter::mapProjection(model);
    QCOMPARE(projection.size(), 1);
    QCOMPARE(projection.front().toMap().value(QStringLiteral("id")).toString(),
             waypoint.id.toString(QUuid::WithoutBraces));
    QCOMPARE(projection.front().toMap().value(QStringLiteral("seq")).toUInt(), 1U);
    QCOMPARE(projection.front().toMap().value(QStringLiteral("displaySequence")).toUInt(), 1U);
    // Home remains available to protocol/file conversion and is only excluded
    // from the operator-facing map projection.
    QCOMPARE(model.rowCount(), 2);
}

void MissionPlanConverterTest::exportRejectsUnrepresentableItems()
{
    MissionPlanModel model;
    MissionPlanItem item; item.id = QUuid::createUuid(); item.hasCoordinate = false;
    item.latitude = std::numeric_limits<double>::quiet_NaN(); item.longitude = 0;
    QVERIFY(model.insertMissionItem(item));
    QTemporaryDir directory; QVERIFY(directory.isValid());
    const auto result = MissionPlanConverter::exportWplFile(directory.filePath(QStringLiteral("bad.waypoints")), model);
    QVERIFY(!result.success);
    QVERIFY(result.error.contains(QStringLiteral("cannot represent")));
}

QTEST_MAIN(MissionPlanConverterTest)
#include "MissionPlanConverterTest.moc"
