#include <QtTest/QtTest>

#include "video/Video.h"

class VideoTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultRtspLinkIsEmpty();
    void setRtspLinkUpdatesValueAndEmits();
    void settingSameRtspLinkDoesNotEmitAgain();
    void startWithoutRtspLinkSetsError();
};

void VideoTest::defaultRtspLinkIsEmpty()
{
    Video video;

    QCOMPARE(video.rtspLink(), QString());
    QVERIFY(!video.streaming());
    QCOMPARE(video.errorString(), QString());
}

void VideoTest::setRtspLinkUpdatesValueAndEmits()
{
    Video video;
    QSignalSpy spy(&video, &Video::rtspLinkChanged);

    video.setRtspLink("rtsp://127.0.0.1:8554/live");

    QCOMPARE(video.rtspLink(), QString("rtsp://127.0.0.1:8554/live"));
    QCOMPARE(spy.count(), 1);
}

void VideoTest::settingSameRtspLinkDoesNotEmitAgain()
{
    Video video;
    video.setRtspLink("rtsp://127.0.0.1:8554/live");

    QSignalSpy spy(&video, &Video::rtspLinkChanged);
    video.setRtspLink("rtsp://127.0.0.1:8554/live");

    QCOMPARE(spy.count(), 0);
}

void VideoTest::startWithoutRtspLinkSetsError()
{
    Video video;
    QSignalSpy errorSpy(&video, &Video::errorStringChanged);

    video.start();

    QVERIFY(!video.streaming());
    QVERIFY(!video.errorString().isEmpty());
    QCOMPARE(errorSpy.count(), 1);
}

QTEST_MAIN(VideoTest)

#include "VideoTest.moc"
