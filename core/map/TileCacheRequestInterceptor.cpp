#include "TileCacheRequestInterceptor.h"

#include <QWebEngineUrlRequestInfo>

TileCacheRequestInterceptor::TileCacheRequestInterceptor(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
}

void TileCacheRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    const QUrl url = info.requestUrl();
    if (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https")) return;
    const QByteArray encodedUrl = url.toEncoded().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    info.redirect(QUrl(QStringLiteral("bonv://tile/") + QString::fromLatin1(encodedUrl)));
}
