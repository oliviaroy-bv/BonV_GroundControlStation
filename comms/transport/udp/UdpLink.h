#pragma once

#include "ILink.h"

#include <QHostAddress>
#include <QHostInfo>
#include <QUdpSocket>

class UdpLink : public ILink
{
    Q_OBJECT

public:
    explicit UdpLink(QObject *parent = nullptr);

    bool connectLink(const QString &host, int port, int timeoutMs = 3000) override;
    void disconnectLink() override;
    QByteArray readAll() override;
    qint64 writeData(const char *data, qint64 size) override;
    bool flush() override;
    bool isConnected() const override;
    QString errorString() const override;

private:
    void connectResolvedAddress(const QHostAddress &address, int port);

    QUdpSocket *socket = nullptr;
    QHostAddress peerAddress;
    quint16 peerPort = 0;
    bool connectedState = false;
    int pendingLookupId = -1;
};
