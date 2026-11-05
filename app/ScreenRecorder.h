#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QThread>
#include <QPointer>
#include <QImage>
#include <QElapsedTimer>

class QWindow;

#ifdef HAVE_GSTREAMER
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#endif

class ScreenRecorder : public QObject
{
    Q_OBJECT

public:
    explicit ScreenRecorder(QObject *parent = nullptr);
    ~ScreenRecorder() override;

    bool recording() const;
    QString recordingFilePath() const;
    QString errorString() const;

    bool start(QWindow *window, const QString &filePath = QString());
    void stop();
    bool toggle(QWindow *window);

signals:
    void recordingChanged();
    void recordingFilePathChanged();
    void errorStringChanged();
    void workerStatus(bool active, const QString &path, const QString &error);
    void workerIdle();

private slots:
    void captureFrame();
    void pollBus();
    void forcePipelineFinalize();

private:
    friend class ScreenRecorderTest;
    explicit ScreenRecorder(bool worker);
    void queueStart(const QImage &frame, const QString &path);
    bool startCaptured(const QImage &frame, const QString &path);
    void publishWorkerStatus();
    void setRecording(bool recording);
    void setRecordingFilePath(const QString &filePath);
    void setErrorString(const QString &errorString);
    QString defaultRecordingFilePath() const;
    QImage captureWindowFrame(QWindow *window) const;
    bool startPipeline(const QString &filePath, int width, int height);
    QString recordingPipelineDescription(const QString &filePath, int width, int height) const;
    bool pushFrame(const QImage &frame);
    void requestPipelineStop();
    void clearPipeline(bool sendEos = true);

    QPointer<QWindow> recordingWindow;
    bool workerMode = false;
    bool startPending = false;
    bool framePending = false;
    quint64 generation = 0;
    QThread encodingThread;
    ScreenRecorder *encoder = nullptr;
    bool recordingState = false;
    bool finalizingRecording = false;
    QString recordingFilePathValue;
    QString errorStringValue;
    QTimer captureTimer;
    QTimer busPollTimer;
    QTimer finalizeTimer;
    QSize frameSize;
    quint64 frameIndex = 0;
    QElapsedTimer recordingClock;

#ifdef HAVE_GSTREAMER
    GstElement *recordingPipeline = nullptr;
    GstElement *recordingAppSrc = nullptr;
    GstBus *recordingBus = nullptr;
#endif
};
