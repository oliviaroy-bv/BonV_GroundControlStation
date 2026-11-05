#include "imagerytileprovider.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QStandardPaths>

#include <algorithm>

namespace {
constexpr int kTileSize = 256;
}

ImageryTileProvider::ImageryTileProvider(QObject *parent)
    : QObject(parent)
{
    // A single fixed, predictable location: imagery and DEM tiles both cache here so
    // one folder holds the whole offline dataset, and it survives across rebuilds and
    // different run locations (unlike the old build-output-relative cache).
    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    const QString root = desktop.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/3DMapsData")
        : desktop + QStringLiteral("/3DMapsData");
    m_cacheRoot = QDir::cleanPath(root);
    ensureCacheDirectories();
}

QUrl ImageryTileProvider::cacheRoot() const
{
    return QUrl::fromLocalFile(m_cacheRoot);
}

void ImageryTileProvider::setCacheRoot(const QUrl &value)
{
    const QString path = value.isLocalFile() ? value.toLocalFile() : value.toString();
    if (path.isEmpty() || path == m_cacheRoot)
        return;
    m_cacheRoot = QDir::cleanPath(path);
    ensureCacheDirectories();
    emit cacheRootChanged();
}

QUrl ImageryTileProvider::demDirectoryUrl() const
{
    return QUrl::fromLocalFile(m_cacheRoot + QStringLiteral("/dem"));
}

void ImageryTileProvider::ensureCacheDirectories()
{
    QDir().mkpath(m_cacheRoot + QStringLiteral("/imagery"));
    QDir().mkpath(m_cacheRoot + QStringLiteral("/dem"));
    QDir().mkpath(m_cacheRoot + QStringLiteral("/stitched"));
}

QString ImageryTileProvider::sourceCacheId(const QString &urlTemplate) const
{
    return QString::fromLatin1(QCryptographicHash::hash(urlTemplate.toUtf8(),
                                                        QCryptographicHash::Sha1).toHex().left(12));
}

QString ImageryTileProvider::quadKey(int x, int y, int zoom)
{
    QString result;
    result.reserve(zoom);
    for (int level = zoom; level > 0; --level) {
        const int mask = 1 << (level - 1);
        int digit = 0;
        if (x & mask)
            ++digit;
        if (y & mask)
            digit += 2;
        result.append(QChar::fromLatin1(static_cast<char>('0' + digit)));
    }
    return result;
}

QString ImageryTileProvider::expandedUrl(int zoom, int x, int y, const QString &urlTemplate) const
{
    QString url = urlTemplate;
    url.replace(QStringLiteral("{z}"), QString::number(zoom));
    url.replace(QStringLiteral("{x}"), QString::number(x));
    url.replace(QStringLiteral("{y}"), QString::number(y));
    url.replace(QStringLiteral("{quadkey}"), quadKey(x, y, zoom));
    const char subdomain = static_cast<char>('a' + ((x + y) % 3));
    url.replace(QStringLiteral("{s}"), QString(QChar::fromLatin1(subdomain)));
    return url;
}

QUrl ImageryTileProvider::placeholderImage()
{
    if (!m_placeholderUrl.isEmpty())
        return m_placeholderUrl;

    const QString path = m_cacheRoot + QStringLiteral("/stitched/placeholder.png");
    if (!QFileInfo::exists(path)) {
        QImage image(kTileSize, kTileSize, QImage::Format_RGB32);
        image.fill(QColor(QStringLiteral("#35454c")));
        QPainter painter(&image);
        painter.setPen(QPen(QColor(QStringLiteral("#60757d")), 2));
        painter.drawRect(image.rect().adjusted(1, 1, -1, -1));
        painter.setPen(QColor(QStringLiteral("#b8c6c9")));
        painter.drawText(image.rect(), Qt::AlignCenter, QStringLiteral("offline"));
        painter.end();
        image.save(path);
    }
    m_placeholderUrl = QUrl::fromLocalFile(path);
    return m_placeholderUrl;
}

QString ImageryTileProvider::tileCachePath(int zoom, int x, int y, const QString &urlTemplate) const
{
    const QString directory = m_cacheRoot + QStringLiteral("/imagery/") + sourceCacheId(urlTemplate)
        + QLatin1Char('/') + QString::number(zoom) + QLatin1Char('/') + QString::number(x);
    QDir().mkpath(directory);
    return directory + QLatin1Char('/') + QString::number(y) + QStringLiteral(".tile");
}

