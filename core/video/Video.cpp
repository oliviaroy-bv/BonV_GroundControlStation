#include "Video.h"
#include "gimbal/GimbalController.h"
#include "storage/AppDataPaths.h"

#include <QDebug>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QMetaObject>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>

#ifdef HAVE_GSTREAMER
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/rtsp/gstrtsptransport.h>
#include <gst/video/video.h>
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
void setElementRank(const char *factoryName, guint rank)
{
    GstRegistry *registry = gst_registry_get();
    GstPluginFeature *feature = gst_registry_lookup_feature(registry, factoryName);
    if (!feature) {
        return;
    }

    gst_plugin_feature_set_rank(feature, rank);
    gst_object_unref(feature);
}

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
        setElementRank("openh264dec", GST_RANK_PRIMARY + 100);
        setElementRank("d3d11h264dec", GST_RANK_NONE);
        setElementRank("qsvh264dec", GST_RANK_NONE);
        setElementRank("d3d11h265dec", GST_RANK_PRIMARY + 90);
        setElementRank("qsvh265dec", GST_RANK_PRIMARY + 80);
        setElementRank("d3d11download", GST_RANK_PRIMARY + 70);
        setElementRank("d3d11convert", GST_RANK_PRIMARY + 60);
        setElementRank("videoconvert", GST_RANK_PRIMARY + 50);
        initialized = true;
    }
}
#endif
}

Video::Video(QObject *parent)
    : QObject(parent)
    , gimbalController(new GimbalController(this))
{
    busPollTimer.setInterval(50);
    connect(&busPollTimer, &QTimer::timeout, this, &Video::pollBus);
}

Video::~Video()
{
    stop();
}

QString Video::rtspLink() const
{
    return rtspLinkValue;
}

void Video::setRtspLink(const QString &rtspLink)
{
    const QString trimmedLink = rtspLink.trimmed();
    if (rtspLinkValue == trimmedLink) {
        return;
    }

    rtspLinkValue = trimmedLink;
    emit rtspLinkChanged();
    syncGimbalTarget();

    if (streamingState) {
        restart();
    }
}

QString Video::cameraName() const
{
    return cameraNameValue;
}

void Video::setCameraName(const QString &cameraName)
{
    const QString trimmedName = cameraName.trimmed();
    if (cameraNameValue == trimmedName) {
        return;
    }

    cameraNameValue = trimmedName;
    emit cameraNameChanged();
    syncGimbalTarget();
}

QObject *Video::gimbal() const
{
    return gimbalController;
}

bool Video::streaming() const
{
    return streamingState;
}

bool Video::recording() const
{
    return recordingState;
}

QString Video::recordingFilePath() const
{
    return recordingFilePathValue;
}

Video::TransportMode Video::transportMode() const
{
    return transportModeValue;
}

void Video::setTransportMode(TransportMode transportMode)
{
    if (transportModeValue == transportMode) {
        return;
    }

    transportModeValue = transportMode;
    emit transportModeChanged();

    if (streamingState) {
        restart();
    }
}

int Video::latencyMs() const
{
    return latencyMsValue;
}

void Video::setLatencyMs(int latencyMs)
{
    const int boundedLatency = std::max(0, latencyMs);
    if (latencyMsValue == boundedLatency) {
        return;
    }

    latencyMsValue = boundedLatency;
    emit latencyMsChanged();

    if (streamingState) {
        restart();
    }
}

int Video::connectionTimeoutMs() const
{
    return connectionTimeoutMsValue;
}

void Video::setConnectionTimeoutMs(int connectionTimeoutMs)
{
    const int boundedTimeout = std::max(1000, connectionTimeoutMs);
    if (connectionTimeoutMsValue == boundedTimeout) {
        return;
    }

    connectionTimeoutMsValue = boundedTimeout;
    emit connectionTimeoutMsChanged();

    if (streamingState) {
        restart();
    }
}

