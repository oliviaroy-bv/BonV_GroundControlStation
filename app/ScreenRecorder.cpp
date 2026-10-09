#include "ScreenRecorder.h"

#include "storage/AppDataPaths.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QWindow>
#include <algorithm>
#include <cstring>

#ifdef HAVE_GSTREAMER
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#endif

namespace {

QString escapedGstString(const QString &value)
{
    QString escaped = value;
    escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    escaped.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return escaped;
}

#ifdef HAVE_GSTREAMER
void prependEnvPath(const char *name, const QString &path)
{
    if (!QDir(path).exists()) {
        return;
    }

    const QByteArray nativePath = QDir::toNativeSeparators(path).toUtf8();
    const QByteArray currentValue = qgetenv(name);
    const char separator = QDir::listSeparator().toLatin1();
    if (currentValue.split(separator).contains(nativePath)) {
        return;
    }

    qputenv(name, currentValue.isEmpty()
            ? nativePath
            : nativePath + separator + currentValue);
}

void configureBundledGStreamerRuntime()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    prependEnvPath("GST_PLUGIN_PATH", appDir.filePath("lib/gstreamer-1.0"));

    const QString scannerPath = appDir.filePath("libexec/gstreamer-1.0/gst-plugin-scanner.exe");
    if (QFileInfo::exists(scannerPath) && qgetenv("GST_PLUGIN_SCANNER").isEmpty()) {
        qputenv("GST_PLUGIN_SCANNER", QDir::toNativeSeparators(scannerPath).toUtf8());
    }
}

void ensureGStreamerInitialized()
{
    static bool initialized = false;
    if (!initialized) {
        configureBundledGStreamerRuntime();
        gst_init(nullptr, nullptr);
        initialized = true;
    }
}
#endif

} // namespace

ScreenRecorder::ScreenRecorder(QObject *parent)
    : QObject(parent)
{
    captureTimer.setInterval(100);
    busPollTimer.setInterval(50);
    finalizeTimer.setInterval(3000);
    finalizeTimer.setSingleShot(true);
    connect(&captureTimer, &QTimer::timeout, this, &ScreenRecorder::captureFrame);
    connect(&busPollTimer, &QTimer::timeout, this, &ScreenRecorder::pollBus);
    connect(&finalizeTimer, &QTimer::timeout, this, &ScreenRecorder::forcePipelineFinalize);
}

ScreenRecorder::~ScreenRecorder()
{
    captureTimer.stop();
    busPollTimer.stop();
    finalizeTimer.stop();
    recordingWindow = nullptr;
    finalizingRecording = false;
    clearPipeline(false);
    recordingState = false;
}

bool ScreenRecorder::recording() const
{
    return recordingState;
}

QString ScreenRecorder::recordingFilePath() const
{
    return recordingFilePathValue;
}

QString ScreenRecorder::errorString() const
{
    return errorStringValue;
}

bool ScreenRecorder::start(QWindow *window, const QString &filePath)
{
    if (recordingState) {
        return true;
    }
    if (recordingPipeline || finalizingRecording) {
        setErrorString(QStringLiteral("Screen recording is still finalizing. Please try again in a moment."));
        return false;
    }

    if (!window) {
        setErrorString(QStringLiteral("Screen recording failed: no application window is available."));
        return false;
    }

#ifndef HAVE_GSTREAMER
    setErrorString(QStringLiteral("Screen recording failed: GStreamer support is not available in this build."));
    return false;
#else
    ensureGStreamerInitialized();

    const QString outputPath = filePath.trimmed().isEmpty() ? defaultRecordingFilePath() : filePath.trimmed();
    QFileInfo outputInfo(outputPath);
    QDir outputDir = outputInfo.dir();
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        setErrorString(QStringLiteral("Could not create screen recording folder: %1").arg(outputDir.absolutePath()));
        return false;
    }

    const QImage firstFrame = captureWindowFrame(window);
    if (firstFrame.isNull()) {
        setErrorString(QStringLiteral("Screen recording failed: could not capture the application window."));
        return false;
    }

    if (!startPipeline(outputInfo.absoluteFilePath(), firstFrame.width(), firstFrame.height())) {
        return false;
    }

    recordingWindow = window;
    frameSize = firstFrame.size();
    frameIndex = 0;
    setRecordingFilePath(outputInfo.absoluteFilePath());
    setRecording(true);
    if (!pushFrame(firstFrame)) {
        setErrorString(QStringLiteral("Screen recording failed: could not write the first frame."));
        stop();
        return false;
    }
    captureTimer.start();
    busPollTimer.start();
    qDebug() << "Recording screen to:" << recordingFilePathValue;
    return true;
