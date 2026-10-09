#pragma once

#include <QWebEngineUrlSchemeHandler>

class RasterOverlaySchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT

public:
    explicit RasterOverlaySchemeHandler(QObject *parent = nullptr);

    void requestStarted(QWebEngineUrlRequestJob *job) override;
};
