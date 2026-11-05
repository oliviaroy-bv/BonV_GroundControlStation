#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQmlEngine>
#include <QUrl>

class QNetworkReply;

class ImageryTileProvider : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl cacheRoot READ cacheRoot WRITE setCacheRoot NOTIFY cacheRootChanged)
    Q_PROPERTY(QUrl demDirectoryUrl READ demDirectoryUrl NOTIFY cacheRootChanged)

public:
    explicit ImageryTileProvider(QObject *parent = nullptr);

    [[nodiscard]] QUrl cacheRoot() const;
    void setCacheRoot(const QUrl &value);
    [[nodiscard]] QUrl demDirectoryUrl() const;

    // detailLevels > 0 sources the tile's texture from a (2^detailLevels)^2 mosaic of
    // finer-zoom imagery tiles stitched together, so texture sharpness isn't capped by
    // the (possibly much coarser) terrain mesh zoom for the same tile.
    Q_INVOKABLE void requestTile(int zoom, int x, int y, const QString &urlTemplate,
                                 int detailLevels = 0);
    Q_INVOKABLE void cancelAll();

signals:
    void cacheRootChanged();
    void tileReady(int zoom, int x, int y, const QUrl &imagePath);

private:
    struct PendingTile {
        int zoom = 0;
        int x = 0;
        int y = 0;
        QString path;
    };

    struct PendingSubTile {
        int compositeId = 0;
        QString path;
    };

    struct CompositeRequest {
        int zoom = 0;
        int x = 0;
        int y = 0;
        int subZoom = 0;
        int subMinX = 0;
        int subMinY = 0;
        int span = 1;
        int remaining = 0;
        QString urlTemplate;
        QString compositePath;
    };

    void ensureCacheDirectories();
    void requestSingleTile(int zoom, int x, int y, const QString &urlTemplate);
    void downloadFinished(QNetworkReply *reply);
    void requestSubTile(int compositeId, int zoom, int x, int y, const QString &urlTemplate);
    void subTileDownloadFinished(QNetworkReply *reply);
    void subTileFinished(int compositeId);
    void stitchComposite(const CompositeRequest &composite);
    [[nodiscard]] QString tileCachePath(int zoom, int x, int y, const QString &urlTemplate) const;
    [[nodiscard]] QString expandedUrl(int zoom, int x, int y, const QString &urlTemplate) const;
    [[nodiscard]] QString sourceCacheId(const QString &urlTemplate) const;
    [[nodiscard]] QUrl placeholderImage();
    [[nodiscard]] static QString quadKey(int x, int y, int zoom);

    QNetworkAccessManager m_network;
    QString m_cacheRoot;
    QUrl m_placeholderUrl;
    QHash<QNetworkReply *, PendingTile> m_pendingReplies;
    QHash<QNetworkReply *, PendingSubTile> m_pendingSubTiles;
    QHash<int, CompositeRequest> m_composites;
    int m_nextCompositeId = 1;
};
