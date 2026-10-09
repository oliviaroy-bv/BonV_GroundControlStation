#include "GimbalController.h"

#include <QDebug>
#include <QHostAddress>
#include <QTimer>
#include <QtGlobal>

GimbalController::GimbalController(QObject *parent)
    : QObject(parent)
{
}

bool GimbalController::available() const
{
    return backendValue != Backend::None && !cameraHostValue.isEmpty();
}

QString GimbalController::backendName() const
{
    switch (backendValue) {
    case Backend::SkydroidC12:
        return QStringLiteral("Skydroid C12");
    case Backend::None:
    default:
        return QString();
    }
}

QString GimbalController::cameraName() const
{
    return cameraNameValue;
}

void GimbalController::setCameraName(const QString &cameraName)
{
    const QString trimmedName = cameraName.trimmed();
    if (cameraNameValue == trimmedName) {
        return;
    }

    const bool wasAvailable = available();
    const QString oldBackend = backendName();
    cameraNameValue = trimmedName;
    emit cameraNameChanged();
    refreshBackend();
    if (wasAvailable != available()) {
        emit availabilityChanged();
    }
    if (oldBackend != backendName()) {
        emit backendChanged();
    }
}

QString GimbalController::cameraHost() const
{
    return cameraHostValue;
}

void GimbalController::setCameraHost(const QString &cameraHost)
{
    const QString trimmedHost = cameraHost.trimmed();
    if (cameraHostValue == trimmedHost) {
        return;
    }

    const bool wasAvailable = available();
    cameraHostValue = trimmedHost;
    emit cameraHostChanged();
    if (wasAvailable != available()) {
        emit availabilityChanged();
    }
}

int GimbalController::cameraPort() const
{
    return cameraPortValue;
}

void GimbalController::setCameraPort(int cameraPort)
{
    const int boundedPort = qBound(1, cameraPort, 65535);
    if (cameraPortValue == boundedPort) {
        return;
    }

    cameraPortValue = boundedPort;
    emit cameraPortChanged();
}

void GimbalController::sendCommand(const QString &command)
{
    const QString normalizedCommand = command.trimmed().toUpper();
    if (normalizedCommand.isEmpty()) {
        return;
    }

    if (!available()) {
        qWarning() << "GimbalController: no compatible gimbal backend for" << cameraNameValue;
        emit commandRejected(normalizedCommand, QStringLiteral("No compatible gimbal backend"));
        return;
    }

    qDebug() << "GimbalController:" << backendName() << "dispatching" << normalizedCommand;
    switch (backendValue) {
    case Backend::SkydroidC12:
        dispatchC12Command(normalizedCommand);
        break;
    case Backend::None:
    default:
        emit commandRejected(normalizedCommand, QStringLiteral("No compatible gimbal backend"));
        break;
    }
}

void GimbalController::stop()
{
    if (!available()) {
        return;
    }

    if (backendValue == Backend::SkydroidC12) {
        sendC12Udp(buildC12Command(QStringLiteral("#TP"), QStringLiteral("UG"), QStringLiteral("2"),
                                   QStringLiteral("w"), QStringLiteral("PTZ"), QStringLiteral("00")));
    }
}

void GimbalController::center()
{
    sendCommand(QStringLiteral("CENTER"));
}

void GimbalController::sendNormalizedRoi(double x1, double y1, double x2, double y2)
{
    qDebug() << "GimbalController ROI:" << x1 << y1 << x2 << y2;
    emit commandRejected(QStringLiteral("ROI"), QStringLiteral("ROI gimbal command is not implemented for this camera"));
}

void GimbalController::refreshBackend()
{
    const QString normalizedName = cameraNameValue.toLower();
    if (normalizedName.contains(QStringLiteral("c12"))
            || normalizedName.contains(QStringLiteral("skydroid"))) {
        setBackend(Backend::SkydroidC12);
        return;
    }

    setBackend(Backend::None);
}

void GimbalController::setBackend(Backend backend)
{
    if (backendValue == backend) {
        return;
    }

    backendValue = backend;
}

void GimbalController::dispatchC12Command(const QString &command)
{
    if (command.contains(QStringLiteral("PITCH_UP"))) {
        setC12PitchAngle(QStringLiteral("232032"));
    } else if (command.contains(QStringLiteral("PITCH_DOWN"))) {
        setC12PitchAngle(QStringLiteral("DCE032"));
    } else if (command.contains(QStringLiteral("PITCH_LEFT"))) {
        sendC12YawSpeed(-100);
    } else if (command.contains(QStringLiteral("PITCH_RIGHT"))) {
        sendC12YawSpeed(100);
    } else if (command.contains(QStringLiteral("UP"))) {
        sendC12PitchSpeed(60);
    } else if (command.contains(QStringLiteral("DOWN"))) {
        sendC12PitchSpeed(-60);
    } else if (command.contains(QStringLiteral("LEFT"))) {
        sendC12YawSpeed(60);
    } else if (command.contains(QStringLiteral("RIGHT"))) {
        sendC12YawSpeed(-60);
    } else if (command.contains(QStringLiteral("CENTER"))) {
        setC12YawAngle(QStringLiteral("000032"));
        QTimer::singleShot(50, this, [this]() {
            setC12PitchAngle(QStringLiteral("000032"));
        });
    } else if (command.contains(QStringLiteral("OPT_ZOOM_IN"))) {
        zoomC12In();
    } else if (command.contains(QStringLiteral("OPT_ZOOM_OUT"))) {
        zoomC12Out();
    } else if (command.contains(QStringLiteral("DIG_ZOOM_IN"))) {
        digitalZoomC12In();
    } else if (command.contains(QStringLiteral("DIG_ZOOM_OUT"))) {
        digitalZoomC12Out();
    } else if (command.contains(QStringLiteral("MODE_DAY"))) {
        setC12DayMode();
    } else if (command.contains(QStringLiteral("MODE_IR"))) {
        setC12IrMode();
    } else {
        qWarning() << "GimbalController: unknown C12 command:" << command;
        emit commandRejected(command, QStringLiteral("Unknown C12 gimbal command"));
    }
}

