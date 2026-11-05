#include "mavlinkreceiver.h"

#include "geomath.h"

#include <QMatrix3x3>
#include <QtEndian>

#include <cmath>
#include <cstring>
#include <numbers>

namespace {

template<typename T>
T integerAt(const QByteArray &payload, int offset)
{
    if (offset + static_cast<int>(sizeof(T)) > payload.size())
        return {};
    return qFromLittleEndian<T>(reinterpret_cast<const uchar *>(payload.constData() + offset));
}

float floatAt(const QByteArray &payload, int offset)
{
    const quint32 bits = integerAt<quint32>(payload, offset);
    float result = 0.0f;
    static_assert(sizeof(result) == sizeof(bits));
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

QQuaternion quaternionFromEuler(double roll, double pitch, double yaw)
{
    const double cr = std::cos(roll * 0.5);
    const double sr = std::sin(roll * 0.5);
    const double cp = std::cos(pitch * 0.5);
    const double sp = std::sin(pitch * 0.5);
    const double cy = std::cos(yaw * 0.5);
    const double sy = std::sin(yaw * 0.5);
    return QQuaternion(static_cast<float>(cr * cp * cy + sr * sp * sy),
                       static_cast<float>(sr * cp * cy - cr * sp * sy),
                       static_cast<float>(cr * sp * cy + sr * cp * sy),
                       static_cast<float>(cr * cp * sy - sr * sp * cy)).normalized();
}

} // namespace

MavlinkReceiver::MavlinkReceiver(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &MavlinkReceiver::readPendingDatagrams);
    connect(&m_tcpSocket, &QTcpSocket::readyRead, this, &MavlinkReceiver::readTcpData);
    connect(&m_tcpSocket, &QTcpSocket::connected, this, [this] {
        m_tcpStreamBuffer.clear();
        emit tcpConnectedChanged();
    });
    connect(&m_tcpSocket, &QTcpSocket::disconnected, this, [this] {
        emit tcpConnectedChanged();
        if (m_tcpEnabled) {
            QTimer::singleShot(2000, this, [this] {
                if (m_tcpEnabled && m_tcpSocket.state() == QAbstractSocket::UnconnectedState)
                    connectTcp();
            });
        }
    });
    connect(&m_tcpSocket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError error) {
        if (error != QAbstractSocket::RemoteHostClosedError && m_tcpEnabled)
            emit errorOccurred(QStringLiteral("TCP %1:%2: %3")
                                   .arg(m_tcpHost).arg(m_tcpPort).arg(m_tcpSocket.errorString()));
    });
    m_renderTimer.setInterval(16);
    connect(&m_renderTimer, &QTimer::timeout, this, [this] {
        const bool live = m_clock.isValid() && m_clock.elapsed() - m_lastPacketMs < 2500;
        setConnected(live);
        if (!live && m_demoMode)
            updateDemo();
        else
            updateScenePose(m_bodyToNed);
    });
    m_clock.start();
    m_renderTimer.start();
    start();
}

void MavlinkReceiver::setListenPort(quint16 port)
{
    if (m_listenPort == port)
        return;
    m_listenPort = port;
    emit listenPortChanged();
    if (m_listening) {
        stop();
        start();
    }
}

void MavlinkReceiver::setTcpHost(const QString &host)
{
    const QString trimmed = host.trimmed();
    if (trimmed.isEmpty() || m_tcpHost == trimmed)
        return;
    m_tcpHost = trimmed;
    emit tcpEndpointChanged();
    if (m_tcpEnabled) {
        disconnectTcp();
        connectTcp();
    }
}

void MavlinkReceiver::setTcpPort(quint16 port)
{
    if (port == 0 || m_tcpPort == port)
        return;
    m_tcpPort = port;
    emit tcpEndpointChanged();
    if (m_tcpEnabled) {
        disconnectTcp();
        connectTcp();
    }
}

void MavlinkReceiver::setTcpEnabled(bool enabled)
{
    if (m_tcpEnabled == enabled)
        return;
    m_tcpEnabled = enabled;
    emit tcpEnabledChanged();
    if (enabled)
        connectTcp();
    else
        disconnectTcp();
}

bool MavlinkReceiver::tcpConnected() const
{
    return m_tcpSocket.state() == QAbstractSocket::ConnectedState;
}

