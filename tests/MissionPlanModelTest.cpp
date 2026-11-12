#include <QtTest/QtTest>

#include "mission/MissionPlanModel.h"

#include <limits>

namespace {
MissionPlanItem waypoint(double latitude = 12.3, double longitude = 45.6)
{
    MissionPlanItem item;
    item.id = QUuid::createUuid();
    item.latitude = latitude;
    item.longitude = longitude;
    item.altitude = 42.0f;
    return item;
}
}

class MissionPlanModelTest : public QObject
{
    Q_OBJECT
private slots:
    void emptyModel();
    void insertUpdateRemoveAndRoles();
    void reorderPreservesIdentityAndRenumbersSequences();
    void rejectsDuplicateAndInvalidItems();
    void preservesNonCoordinateItems();
    void replaceClearAndSourceState();
    void selectionDoesNotDirtyContents();
    void revisionOnlyChangesForSuccessfulContentChanges();
    void qmlFacingMapApis();
};

void MissionPlanModelTest::emptyModel()
{
    MissionPlanModel model;
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.sourceState(), MissionPlanModel::Empty);
    QCOMPARE(model.revision(), 0ULL);
    QCOMPARE(model.lastSavedRevision(), 0ULL);
    QVERIFY(!model.dirty());
    QVERIFY(model.selectedItemId().isEmpty());
}

void MissionPlanModelTest::insertUpdateRemoveAndRoles()
{
    MissionPlanModel model;
    MissionPlanItem item = waypoint();
    QVERIFY(model.insertMissionItem(item));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.sourceState(), MissionPlanModel::ManualDraft);
    QCOMPARE(model.data(model.index(0), MissionPlanModel::IdRole).toString(), item.id.toString(QUuid::WithoutBraces));
    QCOMPARE(model.data(model.index(0), MissionPlanModel::SequenceRole).toUInt(), 0U);
    QCOMPARE(model.data(model.index(0), MissionPlanModel::ValidationStatusRole).toInt(), int(MissionPlanModel::Valid));
    QVERIFY(model.roleNames().values().contains("hasCoordinate"));
    QVERIFY(model.roleNames().values().contains("validationDiagnostics"));

    MissionPlanItem changed = *model.missionItemAt(0);
    changed.altitude = 80.0f;
    QVERIFY(model.updateMissionItem(item.id, changed));
    QCOMPARE(model.missionItemAt(0)->altitude, 80.0f);
    QVERIFY(model.removeMissionItem(item.id));
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.sourceState(), MissionPlanModel::Empty);
}

void MissionPlanModelTest::reorderPreservesIdentityAndRenumbersSequences()
{
    MissionPlanModel model;
    const MissionPlanItem first = waypoint(1, 1);
    const MissionPlanItem second = waypoint(2, 2);
    const MissionPlanItem third = waypoint(3, 3);
    QVERIFY(model.insertMissionItem(first)); QVERIFY(model.insertMissionItem(second)); QVERIFY(model.insertMissionItem(third));
    QVERIFY(model.moveMissionItem(0, 2));
    QCOMPARE(model.missionItemAt(0)->id, second.id);
    QCOMPARE(model.missionItemAt(1)->id, third.id);
    QCOMPARE(model.missionItemAt(2)->id, first.id);
    QCOMPARE(model.missionItemAt(0)->sequence, quint16(0));
    QCOMPARE(model.missionItemAt(1)->sequence, quint16(1));
    QCOMPARE(model.missionItemAt(2)->sequence, quint16(2));
    QCOMPARE(model.indexOfId(first.id), 2);
}

void MissionPlanModelTest::rejectsDuplicateAndInvalidItems()
{
    MissionPlanModel model;
    MissionPlanItem first = waypoint();
    QVERIFY(model.insertMissionItem(first));
    const quint64 revision = model.revision();
    MissionPlanItem duplicate = waypoint(); duplicate.id = first.id;
    QVERIFY(!model.insertMissionItem(duplicate));
    QCOMPARE(model.rowCount(), 1); QCOMPARE(model.revision(), revision);
    MissionPlanItem badLatitude = waypoint(91.0, 0.0);
    QVERIFY(!model.insertMissionItem(badLatitude));
    MissionPlanItem badNumber = waypoint(); badNumber.param1 = std::numeric_limits<float>::infinity();
    QVERIFY(!model.insertMissionItem(badNumber));
}

