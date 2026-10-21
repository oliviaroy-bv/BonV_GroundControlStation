#include "MavlinkStatusClient.h"

#include <QMetaObject>
#include <QSerialPort>
#include <QTcpSocket>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <cstring>

extern "C" {
#include <mavlink/common/mavlink.h>
}

namespace {
constexpr int GCS_SYSTEM_ID = 255;
constexpr int GCS_COMPONENT_ID = MAV_COMP_ID_MISSIONPLANNER;
constexpr int RPI_COMPONENT_ID = MAV_COMP_ID_ONBOARD_COMPUTER;
constexpr uint16_t MAV_CMD_RPI_STATUS_REQUEST = MAV_CMD_USER_1;
constexpr uint16_t MAV_CMD_RPI_LED_COMMAND = MAV_CMD_USER_2;

struct PendingCommand {
    enum Type { Diagnostic, Led } type;
    int value = 0;
    int vehicleSystemId = 1;
    int extra = 0;
};
}

class MavlinkWorker final : public QObject
{
    Q_OBJECT
public:
    explicit MavlinkWorker(QObject *parent = nullptr) : QObject(parent)
    {
        m_heartbeatTimer.setInterval(1000);
        connect(&m_heartbeatTimer, &QTimer::timeout, this, &MavlinkWorker::sendHeartbeat);
    }

    ~MavlinkWorker() override { closeConnection(); }

public slots:
    void connectToVehicle(QString transport, QString host, int port,
                          QString serialDevice, int serialBaud)
    {
        closeConnection();
        m_transport = transport;
        emit connectionStateChanged("Connecting");

        if (transport == "Serial") {
            auto *serial = new QSerialPort(this);
            serial->setPortName(serialDevice);
            serial->setBaudRate(serialBaud);
            if (!serial->open(QIODevice::ReadWrite)) {
                emit connectionStateChanged(QString("Connection error: %1").arg(serial->errorString()));
                serial->deleteLater();
                emit connectionStateChanged("Disconnected");
                return;
            }
            m_serial = serial;
            connect(serial, &QSerialPort::readyRead, this, &MavlinkWorker::readSerial);
            connect(serial, &QSerialPort::errorOccurred, this, [this, serial](QSerialPort::SerialPortError e) {
                if (e != QSerialPort::NoError && serial->isOpen())
                    emit statusMessage(QString("Serial error: %1").arg(serial->errorString()));
            });
        } else {
            auto *socket = new QTcpSocket(this);
            socket->connectToHost(host, static_cast<quint16>(port));
            if (!socket->waitForConnected(5000)) {
                emit connectionStateChanged(QString("Connection error: %1").arg(socket->errorString()));
                socket->deleteLater();
                emit connectionStateChanged("Disconnected");
                return;
            }
            m_socket = socket;
            connect(socket, &QTcpSocket::readyRead, this, &MavlinkWorker::readTcp);
            connect(socket, &QTcpSocket::disconnected, this, [this]() {
                emit connectionStateChanged("Disconnected");
                m_heartbeatTimer.stop();
            });
        }

        emit connectionStateChanged("Connected");
        m_heartbeatTimer.start();
        sendHeartbeat();
    }

    void disconnectFromVehicle()
    {
        closeConnection();
        emit connectionStateChanged("Disconnected");
    }

    void enqueueDiagnostic(int request, int vehicleSystemId)
    {
        m_commands.enqueue({PendingCommand::Diagnostic, request, vehicleSystemId, 0});
        processCommands();
    }

    void enqueueLed(int commandId, int vehicleSystemId, int watchdogMinutes)
    {
        m_commands.enqueue({PendingCommand::Led, commandId, vehicleSystemId, watchdogMinutes});
        processCommands();
    }

signals:
    void connectionStateChanged(const QString &state);
    void statusMessage(const QString &message);
    void canAvailabilityChanged(bool available);
    void diagnosticValue(const QString &name, int value);

private slots:
    void readTcp() { if (m_socket) parseBytes(m_socket->readAll()); }
    void readSerial() { if (m_serial) parseBytes(m_serial->readAll()); }

private:
    void closeConnection()
    {
        m_heartbeatTimer.stop();
        if (m_socket) {
            m_socket->disconnect(this);
            m_socket->abort();
            m_socket->deleteLater();
            m_socket = nullptr;
        }
        if (m_serial) {
            m_serial->disconnect(this);
            m_serial->close();
            m_serial->deleteLater();
            m_serial = nullptr;
        }
        m_commands.clear();
    }

