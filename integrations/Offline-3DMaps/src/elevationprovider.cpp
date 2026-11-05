#include "elevationprovider.h"

#include <QDataStream>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>
#include <QtMath>
#include <QtZlib/zlib.h>

#include <cmath>
#include <limits>

namespace {

quint16 le16(const QByteArray &bytes, int offset)
{
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(bytes.constData() + offset));
}

quint32 le32(const QByteArray &bytes, int offset)
{
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + offset));
}

QByteArray inflateRawDeflate(const QByteArray &input, qsizetype expectedSize)
{
    QByteArray output(expectedSize, '\0');
    z_stream stream = {};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = reinterpret_cast<Bytef *>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return {};
    const int result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (result != Z_STREAM_END)
        return {};
    output.resize(static_cast<qsizetype>(stream.total_out));
    return output;
}

QByteArray readHgtZip(const QString &path, const QString &tileBaseName)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray archive = file.readAll();

    int endOfCentralDirectory = -1;
    for (int i = archive.size() - 22; i >= 0; --i) {
        if (le32(archive, i) == 0x06054b50) {
            endOfCentralDirectory = i;
            break;
        }
    }
    if (endOfCentralDirectory < 0)
        return {};

    const int entryCount = le16(archive, endOfCentralDirectory + 10);
    int centralOffset = static_cast<int>(le32(archive, endOfCentralDirectory + 16));
    for (int entry = 0; entry < entryCount && centralOffset + 46 <= archive.size(); ++entry) {
        if (le32(archive, centralOffset) != 0x02014b50)
            return {};

        const quint16 method = le16(archive, centralOffset + 10);
        const quint32 compressedSize = le32(archive, centralOffset + 20);
        const quint32 uncompressedSize = le32(archive, centralOffset + 24);
        const quint16 nameLength = le16(archive, centralOffset + 28);
        const quint16 extraLength = le16(archive, centralOffset + 30);
        const quint16 commentLength = le16(archive, centralOffset + 32);
        const quint32 localOffset = le32(archive, centralOffset + 42);
        const QString entryName = QString::fromUtf8(
            archive.mid(centralOffset + 46, nameLength)).toUpper();

        const bool wanted = entryName.endsWith(QStringLiteral(".HGT"))
            && (entryName.contains(tileBaseName.toUpper()) || tileBaseName.isEmpty());
        if (wanted && localOffset + 30 <= static_cast<quint32>(archive.size())
            && le32(archive, static_cast<int>(localOffset)) == 0x04034b50) {
            const int local = static_cast<int>(localOffset);
            const quint16 localNameLength = le16(archive, local + 26);
            const quint16 localExtraLength = le16(archive, local + 28);
            const int dataOffset = local + 30 + localNameLength + localExtraLength;
            if (dataOffset + static_cast<int>(compressedSize) > archive.size())
                return {};
            const QByteArray compressed = archive.mid(dataOffset, compressedSize);
            if (method == 0)
                return compressed;
            if (method == 8)
                return inflateRawDeflate(compressed, uncompressedSize);
            return {};
        }

        centralOffset += 46 + nameLength + extraLength + commentLength;
    }
    return {};
}

} // namespace

void ElevationProvider::setDemDirectory(const QString &directory)
{
    if (m_demDirectory == directory)
        return;
    m_demDirectory = directory;
    m_tiles.clear();
    m_missingTiles.clear();
}

QString ElevationProvider::demDirectory() const
{
    return m_demDirectory;
}

QString ElevationProvider::tileName(double latitude, double longitude) const
{
    const int south = static_cast<int>(std::floor(latitude));
    const int west = static_cast<int>(std::floor(longitude));
    return QStringLiteral("%1%2%3%4.hgt")
        .arg(south >= 0 ? QLatin1Char('N') : QLatin1Char('S'))
        .arg(std::abs(south), 2, 10, QLatin1Char('0'))
        .arg(west >= 0 ? QLatin1Char('E') : QLatin1Char('W'))
        .arg(std::abs(west), 3, 10, QLatin1Char('0'));
}