QString Video::errorString() const
{
    return errorStringValue;
}

QImage Video::frame() const
{
    QMutexLocker locker(&frameValueMutex);
    return frameValue;
}

void Video::start()
{
    if (!validateRtspLink()) {
        return;
    }

    if (streamingState) {
        return;
    }

#ifndef HAVE_GSTREAMER
    setErrorString(QStringLiteral("GStreamer support is not available in this build."));
    setStreaming(false);
#else
    ensureGStreamerInitialized();
    stop();
    setErrorString(QString());
    loggedFrameInfo = false;
    frameValueUpdatePending = false;

    qDebug() << "Starting universal GStreamer RTSP pipeline for:" << rtspLinkValue;

    if (!createPipeline()) {
        setStreaming(false);
        return;
    }

    GstAppSinkCallbacks callbacks = {};
    callbacks.new_sample = &Video::onNewSample;
    gst_app_sink_set_callbacks(GST_APP_SINK(appSink), &callbacks, this, nullptr);

    const GstStateChangeReturn stateResult = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (stateResult == GST_STATE_CHANGE_FAILURE) {
        setErrorString(QStringLiteral("Failed to start GStreamer pipeline."));
        clearPipeline();
        setStreaming(false);
        return;
    }

    busPollTimer.start();
    // PLAYING only means GStreamer accepted the pipeline.  It does not mean
    // the RTSP server is reachable or that the stream can be decoded.  Mark
    // the camera connected only after the first valid video frame arrives.
    setStreaming(false);
#endif
}

void Video::stop()
{
    stopRecording();
    busPollTimer.stop();
    clearPipeline();
    setStreaming(false);
}

void Video::restart()
{
    stop();
    start();
}

bool Video::startRecording(const QString &filePath)
{
    if (recordingState) {
        return true;
    }

    if (!validateRtspLink()) {
        return false;
    }

#ifndef HAVE_GSTREAMER
    setErrorString(QStringLiteral("GStreamer support is not available in this build."));
    return false;
#else
    ensureGStreamerInitialized();

    const QString outputPath = filePath.trimmed().isEmpty() ? defaultRecordingFilePath() : filePath.trimmed();
    QFileInfo outputInfo(outputPath);
    QDir outputDir = outputInfo.dir();
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        setErrorString(QStringLiteral("Could not create recording folder: %1").arg(outputDir.absolutePath()));
        return false;
    }

    const QImage currentFrame = frame();
    if (currentFrame.isNull()) {
        setErrorString(QStringLiteral("Wait for the first video frame before recording."));
        return false;
    }

    GError *parseError = nullptr;
    recordingPipeline = gst_parse_launch(
        recordingPipelineDescription(outputInfo.absoluteFilePath(), currentFrame.width(), currentFrame.height()).toUtf8().constData(),
        &parseError);
    if (!recordingPipeline) {
        const QString message = parseError && parseError->message
                ? QString::fromUtf8(parseError->message)
                : QStringLiteral("Failed to create GStreamer recording pipeline.");
        if (parseError) {
            g_error_free(parseError);
        }
        setErrorString(message);
        return false;
    }
    if (parseError) {
        g_error_free(parseError);
    }

    recordingAppSrc = gst_bin_get_by_name(GST_BIN(recordingPipeline), "recordSource");
    if (!recordingAppSrc) {
        setErrorString(QStringLiteral("Failed to create GStreamer recording source."));
        clearRecordingPipeline(false);
        return false;
    }

    recordingFrameIndex = 0;
    recordingBus = gst_element_get_bus(recordingPipeline);
    const GstStateChangeReturn stateResult = gst_element_set_state(recordingPipeline, GST_STATE_PLAYING);
    if (stateResult == GST_STATE_CHANGE_FAILURE) {
        setErrorString(QStringLiteral("Failed to start GStreamer recording pipeline."));
        clearRecordingPipeline(false);
        return false;
    }

    setRecordingFilePath(outputInfo.absoluteFilePath());
    setRecording(true);
    busPollTimer.start();
    qDebug() << "Recording GStreamer video to:" << recordingFilePathValue;
    return true;