#endif
}

void ScreenRecorder::stop()
{
    captureTimer.stop();
    recordingWindow = nullptr;
    setRecording(false);
    requestPipelineStop();
}

bool ScreenRecorder::toggle(QWindow *window)
{
    if (recordingState) {
        stop();
        return true;
    }

    return start(window);
}

void ScreenRecorder::captureFrame()
{
    if (!recordingState || !recordingWindow) {
        return;
    }

    const QImage frame = captureWindowFrame(recordingWindow);
    if (frame.isNull() || !pushFrame(frame)) {
        setErrorString(QStringLiteral("Screen recording stopped: failed to write a frame."));
        stop();
    }
}

void ScreenRecorder::pollBus()
{
#ifdef HAVE_GSTREAMER
    if (!recordingBus) {
        return;
    }

    while (GstMessage *message = gst_bus_pop(recordingBus)) {
        switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *error = nullptr;
            gchar *debugInfo = nullptr;
            gst_message_parse_error(message, &error, &debugInfo);
            const QString errorText = error && error->message
                    ? QString::fromUtf8(error->message)
                    : QStringLiteral("Unknown GStreamer error.");
            if (debugInfo) {
                qWarning() << "Screen recording GStreamer debug:" << debugInfo;
                g_free(debugInfo);
            }
            if (error) {
                g_error_free(error);
            }
            gst_message_unref(message);
            setErrorString(QStringLiteral("Screen recording failed: %1").arg(errorText));
            captureTimer.stop();
            recordingWindow = nullptr;
            finalizingRecording = false;
            clearPipeline(false);
            setRecording(false);
            return;
        }
        case GST_MESSAGE_EOS:
            gst_message_unref(message);
            captureTimer.stop();
            busPollTimer.stop();
            finalizeTimer.stop();
            recordingWindow = nullptr;
            finalizingRecording = false;
            clearPipeline(false);
            setRecording(false);
            return;
        default:
            break;
        }

        gst_message_unref(message);
    }
#endif
}

void ScreenRecorder::forcePipelineFinalize()
{
    captureTimer.stop();
    busPollTimer.stop();
    recordingWindow = nullptr;
    finalizingRecording = false;
    clearPipeline(false);
}

void ScreenRecorder::setRecording(bool recording)
{
    if (recordingState == recording) {
        return;
    }

    recordingState = recording;
    emit recordingChanged();
}

void ScreenRecorder::setRecordingFilePath(const QString &filePath)
{
    if (recordingFilePathValue == filePath) {
        return;
    }

    recordingFilePathValue = filePath;
    emit recordingFilePathChanged();
}

void ScreenRecorder::setErrorString(const QString &errorString)
{
    if (errorStringValue == errorString) {
        return;
    }

    errorStringValue = errorString;
    emit errorStringChanged();
}

QString ScreenRecorder::defaultRecordingFilePath() const
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    return QDir(AppDataPaths::screenRecordingsDirectory()).filePath(QStringLiteral("bonv_screen_%1.avi").arg(timestamp));
}

