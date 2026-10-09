#include "MavlinkInspectorGraphModel.h"

#include <QtMath>

MavlinkInspectorGraphModel::MavlinkInspectorGraphModel(QObject *parent)
    : QObject(parent)
{
}

void MavlinkInspectorGraphModel::selectSeries(const QString &link, int systemId, int componentId,
                                              quint32 messageId, const QString &messageName,
                                              const QString &fieldName)
{
    activeValue = true;
    linkValue = link;
    systemIdValue = systemId;
    componentIdValue = componentId;
    messageIdValue = messageId;
    messageNameValue = messageName;
    fieldNameValue = fieldName;
    samples.clear();
    elapsedTimer.start();
    refreshVisibleCache();
    emit changed();
}

void MavlinkInspectorGraphModel::appendSample(double value)
{
    if (!activeValue || !qIsFinite(value)) {
        return;
    }
    if (!elapsedTimer.isValid()) {
        elapsedTimer.start();
    }
    samples.append({ static_cast<double>(elapsedTimer.nsecsElapsed()) / 1000000000.0, value });
    if (samples.size() > MaximumSamples) {
        samples.remove(0, samples.size() - MaximumSamples);
    }
    refreshVisibleCache();
    emit changed();
}

void MavlinkInspectorGraphModel::clear()
{
    if (samples.isEmpty()) {
        return;
    }
    samples.clear();
    elapsedTimer.restart();
    refreshVisibleCache();
    emit changed();
}

void MavlinkInspectorGraphModel::reset()
{
    activeValue = false;
    linkValue.clear();
    systemIdValue = 0;
    componentIdValue = 0;
    messageIdValue = 0;
    messageNameValue.clear();
    fieldNameValue.clear();
    samples.clear();
    elapsedTimer.invalidate();
    refreshVisibleCache();
    emit changed();
}

double MavlinkInspectorGraphModel::latestValue() const
{
    return samples.isEmpty() ? 0.0 : samples.constLast().value;
}

void MavlinkInspectorGraphModel::refreshVisibleCache()
{
    visiblePointValues.clear();
    visibleMinimumValue = 0.0;
    visibleMaximumValue = 0.0;
    visibleStartTimeValue = 0.0;
    visibleEndTimeValue = 0.0;
    if (samples.isEmpty()) {
        return;
    }
    const double cutoff = qMax(0.0, samples.constLast().timeSeconds - VisibleWindowSeconds);
    qsizetype first = 0;
    while (first < samples.size() && samples.at(first).timeSeconds < cutoff) {
        ++first;
    }
    if (first >= samples.size()) {
        return;
    }

    const qsizetype count = samples.size() - first;
    visiblePointValues.reserve(count);
    visibleStartTimeValue = samples.at(first).timeSeconds;
    visibleEndTimeValue = samples.constLast().timeSeconds;
    visibleMinimumValue = samples.at(first).value;
    visibleMaximumValue = visibleMinimumValue;
    for (qsizetype index = first; index < samples.size(); ++index) {
        const Sample &sample = samples.at(index);
        // Keep each sample as one QVariantList value. Appending a bare
        // QVariantList flattens it into the outer list, whereas QML expects a
        // point pair at points[index][0] and points[index][1].
        visiblePointValues.append(QVariant::fromValue(QVariantList { sample.timeSeconds, sample.value }));
        visibleMinimumValue = qMin(visibleMinimumValue, sample.value);
        visibleMaximumValue = qMax(visibleMaximumValue, sample.value);
    }
}

double MavlinkInspectorGraphModel::minimum() const
{
    if (visiblePointValues.isEmpty()) {
        return 0.0;
    }
    if (qFuzzyCompare(visibleMinimumValue + 1.0, visibleMaximumValue + 1.0)) {
        return visibleMinimumValue - qMax(0.5, qAbs(visibleMinimumValue) * 0.05);
    }
    return visibleMinimumValue;
}

double MavlinkInspectorGraphModel::maximum() const
{
    if (visiblePointValues.isEmpty()) {
        return 0.0;
    }
    if (qFuzzyCompare(visibleMaximumValue + 1.0, visibleMinimumValue + 1.0)) {
        return visibleMaximumValue + qMax(0.5, qAbs(visibleMaximumValue) * 0.05);
    }
    return visibleMaximumValue;
}

double MavlinkInspectorGraphModel::visibleStartTime() const
{
    return visibleStartTimeValue;
}

double MavlinkInspectorGraphModel::visibleEndTime() const
{
    return visibleEndTimeValue;
}

QVariantList MavlinkInspectorGraphModel::points() const
{
    return visiblePointValues;
}
