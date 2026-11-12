#pragma once
#include <QObject>
#include <QVariantList>
#include <QTimer>
#include <QStringDecoder>
#include <QElapsedTimer>
#include <QHash>

class Mavlink;
class MavlinkConsoleController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString target READ target NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(bool commandMode READ commandMode NOTIFY changed)
    Q_PROPERTY(QString modeLabel READ modeLabel NOTIFY changed)
    Q_PROPERTY(bool armingAllowed READ armingAllowed WRITE setArmingAllowed NOTIFY changed)
public:
    explicit MavlinkConsoleController(Mavlink *mavlink);
    QString status() const;
    QString target() const;
    bool available() const;
    bool commandMode() const;
    QString modeLabel() const;
    bool armingAllowed() const { return armingAllowedValue; }
    void setArmingAllowed(bool value) { if (armingAllowedValue != value) { armingAllowedValue = value; emit changed(); } }
    bool running() const { return commandMode() ? available() : session; }
    QVariantList history() const { return entries; }
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool send(const QString &text);
    Q_INVOKABLE bool interrupt();
    Q_INVOKABLE void clear();
signals:
    void changed();
    void historyChanged();
private:
    void reset();
    void write(const QByteArray &bytes, bool release = false);
    void append(const QString &text, const QString &kind);
    bool executeCommand(const QString &text);
    void clearPending();
    void handleCommandResponse(const struct __mavlink_message &msg);
    Mavlink *transport;
    QTimer poll;
    QTimer startup;
    int firmware = -1;
    bool session = false;
    bool confirmed = false;
    bool timedOut = false;
    QVariantList entries;
    QStringDecoder decoder{QStringDecoder::Utf8};
    int escapeState = 0;
    QElapsedTimer heartbeatAge;
    QTimer responseTimer;
    int vehicleType = -1;
    int pendingMessage = -1;
    int pendingCommand = -1;
    int pendingMode = -1;
    QString pendingParameter;
    quint64 pendingWriteId = 0;
    bool armingAllowedValue = false;
};