void MavlinkReceiver::setDemoMode(bool enabled)
{
    if (m_demoMode == enabled)
        return;
    m_demoMode = enabled;
    emit demoModeChanged();
}

void MavlinkReceiver::setOriginLatitude(double value)
{
    if (qFuzzyCompare(m_originLatitude, value))
        return;
    m_originLatitude = value;
    emit originChanged();
}

void MavlinkReceiver::setOriginLongitude(double value)
{
    if (qFuzzyCompare(m_originLongitude, value))
        return;
    m_originLongitude = value;
    emit originChanged();
}

void MavlinkReceiver::setOriginHeight(double value)
{
    if (qFuzzyCompare(m_originHeight, value))
        return;
    m_originHeight = value;
    emit originChanged();
}

bool MavlinkReceiver::start()
{
    if (m_listening)
        return true;
    if (!m_socket.bind(QHostAddress::AnyIPv4, m_listenPort,
                       QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit errorOccurred(QStringLiteral("Cannot listen on UDP %1: %2")
                               .arg(m_listenPort).arg(m_socket.errorString()));
        return false;
    }
    m_listening = true;
    emit listeningChanged();
    return true;
}

void MavlinkReceiver::stop()
{
    if (!m_listening)
        return;
    m_socket.close();
    m_listening = false;
    setConnected(false);
    emit listeningChanged();
}

void MavlinkReceiver::connectTcp()
{
    if (!m_tcpEnabled || m_tcpSocket.state() == QAbstractSocket::ConnectedState
        || m_tcpSocket.state() == QAbstractSocket::ConnectingState) {
        return;
    }
    m_tcpStreamBuffer.clear();
    m_tcpSocket.connectToHost(m_tcpHost, m_tcpPort);
}

void MavlinkReceiver::disconnectTcp()
{
    if (m_tcpSocket.state() == QAbstractSocket::UnconnectedState)
        return;
    m_tcpSocket.abort();
    m_tcpStreamBuffer.clear();
    emit tcpConnectedChanged();
}

void MavlinkReceiver::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    emit connectedChanged();
}

void MavlinkReceiver::readPendingDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<qsizetype>(m_socket.pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        m_socket.readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        if (!sender.isNull() && senderPort != 0) {
            m_udpPeerAddress = sender;
            m_udpPeerPort = senderPort;
        }
        parseBytes(datagram, m_udpStreamBuffer);
    }
}

void MavlinkReceiver::readTcpData()
{
    parseBytes(m_tcpSocket.readAll(), m_tcpStreamBuffer);
}

void MavlinkReceiver::parseBytes(const QByteArray &bytes, QByteArray &streamBuffer)
{
    streamBuffer.append(bytes);
    while (!streamBuffer.isEmpty()) {
        int magicIndex = -1;
        for (int i = 0; i < streamBuffer.size(); ++i) {
            const uchar byte = static_cast<uchar>(streamBuffer.at(i));
            if (byte == 0xfd || byte == 0xfe) {
                magicIndex = i;
                break;
            }
        }
        if (magicIndex < 0) {
            streamBuffer.clear();
            return;
        }
        if (magicIndex > 0)
            streamBuffer.remove(0, magicIndex);
        if (streamBuffer.size() < 8)
            return;

        const bool version2 = static_cast<uchar>(streamBuffer.at(0)) == 0xfd;
        const int payloadLength = static_cast<uchar>(streamBuffer.at(1));
        const bool signedPacket = version2
            && (static_cast<uchar>(streamBuffer.at(2)) & 0x01) != 0;
        const int headerLength = version2 ? 10 : 6;
        const int frameLength = headerLength + payloadLength + 2 + (signedPacket ? 13 : 0);
        if (streamBuffer.size() < frameLength)
            return;

        const int systemId = static_cast<uchar>(streamBuffer.at(version2 ? 5 : 3));
        const int messageId = version2
            ? static_cast<uchar>(streamBuffer.at(7))
                | (static_cast<uchar>(streamBuffer.at(8)) << 8)
                | (static_cast<uchar>(streamBuffer.at(9)) << 16)
            : static_cast<uchar>(streamBuffer.at(5));
        const QByteArray payload = streamBuffer.mid(headerLength, payloadLength);
        handleMessage(messageId, systemId, payload);
        streamBuffer.remove(0, frameLength);
    }
}

