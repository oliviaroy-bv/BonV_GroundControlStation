#pragma once

#include "ILink.h"

#include <QTcpSocket>
#include <QTimer>

class TcpLink : public ILink
{
    Q_OBJECT

public:
    explicit TcpLink(QObject *parent = nullptr);

    bool connectLink(const QString &host, int port, int timeoutMs = 3000) override;
    void disconnectLink() override;
    QByteArray readAll() override;
    qint64 writeData(const char *data, qint64 size) override;
    bool flush() override;
    bool isConnected() const override;
    QString errorString() const override;

private:
    QTcpSocket *socket = nullptr;
    QTimer *connectionTimeoutTimer = nullptr;
};
