#pragma once

#include "terrain/TerrainProvider.h"

#include <QObject>
#include <QUrl>

class MissionFileWriter : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit MissionFileWriter(QObject *parent = nullptr);

    QString lastError() const;

    Q_INVOKABLE QString saveMission(const QUrl &folderUrl, const QString &fileName, const QString &content);
    Q_INVOKABLE QString generateMission(const QUrl &folderUrl,
                                        const QString &droneName,
                                        int missionTypeIndex,
                                        int tripModeIndex,
                                        double takeoffLat,
                                        double takeoffLon,
                                        const QString &takeoffName,
                                        double landingLat,
                                        double landingLon,
                                        const QString &landingName,
                                        double takeoffAlt,
                                        double nearHomeAlt,
                                        double terrainClearance,
                                        double droppingAlt,
                                        double landAlt,
                                        double cruiseSpeed,
                                        double delayTime,
                                        double hoverTime);

signals:
    void lastErrorChanged();

private:
    void setLastError(const QString &error);

    QString lastErrorText;
    TerrainProvider terrainProvider;
};
