#pragma once

#include "elevationprovider.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QUrl>
#include <QVector>

class ImageryTileProvider;

class TerrainManager : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl demDirectory READ demDirectory WRITE setDemDirectory NOTIFY demDirectoryChanged)
    Q_PROPERTY(int gridSize READ gridSize WRITE setGridSize NOTIFY gridSizeChanged)
    Q_PROPERTY(double verticalScale READ verticalScale WRITE setVerticalScale NOTIFY verticalScaleChanged)
    Q_PROPERTY(double skirtDepth READ skirtDepth WRITE setSkirtDepth NOTIFY skirtDepthChanged)
    Q_PROPERTY(ImageryTileProvider *imageryProvider READ imageryProvider WRITE setImageryProvider NOTIFY imageryProviderChanged)
    Q_PROPERTY(int imageryDetailLevels READ imageryDetailLevels WRITE setImageryDetailLevels NOTIFY imageryDetailLevelsChanged)
    Q_PROPERTY(bool srtmAutoDownload READ srtmAutoDownload WRITE setSrtmAutoDownload NOTIFY srtmAutoDownloadChanged)
    Q_PROPERTY(double originLatitude READ originLatitude NOTIFY originChanged)
    Q_PROPERTY(double originLongitude READ originLongitude NOTIFY originChanged)
    Q_PROPERTY(double originHeight READ originHeight NOTIFY originChanged)
    Q_PROPERTY(double regionRadiusMeters READ regionRadiusMeters NOTIFY originChanged)
    Q_PROPERTY(bool hasDemCoverage READ hasDemCoverage NOTIFY regionLoaded)
    Q_PROPERTY(int demTileCount READ demTileCount NOTIFY regionLoaded)
    Q_PROPERTY(int loadedDemTileCount READ loadedDemTileCount NOTIFY regionLoaded)
    Q_PROPERTY(QString missingDemTiles READ missingDemTiles NOTIFY regionLoaded)
    Q_PROPERTY(double elevationMinimum READ elevationMinimum NOTIFY regionLoaded)
    Q_PROPERTY(double elevationMaximum READ elevationMaximum NOTIFY regionLoaded)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    enum Role {
        ZoomRole = Qt::UserRole + 1,
        TileXRole,
        TileYRole,
        ImageryUrlRole,
    };
    Q_ENUM(Role)

    explicit TerrainManager(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QUrl demDirectory() const { return m_demDirectory; }
    void setDemDirectory(const QUrl &value);
    [[nodiscard]] int gridSize() const { return m_gridSize; }
    void setGridSize(int value);
    [[nodiscard]] double verticalScale() const { return m_verticalScale; }
    void setVerticalScale(double value);
    [[nodiscard]] double skirtDepth() const { return m_skirtDepth; }
    void setSkirtDepth(double value);
    [[nodiscard]] ImageryTileProvider *imageryProvider() const { return m_imageryProvider; }
    void setImageryProvider(ImageryTileProvider *provider);
    [[nodiscard]] int imageryDetailLevels() const { return m_imageryDetailLevels; }
    void setImageryDetailLevels(int value);
    [[nodiscard]] bool srtmAutoDownload() const { return m_srtmAutoDownload; }
    void setSrtmAutoDownload(bool value);
    [[nodiscard]] double originLatitude() const { return m_originLatitude; }
    [[nodiscard]] double originLongitude() const { return m_originLongitude; }
    [[nodiscard]] double originHeight() const { return m_originHeight; }
    [[nodiscard]] double regionRadiusMeters() const { return m_regionRadiusMeters; }
    [[nodiscard]] bool hasDemCoverage() const;
    [[nodiscard]] int demTileCount() const { return m_demTileCount; }
    [[nodiscard]] int loadedDemTileCount() const { return m_loadedDemTileCount; }
    [[nodiscard]] QString missingDemTiles() const { return m_missingDemTiles; }
    [[nodiscard]] double elevationMinimum() const { return m_elevationMinimum; }
    [[nodiscard]] double elevationMaximum() const { return m_elevationMaximum; }
    [[nodiscard]] QString status() const { return m_status; }
    [[nodiscard]] double progress() const { return m_progress; }
    [[nodiscard]] bool busy() const { return m_busy; }

    Q_INVOKABLE double elevationAt(double latitude, double longitude) const;
    Q_INVOKABLE void loadRegion(double centerLatitude, double centerLongitude, double radiusKm,
                                int zoom, const QString &imageryUrlTemplate);
    Q_INVOKABLE void cancel();
    // Per-tile distance-based LOD: called continuously (throttled) from QML with the
    // camera's world position in the same ENU meters frame as the terrain mesh. Only
    // tiles whose target detail level actually changed get re-requested, so this is
    // cheap to call often and never disturbs tiles the camera isn't near.
    Q_INVOKABLE void updateCameraPosition(double x, double y, double z);

signals:
    void demDirectoryChanged();
    void gridSizeChanged();
    void verticalScaleChanged();
    void skirtDepthChanged();
    void imageryProviderChanged();
    void imageryDetailLevelsChanged();
    void srtmAutoDownloadChanged();
    void originChanged();
    void regionLoaded();
    void statusChanged();
    void progressChanged();
    void busyChanged();
    void errorOccurred(const QString &message);

private:
    struct Tile {
        int zoom = 0;
        int tileX = 0;
        int tileY = 0;
        QUrl imageryUrl;
        // ENU ground position (meters, relative to the region origin) of the tile's
        // center, precomputed at load time so per-tile LOD distance checks are cheap.
        double centerX = 0.0;
        double centerZ = 0.0;
        int detailLevel = 0;
    };

    void setStatus(const QString &status);
    void setProgress(double progress);
    void setBusy(bool busy);
    void requestImagery(int row);
    void startImageryRequests();

    QUrl m_demDirectory;
    int m_gridSize = 33;
    double m_verticalScale = 4.0;
    double m_skirtDepth = 60.0;
    ImageryTileProvider *m_imageryProvider = nullptr;
    int m_imageryDetailLevels = 3;
    bool m_srtmAutoDownload = true;
    double m_originLatitude = 0.0;
    double m_originLongitude = 0.0;
    double m_originHeight = 0.0;
    double m_regionRadiusMeters = 0.0;
    int m_demTileCount = 0;
    int m_loadedDemTileCount = 0;
    QString m_missingDemTiles;
    double m_elevationMinimum = 0.0;
    double m_elevationMaximum = 0.0;
    QString m_status;
    double m_progress = 0.0;
    bool m_busy = false;
    int m_totalTileRequests = 0;
    // Floor for per-tile distance refinement: whatever detail the region loaded at,
    // proximity only ever adds to it, never drops a tile below what it started with.
    int m_baseImageryDetailLevel = 0;
    int m_completedTileRequests = 0;
    QString m_imageryUrlTemplate;
    ElevationProvider m_elevationProvider;
    QVector<Tile> m_tiles;
};
