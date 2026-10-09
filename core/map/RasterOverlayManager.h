#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantList>

class QUrl;

class RasterOverlayManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QVariantList overlays READ overlays NOTIFY overlaysChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    explicit RasterOverlayManager(QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool enabled);

    QVariantList overlays() const;
    QString statusMessage() const;

    Q_INVOKABLE void importGeoTiff(const QUrl &fileUrl);
    Q_INVOKABLE void removeOverlay(const QString &id);
    Q_INVOKABLE void setOverlayVisible(const QString &id, bool visible);
    Q_INVOKABLE void setOverlayOpacity(const QString &id, double opacity);

signals:
    void enabledChanged();
    void overlaysChanged();
    void overlayOpacityChanged(const QString &id, double opacity);
    void statusMessageChanged();

private:
    void setStatusMessage(const QString &message);
    int overlayIndex(const QString &id) const;

    QSettings _settings;
    bool _enabled = false;
    QVariantList _overlays;
    QString _statusMessage;
};
