#pragma once

#include "ILink.h"

#include <QSerialPort>

class SerialLink : public ILink
{
    Q_OBJECT

public:
    explicit SerialLink(QObject *parent = nullptr);

    bool connectLink(const QString &portName, int baudRate, int timeoutMs = 3000) override;
    void disconnectLink() override;
    QByteArray readAll() override;
    qint64 writeData(const char *data, qint64 size) override;
    bool flush() override;
    bool isConnected() const override;
    QString errorString() const override;

private:
    QSerialPort *serialPort = nullptr;
};
