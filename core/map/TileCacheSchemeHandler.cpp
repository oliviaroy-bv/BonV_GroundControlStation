#include "TileCacheSchemeHandler.h"

#include "TileCacheManager.h"
#include "RasterOverlayStorage.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QWebEngineUrlRequestJob>

TileCacheSchemeHandler::TileCacheSchemeHandler(TileCacheManager *cacheManager, QObject *parent)
    : QWebEngineUrlSchemeHandler(parent), _cacheManager(cacheManager)
{
}

void TileCacheSchemeHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
    const QUrl requestUrl = job->requestUrl();
    if (requestUrl.host() == QStringLiteral("tile")) {
        QString resourcePath;
        QByteArray mimeType;

        if (requestUrl.path() == QStringLiteral("/map.html")) {
            resourcePath = QStringLiteral(":/resources/map.html");
            mimeType = QByteArrayLiteral("text/html; charset=utf-8");
        } else if (requestUrl.path() == QStringLiteral("/maplibre-gl.js")) {
            resourcePath = QStringLiteral(":/resources/maplibre-gl.js");
            mimeType = QByteArrayLiteral("application/javascript");
        } else if (requestUrl.path() == QStringLiteral("/maplibre-gl.css")) {
            resourcePath = QStringLiteral(":/resources/maplibre-gl.css");
            mimeType = QByteArrayLiteral("text/css; charset=utf-8");
        }

        if (!resourcePath.isEmpty()) {
            auto *file = new QFile(resourcePath, job);
            if (!file->open(QIODevice::ReadOnly)) {
                delete file;
                job->fail(QWebEngineUrlRequestJob::UrlNotFound);
                return;
            }
            job->reply(mimeType, file);
            return;
        }
    }
    if (requestUrl.host() == QStringLiteral("overlay")) {
        const QString fileName = QFileInfo(requestUrl.path()).fileName();
        auto *file = new QFile(RasterOverlayStorage::overlayFilePath(fileName), job);
        if (fileName.isEmpty() || !fileName.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive) || !file->open(QIODevice::ReadOnly)) {
            delete file;
            job->fail(QWebEngineUrlRequestJob::UrlNotFound);
            return;
        }
        job->reply(QByteArrayLiteral("image/png"), file);
        return;
    }
    if (!_cacheManager || requestUrl.host() != QStringLiteral("tile")) {
        job->fail(QWebEngineUrlRequestJob::UrlInvalid);
        return;
    }
    const QByteArray encodedUrl = requestUrl.path().mid(1).toLatin1();
    const QUrl upstreamUrl = QUrl::fromEncoded(QByteArray::fromBase64(encodedUrl, QByteArray::Base64UrlEncoding));
    if (!upstreamUrl.isValid() || upstreamUrl.scheme() != QStringLiteral("http") && upstreamUrl.scheme() != QStringLiteral("https")) {
        job->fail(QWebEngineUrlRequestJob::UrlInvalid);
        return;
    }

    QPointer<QWebEngineUrlRequestJob> guardedJob(job);
    _cacheManager->fetch(upstreamUrl, [guardedJob](const QByteArray &data, const QByteArray &mimeType, bool ok) {
        if (!guardedJob) return;
        if (!ok) {
            guardedJob->fail(QWebEngineUrlRequestJob::UrlNotFound);
            return;
        }
        auto *buffer = new QBuffer(guardedJob);
        buffer->setData(data);
        buffer->open(QIODevice::ReadOnly);
        guardedJob->reply(mimeType.isEmpty() ? QByteArrayLiteral("application/octet-stream") : mimeType, buffer);
    });
}
