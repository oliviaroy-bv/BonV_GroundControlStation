#pragma once

#include <QQuick3DGeometry>
#include <QVariantMap>
#include <QVector2D>
#include <QVector3D>

class TerrainManager;

class TerrainTileGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int zoom READ zoom WRITE setZoom NOTIFY tileChanged)
    Q_PROPERTY(int tileX READ tileX WRITE setTileX NOTIFY tileChanged)
    Q_PROPERTY(int tileY READ tileY WRITE setTileY NOTIFY tileChanged)
    Q_PROPERTY(int gridSize READ gridSize WRITE setGridSize NOTIFY gridSizeChanged)
    Q_PROPERTY(double verticalScale READ verticalScale WRITE setVerticalScale NOTIFY verticalScaleChanged)
    Q_PROPERTY(double skirtDepth READ skirtDepth WRITE setSkirtDepth NOTIFY skirtDepthChanged)
    Q_PROPERTY(double originLatitude READ originLatitude WRITE setOriginLatitude NOTIFY originChanged)
    Q_PROPERTY(double originLongitude READ originLongitude WRITE setOriginLongitude NOTIFY originChanged)
    Q_PROPERTY(double originHeight READ originHeight WRITE setOriginHeight NOTIFY originChanged)
    Q_PROPERTY(TerrainManager *manager READ manager WRITE setManager NOTIFY managerChanged)

public:
    explicit TerrainTileGeometry(QQuick3DObject *parent = nullptr);

    [[nodiscard]] int zoom() const { return m_zoom; }
    [[nodiscard]] int tileX() const { return m_tileX; }
    [[nodiscard]] int tileY() const { return m_tileY; }
    [[nodiscard]] int gridSize() const { return m_gridSize; }
    [[nodiscard]] double verticalScale() const { return m_verticalScale; }
    [[nodiscard]] double skirtDepth() const { return m_skirtDepth; }
    [[nodiscard]] double originLatitude() const { return m_originLatitude; }
    [[nodiscard]] double originLongitude() const { return m_originLongitude; }
    [[nodiscard]] double originHeight() const { return m_originHeight; }
    [[nodiscard]] TerrainManager *manager() const { return m_manager; }

    void setZoom(int value);
    void setTileX(int value);
    void setTileY(int value);
    void setGridSize(int value);
    void setVerticalScale(double value);
    void setSkirtDepth(double value);
    void setOriginLatitude(double value);
    void setOriginLongitude(double value);
    void setOriginHeight(double value);
    void setManager(TerrainManager *manager);

    Q_INVOKABLE QVariantMap coordinatesForUv(const QVector2D &uv) const;
    Q_INVOKABLE QVector3D scenePositionForCoordinate(double latitude, double longitude,
                                                     double heightAboveGround = 0.0) const;
    Q_INVOKABLE void rebuild();

signals:
    void tileChanged();
    void gridSizeChanged();
    void verticalScaleChanged();
    void skirtDepthChanged();
    void originChanged();
    void managerChanged();

private:
    struct Vertex {
        float px;
        float py;
        float pz;
        float nx;
        float ny;
        float nz;
        float u;
        float v;
    };

    [[nodiscard]] double minLatitude() const;
    [[nodiscard]] double maxLatitude() const;
    [[nodiscard]] double minLongitude() const;
    [[nodiscard]] double maxLongitude() const;
    [[nodiscard]] double elevationAt(double latitude, double longitude) const;
    [[nodiscard]] QVector3D vertexPosition(double latitude, double longitude,
                                           double elevation) const;

    int m_zoom = 0;
    int m_tileX = 0;
    int m_tileY = 0;
    int m_gridSize = 33;
    double m_verticalScale = 4.0;
    double m_skirtDepth = 60.0;
    double m_originLatitude = 0.0;
    double m_originLongitude = 0.0;
    double m_originHeight = 0.0;
    TerrainManager *m_manager = nullptr;
};
