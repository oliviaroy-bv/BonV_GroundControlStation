#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include <functional>

class TileCacheManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool offlineMode READ offlineMode WRITE setOfflineMode NOTIFY offlineModeChanged)
    Q_PROPERTY(qint64 cacheSizeBytes READ cacheSizeBytes NOTIFY cacheStatsChanged)
    Q_PROPERTY(qint64 maximumCacheSizeBytes READ maximumCacheSizeBytes NOTIFY cacheStatsChanged)
    Q_PROPERTY(QString cacheDirectory READ cacheDirectory CONSTANT)
    Q_PROPERTY(bool prefetchActive READ prefetchActive NOTIFY prefetchChanged)
    Q_PROPERTY(int prefetchCompleted READ prefetchCompleted NOTIFY prefetchChanged)
    Q_PROPERTY(int prefetchTotal READ prefetchTotal NOTIFY prefetchChanged)
    Q_PROPERTY(QString prefetchStatus READ prefetchStatus NOTIFY prefetchChanged)

public:
    using FetchCallback = std::function<void(const QByteArray &, const QByteArray &, bool)>;

    explicit TileCacheManager(QObject *parent = nullptr);

    bool offlineMode() const;
    void setOfflineMode(bool offline);
    qint64 cacheSizeBytes() const;
    qint64 maximumCacheSizeBytes() const;
    QString cacheDirectory() const;
    bool prefetchActive() const;
    int prefetchCompleted() const;
    int prefetchTotal() const;
    QString prefetchStatus() const;

    bool readCached(const QUrl &url, QByteArray *data, QByteArray *mimeType);
    void fetch(const QUrl &url, FetchCallback callback);

    Q_INVOKABLE void clearCache();
    Q_INVOKABLE void setMaximumCacheSizeMB(int megabytes);
    Q_INVOKABLE void prefetchMissionArea(const QVariantList &waypoints, int minimumZoom,
                                         int maximumZoom, double bufferMeters,
                                         const QString &styleUrl);

signals:
    void offlineModeChanged();
    void cacheStatsChanged();
    void prefetchChanged();

private:
    QString cacheFilePath(const QUrl &url) const;
    QString mimeFilePath(const QUrl &url) const;
    void storeCached(const QUrl &url, const QByteArray &data, const QByteArray &mimeType);
    void refreshCacheSize();
    void enforceCacheLimit();
    void startPrefetchRequests();
    void finishPrefetch();
    void fetchStyleForPrefetch(const QUrl &styleUrl, const QVariantList &waypoints,
                               int minimumZoom, int maximumZoom, double bufferMeters);
    void beginPrefetchTiles(const QStringList &templates, const QUrl &baseUrl,
                            const QVariantList &waypoints, int minimumZoom,
                            int maximumZoom, double bufferMeters);

    QNetworkAccessManager _network;
    bool _offlineMode = false;
    qint64 _cacheSizeBytes = 0;
    qint64 _maximumCacheSizeBytes = 1024LL * 1024LL * 1024LL;
    QStringList _prefetchUrls;
    int _prefetchNextIndex = 0;
    int _prefetchInFlight = 0;
    int _prefetchCompleted = 0;
    int _prefetchFailed = 0;
    int _prefetchTotal = 0;
    bool _prefetchActive = false;
    QString _prefetchStatus;
};
