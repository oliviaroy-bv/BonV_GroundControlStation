#include "TcpLink.h"

#include <QAbstractSocket>

TcpLink::TcpLink(QObject *parent)
    : ILink(parent)
    , socket(new QTcpSocket(this))
    , connectionTimeoutTimer(new QTimer(this))
{
    connectionTimeoutTimer->setSingleShot(true);

    connect(socket, &QTcpSocket::connected, this, [this]() {
        connectionTimeoutTimer->stop();
        emit connected();
    });
    connect(socket, &QTcpSocket::disconnected, this, &ILink::disconnected);
    connect(socket, &QTcpSocket::readyRead, this, &ILink::readyRead);
    connect(socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        connectionTimeoutTimer->stop();
        if (socket->state() != QAbstractSocket::ConnectedState) {
            emit disconnected();
        }
    });
    connect(connectionTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (socket->state() == QAbstractSocket::ConnectedState) {
            return;
        }

        socket->abort();
        emit disconnected();
    });
}

bool TcpLink::connectLink(const QString &host, int port, int timeoutMs)
{
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->abort();
    }

    socket->connectToHost(host, port);
    connectionTimeoutTimer->start(qMax(1, timeoutMs));
    return true;
}

void TcpLink::disconnectLink()
{
    socket->disconnectFromHost();
}

QByteArray TcpLink::readAll()
{
    return socket->readAll();
}

qint64 TcpLink::writeData(const char *data, qint64 size)
{
    return socket->write(data, size);
}

bool TcpLink::flush()
{
    return socket->flush();
}

bool TcpLink::isConnected() const
{
    return socket->state() == QAbstractSocket::ConnectedState;
}

QString TcpLink::errorString() const
{
    return socket->errorString();
}
