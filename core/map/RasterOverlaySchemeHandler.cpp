#include "RasterOverlaySchemeHandler.h"

#include "RasterOverlayStorage.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QWebEngineUrlRequestJob>

RasterOverlaySchemeHandler::RasterOverlaySchemeHandler(QObject *parent)
    : QWebEngineUrlSchemeHandler(parent)
{
}

void RasterOverlaySchemeHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
    qDebug() << "RasterOverlaySchemeHandler::requestStarted()";

    const QUrl url = job->requestUrl();
    if (url.scheme() != QStringLiteral("bonv") || url.host() != QStringLiteral("overlay")) {
        job->fail(QWebEngineUrlRequestJob::UrlInvalid);
        return;
    }

    const QString fileName = QFileInfo(url.path()).fileName();
    qInfo().noquote() << QStringLiteral("Raster overlay request: %1").arg(url.toString());
    if (fileName.isEmpty() || !fileName.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
        job->fail(QWebEngineUrlRequestJob::UrlInvalid);
        return;
    }

    const QString filePath = RasterOverlayStorage::overlayFilePath(fileName);
    qInfo().noquote() << QStringLiteral("Raster overlay file: %1").arg(QDir::toNativeSeparators(filePath));
    auto *file = new QFile(filePath, job);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }

    job->reply(QByteArrayLiteral("image/png"), file);
}
