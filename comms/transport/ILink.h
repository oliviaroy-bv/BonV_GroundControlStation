#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class ILink : public QObject
{
    Q_OBJECT

public:
    explicit ILink(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~ILink() override = default;

    virtual bool connectLink(const QString &host, int port, int timeoutMs = 3000) = 0;
    virtual void disconnectLink() = 0;
    virtual QByteArray readAll() = 0;
    virtual qint64 writeData(const char *data, qint64 size) = 0;
    virtual bool flush() = 0;
    virtual bool isConnected() const = 0;
    virtual QString errorString() const = 0;

signals:
    void connected();
    void disconnected();
    void readyRead();
};