#endif
}

void Video::stopRecording()
{
#ifdef HAVE_GSTREAMER
    clearRecordingPipeline(true);
#endif
    setRecording(false);
}

void Video::toggleRecording()
{
    if (recordingState) {
        stopRecording();
    } else {
        startRecording();
    }
}

void Video::pollBus()
{
#ifdef HAVE_GSTREAMER
    auto messageTextFromError = [this](GstMessage *message) {
        GError *error = nullptr;
        gchar *debugInfo = nullptr;
        gst_message_parse_error(message, &error, &debugInfo);

        QString messageText = error && error->message
                ? QString::fromUtf8(error->message)
                : QStringLiteral("Unknown GStreamer error.");
        if (debugInfo && *debugInfo) {
            messageText += QStringLiteral(" (%1)").arg(QString::fromUtf8(debugInfo));
        }
        if (messageText.contains(QStringLiteral("Failed to connect"), Qt::CaseInsensitive)) {
            const QUrl url(rtspLinkValue);
            messageText += QStringLiteral(" Check that %1:%2 is reachable and that the RTSP path is correct.")
                    .arg(url.host())
                    .arg(url.port(554));
        }

        if (debugInfo) {
            qWarning() << "GStreamer debug:" << debugInfo;
            g_free(debugInfo);
        }
        if (error) {
            g_error_free(error);
        }
        return messageText;
    };

    if (bus) {
        while (GstMessage *message = gst_bus_pop(bus)) {
            switch (GST_MESSAGE_TYPE(message)) {
            case GST_MESSAGE_ERROR:
                setErrorString(messageTextFromError(message));
                gst_message_unref(message);
                stop();
                return;
            case GST_MESSAGE_EOS:
                gst_message_unref(message);
                stop();
                return;
            default:
                break;
            }

            gst_message_unref(message);
        }
    }

    if (recordingBus) {
        while (GstMessage *message = gst_bus_pop(recordingBus)) {
            switch (GST_MESSAGE_TYPE(message)) {
            case GST_MESSAGE_ERROR:
                setErrorString(QStringLiteral("Recording failed: %1").arg(messageTextFromError(message)));
                gst_message_unref(message);
                stopRecording();
                return;
            case GST_MESSAGE_EOS:
                gst_message_unref(message);
                clearRecordingPipeline(false);
                setRecording(false);
                return;
            default:
                break;
            }

            gst_message_unref(message);
        }
    }
#endif
}

void Video::setStreaming(bool streaming)
{
    if (streamingState == streaming) {
        return;
    }

    streamingState = streaming;
    emit streamingChanged();
}

void Video::setRecording(bool recording)
{
    if (recordingState == recording) {
        return;
    }

    recordingState = recording;
    emit recordingChanged();
}

void Video::setRecordingFilePath(const QString &filePath)
{
    if (recordingFilePathValue == filePath) {
        return;
    }

    recordingFilePathValue = filePath;
    emit recordingFilePathChanged();
}

void Video::setErrorString(const QString &errorString)
{
    if (errorStringValue == errorString) {
        return;
    }

    errorStringValue = errorString;
    emit errorStringChanged();
}

void Video::setFrame(const QImage &frame)
{
    {
        QMutexLocker locker(&frameValueMutex);
        frameValue = frame;
    }
    emit frameChanged();
}

QString Video::defaultRecordingFilePath() const
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    return QDir(AppDataPaths::videoRecordingsDirectory()).filePath(QStringLiteral("bonv_video_%1.avi").arg(timestamp));
}

