#pragma once

#include <QWebEngineUrlRequestInterceptor>

class TileCacheRequestInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    explicit TileCacheRequestInterceptor(QObject *parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
};
