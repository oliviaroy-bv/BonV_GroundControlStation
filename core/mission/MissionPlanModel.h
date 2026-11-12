#pragma once

#include <QAbstractListModel>
#include <QUuid>
#include <QVector>

#include <optional>

// This is deliberately independent of Mavlink::MissionItem.  Mavlink's type
// describes an in-progress transfer; this type describes the editable plan
// that future planning UI will own.
struct MissionPlanItem
{
    QUuid id;
    quint16 sequence = 0;
    quint16 command = 16; // MAV_CMD_NAV_WAYPOINT
    quint8 frame = 3;     // MAV_FRAME_GLOBAL_RELATIVE_ALT
    double latitude = 0.0;
    double longitude = 0.0;
    float altitude = 0.0f;
    float param1 = 0.0f;
    float param2 = 0.0f;
    float param3 = 0.0f;
    float param4 = 0.0f;
    bool current = false;
    bool autocontinue = true;
    bool hasCoordinate = true;
};

class MissionPlanModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(SourceState sourceState READ sourceState WRITE setSourceState NOTIFY sourceStateChanged)
    Q_PROPERTY(quint64 revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(quint64 lastSavedRevision READ lastSavedRevision NOTIFY lastSavedRevisionChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString selectedItemId READ selectedItemId WRITE setSelectedItemId NOTIFY selectedItemIdChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        SequenceRole,
        CommandRole,
        FrameRole,
        LatitudeRole,
        LongitudeRole,
        AltitudeRole,
        Param1Role,
        Param2Role,
        Param3Role,
        Param4Role,
        CurrentRole,
        AutocontinueRole,
        HasCoordinateRole,
        ValidationStatusRole,
        ValidationDiagnosticsRole,
        SelectedRole,
    };
    Q_ENUM(Role)

    enum SourceState {
        Empty,
        ManualDraft,
        ImportedMission,
        DownloadedVehicleMission,
        AutomaticPlannerResult,
    };
    Q_ENUM(SourceState)

    enum ValidationStatus {
        Valid,
        Invalid,
    };
    Q_ENUM(ValidationStatus)

    explicit MissionPlanModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    SourceState sourceState() const;
    void setSourceState(SourceState sourceState);
    quint64 revision() const;
    quint64 lastSavedRevision() const;
    bool dirty() const;
    QString selectedItemId() const;

    bool insertMissionItem(MissionPlanItem item, int row = -1, QString *error = nullptr);
    bool updateMissionItem(const QUuid &id, const MissionPlanItem &item, QString *error = nullptr);
    bool removeMissionItem(const QUuid &id, QString *error = nullptr);
    bool moveMissionItem(int from, int to, QString *error = nullptr);
    bool clearMission(QString *error = nullptr);
    bool replaceMissionItems(QVector<MissionPlanItem> items, SourceState sourceState, QString *error = nullptr);
    std::optional<MissionPlanItem> missionItemAt(int row) const;
    std::optional<MissionPlanItem> missionItemById(const QUuid &id) const;
    int indexOfId(const QUuid &id) const;

    Q_INVOKABLE bool insertItem(const QVariantMap &item, int row = -1);
    Q_INVOKABLE bool updateItem(const QString &id, const QVariantMap &item);
    Q_INVOKABLE bool removeItem(const QString &id);
    Q_INVOKABLE bool moveItem(int from, int to);
    Q_INVOKABLE bool clear();
    Q_INVOKABLE bool replaceAll(const QVariantList &items, int sourceState);
    Q_INVOKABLE QVariantMap itemAt(int row) const;
    Q_INVOKABLE QVariantMap itemById(const QString &id) const;
    Q_INVOKABLE int indexOfId(const QString &id) const;
    Q_INVOKABLE void setSelectedItemId(const QString &id);
    Q_INVOKABLE void markCurrentRevisionSaved();
    Q_INVOKABLE QString lastOperationError() const;

signals:
    void sourceStateChanged();
    void revisionChanged();
    void lastSavedRevisionChanged();
    void dirtyChanged();
    void countChanged();
    void selectedItemIdChanged();
    void lastOperationErrorChanged();

private:
    static bool validateItem(const MissionPlanItem &item, QString *diagnostics = nullptr);
    static QVariantMap itemToVariantMap(const MissionPlanItem &item, bool selected);
    static bool itemEquals(const MissionPlanItem &left, const MissionPlanItem &right);
    static bool parseId(const QString &text, QUuid *id);
    static bool sourceStateFromInt(int value, SourceState *sourceState);
    bool itemFromVariantMap(const QVariantMap &map, MissionPlanItem *item, bool partial,
                            QString *error) const;
    bool validateCollection(const QVector<MissionPlanItem> &items, QString *error) const;
    void normalizeSequences();
    void contentChanged();
    void setLastOperationError(const QString &error);
    void setSourceStateInternal(SourceState sourceState);

    QVector<MissionPlanItem> items;
    SourceState sourceStateValue = Empty;
    quint64 revisionValue = 0;
    quint64 lastSavedRevisionValue = 0;
    QUuid selectedItem;
    QString lastOperationErrorValue;
};