const ElevationProvider::Tile *ElevationProvider::loadTile(const QString &name) const
{
    const auto existing = m_tiles.constFind(name);
    if (existing != m_tiles.cend())
        return &existing.value();
    if (m_missingTiles.contains(name))
        return nullptr;

    const QString baseName = QFileInfo(name).completeBaseName().toUpper();
    QString path = m_demDirectory + QLatin1Char('/') + name;
    if (!QFileInfo::exists(path)) {
        const QString upperName = name.toUpper();
        const QString lowerName = name.toLower();
        const QDir directory(m_demDirectory);
        if (QFileInfo::exists(directory.filePath(upperName))) {
            path = directory.filePath(upperName);
        } else if (QFileInfo::exists(directory.filePath(lowerName))) {
            path = directory.filePath(lowerName);
        } else {
            QDirIterator it(m_demDirectory,
                            QStringList{QStringLiteral("*.hgt"), QStringLiteral("*.HGT"),
                                        QStringLiteral("*.hgt.zip"), QStringLiteral("*.HGT.zip"),
                                        QStringLiteral("*.zip"), QStringLiteral("*.ZIP")},
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString candidate = it.next();
                QString candidateBase = QFileInfo(candidate).completeBaseName().toUpper();
                if (candidateBase.endsWith(QStringLiteral(".HGT")))
                    candidateBase.chop(4);
                if (candidateBase == baseName || candidateBase.startsWith(baseName + QLatin1Char('.'))) {
                    path = candidate;
                    break;
                }
            }
        }
    }

    QByteArray bytes;
    if (path.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
        bytes = readHgtZip(path, baseName);
    } else {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly))
            bytes = file.readAll();
    }

    if (bytes.isEmpty()) {
        m_missingTiles.insert(name, true);
        return nullptr;
    }

    const qint64 sampleCount = bytes.size() / 2;
    const int side = static_cast<int>(std::llround(std::sqrt(static_cast<double>(sampleCount))));
    if (bytes.size() % 2 != 0 || side * side != sampleCount || side < 2) {
        m_missingTiles.insert(name, true);
        return nullptr;
    }

    Tile tile;
    tile.samplesPerSide = side;
    tile.samples.resize(static_cast<qsizetype>(sampleCount));
    QDataStream stream(bytes);
    stream.setByteOrder(QDataStream::BigEndian);
    for (qint16 &sample : tile.samples)
        stream >> sample;

    const auto inserted = m_tiles.insert(name, std::move(tile));
    return &inserted.value();
}

bool ElevationProvider::hasCoverage(double latitude, double longitude) const
{
    return loadTile(tileName(latitude, longitude)) != nullptr;
}

QStringList ElevationProvider::tileNamesForRegion(double minLatitude, double minLongitude,
                                                  double maxLatitude, double maxLongitude) const
{
    QStringList names;
    if (!(minLatitude < maxLatitude) || !(minLongitude < maxLongitude))
        return names;

    const int southMin = static_cast<int>(std::floor(minLatitude));
    const int southMax = static_cast<int>(std::floor(std::nextafter(maxLatitude, minLatitude)));
    const int westMin = static_cast<int>(std::floor(minLongitude));
    const int westMax = static_cast<int>(std::floor(std::nextafter(maxLongitude, minLongitude)));
    for (int south = southMin; south <= southMax; ++south) {
        for (int west = westMin; west <= westMax; ++west)
            names.append(tileName(south + 0.5, west + 0.5));
    }
    names.removeDuplicates();
    names.sort();
    return names;
}

int ElevationProvider::coverageCountForRegion(double minLatitude, double minLongitude,
                                              double maxLatitude, double maxLongitude) const
{
    int count = 0;
    for (const QString &name : tileNamesForRegion(minLatitude, minLongitude,
                                                  maxLatitude, maxLongitude)) {
        if (loadTile(name))
            ++count;
    }
    return count;
}

double ElevationProvider::elevationAt(double latitude, double longitude,
                                      bool syntheticFallback) const
{
    const int south = static_cast<int>(std::floor(latitude));
    const int west = static_cast<int>(std::floor(longitude));
    const Tile *tile = loadTile(tileName(latitude, longitude));
    if (!tile)
        return syntheticFallback ? syntheticElevation(latitude, longitude) : 0.0;

    const double x = (longitude - west) * (tile->samplesPerSide - 1);
    const double y = (1.0 - (latitude - south)) * (tile->samplesPerSide - 1);
    const int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, tile->samplesPerSide - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(y)), 0, tile->samplesPerSide - 1);
    const int x1 = std::min(x0 + 1, tile->samplesPerSide - 1);
    const int y1 = std::min(y0 + 1, tile->samplesPerSide - 1);
    const double tx = x - x0;
    const double ty = y - y0;

    const auto sample = [tile](int row, int column) {
        const qint16 value = tile->samples.at(row * tile->samplesPerSide + column);
        return value == std::numeric_limits<qint16>::min() ? 0.0 : static_cast<double>(value);
    };
    const double top = std::lerp(sample(y0, x0), sample(y0, x1), tx);
    const double bottom = std::lerp(sample(y1, x0), sample(y1, x1), tx);
    return std::lerp(top, bottom, ty);
}

double ElevationProvider::syntheticElevation(double latitude, double longitude)
{
    const double a = std::sin(latitude * 530.0) * std::cos(longitude * 410.0);
    const double b = std::sin((latitude + longitude) * 260.0);
    return 18.0 * a + 7.0 * b;
}
