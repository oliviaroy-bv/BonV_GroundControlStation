#pragma once

#include <QObject>
#include <QQueue>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <atomic>
#include <memory>

class MavlinkWorker;

class MavlinkStatusClient final : public QObject
{
    Q_OBJECT
public:
    explicit MavlinkStatusClient(QObject *parent = nullptr);
    ~MavlinkStatusClient() override;

    void connectToVehicle(const QString &transport, const QString &host, int port,
                          const QString &serialDevice, int serialBaud);
    void disconnectFromVehicle();
    void requestRpiStatus(int vehicleSystemId);
    void requestDiagnostic(int request, int vehicleSystemId);
    void requestLedCommand(int commandId, int vehicleSystemId, int watchdogMinutes = 0);

signals:
    void connectionStateChanged(const QString &state);
    void statusMessage(const QString &message);
    void canAvailabilityChanged(bool available);
    void diagnosticValue(const QString &name, int value);

private:
    QThread m_thread;
    MavlinkWorker *m_worker = nullptr;
};
