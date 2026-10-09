#include "SerialLink.h"

SerialLink::SerialLink(QObject *parent)
    : ILink(parent)
    , serialPort(new QSerialPort(this))
{
    connect(serialPort, &QSerialPort::readyRead, this, &ILink::readyRead);
    connect(serialPort, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::ResourceError) {
            emit disconnected();
        }
    });
}

bool SerialLink::connectLink(const QString &portName, int baudRate, int timeoutMs)
{
    Q_UNUSED(timeoutMs)

    if (serialPort->isOpen()) {
        serialPort->close();
    }

    serialPort->setPortName(portName);
    serialPort->setBaudRate(baudRate);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!serialPort->open(QIODevice::ReadWrite)) {
        return false;
    }

    serialPort->setDataTerminalReady(true);
    serialPort->setRequestToSend(true);
    emit connected();
    return true;
}

void SerialLink::disconnectLink()
{
    if (!serialPort->isOpen()) {
        return;
    }

    serialPort->close();
    emit disconnected();
}

QByteArray SerialLink::readAll()
{
    return serialPort->readAll();
}

qint64 SerialLink::writeData(const char *data, qint64 size)
{
    return serialPort->write(data, size);
}

bool SerialLink::flush()
{
    return serialPort->flush();
}

bool SerialLink::isConnected() const
{
    return serialPort->isOpen();
}

QString SerialLink::errorString() const
{
    return serialPort->errorString();
}
