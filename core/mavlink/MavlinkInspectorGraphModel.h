#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QVariantList>
#include <QVector>

// Presentation-neutral, bounded history for the inspector's one active graph.
// Samples enter through the controller's existing 10 Hz batch, never directly
// from a MAVLink transport or decoder.
class MavlinkInspectorGraphModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString link READ link NOTIFY changed)
    Q_PROPERTY(int systemId READ systemId NOTIFY changed)
    Q_PROPERTY(int componentId READ componentId NOTIFY changed)
    Q_PROPERTY(quint32 messageId READ messageId NOTIFY changed)
    Q_PROPERTY(QString messageName READ messageName NOTIFY changed)
    Q_PROPERTY(QString fieldName READ fieldName NOTIFY changed)
    Q_PROPERTY(int sampleCount READ sampleCount NOTIFY changed)
    Q_PROPERTY(double latestValue READ latestValue NOTIFY changed)
    Q_PROPERTY(double minimum READ minimum NOTIFY changed)
    Q_PROPERTY(double maximum READ maximum NOTIFY changed)
    Q_PROPERTY(double visibleStartTime READ visibleStartTime NOTIFY changed)
    Q_PROPERTY(double visibleEndTime READ visibleEndTime NOTIFY changed)
    Q_PROPERTY(QVariantList points READ points NOTIFY changed)

public:
    static constexpr int MaximumSamples = 1000;
    static constexpr double VisibleWindowSeconds = 30.0;

    explicit MavlinkInspectorGraphModel(QObject *parent = nullptr);

    bool active() const { return activeValue; }
    QString link() const { return linkValue; }
    int systemId() const { return systemIdValue; }
    int componentId() const { return componentIdValue; }
    quint32 messageId() const { return messageIdValue; }
    QString messageName() const { return messageNameValue; }
    QString fieldName() const { return fieldNameValue; }
    int sampleCount() const { return samples.size(); }
    double latestValue() const;
    double minimum() const;
    double maximum() const;
    double visibleStartTime() const;
    double visibleEndTime() const;
    QVariantList points() const;

    void selectSeries(const QString &link, int systemId, int componentId, quint32 messageId,
                      const QString &messageName, const QString &fieldName);
    void appendSample(double value);
    void reset();

public slots:
    void clear();

signals:
    void changed();

private:
    struct Sample {
        double timeSeconds = 0.0;
        double value = 0.0;
    };

    void refreshVisibleCache();

    bool activeValue = false;
    QString linkValue;
    int systemIdValue = 0;
    int componentIdValue = 0;
    quint32 messageIdValue = 0;
    QString messageNameValue;
    QString fieldNameValue;
    QElapsedTimer elapsedTimer;
    QVector<Sample> samples;
    QVariantList visiblePointValues;
    double visibleMinimumValue = 0.0;
    double visibleMaximumValue = 0.0;
    double visibleStartTimeValue = 0.0;
    double visibleEndTimeValue = 0.0;
};
