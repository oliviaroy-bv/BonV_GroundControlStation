#pragma once

#include "MissionPlanModel.h"

#include <QObject>
#include <QUrl>

#include "mavlink/Mavlink.h"

// Converts external and protocol representations at explicit boundaries.
// It never mutates a MissionPlanModel until a complete input has parsed and
// validated successfully.
class MissionPlanConverter final : public QObject
{
    Q_OBJECT
public:
    struct Result {
        bool success = false;
        QString error;
        int line = -1;
    };

    explicit MissionPlanConverter(QObject *parent = nullptr);

    static Result importWplFile(const QString &path, MissionPlanModel *model,
                                MissionPlanModel::SourceState sourceState);
    static Result exportWplFile(const QString &path, const MissionPlanModel &model);
    static Result importVehicleMission(const QVector<MavlinkMissionItem> &items,
                                       MissionPlanModel *model, bool replaceDirty = false);
    // Produces a value snapshot suitable for a MAVLink transfer.  The model
    // remains untouched while a transfer is active.
    static Result exportVehicleMission(const MissionPlanModel &model,
                                       QVector<MavlinkMissionItem> *items);
    static QVariantList mapProjection(const MissionPlanModel &model);

    // Context-property adapters used by FlyView.  The QObject argument keeps
    // the model independent of QML registration and gives QML no mutable row
    // storage.
    Q_INVOKABLE QVariantMap importWplFile(const QUrl &path, QObject *model, int sourceState);
    Q_INVOKABLE QString exportWplFile(const QUrl &path, QObject *model);
    Q_INVOKABLE QVariantList mapProjection(QObject *model) const;
    Q_INVOKABLE QString lastError() const;

private:
    static Result parseWpl(const QString &path, QVector<MissionPlanItem> *items);
    static bool isGlobalFrame(quint8 frame);
    static bool finite(float value);
    static bool finite(double value);
    static Result makeResultError(const QString &error, int line = -1);
    void setLastError(const QString &error);

    QString lastErrorValue;
};
