#include "terraintilegeometry.h"

#include "geomath.h"
#include "terrainmanager.h"

#include <algorithm>
#include <cstddef>
#include <limits>

TerrainTileGeometry::TerrainTileGeometry(QQuick3DObject *parent)
    : QQuick3DGeometry(parent)
{
    rebuild();
}

namespace {

bool updateDouble(double &target, double value)
{
    if (qFuzzyCompare(target, value))
        return false;
    target = value;
    return true;
}

} // namespace

void TerrainTileGeometry::setZoom(int value)
{
    if (m_zoom == value)
        return;
    m_zoom = value;
    emit tileChanged();
    rebuild();
}

void TerrainTileGeometry::setTileX(int value)
{
    if (m_tileX == value)
        return;
    m_tileX = value;
    emit tileChanged();
    rebuild();
}

void TerrainTileGeometry::setTileY(int value)
{
    if (m_tileY == value)
        return;
    m_tileY = value;
    emit tileChanged();
    rebuild();
}

void TerrainTileGeometry::setGridSize(int value)
{
    value = std::clamp(value, 5, 257);
    if (m_gridSize == value)
        return;
    m_gridSize = value;
    emit gridSizeChanged();
    rebuild();
}

void TerrainTileGeometry::setVerticalScale(double value)
{
    value = std::clamp(value, 0.1, 20.0);
    if (!updateDouble(m_verticalScale, value))
        return;
    emit verticalScaleChanged();
    rebuild();
}

void TerrainTileGeometry::setSkirtDepth(double value)
{
    value = std::max(value, 0.0);
    if (!updateDouble(m_skirtDepth, value))
        return;
    emit skirtDepthChanged();
    rebuild();
}

void TerrainTileGeometry::setOriginLatitude(double value)
{
    if (!updateDouble(m_originLatitude, value))
        return;
    emit originChanged();
    rebuild();
}

void TerrainTileGeometry::setOriginLongitude(double value)
{
    if (!updateDouble(m_originLongitude, value))
        return;
    emit originChanged();
    rebuild();
}

void TerrainTileGeometry::setOriginHeight(double value)
{
    if (!updateDouble(m_originHeight, value))
        return;
    emit originChanged();
    rebuild();
}

void TerrainTileGeometry::setManager(TerrainManager *manager)
{
    if (m_manager == manager)
        return;
    m_manager = manager;
    emit managerChanged();
    rebuild();
}

double TerrainTileGeometry::minLatitude() const
{
    return GeoMath::tileYToLatitude(m_tileY + 1, m_zoom);
}

double TerrainTileGeometry::maxLatitude() const
{
    return GeoMath::tileYToLatitude(m_tileY, m_zoom);
}

double TerrainTileGeometry::minLongitude() const
{
    return GeoMath::tileXToLongitude(m_tileX, m_zoom);
}

double TerrainTileGeometry::maxLongitude() const
{
    return GeoMath::tileXToLongitude(m_tileX + 1, m_zoom);
}

double TerrainTileGeometry::elevationAt(double latitude, double longitude) const
{
    return m_manager ? m_manager->elevationAt(latitude, longitude) : 0.0;
}

QVector3D TerrainTileGeometry::vertexPosition(double latitude, double longitude,
                                              double elevation) const
{
    const auto horizontal = GeoMath::latLonHeightToEnu(latitude, longitude, m_originHeight,
                                                       m_originLatitude, m_originLongitude,
                                                       m_originHeight);
    const double up = (elevation - m_originHeight) * m_verticalScale;
    return QVector3D(static_cast<float>(horizontal.x), static_cast<float>(up),
                     static_cast<float>(-horizontal.y));
}

