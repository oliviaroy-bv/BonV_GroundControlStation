#include "RasterOverlayStorage.h"

#include "storage/AppDataPaths.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
namespace {

QString cleanFileName(const QString &fileName)
{
    return QFileInfo(fileName).fileName();
}

} // namespace

namespace RasterOverlayStorage
{

QString overlayDirectory()
{
    const QString directory = AppDataPaths::rasterOverlaysDirectory();
    QDir().mkpath(directory);
    return directory;
}

QString overlayFilePath(const QString &fileName)
{
    return QDir(overlayDirectory()).filePath(cleanFileName(fileName));
}

QString overlayUrl(const QString &fileName)
{
    QUrl url;
    url.setScheme(QStringLiteral("bonv"));
    url.setHost(QStringLiteral("overlay"));
    url.setPath(QLatin1Char('/') + cleanFileName(fileName));
    return url.toString();
}

} // namespace RasterOverlayStorage