bool Video::validateRtspLink() const
{
    if (rtspLinkValue.isEmpty()) {
        const_cast<Video *>(this)->setErrorString(QStringLiteral("RTSP link is empty."));
        return false;
    }

    const QUrl url(rtspLinkValue);
    if (!url.isValid() || url.scheme().toLower() != QStringLiteral("rtsp")) {
        const_cast<Video *>(this)->setErrorString(QStringLiteral("Invalid RTSP link. Expected rtsp://host:port/path."));
        return false;
    }
    if (url.host().isEmpty()) {
        const_cast<Video *>(this)->setErrorString(QStringLiteral("Invalid RTSP link. Missing camera host."));
        return false;
    }

    return true;
}

void Video::syncGimbalTarget()
{
    if (!gimbalController) {
        return;
    }

    gimbalController->setCameraName(cameraNameValue);

    const QUrl url(rtspLinkValue);
    gimbalController->setCameraHost(url.host());

    const QUrlQuery query(url);
    bool ok = false;
    const int queryPort = query.queryItemValue(QStringLiteral("gimbalPort")).toInt(&ok);
    if (ok && queryPort > 0) {
        gimbalController->setCameraPort(queryPort);
    }
}

QString Video::pipelineDescription() const
{
    const qint64 timeoutUs = static_cast<qint64>(connectionTimeoutMsValue) * 1000;
    const int effectiveLatencyMs = std::max(latencyMsValue, 50);
    QString source = QStringLiteral(
        "rtspsrc name=src location=\"%1\" latency=%2 drop-on-latency=true "
        "do-retransmission=false timeout=%3 tcp-timeout=%3"
    ).arg(escapedGstString(rtspLinkValue)).arg(effectiveLatencyMs).arg(timeoutUs);

    switch (transportModeValue) {
    case TransportMode::Tcp:
        source += QStringLiteral(" protocols=tcp");
        break;
    case TransportMode::Udp:
        source += QStringLiteral(" protocols=udp");
        break;
    case TransportMode::Auto:
        break;
    }

    return source + QStringLiteral(
        " ! queue max-size-buffers=0 max-size-time=200000000 max-size-bytes=0"
        " ! decodebin"
        " ! queue leaky=downstream max-size-buffers=1 max-size-time=0 max-size-bytes=0"
        " ! d3d11download"
        " ! videoconvert n-threads=2"
        " ! video/x-raw,format=BGRx"
        " ! appsink name=videoSink emit-signals=true max-buffers=1 drop=true sync=false"
    );
}

void Video::clearPipeline()
{
#ifdef HAVE_GSTREAMER
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
    }

    if (bus) {
        gst_object_unref(bus);
        bus = nullptr;
    }

    if (appSink) {
        gst_object_unref(appSink);
        appSink = nullptr;
    }

    if (pipeline) {
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }

    videoQueue = nullptr;
    videoConvert = nullptr;
    capsFilter = nullptr;
#endif
}

#ifdef HAVE_GSTREAMER
QString Video::recordingPipelineDescription(const QString &filePath, int width, int height) const
{
    return QStringLiteral(
        "appsrc name=recordSource is-live=true format=time do-timestamp=false block=true "
        "caps=video/x-raw,format=BGRx,width=%1,height=%2,framerate=30/1"
        " ! queue max-size-buffers=120 leaky=downstream max-size-time=0 max-size-bytes=0"
        " ! videoconvert n-threads=2"
        " ! jpegenc quality=90"
        " ! avimux"
        " ! filesink location=\"%3\" sync=false"
    ).arg(width).arg(height).arg(escapedGstString(QDir::toNativeSeparators(filePath)));
}

