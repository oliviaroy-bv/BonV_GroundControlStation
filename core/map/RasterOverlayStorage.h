#pragma once

#include <QString>

namespace RasterOverlayStorage
{
QString overlayDirectory();
QString overlayFilePath(const QString &fileName);
QString overlayUrl(const QString &fileName);
}
