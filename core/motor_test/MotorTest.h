#pragma once

#include <QObject>
#include <QTimer>

extern "C" {
#include "common/mavlink.h"
}

class MavlinkHandler;

class MotorController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int motorCount
               READ motorCount
               NOTIFY motorCountChanged)

    Q_PROPERTY(bool propellerConfirmed
               READ propellerConfirmed
               WRITE setPropellerConfirmed
               NOTIFY propellerConfirmedChanged)

    Q_PROPERTY(bool areaClear
               READ areaClear
               WRITE setAreaClear
               NOTIFY areaClearChanged)

public:
    explicit MotorController(
            MavlinkHandler* mavlink,
            QObject* parent = nullptr);

    int motorCount() const;

    bool propellerConfirmed() const;
    void setPropellerConfirmed(bool value);

    bool areaClear() const;
    void setAreaClear(bool value);

    void handleMessage(const mavlink_message_t& msg);

    Q_INVOKABLE void testMotor(
            int motor,
            int throttlePercent,
            int durationSeconds);

    Q_INVOKABLE void runAllMotors(
            int throttlePercent,
            int durationSeconds);

    Q_INVOKABLE void runSequentialTest(
            int throttlePercent,
            int durationSeconds);

    Q_INVOKABLE void stopAllMotors();

signals:

    void motorCountChanged();

    void propellerConfirmedChanged();

    void areaClearChanged();

    void motorStarted(int motor);

    void motorStopped(int motor);

    void allMotorsStopped();

private:

    bool safetyCheck() const;

    void sendMotorCommand(
            int motor,
            int throttlePercent,
            int durationSeconds);

            

private:

    MavlinkHandler* _mavlink = nullptr;

    int _motorCount = 0;

    bool _propellerConfirmed = false;

    bool _areaClear = false;

    uint8_t _systemId = 1;

    uint8_t _componentId = 1;
};