void TerrainTileGeometry::rebuild()
{
    clear();
    if (m_gridSize < 2 || !m_manager)
        return;

    const double south = minLatitude();
    const double north = maxLatitude();
    const double west = minLongitude();
    const double east = maxLongitude();
    if (!(south < north) || !(west < east))
        return;

    const int gridCount = m_gridSize * m_gridSize;
    QVector<Vertex> vertices(gridCount);
    QVector<QVector3D> positions(gridCount);

    QVector3D boundsMin(std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max());
    QVector3D boundsMax(std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest());

    // Vertex rows are spaced evenly in latitude, but V must follow the tile's Mercator
    // projection so the raster imagery (itself Mercator-projected) maps without warping.
    const double topMercator = GeoMath::mercatorY(north);
    const double bottomMercator = GeoMath::mercatorY(south);
    const double mercatorRange = std::max(topMercator - bottomMercator, 1.0e-15);

    for (int row = 0; row < m_gridSize; ++row) {
        const double rowFraction = static_cast<double>(row) / (m_gridSize - 1);
        const double latitude = std::lerp(north, south, rowFraction);
        for (int column = 0; column < m_gridSize; ++column) {
            const double columnFraction = static_cast<double>(column) / (m_gridSize - 1);
            const double longitude = std::lerp(west, east, columnFraction);
            const double elevation = elevationAt(latitude, longitude);
            const QVector3D position = vertexPosition(latitude, longitude, elevation);
            const int index = row * m_gridSize + column;
            positions[index] = position;
            Vertex &vertex = vertices[index];
            vertex.px = position.x();
            vertex.py = position.y();
            vertex.pz = position.z();
            vertex.nx = 0.0f;
            vertex.ny = 1.0f;
            vertex.nz = 0.0f;
            vertex.u = static_cast<float>(columnFraction);
            vertex.v = static_cast<float>((topMercator - GeoMath::mercatorY(latitude)) / mercatorRange);
            boundsMin.setX(std::min(boundsMin.x(), position.x()));
            boundsMin.setY(std::min(boundsMin.y(), position.y()));
            boundsMin.setZ(std::min(boundsMin.z(), position.z()));
            boundsMax.setX(std::max(boundsMax.x(), position.x()));
            boundsMax.setY(std::max(boundsMax.y(), position.y()));
            boundsMax.setZ(std::max(boundsMax.z(), position.z()));
        }
    }

    for (int row = 0; row < m_gridSize; ++row) {
        for (int column = 0; column < m_gridSize; ++column) {
            const int left = row * m_gridSize + std::max(column - 1, 0);
            const int right = row * m_gridSize + std::min(column + 1, m_gridSize - 1);
            const int top = std::max(row - 1, 0) * m_gridSize + column;
            const int bottom = std::min(row + 1, m_gridSize - 1) * m_gridSize + column;
            const QVector3D tangentEast = positions[right] - positions[left];
            const QVector3D tangentSouth = positions[bottom] - positions[top];
            QVector3D normal = QVector3D::crossProduct(tangentSouth, tangentEast).normalized();
            if (normal.y() < 0.0f)
                normal = -normal;
            Vertex &vertex = vertices[row * m_gridSize + column];
            vertex.nx = normal.x();
            vertex.ny = normal.y();
            vertex.nz = normal.z();
        }
    }

    QVector<quint32> indices;
    indices.reserve((m_gridSize - 1) * (m_gridSize - 1) * 6);
    for (int row = 0; row < m_gridSize - 1; ++row) {
        for (int column = 0; column < m_gridSize - 1; ++column) {
            const quint32 topLeft = row * m_gridSize + column;
            const quint32 topRight = topLeft + 1;
            const quint32 bottomLeft = topLeft + m_gridSize;
            const quint32 bottomRight = bottomLeft + 1;
            indices << topLeft << bottomLeft << topRight;
            indices << topRight << bottomLeft << bottomRight;
        }
    }

    // Skirts: a curtain of triangles dropped from each edge to hide cracks against
    // neighboring tiles. Terrain materials use NoCulling so winding here doesn't matter.
    const float skirtDrop = static_cast<float>(m_skirtDepth * m_verticalScale);
    const auto appendSkirtEdge = [&](const QVector<int> &edgeIndices) {
        const quint32 skirtBase = static_cast<quint32>(vertices.size());
        for (int mainIndex : edgeIndices) {
            Vertex vertex = vertices[mainIndex];
            vertex.py -= skirtDrop;
            vertices.append(vertex);
        }
        for (int i = 0; i + 1 < edgeIndices.size(); ++i) {
            const quint32 mainA = static_cast<quint32>(edgeIndices[i]);
            const quint32 mainB = static_cast<quint32>(edgeIndices[i + 1]);
            const quint32 skirtA = skirtBase + static_cast<quint32>(i);
            const quint32 skirtB = skirtA + 1;
            indices << mainA << skirtA << mainB;
            indices << mainB << skirtA << skirtB;
        }
    };

    QVector<int> topEdge;
    QVector<int> bottomEdge;
    QVector<int> leftEdge;
    QVector<int> rightEdge;
    topEdge.reserve(m_gridSize);
    bottomEdge.reserve(m_gridSize);
    leftEdge.reserve(m_gridSize);
    rightEdge.reserve(m_gridSize);
    for (int i = 0; i < m_gridSize; ++i) {
        topEdge.append(i);
        bottomEdge.append((m_gridSize - 1) * m_gridSize + i);
        leftEdge.append(i * m_gridSize);
        rightEdge.append(i * m_gridSize + (m_gridSize - 1));
    }
    appendSkirtEdge(topEdge);
    appendSkirtEdge(bottomEdge);
    appendSkirtEdge(leftEdge);
    appendSkirtEdge(rightEdge);
    boundsMin.setY(boundsMin.y() - skirtDrop);

    setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
    setStride(sizeof(Vertex));
    setVertexData(QByteArray(reinterpret_cast<const char *>(vertices.constData()),
                             vertices.size() * sizeof(Vertex)));
    setIndexData(QByteArray(reinterpret_cast<const char *>(indices.constData()),
                            indices.size() * sizeof(quint32)));
    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, offsetof(Vertex, px),
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, offsetof(Vertex, nx),
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::TexCoord0Semantic, offsetof(Vertex, u),
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::IndexSemantic, 0,
                 QQuick3DGeometry::Attribute::U32Type);
    setBounds(boundsMin, boundsMax);
}

QVariantMap TerrainTileGeometry::coordinatesForUv(const QVector2D &uv) const
{
    const double clampedU = std::clamp(static_cast<double>(uv.x()), 0.0, 1.0);
    const double clampedV = std::clamp(static_cast<double>(uv.y()), 0.0, 1.0);
    const double longitude = std::lerp(minLongitude(), maxLongitude(), clampedU);
    const double top = GeoMath::mercatorY(maxLatitude());
    const double bottom = GeoMath::mercatorY(minLatitude());
    const double latitude = GeoMath::inverseMercatorY(std::lerp(top, bottom, clampedV));
    const double elevation = elevationAt(latitude, longitude);
    return {{QStringLiteral("latitude"), latitude},
            {QStringLiteral("longitude"), longitude},
            {QStringLiteral("altitude"), elevation}};
}

QVector3D TerrainTileGeometry::scenePositionForCoordinate(double latitude, double longitude,
                                                          double heightAboveGround) const
{
    return vertexPosition(latitude, longitude,
                          elevationAt(latitude, longitude) + heightAboveGround);
}
