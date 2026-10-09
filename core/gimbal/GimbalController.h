#pragma once

#include <QObject>
#include <QString>
#include <QUdpSocket>

class GimbalController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)
    Q_PROPERTY(QString backendName READ backendName NOTIFY backendChanged)
    Q_PROPERTY(QString cameraName READ cameraName WRITE setCameraName NOTIFY cameraNameChanged)
    Q_PROPERTY(QString cameraHost READ cameraHost WRITE setCameraHost NOTIFY cameraHostChanged)
    Q_PROPERTY(int cameraPort READ cameraPort WRITE setCameraPort NOTIFY cameraPortChanged)

public:
    explicit GimbalController(QObject *parent = nullptr);

    bool available() const;
    QString backendName() const;

    QString cameraName() const;
    void setCameraName(const QString &cameraName);

    QString cameraHost() const;
    void setCameraHost(const QString &cameraHost);

    int cameraPort() const;
    void setCameraPort(int cameraPort);

    Q_INVOKABLE void sendCommand(const QString &command);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void center();
    Q_INVOKABLE void sendNormalizedRoi(double x1, double y1, double x2, double y2);

signals:
    void availabilityChanged();
    void backendChanged();
    void cameraNameChanged();
    void cameraHostChanged();
    void cameraPortChanged();
    void commandRejected(QString command, QString reason);

private:
    enum class Backend {
        None,
        SkydroidC12
    };

    void refreshBackend();
    void setBackend(Backend backend);
    void dispatchC12Command(const QString &command);
    QString calculateC12Checksum(const QString &command) const;
    QString buildC12Command(const QString &frameHead,
                            const QString &address,
                            const QString &dataLength,
                            const QString &control,
                            const QString &identifier,
                            const QString &data) const;
    void sendC12Udp(const QString &command);
    void setC12PitchAngle(const QString &hex);
    void setC12YawAngle(const QString &hex);
    void sendC12YawSpeed(int speed);
    void sendC12PitchSpeed(int speed);
    void zoomC12In();
    void zoomC12Out();
    void digitalZoomC12In();
    void digitalZoomC12Out();
    void setC12DayMode();
    void setC12IrMode();

    QUdpSocket socket;
    Backend backendValue = Backend::None;
    QString cameraNameValue;
    QString cameraHostValue;
    int cameraPortValue = 5000;
};