void MissionPlanModelTest::preservesNonCoordinateItems()
{
    MissionPlanModel model;
    MissionPlanItem rtl = waypoint();
    rtl.command = 20; // MAV_CMD_NAV_RETURN_TO_LAUNCH
    rtl.hasCoordinate = false;
    rtl.latitude = std::numeric_limits<double>::quiet_NaN();
    rtl.longitude = std::numeric_limits<double>::quiet_NaN();
    QVERIFY(model.insertMissionItem(rtl));
    QCOMPARE(model.missionItemAt(0)->command, quint16(20));
    QVERIFY(!model.missionItemAt(0)->hasCoordinate);
    QVERIFY(!model.data(model.index(0), MissionPlanModel::LatitudeRole).isValid());
}

void MissionPlanModelTest::replaceClearAndSourceState()
{
    MissionPlanModel model;
    QVector<MissionPlanItem> imported { waypoint(), waypoint() };
    QVERIFY(model.replaceMissionItems(imported, MissionPlanModel::ImportedMission));
    QCOMPARE(model.sourceState(), MissionPlanModel::ImportedMission);
    QVERIFY(!model.dirty());
    QCOMPARE(model.lastSavedRevision(), model.revision());
    model.setSourceState(MissionPlanModel::AutomaticPlannerResult);
    QCOMPARE(model.sourceState(), MissionPlanModel::AutomaticPlannerResult);
    QVERIFY(model.clearMission());
    QCOMPARE(model.sourceState(), MissionPlanModel::Empty);
    QCOMPARE(model.rowCount(), 0);
}

void MissionPlanModelTest::selectionDoesNotDirtyContents()
{
    MissionPlanModel model;
    const MissionPlanItem item = waypoint();
    QVERIFY(model.insertMissionItem(item));
    model.markCurrentRevisionSaved();
    const quint64 revision = model.revision();
    model.setSelectedItemId(item.id.toString(QUuid::WithoutBraces));
    QCOMPARE(model.selectedItemId(), item.id.toString(QUuid::WithoutBraces));
    QCOMPARE(model.revision(), revision);
    QVERIFY(!model.dirty());
    QCOMPARE(model.data(model.index(0), MissionPlanModel::SelectedRole).toBool(), true);
}

void MissionPlanModelTest::revisionOnlyChangesForSuccessfulContentChanges()
{
    MissionPlanModel model;
    const MissionPlanItem item = waypoint();
    QVERIFY(model.insertMissionItem(item));
    const quint64 revision = model.revision();
    QVERIFY(model.updateMissionItem(item.id, *model.missionItemAt(0))); // no content change
    QCOMPARE(model.revision(), revision);
    QVERIFY(!model.moveMissionItem(0, 1));
    QCOMPARE(model.revision(), revision);
    QVERIFY(!model.removeMissionItem(QUuid::createUuid()));
    QCOMPARE(model.revision(), revision);
    model.setSourceState(MissionPlanModel::AutomaticPlannerResult);
    QCOMPARE(model.revision(), revision);
}

void MissionPlanModelTest::qmlFacingMapApis()
{
    MissionPlanModel model;
    const QVariantMap map {
        { QStringLiteral("command"), 16 }, { QStringLiteral("frame"), 3 },
        { QStringLiteral("latitude"), 10.0 }, { QStringLiteral("longitude"), 20.0 },
        { QStringLiteral("altitude"), 30.0 }, { QStringLiteral("hasCoordinate"), true },
    };
    QVERIFY(model.insertItem(map));
    const QVariantMap row = model.itemAt(0);
    const QString id = row.value(QStringLiteral("id")).toString();
    QVERIFY(!id.isEmpty());
    QCOMPARE(model.indexOfId(id), 0);
    QVERIFY(model.updateItem(id, {{ QStringLiteral("altitude"), 40.0 }}));
    QCOMPARE(model.itemById(id).value(QStringLiteral("altitude")).toFloat(), 40.0f);
    QVERIFY(!model.insertItem({{ QStringLiteral("latitude"), 100.0 }}));
    QVERIFY(!model.lastOperationError().isEmpty());
}

QTEST_MAIN(MissionPlanModelTest)
#include "MissionPlanModelTest.moc"
