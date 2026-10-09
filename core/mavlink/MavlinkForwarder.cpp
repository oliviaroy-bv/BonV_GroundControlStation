#include "MavlinkForwarder.h"

#include <QSettings>
#include <QUdpSocket>

namespace {
constexpr auto enabledKey = "MavlinkForwarding/enabled";
constexpr auto hostKey = "MavlinkForwarding/host";
constexpr auto portKey = "MavlinkForwarding/port";
}

MavlinkForwarder::MavlinkForwarder(QObject *parent)
    : QObject(parent)
    , socket(new QUdpSocket(this))
{
    statisticsTimer.setSingleShot(true);
    statisticsTimer.setInterval(100);
    connect(&statisticsTimer, &QTimer::timeout, this, &MavlinkForwarder::statisticsChanged);
    QSettings settings;
    const bool persistedEnabled = settings.value(QLatin1StringView(enabledKey), false).toBool();
    const QString persistedHost = settings.value(QLatin1StringView(hostKey), QStringLiteral("127.0.0.1")).toString().trimmed();
    const int persistedPort = settings.value(QLatin1StringView(portKey), 14550).toInt();
    apply(persistedEnabled, persistedHost, persistedPort);

    connect(socket, &QUdpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (enabledValue) {
            setState(QStringLiteral("ERROR"), socket->errorString());
        }
    });
}

MavlinkForwarder::~MavlinkForwarder()
{
    socket->close();
}

bool MavlinkForwarder::apply(bool enabled, const QString &host, int port)
{
    const QString cleanedHost = host.trimmed();
    QString error;
    if (enabled && !isValidConfiguration(cleanedHost, port, &error)) {
        // A rejected reconfiguration must never leave a previous destination
        // silently forwarding in the background.
        const bool changed = enabledValue || hostValue != cleanedHost || portValue != port;
        socket->close();
        enabledValue = false;
        hostValue = cleanedHost;
        portValue = port;
        numericAddress = QHostAddress();
        persistConfiguration();
        setState(QStringLiteral("ERROR"), error);
        if (changed) {
            emit configurationChanged();
        }
        return false;
    }

    const bool changed = enabledValue != enabled || hostValue != cleanedHost || portValue != port;
    socket->close();
    enabledValue = enabled;
    hostValue = cleanedHost;
    portValue = port;
    numericAddress = QHostAddress();
    persistConfiguration();

    if (!enabledValue) {
        setState(QStringLiteral("DISABLED"));
    } else {
        // connectToHost uses Qt's normal asynchronous hostname resolution and
        // does not bind or accept any inbound UDP traffic.
        socket->connectToHost(hostValue, static_cast<quint16>(portValue));
        numericAddress.setAddress(hostValue);
        setState(QStringLiteral("READY"));
    }
    if (changed) {
        emit configurationChanged();
    }
    return true;
}

void MavlinkForwarder::disable()
{
    apply(false, hostValue, portValue);
}

void MavlinkForwarder::resetStatistics()
{
    forwardedPacketsValue = 0;
    forwardedBytesValue = 0;
    failedPacketsValue = 0;
    lastForwardedPacketTimeValue = {};
    rateWindowPackets = 0;
    forwardRateValue = 0.0;
    rateClock.invalidate();
    scheduleStatisticsChanged();
}

void MavlinkForwarder::forwardFrame(const MavlinkFrame &frame)
{
    if (!enabledValue || frame.rawPacket().isEmpty()) {
        return;
    }

    // QByteArray is passed directly as one UDP datagram. No MAVLink data is
    // decoded, reserialized, or otherwise changed on this path.
    const qint64 written = numericAddress.isNull()
            ? socket->write(frame.rawPacket())
            : socket->writeDatagram(frame.rawPacket(), numericAddress, static_cast<quint16>(portValue));
    if (written != frame.rawPacket().size()) {
        ++failedPacketsValue;
        setState(QStringLiteral("ERROR"), socket->errorString().isEmpty()
                 ? QStringLiteral("UDP datagram could not be sent") : socket->errorString());
        scheduleStatisticsChanged();
        return;
    }

    ++forwardedPacketsValue;
    forwardedBytesValue += static_cast<quint64>(written);
    ++rateWindowPackets;
    lastForwardedPacketTimeValue = QDateTime::currentDateTimeUtc();
    if (!rateClock.isValid()) {
        rateClock.start();
    }
    updateRate();
    setState(QStringLiteral("FORWARDING"));
    scheduleStatisticsChanged();
}

bool MavlinkForwarder::isValidConfiguration(const QString &host, int port, QString *error) const
{
    if (host.isEmpty()) {
        *error = QStringLiteral("Destination host is required");
        return false;
    }
    if (port < 1 || port > 65535) {
        *error = QStringLiteral("UDP port must be between 1 and 65535");
        return false;
    }
    return true;
}

void MavlinkForwarder::setState(const QString &state, const QString &error)
{
    if (stateValue == state && lastErrorValue == error) {
        return;
    }
    stateValue = state;
    lastErrorValue = error;
    emit stateChanged();
}

void MavlinkForwarder::persistConfiguration() const
{
    QSettings settings;
    settings.setValue(QLatin1StringView(enabledKey), enabledValue);
    settings.setValue(QLatin1StringView(hostKey), hostValue);
    settings.setValue(QLatin1StringView(portKey), portValue);
}

void MavlinkForwarder::updateRate()
{
    const qint64 elapsed = rateClock.elapsed();
    if (elapsed < 1000) {
        return;
    }
    forwardRateValue = static_cast<double>(rateWindowPackets) * 1000.0 / static_cast<double>(elapsed);
    rateWindowPackets = 0;
    rateClock.restart();
}

void MavlinkForwarder::scheduleStatisticsChanged()
{
    if (!statisticsTimer.isActive()) {
        statisticsTimer.start();
    }
}