void MavlinkReceiver::handleMessage(int messageId, int systemId, const QByteArray &payload)
{
    if (m_systemId != 0 && systemId != m_systemId)
        return;
    m_systemId = systemId;
    bool changed = false;

    switch (messageId) {
    case 0: // HEARTBEAT
        if (payload.size() >= 9) {
            const quint32 customMode = integerAt<quint32>(payload, 0);
            m_armed = (static_cast<uchar>(payload.at(6)) & 0x80) != 0;
            m_flightMode = copterMode(customMode);
            changed = true;
        }
        break;
    case 30: // ATTITUDE
        if (payload.size() >= 16) {
            m_bodyToNed = quaternionFromEuler(floatAt(payload, 4), floatAt(payload, 8),
                                              floatAt(payload, 12));
            changed = true;
        }
        break;
    case 31: // ATTITUDE_QUATERNION
        if (payload.size() >= 20) {
            m_bodyToNed = QQuaternion(floatAt(payload, 4), floatAt(payload, 8),
                                      floatAt(payload, 12), floatAt(payload, 16)).normalized();
            changed = true;
        }
        break;
    case 33: // GLOBAL_POSITION_INT
        // MAVLink 2 may trim trailing zero fields. Position and altitude occupy
        // the first 20 bytes; velocity begins at byte 20 and is optional here.
        if (payload.size() >= 20) {
            m_latitude = integerAt<qint32>(payload, 4) / 1.0e7;
            m_longitude = integerAt<qint32>(payload, 8) / 1.0e7;
            m_altitudeMsl = integerAt<qint32>(payload, 12) / 1000.0;
            m_relativeAltitude = integerAt<qint32>(payload, 16) / 1000.0;
            if (payload.size() >= 24) {
                const double north = integerAt<qint16>(payload, 20) / 100.0;
                const double east = integerAt<qint16>(payload, 22) / 100.0;
                m_groundspeed = std::hypot(north, east);
            } else {
                m_groundspeed = 0.0;
            }
            changed = true;
        }
        break;
    default:
        break;
    }

    if (changed) {
        m_lastPacketMs = m_clock.elapsed();
        requestTelemetryRates();
        emit telemetryChanged();
    }
}

quint16 MavlinkReceiver::mavlinkChecksum(const QByteArray &frameWithoutMagic, quint8 crcExtra)
{
    quint16 crc = 0xffff;
    const auto accumulate = [&crc](quint8 value) {
        value ^= static_cast<quint8>(crc & 0xff);
        value ^= value << 4;
        crc = (crc >> 8) ^ (static_cast<quint16>(value) << 8)
            ^ (static_cast<quint16>(value) << 3)
            ^ (static_cast<quint16>(value) >> 4);
    };
    for (const char byte : frameWithoutMagic)
        accumulate(static_cast<quint8>(byte));
    accumulate(crcExtra);
    return crc;
}

void MavlinkReceiver::requestTelemetryRates()
{
    if (m_systemId <= 0)
        return;
    const qint64 now = m_clock.elapsed();
    if (now - m_lastRateRequestMs < 2500)
        return;
    m_lastRateRequestMs = now;

    sendSetMessageInterval(33, 30.0); // GLOBAL_POSITION_INT
    sendSetMessageInterval(30, 30.0); // ATTITUDE
    sendSetMessageInterval(31, 30.0); // ATTITUDE_QUATERNION
    sendSetMessageInterval(0, 1.0);   // HEARTBEAT
}

void MavlinkReceiver::sendMavlink2Message(quint32 messageId, quint8 targetSystem,
                                          quint8 targetComponent, const QByteArray &payload,
                                          quint8 crcExtra)
{
    QByteArray body;
    body.reserve(9 + payload.size() + 2);
    body.append(static_cast<char>(payload.size()));
    body.append('\0'); // incompat flags
    body.append('\0'); // compat flags
    body.append(static_cast<char>(m_txSequence++));
    body.append(static_cast<char>(targetSystem));
    body.append(static_cast<char>(targetComponent));
    body.append(static_cast<char>(messageId & 0xff));
    body.append(static_cast<char>((messageId >> 8) & 0xff));
    body.append(static_cast<char>((messageId >> 16) & 0xff));
    body.append(payload);

    const quint16 crc = mavlinkChecksum(body, crcExtra);
    body.append(static_cast<char>(crc & 0xff));
    body.append(static_cast<char>((crc >> 8) & 0xff));

    QByteArray frame;
    frame.reserve(1 + body.size());
    frame.append(static_cast<char>(0xfd));
    frame.append(body);

    if (m_tcpSocket.state() == QAbstractSocket::ConnectedState)
        m_tcpSocket.write(frame);
    if (!m_udpPeerAddress.isNull() && m_udpPeerPort != 0)
        m_socket.writeDatagram(frame, m_udpPeerAddress, m_udpPeerPort);
}