QString GimbalController::calculateC12Checksum(const QString &command) const
{
    int crc = 0;
    for (const QChar &character : command) {
        crc += character.unicode();
    }
    return QStringLiteral("%1").arg(crc & 0xFF, 2, 16, QLatin1Char('0')).toUpper();
}

QString GimbalController::buildC12Command(const QString &frameHead,
                                          const QString &address,
                                          const QString &dataLength,
                                          const QString &control,
                                          const QString &identifier,
                                          const QString &data) const
{
    const QString base = frameHead + address + dataLength + control + identifier + data;
    return base + calculateC12Checksum(base);
}

void GimbalController::sendC12Udp(const QString &command)
{
    if (cameraHostValue.isEmpty()) {
        emit commandRejected(command, QStringLiteral("Camera host is empty"));
        return;
    }

    const QByteArray payload = command.toUtf8();
    const qint64 bytesWritten = socket.writeDatagram(payload, QHostAddress(cameraHostValue), cameraPortValue);
    if (bytesWritten != payload.size()) {
        const QString reason = socket.errorString().isEmpty()
                ? QStringLiteral("UDP datagram was not fully written")
                : socket.errorString();
        qWarning() << "GimbalController: UDP send failed" << cameraHostValue << cameraPortValue << command << reason;
        emit commandRejected(command, reason);
        return;
    }

    qDebug() << "GimbalController: UDP" << cameraHostValue << cameraPortValue << command;
}

void GimbalController::setC12PitchAngle(const QString &hex)
{
    sendC12Udp(buildC12Command(QStringLiteral("#TP"), QStringLiteral("UG"), QStringLiteral("6"),
                               QStringLiteral("w"), QStringLiteral("GAP"), hex));
}

void GimbalController::setC12YawAngle(const QString &hex)
{
    sendC12Udp(buildC12Command(QStringLiteral("#TP"), QStringLiteral("UG"), QStringLiteral("6"),
                               QStringLiteral("w"), QStringLiteral("GAY"), hex));
}

void GimbalController::sendC12YawSpeed(int speed)
{
    speed = qBound(-100, speed, 100);
    speed = -speed;

    const int value = static_cast<int>((speed / 100.0) * 127);
    const int8_t signedByte = static_cast<int8_t>(value);
    const uint8_t byte = static_cast<uint8_t>(signedByte);
    const QString data = QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0')).toUpper();
    sendC12Udp(buildC12Command(QStringLiteral("#TP"), QStringLiteral("UG"), QStringLiteral("2"),
                               QStringLiteral("w"), QStringLiteral("GSY"), data));
}

void GimbalController::sendC12PitchSpeed(int speed)
{
    speed = qBound(-100, speed, 100);

    const int value = static_cast<int>((speed / 100.0) * 127);
    const int8_t signedByte = static_cast<int8_t>(value);
    const uint8_t byte = static_cast<uint8_t>(signedByte);
    const QString data = QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0')).toUpper();
    sendC12Udp(buildC12Command(QStringLiteral("#TP"), QStringLiteral("UG"), QStringLiteral("2"),
                               QStringLiteral("w"), QStringLiteral("GSP"), data));
}

void GimbalController::zoomC12In()
{
    sendC12Udp(buildC12Command(QStringLiteral("#TP"), QStringLiteral("UD"), QStringLiteral("2"),
                               QStringLiteral("w"), QStringLiteral("DZM"), QStringLiteral("0A")));
}

void GimbalController::zoomC12Out()
{
    sendC12Udp(buildC12Command(QStringLiteral("#TP"), QStringLiteral("UD"), QStringLiteral("2"),
                               QStringLiteral("w"), QStringLiteral("DZM"), QStringLiteral("0B")));
}

void GimbalController::digitalZoomC12In()
{
    sendC12Udp(buildC12Command(QStringLiteral("#TP"), QStringLiteral("UD"), QStringLiteral("2"),
                               QStringLiteral("w"), QStringLiteral("DZM"), QStringLiteral("02")));
}

void GimbalController::digitalZoomC12Out()
{
    sendC12Udp(buildC12Command(QStringLiteral("#TP"), QStringLiteral("UD"), QStringLiteral("2"),
                               QStringLiteral("w"), QStringLiteral("DZM"), QStringLiteral("01")));
}

void GimbalController::setC12DayMode()
{
    sendC12Udp(buildC12Command(QStringLiteral("#TP"), QStringLiteral("UD"), QStringLiteral("2"),
                               QStringLiteral("w"), QStringLiteral("NIT"), QStringLiteral("00")));
}

void GimbalController::setC12IrMode()
{
    sendC12Udp(buildC12Command(QStringLiteral("#TP"), QStringLiteral("UD"), QStringLiteral("2"),
                               QStringLiteral("w"), QStringLiteral("NIT"), QStringLiteral("01")));
}
