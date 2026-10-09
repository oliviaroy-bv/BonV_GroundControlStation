#pragma once

#include <QWebEngineUrlSchemeHandler>

class TileCacheManager;

class TileCacheSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT
public:
    explicit TileCacheSchemeHandler(TileCacheManager *cacheManager, QObject *parent = nullptr);
    void requestStarted(QWebEngineUrlRequestJob *job) override;

private:
    TileCacheManager *_cacheManager = nullptr;
};
