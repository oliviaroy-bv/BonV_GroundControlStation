#include "srtmdownloader.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace {
constexpr int kDownloadTimeoutMs = 10000;
}

bool SrtmDownloader::ensureTile(const QString &demDirectory, const QString &tileBaseName)
{
    if (demDirectory.isEmpty() || tileBaseName.isEmpty())
        return false;

    const QString zipPath = demDirectory + QLatin1Char('/') + tileBaseName + QStringLiteral(".hgt.zip");
    const QString hgtPath = demDirectory + QLatin1Char('/') + tileBaseName + QStringLiteral(".hgt");
    if (QFileInfo::exists(zipPath) || QFileInfo::exists(hgtPath))
        return true;

    QDir().mkpath(demDirectory);

    QNetworkAccessManager network;
    QNetworkRequest request(QUrl(QStringLiteral("https://terrain.ardupilot.org/SRTM1/")
                                 + tileBaseName + QStringLiteral(".hgt.zip")));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("DroneTerrainSim/0.1 (Qt SRTM fetch)"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = network.get(request);

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeoutTimer.start(kDownloadTimeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
        reply->disconnect();
        reply->abort();
        reply->deleteLater();
        return false;
    }

    const bool ok = reply->error() == QNetworkReply::NoError;
    const QByteArray bytes = ok ? reply->readAll() : QByteArray();
    reply->deleteLater();
    if (!ok || bytes.isEmpty())
        return false;

    QFile file(zipPath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(bytes);
    return true;
}
