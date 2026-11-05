#include "elevationprovider.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

namespace {

void appendLe16(QByteArray &bytes, quint16 value)
{
    const qsizetype offset = bytes.size();
    bytes.resize(offset + 2);
    qToLittleEndian(value, reinterpret_cast<uchar *>(bytes.data() + offset));
}

void appendLe32(QByteArray &bytes, quint32 value)
{
    const qsizetype offset = bytes.size();
    bytes.resize(offset + 4);
    qToLittleEndian(value, reinterpret_cast<uchar *>(bytes.data() + offset));
}

QByteArray storedZip(const QString &name, const QByteArray &payload)
{
    const QByteArray filename = name.toUtf8();
    QByteArray zip;
    appendLe32(zip, 0x04034b50);
    appendLe16(zip, 20);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe32(zip, 0);
    appendLe32(zip, payload.size());
    appendLe32(zip, payload.size());
    appendLe16(zip, filename.size());
    appendLe16(zip, 0);
    zip.append(filename);
    zip.append(payload);

    const quint32 centralOffset = zip.size();
    appendLe32(zip, 0x02014b50);
    appendLe16(zip, 20);
    appendLe16(zip, 20);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe32(zip, 0);
    appendLe32(zip, payload.size());
    appendLe32(zip, payload.size());
    appendLe16(zip, filename.size());
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe32(zip, 0);
    appendLe32(zip, 0);
    zip.append(filename);

    const quint32 centralSize = zip.size() - centralOffset;
    appendLe32(zip, 0x06054b50);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 1);
    appendLe16(zip, 1);
    appendLe32(zip, centralSize);
    appendLe32(zip, centralOffset);
    appendLe16(zip, 0);
    return zip;
}

} // namespace

class ElevationProviderTests : public QObject
{
    Q_OBJECT

private slots:
    void loadsNestedSrtmNamedWithSuffix();
};

void ElevationProviderTests::loadsNestedSrtmNamedWithSuffix()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString demDirectory = temp.path() + QStringLiteral("/nested");
    QVERIFY(QDir().mkpath(demDirectory));

    QByteArray hgt;
    QDataStream stream(&hgt, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    const qint16 samples[] = {
        120, 130, 140,
        100, 110, 120,
        80,  90,  100
    };
    for (qint16 sample : samples)
        stream << sample;

    QFile file(demDirectory + QStringLiteral("/N12E077.SRTMGL1.hgt.zip"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(storedZip(QStringLiteral("N12E077.SRTMGL1.hgt"), hgt)),
             storedZip(QStringLiteral("N12E077.SRTMGL1.hgt"), hgt).size());
    file.close();

    ElevationProvider provider;
    provider.setDemDirectory(temp.path());
    QVERIFY(provider.hasCoverage(12.5, 77.5));
    QCOMPARE(provider.coverageCountForRegion(12.1, 77.2, 12.9, 77.8), 1);
    QCOMPARE(provider.elevationAt(12.5, 77.5, false), 110.0);
}

QTEST_GUILESS_MAIN(ElevationProviderTests)
#include "test_elevationprovider.moc"
