#include "MotorController.h"
#include "MavlinkHandler.h"

#include <QDebug>

MotorController::MotorController(
        MavlinkHandler* mavlink,
        QObject* parent)
    : QObject(parent)
    , _mavlink(mavlink)
{
}

int MotorController::motorCount() const
{
    return _motorCount;
}

bool MotorController::propellerConfirmed() const
{
    return _propellerConfirmed;
}

void MotorController::setPropellerConfirmed(bool value)
{
    if (_propellerConfirmed == value)
        return;

    _propellerConfirmed = value;

    emit propellerConfirmedChanged();
}

bool MotorController::areaClear() const
{
    return _areaClear;
}

void MotorController::setAreaClear(bool value)
{
    if (_areaClear == value)
        return;

    _areaClear = value;

    emit areaClearChanged();
}

void MotorController::handleMessage(
        const mavlink_message_t& msg)
{
    switch(msg.msgid)
    {
    case MAVLINK_MSG_ID_HEARTBEAT:
    {
        mavlink_heartbeat_t hb;

        mavlink_msg_heartbeat_decode(
                    &msg,
                    &hb);

        _systemId = msg.sysid;
        _componentId = msg.compid;

        int detectedMotors = 0;

        switch(hb.type)
        {
        case MAV_TYPE_TRICOPTER:
            detectedMotors = 3;
            break;

        case MAV_TYPE_QUADROTOR:
            detectedMotors = 4;
            break;

        case MAV_TYPE_HEXAROTOR:
            detectedMotors = 6;
            break;

        case MAV_TYPE_OCTOROTOR:
            detectedMotors = 8;
            break;

        default:
            detectedMotors = 0;
            break;
        }

        if (detectedMotors > 0
        )
        {
            if(_motorCount != detectedMotors)
        {
            _motorCount = detectedMotors;

            emit motorCountChanged();

            qDebug()
                    << "[MotorController]"
                    << "Detected motors:"
                    << _motorCount;
        }}

        

        break;
    }

    default:
        break;
    }
}

bool MotorController::safetyCheck() const
{
    if(!_mavlink)
    {
        qWarning()
                << "[MotorController]"
                << "No MavlinkHandler";

        return false;
    }

    if(!_mavlink->connected())
    {
        qWarning()
                << "[MotorController]"
                << "Vehicle not connected";

        return false;
    }

    if(_motorCount <= 0)
    {
        qWarning()
                << "[MotorController]"
                << "Motor count not detected";

        return false;
    }

    if(!_propellerConfirmed)
    {
        qWarning()
                << "[MotorController]"
                << "Propellers not removed";

        return false;
    }

    // if(!_areaClear)
    // {
    //     qWarning()
    //             << "[MotorController]"
    //             << "Area not clear";

    //     return false;
    // }

    return true;
}

void MotorController::testMotor(
        int motor,
        int throttlePercent,
        int durationSeconds)
{
    if(!safetyCheck())
        return;

    sendMotorCommand(
                motor,
                throttlePercent,
                durationSeconds);

    emit motorStarted(motor);

    QTimer::singleShot(
        durationSeconds * 1000,
        this,
        [this,motor]()
        {
            emit motorStopped(motor);
        }
    );
}

void MotorController::runAllMotors(
        int throttlePercent,
        int durationSeconds)
{
    if(!safetyCheck())
        return;

    for(int i = 1; i <= _motorCount; ++i)
    {
        sendMotorCommand(
                    i,
                    throttlePercent,
                    durationSeconds);

        emit motorStarted(i);
        QTimer::singleShot(
    durationSeconds * 1000,
    this,
    [this, i]()
    {
        emit motorStopped(i);
    });
    }
}

void MotorController::runSequentialTest(
        int throttlePercent,
        int durationSeconds)
{
    if(!safetyCheck())
        return;

    int delay = 0;

    for(int i = 1; i <= _motorCount; ++i)
    {
        QTimer::singleShot(
                    delay,
                    this,
                    [=]()
                    {
                        sendMotorCommand(
                                    i,
                                    throttlePercent,
                                    durationSeconds);

                        emit motorStarted(i);

                        QTimer::singleShot(
    durationSeconds * 1000,
    this,
    [this, i]()
    {
        emit motorStopped(i);
    });
                    });

        delay += durationSeconds * 1000;
    }
}

void MotorController::stopAllMotors()
{
    if(!_mavlink)
        return;

    for(int i = 1; i <= _motorCount; ++i)
    {
        _mavlink->sendCommandLong(
                    MAV_CMD_DO_MOTOR_TEST,
                    i,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0);

        emit motorStopped(i);
    }

    emit allMotorsStopped();

    qDebug()
            << "[MotorController]"
            << "STOP ALL MOTORS";
}

void MotorController::sendMotorCommand(
        int motor,
        int throttlePercent,
        int durationSeconds)
{
    if(!_mavlink)
        return;

    throttlePercent =
            qBound(1,
                   throttlePercent,
                   20);

    durationSeconds =
            qBound(1,
                   durationSeconds,
                   30);

    /*
      MAV_CMD_DO_MOTOR_TEST

      param1 = Motor Number

      param2 = Throttle Type
               0 = Percentage

      param3 = Throttle %

      param4 = Duration (seconds)
    */

    _mavlink->sendCommandLong(
                MAV_CMD_DO_MOTOR_TEST,
                motor,
                0,
                throttlePercent,
                durationSeconds,
                0,
                0,
                0);

    qDebug()
            << "[MotorController]"
            << "Motor:"
            << motor
            << "Throttle:"
            << throttlePercent
            << "%"
            << "Duration:"
            << durationSeconds
            << "s";
}