#include "UdpLink.h"

#include <QHostInfo>

UdpLink::UdpLink(QObject *parent)
    : ILink(parent)
    , socket(new QUdpSocket(this))
{
    connect(socket, &QUdpSocket::readyRead, this, &ILink::readyRead);
}

bool UdpLink::connectLink(const QString &host, int port, int timeoutMs)
{
    Q_UNUSED(timeoutMs)

    if (connectedState) {
        disconnectLink();
    }

    if (pendingLookupId != -1) {
        QHostInfo::abortHostLookup(pendingLookupId);
        pendingLookupId = -1;
    }

    QHostAddress parsedAddress;
    if (parsedAddress.setAddress(host)) {
        connectResolvedAddress(parsedAddress, port);
        return true;
    }

    pendingLookupId = QHostInfo::lookupHost(host, this, [this, port](const QHostInfo &hostInfo) {
        pendingLookupId = -1;
        if (hostInfo.error() != QHostInfo::NoError || hostInfo.addresses().isEmpty()) {
            emit disconnected();
            return;
        }

        connectResolvedAddress(hostInfo.addresses().first(), port);
    });
    return true;
}

void UdpLink::disconnectLink()
{
    if (!connectedState) {
        if (pendingLookupId != -1) {
            QHostInfo::abortHostLookup(pendingLookupId);
            pendingLookupId = -1;
            emit disconnected();
        }
        return;
    }

    socket->disconnectFromHost();
    connectedState = false;
    emit disconnected();
}

QByteArray UdpLink::readAll()
{
    QByteArray data;

    while (socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(socket->pendingDatagramSize()));
        socket->readDatagram(datagram.data(), datagram.size());
        data.append(datagram);
    }

    return data;
}

qint64 UdpLink::writeData(const char *data, qint64 size)
{
    if (!connectedState) {
        return -1;
    }

    return socket->write(data, size);
}

bool UdpLink::flush()
{
    return socket->flush();
}

bool UdpLink::isConnected() const
{
    return connectedState;
}

QString UdpLink::errorString() const
{
    return socket->errorString();
}

void UdpLink::connectResolvedAddress(const QHostAddress &address, int port)
{
    peerAddress = address;
    peerPort = static_cast<quint16>(port);
    socket->connectToHost(peerAddress, peerPort);

    connectedState = true;
    emit connected();
}