    void writeBytes(const char *data, qsizetype length)
    {
        if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState)
            m_socket->write(data, length);
        else if (m_serial && m_serial->isOpen())
            m_serial->write(data, length);
    }

    void sendMessage(mavlink_message_t &message)
    {
        uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
        const uint16_t length = mavlink_msg_to_send_buffer(buffer, &message);
        writeBytes(reinterpret_cast<const char *>(buffer), length);
    }

    void sendHeartbeat()
    {
        if (!isConnected()) return;
        mavlink_message_t message{};
        mavlink_msg_heartbeat_pack(GCS_SYSTEM_ID, GCS_COMPONENT_ID, &message,
                                   MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0, 0, MAV_STATE_ACTIVE);
        sendMessage(message);
        processCommands();
    }

    bool isConnected() const
    {
        return (m_socket && m_socket->state() == QAbstractSocket::ConnectedState)
               || (m_serial && m_serial->isOpen());
    }

    void processCommands()
    {
        if (!isConnected()) return;
        while (!m_commands.isEmpty()) {
            const PendingCommand cmd = m_commands.dequeue();
            mavlink_message_t message{};
            if (cmd.type == PendingCommand::Diagnostic) {
                mavlink_msg_command_long_pack(
                    GCS_SYSTEM_ID, GCS_COMPONENT_ID, &message,
                    static_cast<uint8_t>(cmd.vehicleSystemId), RPI_COMPONENT_ID,
                    MAV_CMD_RPI_STATUS_REQUEST, 0,
                    static_cast<float>(cmd.value), 0, 0, 0, 0, 0, 0);
                sendMessage(message);
                emit statusMessage("Requested RPi diagnostics");
            } else {
                mavlink_msg_command_long_pack(
                    GCS_SYSTEM_ID, GCS_COMPONENT_ID, &message,
                    static_cast<uint8_t>(cmd.vehicleSystemId), RPI_COMPONENT_ID,
                    MAV_CMD_RPI_LED_COMMAND, 0,
                    static_cast<float>(cmd.value), static_cast<float>(cmd.extra),
                    0, 0, 0, 0, 0);
                sendMessage(message);
                emit statusMessage("Sent LED/actuator command");
            }
        }
    }

    void parseBytes(const QByteArray &bytes)
    {
        for (unsigned char byte : bytes) {
            mavlink_message_t message{};
            if (mavlink_parse_char(MAVLINK_COMM_0, byte, &message, &m_status)) {
                processMessage(message);
            }
        }
    }

    void processMessage(const mavlink_message_t &message)
    {
        switch (message.msgid) {
        case MAVLINK_MSG_ID_COMMAND_ACK: {
            mavlink_command_ack_t ack{};
            mavlink_msg_command_ack_decode(&message, &ack);
            if (ack.command == MAV_CMD_RPI_STATUS_REQUEST)
                emit statusMessage(QString("RPi request result: %1").arg(ack.result));
            else if (ack.command == MAV_CMD_RPI_LED_COMMAND)
                emit statusMessage(QString("LED command result: %1").arg(ack.result));
            break;
        }
        case MAVLINK_MSG_ID_STATUSTEXT: {
            mavlink_statustext_t status{};
            mavlink_msg_statustext_decode(&message, &status);
            emit statusMessage(QString::fromLatin1(status.text, strnlen(status.text, sizeof(status.text))));
            break;
        }
        case MAVLINK_MSG_ID_NAMED_VALUE_INT: {
            mavlink_named_value_int_t value{};
            mavlink_msg_named_value_int_decode(&message, &value);
            const QString name = QString::fromLatin1(value.name, strnlen(value.name, sizeof(value.name)));
            if (name == "CAN_AVAIL")
                emit canAvailabilityChanged(value.value != 0);
            else if (name == "LED_RESULT")
                emit statusMessage(QString("LED command status byte: 0x%1").arg(value.value, 2, 16, QLatin1Char('0')).toUpper());
            else
                emit diagnosticValue(name, value.value);
            break;
        }
        default:
            break;
        }
    }

    QString m_transport;
    QTcpSocket *m_socket = nullptr;
    QSerialPort *m_serial = nullptr;
    QTimer m_heartbeatTimer;
    QQueue<PendingCommand> m_commands;
    mavlink_status_t m_status{};
};

MavlinkStatusClient::MavlinkStatusClient(QObject *parent) : QObject(parent)
{
    m_worker = new MavlinkWorker;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &MavlinkWorker::connectionStateChanged, this, &MavlinkStatusClient::connectionStateChanged);
    connect(m_worker, &MavlinkWorker::statusMessage, this, &MavlinkStatusClient::statusMessage);
    connect(m_worker, &MavlinkWorker::canAvailabilityChanged, this, &MavlinkStatusClient::canAvailabilityChanged);
    connect(m_worker, &MavlinkWorker::diagnosticValue, this, &MavlinkStatusClient::diagnosticValue);
    m_thread.start();
}

MavlinkStatusClient::~MavlinkStatusClient()
{
    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "disconnectFromVehicle", Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}

void MavlinkStatusClient::connectToVehicle(const QString &transport, const QString &host, int port,
                                           const QString &serialDevice, int serialBaud)
{
    QMetaObject::invokeMethod(m_worker, "connectToVehicle", Qt::QueuedConnection,
                              Q_ARG(QString, transport), Q_ARG(QString, host), Q_ARG(int, port),
                              Q_ARG(QString, serialDevice), Q_ARG(int, serialBaud));
}

void MavlinkStatusClient::disconnectFromVehicle()
{
    QMetaObject::invokeMethod(m_worker, "disconnectFromVehicle", Qt::QueuedConnection);
}

void MavlinkStatusClient::requestRpiStatus(int vehicleSystemId)
{
    requestDiagnostic(1, vehicleSystemId);
}

void MavlinkStatusClient::requestDiagnostic(int request, int vehicleSystemId)
{
    QMetaObject::invokeMethod(m_worker, "enqueueDiagnostic", Qt::QueuedConnection,
                              Q_ARG(int, request), Q_ARG(int, vehicleSystemId));
}

void MavlinkStatusClient::requestLedCommand(int commandId, int vehicleSystemId, int watchdogMinutes)
{
    QMetaObject::invokeMethod(m_worker, "enqueueLed", Qt::QueuedConnection,
                              Q_ARG(int, commandId), Q_ARG(int, vehicleSystemId), Q_ARG(int, watchdogMinutes));
}

#include "MavlinkStatusClient.moc"
