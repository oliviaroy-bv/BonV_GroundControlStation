#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QWindow>
#include "ScreenRecorder.h"

class ScreenRecorderTest : public QObject
{
    Q_OBJECT
private slots:
    void startupDoesNotBlockEventLoop();
    void recordingFinalizesAndCanRestart();
    void cancellationDuringStartupStaysStopped();
};

void ScreenRecorderTest::startupDoesNotBlockEventLoop()
{
#ifndef HAVE_GSTREAMER
    QSKIP("GStreamer unavailable");
#else
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile blocker(dir.filePath("file"));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    ScreenRecorder recorder;
    // Model slow first-use discovery without relying on the machine's plugin cache.
    QMetaObject::invokeMethod(recorder.encoder, []() { QThread::msleep(300); }, Qt::QueuedConnection);
    QImage frame(160, 120, QImage::Format_RGB32);
    frame.fill(Qt::black);
    int ticks = 0;
    QTimer heartbeat;
    connect(&heartbeat, &QTimer::timeout, [&]() { ++ticks; });
    heartbeat.start(10);
    recorder.queueStart(frame, dir.filePath("file/invalid.avi"));
    QTest::qWait(100);
    QVERIFY(recorder.startPending);
    QVERIFY(ticks >= 3);
    QTRY_VERIFY_WITH_TIMEOUT(!recorder.startPending, 20000);
    QVERIFY(!recorder.recording());
    QVERIFY(!recorder.errorString().isEmpty());
    // A repeated failure must still be reported after clearing the UI error.
    recorder.queueStart(frame, dir.filePath("file/invalid.avi"));
    QTRY_VERIFY_WITH_TIMEOUT(!recorder.startPending, 20000);
    QVERIFY(!recorder.errorString().isEmpty());
#endif
}

void ScreenRecorderTest::recordingFinalizesAndCanRestart()
{
#ifndef HAVE_GSTREAMER
    QSKIP("GStreamer unavailable");
#else
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QWindow window;
    ScreenRecorder recorder;
    QImage frame(160, 120, QImage::Format_RGB32);
    frame.fill(Qt::blue);
    for (int run = 0; run < 2; ++run) {
        const QString path = dir.filePath(QString::number(run) + ".avi");
        recorder.recordingWindow = &window;
        recorder.queueStart(frame, path);
        QTRY_VERIFY_WITH_TIMEOUT(!recorder.startPending, 20000);
        QVERIFY2(recorder.recording(), qPrintable(recorder.errorString()));
        // Synthetic frames exercise the encoder without a physical display.
        recorder.captureTimer.stop();
        QMetaObject::invokeMethod(recorder.encoder, [&recorder, frame]() {
            recorder.encoder->pushFrame(frame);
        }, Qt::QueuedConnection);
        recorder.stop();
        QTRY_VERIFY_WITH_TIMEOUT(!recorder.finalizingRecording, 10000);
        QVERIFY(!recorder.recording());
        QVERIFY(QFileInfo(path).size() > 0);
        QFile avi(path);
        QVERIFY(avi.open(QIODevice::ReadOnly));
        QCOMPARE(avi.read(4), QByteArray("RIFF"));
    }
#endif
}

void ScreenRecorderTest::cancellationDuringStartupStaysStopped()
{
#ifndef HAVE_GSTREAMER
    QSKIP("GStreamer unavailable");
#else
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QWindow window;
    ScreenRecorder recorder;
    QImage frame(160, 120, QImage::Format_RGB32);
    frame.fill(Qt::black);
    recorder.recordingWindow = &window;
    QMetaObject::invokeMethod(recorder.encoder, []() { QThread::msleep(100); }, Qt::QueuedConnection);
    recorder.queueStart(frame, dir.filePath("cancelled.avi"));
    recorder.stop();
    QVERIFY(!recorder.recording());
    QTRY_VERIFY_WITH_TIMEOUT(!recorder.finalizingRecording, 20000);
    QVERIFY(!recorder.recording());
    QVERIFY(!recorder.captureTimer.isActive());
#endif
}

QTEST_MAIN(ScreenRecorderTest)
#include "ScreenRecorderTest.moc"