void MavlinkReceiver::sendSetMessageInterval(quint32 messageId, double hz)
{
    constexpr quint16 MavCmdSetMessageInterval = 511;
    QByteArray payload(33, '\0');
    const auto writeFloat = [&payload](int offset, float value) {
        quint32 bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        qToLittleEndian(bits, reinterpret_cast<uchar *>(payload.data() + offset));
    };

    writeFloat(0, static_cast<float>(messageId));
    writeFloat(4, static_cast<float>(1000000.0 / hz));
    qToLittleEndian<quint16>(MavCmdSetMessageInterval,
                             reinterpret_cast<uchar *>(payload.data() + 28));
    payload[30] = static_cast<char>(m_systemId);
    payload[31] = 1;

    sendMavlink2Message(76, static_cast<quint8>(m_systemId), 1, payload, 152);
}

void MavlinkReceiver::updateDemo()
{
    const double seconds = m_clock.elapsed() / 1000.0;
    const double angle = seconds * 0.18;
    const double radius = 330.0;
    const double north = radius * std::cos(angle);
    const double east = radius * std::sin(angle);
    m_latitude = m_originLatitude + north / 111320.0;
    m_longitude = m_originLongitude
        + east / (111320.0 * std::cos(GeoMath::degreesToRadians(m_originLatitude)));
    m_relativeAltitude = 95.0 + 24.0 * std::sin(seconds * 0.35);
    m_altitudeMsl = m_originHeight + m_relativeAltitude;
    m_groundspeed = radius * 0.18;
    m_armed = true;
    m_systemId = 1;
    m_flightMode = QStringLiteral("DEMO ORBIT");
    m_bodyToNed = quaternionFromEuler(0.08 * std::sin(seconds * 0.7), 0.03,
                                      angle + std::numbers::pi / 2.0);
    emit telemetryChanged();
    updateScenePose(m_bodyToNed);
}

void MavlinkReceiver::updateScenePose(const QQuaternion &bodyToNed)
{
    const auto enu = GeoMath::latLonHeightToEnu(
        m_latitude, m_longitude, m_originHeight + m_relativeAltitude,
        m_originLatitude, m_originLongitude, m_originHeight);
    m_scenePosition = QVector3D(static_cast<float>(enu.x), static_cast<float>(enu.z),
                                static_cast<float>(-enu.y));
    m_sceneRotation = sceneQuaternion(bodyToNed);
    emit poseChanged();
}

QQuaternion MavlinkReceiver::sceneQuaternion(const QQuaternion &bodyToNed)
{
    const QMatrix3x3 rotation = bodyToNed.toRotationMatrix();
    const float mapping[3][3] = {{0.0f, 1.0f, 0.0f},
                                 {0.0f, 0.0f, -1.0f},
                                 {-1.0f, 0.0f, 0.0f}};
    QMatrix3x3 scene;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            float value = 0.0f;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j)
                    value += mapping[row][i] * rotation(i, j) * mapping[column][j];
            }
            scene(row, column) = value;
        }
    }
    return QQuaternion::fromRotationMatrix(scene).normalized();
}

QString MavlinkReceiver::copterMode(quint32 customMode)
{
    switch (customMode) {
    case 0: return QStringLiteral("STABILIZE");
    case 2: return QStringLiteral("ALT HOLD");
    case 3: return QStringLiteral("AUTO");
    case 4: return QStringLiteral("GUIDED");
    case 5: return QStringLiteral("LOITER");
    case 6: return QStringLiteral("RTL");
    case 9: return QStringLiteral("LAND");
    case 16: return QStringLiteral("POSHOLD");
    default: return QStringLiteral("MODE %1").arg(customMode);
    }
}