void Video::clearRecordingPipeline(bool sendEos)
{
    if (recordingPipeline && sendEos) {
        if (recordingAppSrc) {
            gst_app_src_end_of_stream(GST_APP_SRC(recordingAppSrc));
        } else {
            gst_element_send_event(recordingPipeline, gst_event_new_eos());
        }
        if (recordingBus) {
            GstMessage *message = gst_bus_timed_pop_filtered(
                recordingBus,
                5 * GST_SECOND,
                static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
            if (message) {
                gst_message_unref(message);
            }
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

    recordingFrameIndex = 0;
}

bool Video::pushRecordingFrame(const QImage &frame)
{
    if (!recordingState || !recordingAppSrc || frame.isNull()) {
        return true;
    }

    QImage sourceFrame = frame;
    if (sourceFrame.format() != QImage::Format_RGB32) {
        sourceFrame = sourceFrame.convertToFormat(QImage::Format_RGB32);
    }

    const int bytesPerLine = sourceFrame.bytesPerLine();
    const int bufferSize = bytesPerLine * sourceFrame.height();
    GstBuffer *recordingBuffer = gst_buffer_new_allocate(nullptr, bufferSize, nullptr);
    if (!recordingBuffer) {
        return false;
    }

    GstMapInfo mapInfo;
    if (!gst_buffer_map(recordingBuffer, &mapInfo, GST_MAP_WRITE)) {
        gst_buffer_unref(recordingBuffer);
        return false;
    }

    memcpy(mapInfo.data, sourceFrame.constBits(), bufferSize);
    gst_buffer_unmap(recordingBuffer, &mapInfo);

    constexpr GstClockTime frameDuration = GST_SECOND / 30;
    GST_BUFFER_PTS(recordingBuffer) = recordingFrameIndex * frameDuration;
    GST_BUFFER_DTS(recordingBuffer) = GST_BUFFER_PTS(recordingBuffer);
    GST_BUFFER_DURATION(recordingBuffer) = frameDuration;
    ++recordingFrameIndex;

    const GstFlowReturn flowResult = gst_app_src_push_buffer(GST_APP_SRC(recordingAppSrc), recordingBuffer);
    return flowResult == GST_FLOW_OK;
}

bool Video::createPipeline()
{
    GError *parseError = nullptr;
    const QByteArray pipelineText = pipelineDescription().toUtf8();
    pipeline = gst_parse_launch(pipelineText.constData(), &parseError);
    if (!pipeline) {
        const QString message = parseError && parseError->message
                ? QString::fromUtf8(parseError->message)
                : QStringLiteral("Failed to create GStreamer video pipeline.");
        if (parseError) {
            g_error_free(parseError);
        }
        setErrorString(message);
        clearPipeline();
        return false;
    }
    if (parseError) {
        qWarning() << "GStreamer pipeline warning:" << parseError->message;
        g_error_free(parseError);
    }

    appSink = gst_bin_get_by_name(GST_BIN(pipeline), "videoSink");
    if (!appSink) {
        setErrorString(QStringLiteral("Failed to create GStreamer video sink."));
        clearPipeline();
        return false;
    }

    bus = gst_element_get_bus(pipeline);
    return true;
}

GstFlowReturn Video::onNewSample(GstAppSink *sink, gpointer userData)
{
    auto *video = static_cast<Video *>(userData);
    return video ? video->handleNewSample(sink) : GST_FLOW_ERROR;
}

void Video::onDecodePadAdded(GstElement *element, GstPad *pad, gpointer userData)
{
    Q_UNUSED(element)

    auto *video = static_cast<Video *>(userData);
    if (video) {
        video->handleDecodePadAdded(pad);
    }
}

void Video::onSourceSetup(GstElement *element, GstElement *source, gpointer userData)
{
    Q_UNUSED(element)

    auto *video = static_cast<Video *>(userData);
    if (video) {
        video->configureSource(source);
    }
}

void Video::handleDecodePadAdded(GstPad *pad)
{
    if (!videoQueue) {
        return;
    }

    GstPad *sinkPad = gst_element_get_static_pad(videoQueue, "sink");
    if (!sinkPad || gst_pad_is_linked(sinkPad)) {
        if (sinkPad) {
            gst_object_unref(sinkPad);
        }
        return;
    }

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps) {
        caps = gst_pad_query_caps(pad, nullptr);
    }

    bool isVideo = false;
    if (caps && gst_caps_get_size(caps) > 0) {
        const GstStructure *structure = gst_caps_get_structure(caps, 0);
        const char *name = gst_structure_get_name(structure);
        isVideo = name && g_str_has_prefix(name, "video/x-raw");
    }

    if (isVideo && gst_pad_link(pad, sinkPad) == GST_PAD_LINK_OK) {
        qDebug() << "Linked decoded video pad to renderer.";
    }

    if (caps) {
        gst_caps_unref(caps);
    }
    gst_object_unref(sinkPad);
}

void Video::configureSource(GstElement *source)
{
    if (!source || !g_str_has_prefix(G_OBJECT_TYPE_NAME(source), "GstRTSPSrc")) {
        return;
    }

    const qint64 timeoutUs = static_cast<qint64>(connectionTimeoutMsValue) * 1000;
    g_object_set(source,
                 "latency", latencyMsValue,
                 "drop-on-latency", TRUE,
                 "timeout", timeoutUs,
                 "tcp-timeout", timeoutUs,
                 nullptr);

    switch (transportModeValue) {
    case TransportMode::Tcp:
        g_object_set(source, "protocols", GST_RTSP_LOWER_TRANS_TCP, nullptr);
        break;
    case TransportMode::Udp:
        g_object_set(source, "protocols", GST_RTSP_LOWER_TRANS_UDP, nullptr);
        break;
    case TransportMode::Auto:
        break;
    }
}

GstFlowReturn Video::handleNewSample(GstAppSink *sink)
{
    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        return GST_FLOW_ERROR;
    }

    {
        QMutexLocker locker(&frameValueMutex);
        if (frameValueUpdatePending && !recordingState) {
            gst_sample_unref(sample);
            return GST_FLOW_OK;
        }
    }

    GstCaps *caps = gst_sample_get_caps(sample);
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    if (!caps || !buffer) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    GstVideoInfo videoInfo;
    if (!gst_video_info_from_caps(&videoInfo, caps)
            || GST_VIDEO_INFO_WIDTH(&videoInfo) <= 0
            || GST_VIDEO_INFO_HEIGHT(&videoInfo) <= 0) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    const int width = GST_VIDEO_INFO_WIDTH(&videoInfo);
    const int height = GST_VIDEO_INFO_HEIGHT(&videoInfo);
    const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&videoInfo, 0);
    const GstVideoFormat format = GST_VIDEO_INFO_FORMAT(&videoInfo);

    if (!loggedFrameInfo) {
        loggedFrameInfo = true;
        qDebug() << "GStreamer video frame:"
                 << width << "x" << height
                 << "stride" << stride
                 << "format" << gst_video_format_to_string(format);
    }

    GstVideoFrame videoFrame;
    if (!gst_video_frame_map(&videoFrame, &videoInfo, buffer, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    const QImage image(
        static_cast<const uchar *>(GST_VIDEO_FRAME_PLANE_DATA(&videoFrame, 0)),
        width,
        height,
        GST_VIDEO_FRAME_PLANE_STRIDE(&videoFrame, 0),
        QImage::Format_RGB32);
    const QImage copiedImage = image.copy();
    pushRecordingFrame(copiedImage);

    gst_video_frame_unmap(&videoFrame);
    gst_sample_unref(sample);

    bool shouldPostUpdate = false;
    {
        QMutexLocker locker(&frameValueMutex);
        frameValue = copiedImage;
        if (!frameValueUpdatePending) {
            frameValueUpdatePending = true;
            shouldPostUpdate = true;
        }
    }

    if (shouldPostUpdate) {
        QMetaObject::invokeMethod(this, [this]() {
            {
                QMutexLocker locker(&frameValueMutex);
                frameValueUpdatePending = false;
            }
            setStreaming(true);
            emit frameChanged();
        }, Qt::QueuedConnection);
    }

    return GST_FLOW_OK;
}
#endif
