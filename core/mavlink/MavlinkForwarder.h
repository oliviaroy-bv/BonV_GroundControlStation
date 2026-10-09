#pragma once

#include "MavlinkFrame.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QTimer>

class QUdpSocket;

// Copies accepted inbound MAVLink wire frames to one outbound UDP endpoint.
// It deliberately has no MAVLink parser, vehicle, or command responsibility.
class MavlinkForwarder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY configurationChanged)
    Q_PROPERTY(QString host READ host NOTIFY configurationChanged)
    Q_PROPERTY(int port READ port NOTIFY configurationChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(quint64 forwardedPackets READ forwardedPackets NOTIFY statisticsChanged)
    Q_PROPERTY(quint64 forwardedBytes READ forwardedBytes NOTIFY statisticsChanged)
    Q_PROPERTY(quint64 failedPackets READ failedPackets NOTIFY statisticsChanged)
    Q_PROPERTY(QDateTime lastForwardedPacketTime READ lastForwardedPacketTime NOTIFY statisticsChanged)
    Q_PROPERTY(double forwardRate READ forwardRate NOTIFY statisticsChanged)

public:
    explicit MavlinkForwarder(QObject *parent = nullptr);
    ~MavlinkForwarder() override;

    bool enabled() const { return enabledValue; }
    QString host() const { return hostValue; }
    int port() const { return portValue; }
    QString state() const { return stateValue; }
    QString lastError() const { return lastErrorValue; }
    quint64 forwardedPackets() const { return forwardedPacketsValue; }
    quint64 forwardedBytes() const { return forwardedBytesValue; }
    quint64 failedPackets() const { return failedPacketsValue; }
    QDateTime lastForwardedPacketTime() const { return lastForwardedPacketTimeValue; }
    double forwardRate() const { return forwardRateValue; }

public slots:
    void forwardFrame(const MavlinkFrame &frame);

    Q_INVOKABLE bool apply(bool enabled, const QString &host, int port);
    Q_INVOKABLE void disable();
    Q_INVOKABLE void resetStatistics();

signals:
    void configurationChanged();
    void stateChanged();
    void statisticsChanged();

private:
    bool isValidConfiguration(const QString &host, int port, QString *error) const;
    void setState(const QString &state, const QString &error = {});
    void persistConfiguration() const;
    void updateRate();
    void scheduleStatisticsChanged();

    QUdpSocket *socket = nullptr;
    bool enabledValue = false;
    QString hostValue;
    int portValue = 14550;
    QHostAddress numericAddress;
    QString stateValue = QStringLiteral("DISABLED");
    QString lastErrorValue;
    quint64 forwardedPacketsValue = 0;
    quint64 forwardedBytesValue = 0;
    quint64 failedPacketsValue = 0;
    QDateTime lastForwardedPacketTimeValue;
    QElapsedTimer rateClock;
    quint64 rateWindowPackets = 0;
    double forwardRateValue = 0.0;
    QTimer statisticsTimer;
};
