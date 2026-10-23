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
    Q_UNUSED(host)
    Q_UNUSED(timeoutMs)

    if (connectedState) {
        disconnectLink();
    }

    if (pendingLookupId != -1) {
        QHostInfo::abortHostLookup(pendingLookupId);
        pendingLookupId = -1;
    }

    if (!socket->bind(QHostAddress::AnyIPv4, static_cast<quint16>(port)))
        return false;

    connectedState = true;
    hasPeer = false;
    emit connected();
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
    socket->close();
    connectedState = false;
    hasPeer = false;
    emit disconnected();
}

QByteArray UdpLink::readAll()
{
    QByteArray data;

    while (socket->hasPendingDatagrams()) {
        QByteArray datagram;
        QHostAddress sender;
        quint16 senderPort = 0;
        datagram.resize(static_cast<int>(socket->pendingDatagramSize()));
        socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        peerAddress = sender;
        peerPort = senderPort;
        hasPeer = true;
        data.append(datagram);
    }

    return data;
}

qint64 UdpLink::writeData(const char *data, qint64 size)
{
    if (!connectedState) {
        return -1;
    }

    if (!hasPeer)
        return -1;
    return socket->writeDatagram(data, size, peerAddress, peerPort);
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