QImage ScreenRecorder::captureWindowFrame(QWindow *window) const
{
    if (!window) {
        return QImage();
    }

    QScreen *screen = window->screen() ? window->screen() : QGuiApplication::primaryScreen();
    if (!screen) {
        return QImage();
    }

    const QRect geometry = screen->geometry();
    QImage frame = screen->grabWindow(0, geometry.x(), geometry.y(), geometry.width(), geometry.height())
            .toImage()
            .convertToFormat(QImage::Format_RGB32);
    if (frame.isNull()) {
        return QImage();
    }

    const int maxWidth = 1280;
    if (frame.width() > maxWidth) {
        const int scaledHeight = std::max(1, qRound(frame.height() * (static_cast<double>(maxWidth) / frame.width())));
        frame = frame.scaled(maxWidth, scaledHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return frame;
}

bool ScreenRecorder::startPipeline(const QString &filePath, int width, int height)
{
#ifndef HAVE_GSTREAMER
    Q_UNUSED(filePath);
    Q_UNUSED(width);
    Q_UNUSED(height);
    return false;
#else
    GError *parseError = nullptr;
    recordingPipeline = gst_parse_launch(
        recordingPipelineDescription(filePath, width, height).toUtf8().constData(),
        &parseError);
    if (!recordingPipeline) {
        const QString message = parseError && parseError->message
                ? QString::fromUtf8(parseError->message)
                : QStringLiteral("Failed to create screen recording pipeline.");
        if (parseError) {
            g_error_free(parseError);
        }
        setErrorString(message);
        return false;
    }
    if (parseError) {
        g_error_free(parseError);
    }

    recordingAppSrc = gst_bin_get_by_name(GST_BIN(recordingPipeline), "screenRecordSource");
    if (!recordingAppSrc) {
        setErrorString(QStringLiteral("Failed to create screen recording source."));
        clearPipeline(false);
        return false;
    }

    recordingBus = gst_element_get_bus(recordingPipeline);
    const GstStateChangeReturn stateResult = gst_element_set_state(recordingPipeline, GST_STATE_PLAYING);
    if (stateResult == GST_STATE_CHANGE_FAILURE) {
        setErrorString(QStringLiteral("Failed to start screen recording pipeline."));
        clearPipeline(false);
        return false;
    }

    return true;
#endif
}

QString ScreenRecorder::recordingPipelineDescription(const QString &filePath, int width, int height) const
{
    return QStringLiteral(
        "appsrc name=screenRecordSource is-live=true format=time do-timestamp=false block=false "
        "caps=video/x-raw,format=BGRx,width=%1,height=%2,framerate=10/1"
        " ! queue max-size-buffers=8 leaky=downstream max-size-time=0 max-size-bytes=0"
        " ! videoconvert n-threads=2"
        " ! jpegenc quality=90"
        " ! avimux"
        " ! filesink location=\"%3\" sync=false"
    ).arg(width).arg(height).arg(escapedGstString(QDir::toNativeSeparators(filePath)));
}

bool ScreenRecorder::pushFrame(const QImage &frame)
{
#ifndef HAVE_GSTREAMER
    Q_UNUSED(frame);
    return false;
#else
    if (!recordingState || !recordingAppSrc || frame.isNull()) {
        return true;
    }

    QImage sourceFrame = frameSize.isValid() && frame.size() != frameSize
            ? frame.scaled(frameSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            : frame;
    if (sourceFrame.format() != QImage::Format_RGB32) {
        sourceFrame = sourceFrame.convertToFormat(QImage::Format_RGB32);
    }

    const int bytesPerLine = sourceFrame.bytesPerLine();
    const int bufferSize = bytesPerLine * sourceFrame.height();
    GstBuffer *buffer = gst_buffer_new_allocate(nullptr, bufferSize, nullptr);
    if (!buffer) {
        return false;
    }

    GstMapInfo mapInfo;
    if (!gst_buffer_map(buffer, &mapInfo, GST_MAP_WRITE)) {
        gst_buffer_unref(buffer);
        return false;
    }

    std::memcpy(mapInfo.data, sourceFrame.constBits(), bufferSize);
    gst_buffer_unmap(buffer, &mapInfo);

    constexpr GstClockTime frameDuration = GST_SECOND / 10;
    GST_BUFFER_PTS(buffer) = frameIndex * frameDuration;
    GST_BUFFER_DTS(buffer) = GST_BUFFER_PTS(buffer);
    GST_BUFFER_DURATION(buffer) = frameDuration;
    ++frameIndex;

    const GstFlowReturn result = gst_app_src_push_buffer(GST_APP_SRC(recordingAppSrc), buffer);
    return result == GST_FLOW_OK;
#endif
}

void ScreenRecorder::requestPipelineStop()
{
#ifdef HAVE_GSTREAMER
    if (!recordingPipeline) {
        finalizingRecording = false;
        busPollTimer.stop();
        return;
    }

    if (!finalizingRecording) {
        finalizingRecording = true;
        if (recordingAppSrc) {
            gst_app_src_end_of_stream(GST_APP_SRC(recordingAppSrc));
        } else {
            gst_element_send_event(recordingPipeline, gst_event_new_eos());
        }
    }
    busPollTimer.start();
    finalizeTimer.start();
#endif
}

void ScreenRecorder::clearPipeline(bool sendEos)
{
#ifdef HAVE_GSTREAMER
    if (recordingPipeline && sendEos) {
        if (recordingAppSrc) {
            gst_app_src_end_of_stream(GST_APP_SRC(recordingAppSrc));
        } else {
            gst_element_send_event(recordingPipeline, gst_event_new_eos());
        }
    }

    if (recordingPipeline) {
        gst_element_set_state(recordingPipeline, GST_STATE_NULL);
    }

    if (recordingAppSrc) {
        gst_object_unref(recordingAppSrc);
        recordingAppSrc = nullptr;
    }

    if (recordingBus) {
        gst_object_unref(recordingBus);
        recordingBus = nullptr;
    }

    if (recordingPipeline) {
        gst_object_unref(recordingPipeline);
        recordingPipeline = nullptr;
    }
#else
    Q_UNUSED(sendEos);
#endif

    finalizeTimer.stop();
    frameIndex = 0;
    frameSize = QSize();
    finalizingRecording = false;
}
