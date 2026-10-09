#pragma once

#include <QPointer>
#include <QQuickPaintedItem>

#include "Video.h"

class VideoItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(Video *video READ video WRITE setVideo NOTIFY videoChanged)

public:
    explicit VideoItem(QQuickItem *parent = nullptr);

    Video *video() const;
    void setVideo(Video *video);

    void paint(QPainter *painter) override;

signals:
    void videoChanged();

private:
    QPointer<Video> videoObject;
};