void ImageryTileProvider::requestTile(int zoom, int x, int y, const QString &urlTemplate,
                                      int detailLevels)
{
    const QString trimmedTemplate = urlTemplate.trimmed();
    if (trimmedTemplate.isEmpty()) {
        emit tileReady(zoom, x, y, placeholderImage());
        return;
    }

    // Keep the finer sub-tile zoom within the usual slippy-map ceiling.
    detailLevels = std::clamp(detailLevels, 0, std::max(0, 22 - zoom));
    if (detailLevels <= 0) {
        requestSingleTile(zoom, x, y, trimmedTemplate);
        return;
    }

    const int span = 1 << detailLevels;
    const int subZoom = zoom + detailLevels;
    const int subMinX = x * span;
    const int subMinY = y * span;

    const QString compositeDir = m_cacheRoot + QStringLiteral("/stitched/")
        + sourceCacheId(trimmedTemplate);
    QDir().mkpath(compositeDir);
    const QString compositePath = compositeDir + QLatin1Char('/')
        + QStringLiteral("z%1_%2_%3_d%4.png").arg(zoom).arg(x).arg(y).arg(detailLevels);

    const QImage cachedComposite(compositePath);
    if (!cachedComposite.isNull()) {
        emit tileReady(zoom, x, y, QUrl::fromLocalFile(compositePath));
        return;
    }

    const int compositeId = m_nextCompositeId++;
    CompositeRequest &composite = m_composites[compositeId];
    composite.zoom = zoom;
    composite.x = x;
    composite.y = y;
    composite.subZoom = subZoom;
    composite.subMinX = subMinX;
    composite.subMinY = subMinY;
    composite.span = span;
    composite.remaining = span * span;
    composite.urlTemplate = trimmedTemplate;
    composite.compositePath = compositePath;

    for (int row = 0; row < span; ++row) {
        for (int column = 0; column < span; ++column)
            requestSubTile(compositeId, subZoom, subMinX + column, subMinY + row, trimmedTemplate);
    }
}

void ImageryTileProvider::requestSingleTile(int zoom, int x, int y, const QString &urlTemplate)
{
    const QString path = tileCachePath(zoom, x, y, urlTemplate);
    const QImage cached(path);
    if (!cached.isNull()) {
        emit tileReady(zoom, x, y, QUrl::fromLocalFile(path));
        return;
    }

    QNetworkRequest request(QUrl(expandedUrl(zoom, x, y, urlTemplate)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DroneTerrainSim/0.1 (Qt tile cache)"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_network.get(request);
    m_pendingReplies.insert(reply, {zoom, x, y, path});
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        downloadFinished(reply);
    });
}

void ImageryTileProvider::downloadFinished(QNetworkReply *reply)
{
    const auto it = m_pendingReplies.find(reply);
    if (it == m_pendingReplies.end()) {
        reply->deleteLater();
        return;
    }

    const PendingTile pending = it.value();
    m_pendingReplies.erase(it);
    const QByteArray bytes = reply->readAll();
    QImage image;
    const bool validImage = reply->error() == QNetworkReply::NoError && image.loadFromData(bytes);
    reply->deleteLater();

    if (validImage) {
        QFile file(pending.path);
        if (file.open(QIODevice::WriteOnly))
            file.write(bytes);
        emit tileReady(pending.zoom, pending.x, pending.y, QUrl::fromLocalFile(pending.path));
    } else {
        emit tileReady(pending.zoom, pending.x, pending.y, placeholderImage());
    }
}

void ImageryTileProvider::requestSubTile(int compositeId, int zoom, int x, int y,
                                         const QString &urlTemplate)
{
    const QString path = tileCachePath(zoom, x, y, urlTemplate);
    const QImage cached(path);
    if (!cached.isNull()) {
        subTileFinished(compositeId);
        return;
    }

    QNetworkRequest request(QUrl(expandedUrl(zoom, x, y, urlTemplate)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DroneTerrainSim/0.1 (Qt tile cache)"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_network.get(request);
    m_pendingSubTiles.insert(reply, {compositeId, path});
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        subTileDownloadFinished(reply);
    });
}

void ImageryTileProvider::subTileDownloadFinished(QNetworkReply *reply)
{
    const auto it = m_pendingSubTiles.find(reply);
    if (it == m_pendingSubTiles.end()) {
        reply->deleteLater();
        return;
    }

    const PendingSubTile pending = it.value();
    m_pendingSubTiles.erase(it);
    const QByteArray bytes = reply->readAll();
    QImage image;
    const bool validImage = reply->error() == QNetworkReply::NoError && image.loadFromData(bytes);
    reply->deleteLater();

    if (validImage) {
        QFile file(pending.path);
        if (file.open(QIODevice::WriteOnly))
            file.write(bytes);
    }
    subTileFinished(pending.compositeId);
}

void ImageryTileProvider::subTileFinished(int compositeId)
{
    const auto it = m_composites.find(compositeId);
    if (it == m_composites.end())
        return;
    if (--it.value().remaining > 0)
        return;

    stitchComposite(it.value());
    m_composites.erase(it);
}

void ImageryTileProvider::stitchComposite(const CompositeRequest &composite)
{
    QImage image(composite.span * kTileSize, composite.span * kTileSize, QImage::Format_RGB32);
    image.fill(QColor(QStringLiteral("#35454c")));
    QPainter painter(&image);
    for (int row = 0; row < composite.span; ++row) {
        for (int column = 0; column < composite.span; ++column) {
            const QString path = tileCachePath(composite.subZoom, composite.subMinX + column,
                                               composite.subMinY + row, composite.urlTemplate);
            const QImage subImage(path);
            if (!subImage.isNull())
                painter.drawImage(QRect(column * kTileSize, row * kTileSize, kTileSize, kTileSize),
                                  subImage);
        }
    }
    painter.end();
    image.save(composite.compositePath);
    emit tileReady(composite.zoom, composite.x, composite.y,
                   QUrl::fromLocalFile(composite.compositePath));
}

void ImageryTileProvider::cancelAll()
{
    const auto replies = m_pendingReplies.keys();
    m_pendingReplies.clear();
    for (QNetworkReply *reply : replies) {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }

    const auto subReplies = m_pendingSubTiles.keys();
    m_pendingSubTiles.clear();
    for (QNetworkReply *reply : subReplies) {
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }

    m_composites.clear();
}
