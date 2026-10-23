#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>

class GimbalController;

#ifdef HAVE_GSTREAMER
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#endif

class Video : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString rtspLink READ rtspLink WRITE setRtspLink NOTIFY rtspLinkChanged)
    Q_PROPERTY(QString cameraName READ cameraName WRITE setCameraName NOTIFY cameraNameChanged)
    Q_PROPERTY(QObject* gimbal READ gimbal CONSTANT)
    Q_PROPERTY(TransportMode transportMode READ transportMode WRITE setTransportMode NOTIFY transportModeChanged)
    Q_PROPERTY(int latencyMs READ latencyMs WRITE setLatencyMs NOTIFY latencyMsChanged)
    Q_PROPERTY(int connectionTimeoutMs READ connectionTimeoutMs WRITE setConnectionTimeoutMs NOTIFY connectionTimeoutMsChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY streamingChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(QString recordingFilePath READ recordingFilePath NOTIFY recordingFilePathChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QImage frame READ frame NOTIFY frameChanged)

public:
    enum class TransportMode {
        Auto,
        Tcp,
        Udp
    };
    Q_ENUM(TransportMode)

    explicit Video(QObject *parent = nullptr);
    ~Video() override;

    QString rtspLink() const;
    void setRtspLink(const QString &rtspLink);
    QString cameraName() const;
    void setCameraName(const QString &cameraName);
    QObject *gimbal() const;

    TransportMode transportMode() const;
    void setTransportMode(TransportMode transportMode);

    int latencyMs() const;
    void setLatencyMs(int latencyMs);

    int connectionTimeoutMs() const;
    void setConnectionTimeoutMs(int connectionTimeoutMs);

    bool streaming() const;
    bool recording() const;
    QString recordingFilePath() const;
    QString errorString() const;
    QImage frame() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void restart();
    Q_INVOKABLE bool startRecording(const QString &filePath = QString());
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void toggleRecording();

signals:
    void rtspLinkChanged();
    void cameraNameChanged();
    void transportModeChanged();
    void latencyMsChanged();
    void connectionTimeoutMsChanged();
    void streamingChanged();
    void recordingChanged();
    void recordingFilePathChanged();
    void errorStringChanged();
    void frameChanged();

private slots:
    void pollBus();

private:
    void setStreaming(bool streaming);
    void setRecording(bool recording);
    void setRecordingFilePath(const QString &filePath);
    void setErrorString(const QString &errorString);
    void setFrame(const QImage &frame);
    QString pipelineDescription() const;
    void clearPipeline();
    QString defaultRecordingFilePath() const;
    bool validateRtspLink() const;
    void syncGimbalTarget();

    QString rtspLinkValue;
    QString cameraNameValue;
    GimbalController *gimbalController = nullptr;
    // Let rtspsrc negotiate UDP/TCP like QGroundControl. Some cameras send
    // clean RTP over UDP but produce incomplete H.264 access units when
    // forced through TCP.
    TransportMode transportModeValue = TransportMode::Auto;
    int latencyMsValue = 200;
    int connectionTimeoutMsValue = 60000;
    bool streamingState = false;
    bool recordingState = false;
    QString recordingFilePathValue;
    QString errorStringValue;
    QImage frameValue;
    mutable QMutex frameValueMutex;
    bool loggedFrameInfo = false;
    bool frameValueUpdatePending = false;
    QTimer busPollTimer;

#ifdef HAVE_GSTREAMER
    static GstFlowReturn onNewSample(GstAppSink *sink, gpointer userData);
    static void onDecodePadAdded(GstElement *element, GstPad *pad, gpointer userData);
    static void onSourceSetup(GstElement *element, GstElement *source, gpointer userData);

    bool createPipeline();
    GstFlowReturn handleNewSample(GstAppSink *sink);
    void handleDecodePadAdded(GstPad *pad);
    void configureSource(GstElement *source);
    QString recordingPipelineDescription(const QString &filePath, int width, int height) const;
    bool pushRecordingFrame(const QImage &frame);
    void clearRecordingPipeline(bool sendEos = true);

    GstElement *pipeline = nullptr;
    GstBus *bus = nullptr;
    GstElement *appSink = nullptr;
    GstElement *videoQueue = nullptr;
    GstElement *videoConvert = nullptr;
    GstElement *capsFilter = nullptr;
    GstElement *recordingPipeline = nullptr;
    GstElement *recordingAppSrc = nullptr;
    guint64 recordingFrameIndex = 0;
    GstBus *recordingBus = nullptr;
#endif
};
