#pragma once

#include <QElapsedTimer>
#include <QHostAddress>
#include <QObject>
#include <QQmlEngine>
#include <QQuaternion>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <QVector3D>

class MavlinkReceiver : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(quint16 listenPort READ listenPort WRITE setListenPort NOTIFY listenPortChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
    Q_PROPERTY(QString tcpHost READ tcpHost WRITE setTcpHost NOTIFY tcpEndpointChanged)
    Q_PROPERTY(quint16 tcpPort READ tcpPort WRITE setTcpPort NOTIFY tcpEndpointChanged)
    Q_PROPERTY(bool tcpEnabled READ tcpEnabled WRITE setTcpEnabled NOTIFY tcpEnabledChanged)
    Q_PROPERTY(bool tcpConnected READ tcpConnected NOTIFY tcpConnectedChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool demoMode READ demoMode WRITE setDemoMode NOTIFY demoModeChanged)
    Q_PROPERTY(int systemId READ systemId NOTIFY telemetryChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY telemetryChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY telemetryChanged)
    Q_PROPERTY(double altitudeMsl READ altitudeMsl NOTIFY telemetryChanged)
    Q_PROPERTY(double relativeAltitude READ relativeAltitude NOTIFY telemetryChanged)
    Q_PROPERTY(double groundspeed READ groundspeed NOTIFY telemetryChanged)
    Q_PROPERTY(bool armed READ armed NOTIFY telemetryChanged)
    Q_PROPERTY(QString flightMode READ flightMode NOTIFY telemetryChanged)
    Q_PROPERTY(QVector3D scenePosition READ scenePosition NOTIFY poseChanged)
    Q_PROPERTY(QQuaternion sceneRotation READ sceneRotation NOTIFY poseChanged)
    Q_PROPERTY(double originLatitude READ originLatitude WRITE setOriginLatitude NOTIFY originChanged)
    Q_PROPERTY(double originLongitude READ originLongitude WRITE setOriginLongitude NOTIFY originChanged)
    Q_PROPERTY(double originHeight READ originHeight WRITE setOriginHeight NOTIFY originChanged)

public:
    explicit MavlinkReceiver(QObject *parent = nullptr);

    [[nodiscard]] quint16 listenPort() const { return m_listenPort; }
    void setListenPort(quint16 port);
    [[nodiscard]] bool listening() const { return m_listening; }
    [[nodiscard]] QString tcpHost() const { return m_tcpHost; }
    void setTcpHost(const QString &host);
    [[nodiscard]] quint16 tcpPort() const { return m_tcpPort; }
    void setTcpPort(quint16 port);
    [[nodiscard]] bool tcpEnabled() const { return m_tcpEnabled; }
    void setTcpEnabled(bool enabled);
    [[nodiscard]] bool tcpConnected() const;
    [[nodiscard]] bool connected() const { return m_connected; }
    [[nodiscard]] bool demoMode() const { return m_demoMode; }
    void setDemoMode(bool enabled);
    [[nodiscard]] int systemId() const { return m_systemId; }
    [[nodiscard]] double latitude() const { return m_latitude; }
    [[nodiscard]] double longitude() const { return m_longitude; }
    [[nodiscard]] double altitudeMsl() const { return m_altitudeMsl; }
    [[nodiscard]] double relativeAltitude() const { return m_relativeAltitude; }
    [[nodiscard]] double groundspeed() const { return m_groundspeed; }
    [[nodiscard]] bool armed() const { return m_armed; }
    [[nodiscard]] QString flightMode() const { return m_flightMode; }
    [[nodiscard]] QVector3D scenePosition() const { return m_scenePosition; }
    [[nodiscard]] QQuaternion sceneRotation() const { return m_sceneRotation; }
    [[nodiscard]] double originLatitude() const { return m_originLatitude; }
    [[nodiscard]] double originLongitude() const { return m_originLongitude; }
    [[nodiscard]] double originHeight() const { return m_originHeight; }
    void setOriginLatitude(double value);
    void setOriginLongitude(double value);
    void setOriginHeight(double value);

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void connectTcp();
    Q_INVOKABLE void disconnectTcp();

signals:
    void listenPortChanged();
    void listeningChanged();
    void tcpEndpointChanged();
    void tcpEnabledChanged();
    void tcpConnectedChanged();
    void connectedChanged();
    void demoModeChanged();
    void telemetryChanged();
    void poseChanged();
    void originChanged();
    void errorOccurred(const QString &message);

private:
    void readPendingDatagrams();
    void readTcpData();
    void parseBytes(const QByteArray &bytes, QByteArray &streamBuffer);
    void handleMessage(int messageId, int systemId, const QByteArray &payload);
    void requestTelemetryRates();
    void sendMavlink2Message(quint32 messageId, quint8 targetSystem,
                             quint8 targetComponent, const QByteArray &payload,
                             quint8 crcExtra);
    void sendSetMessageInterval(quint32 messageId, double hz);
    void updateDemo();
    void updateScenePose(const QQuaternion &bodyToNed);
    void setConnected(bool connected);
    [[nodiscard]] static quint16 mavlinkChecksum(const QByteArray &frameWithoutMagic,
                                                 quint8 crcExtra);
    [[nodiscard]] static QQuaternion sceneQuaternion(const QQuaternion &bodyToNed);
    [[nodiscard]] static QString copterMode(quint32 customMode);

    QUdpSocket m_socket;
    QTcpSocket m_tcpSocket;
    QTimer m_renderTimer;
    QElapsedTimer m_clock;
    qint64 m_lastPacketMs = -10000;
    qint64 m_lastRateRequestMs = -10000;
    QByteArray m_udpStreamBuffer;
    QByteArray m_tcpStreamBuffer;
    QHostAddress m_udpPeerAddress;
    quint16 m_udpPeerPort = 0;
    quint8 m_txSequence = 0;
    quint16 m_listenPort = 14550;
    QString m_tcpHost = QStringLiteral("127.0.0.1");
    quint16 m_tcpPort = 5760;
    bool m_listening = false;
    bool m_tcpEnabled = false;
    bool m_connected = false;
    bool m_demoMode = true;
    int m_systemId = 0;
    double m_latitude = 12.9716;
    double m_longitude = 77.5946;
    double m_altitudeMsl = 0.0;
    double m_relativeAltitude = 70.0;
    double m_groundspeed = 0.0;
    bool m_armed = false;
    QString m_flightMode = QStringLiteral("DEMO");
    QVector3D m_scenePosition;
    QQuaternion m_sceneRotation;
    QQuaternion m_bodyToNed;
    double m_originLatitude = 12.9716;
    double m_originLongitude = 77.5946;
    double m_originHeight = 0.0;
};
