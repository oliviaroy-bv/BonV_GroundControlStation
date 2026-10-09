#include "VideoItem.h"

#include <QPainter>

VideoItem::VideoItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(false);
    setOpaquePainting(true);
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing, true);
}

Video *VideoItem::video() const
{
    return videoObject;
}

void VideoItem::setVideo(Video *video)
{
    if (videoObject == video) {
        return;
    }

    if (videoObject) {
        disconnect(videoObject, nullptr, this, nullptr);
    }

    videoObject = video;

    if (videoObject) {
        connect(videoObject, &Video::frameChanged, this, [this]() {
            update();
        });
        connect(videoObject, &QObject::destroyed, this, [this]() {
            videoObject = nullptr;
            update();
            emit videoChanged();
        });
    }

    update();
    emit videoChanged();
}

void VideoItem::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), Qt::black);

    if (!videoObject) {
        return;
    }

    const QImage frame = videoObject->frame();
    if (frame.isNull()) {
        return;
    }

    const QSizeF targetSize = boundingRect().size();
    QSizeF scaledSize = frame.size();
    scaledSize.scale(targetSize, Qt::KeepAspectRatioByExpanding);

    const QRectF targetRect(
        (targetSize.width() - scaledSize.width()) / 2.0,
        (targetSize.height() - scaledSize.height()) / 2.0,
        scaledSize.width(),
        scaledSize.height());

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawImage(targetRect, frame);
}
