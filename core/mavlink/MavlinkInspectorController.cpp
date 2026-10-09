#include "MavlinkInspectorController.h"

#include "mavlink/Mavlink.h"

#include "MavlinkMessageMetadata.h"

#include <algorithm>
#include <QClipboard>
#include <QGuiApplication>
#include <QMetaType>

namespace {
constexpr qsizetype maximumStreams = 4096;
constexpr qsizetype maximumRawPackets = 500;
}

MavlinkInspectorController::MavlinkInspectorController(QObject *parent)
    : QObject(parent)
    , inspectorModel(new MavlinkInspectorModel(this))
    , filterModel(new MavlinkInspectorFilterModel(this))
    , inspectorGraphModel(new MavlinkInspectorGraphModel(this))
{
    filterModel->setSourceModel(inspectorModel);
    connect(filterModel, &MavlinkInspectorFilterModel::filtersChanged, this, &MavlinkInspectorController::filterStateChanged);
    connect(filterModel, &MavlinkInspectorFilterModel::filterOptionsChanged, this, &MavlinkInspectorController::filterStateChanged);
    connect(filterModel, &QAbstractItemModel::modelReset, this, &MavlinkInspectorController::filterStateChanged);
    connect(filterModel, &QAbstractItemModel::rowsInserted, this, &MavlinkInspectorController::filterStateChanged);
    connect(filterModel, &QAbstractItemModel::rowsRemoved, this, &MavlinkInspectorController::filterStateChanged);
    // Mavlink, its ILink implementations, and this child controller are all
    // created on the application's Qt event thread. The direct observation
    // connection therefore needs no mutex; a future moved-link implementation
    // will receive a queued Qt signal before touching this state.
    refreshTimer.setInterval(100);
    refreshTimer.setSingleShot(false);
    connect(&refreshTimer, &QTimer::timeout, this, &MavlinkInspectorController::refreshModel);
    refreshTimer.start();
}

MavlinkInspectorController::MavlinkInspectorController(Mavlink *mavlink, QObject *parent)
    : MavlinkInspectorController(parent)
{
    attach(mavlink);
}

QAbstractItemModel *MavlinkInspectorController::model() const
{
    return inspectorModel;
}

QAbstractItemModel *MavlinkInspectorController::filteredModel() const { return filterModel; }
int MavlinkInspectorController::filteredStreamCount() const { return filterModel->rowCount(); }
QVariantList MavlinkInspectorController::vehicleFilterOptions() const { return filterModel->vehicleOptions(); }
QVariantList MavlinkInspectorController::componentFilterOptions() const { return filterModel->componentOptions(); }
QVariantList MavlinkInspectorController::messageFilterOptions() const { return filterModel->messageOptions(); }
QVariantList MavlinkInspectorController::linkFilterOptions() const { return filterModel->linkOptions(); }

int MavlinkInspectorController::streamCount() const
{
    return streamCountValue;
}

double MavlinkInspectorController::totalRxRate() const
{
    return totalRxRateValue;
}

double MavlinkInspectorController::totalRxBytesPerSecond() const
{
    return totalRxBytesPerSecondValue;
}

QVariantMap MavlinkInspectorController::selectedDetails() const
{
    if (!selectedKey || !streams.contains(*selectedKey)) {
        return {};
    }

    const StreamState &state = streams.value(*selectedKey);
    const MavlinkInspectorEntry &entry = state.entry;
    return {
        { QStringLiteral("timestamp"), entry.timestamp },
        { QStringLiteral("link"), entry.link },
        { QStringLiteral("sysid"), entry.systemId },
        { QStringLiteral("compid"), entry.componentId },
        { QStringLiteral("messageName"), entry.messageName },
        { QStringLiteral("messageId"), entry.messageId },
        { QStringLiteral("packetSize"), entry.packetSize },
        { QStringLiteral("rateHz"), entry.rateHz },
        { QStringLiteral("count"), entry.count },
        { QStringLiteral("bytesPerSecond"), entry.bytesPerSecond },
        { QStringLiteral("sequence"), entry.sequence },
        { QStringLiteral("mavlinkVersion"), entry.mavlinkVersion },
        { QStringLiteral("fields"), state.latestFields },
    };
}

bool MavlinkInspectorController::hasRawPacket() const
{
    return selectedKey && latestRawPacketFor(*selectedKey);
}

QVariantMap MavlinkInspectorController::selectedRawPacket() const
{
    if (!selectedKey) {
        return {};
    }
    const RawPacketRecord *packet = latestRawPacketFor(*selectedKey);
    if (!packet) {
        return {};
    }
    return {
        { QStringLiteral("timestamp"), packet->timestamp },
        { QStringLiteral("link"), packet->key.link },
        { QStringLiteral("sysid"), packet->key.systemId },
        { QStringLiteral("compid"), packet->key.componentId },
        { QStringLiteral("messageName"), packet->messageName },
        { QStringLiteral("messageId"), packet->key.messageId },
        { QStringLiteral("packetLength"), packet->packetLength },
        { QStringLiteral("mavlinkVersion"), packet->mavlinkVersion },
        { QStringLiteral("sequence"), packet->sequence },
        { QStringLiteral("hex"), hexBytes(packet->rawPacket) },
        { QStringLiteral("ascii"), asciiBytes(packet->rawPacket) },
        { QStringLiteral("formatted"), formattedPacket(packet->rawPacket) },
    };
}

QVariantList MavlinkInspectorController::availableGraphFields() const
{
    if (!selectedKey || !streams.contains(*selectedKey)) {
        return {};
    }
    return graphableFields(streams.value(*selectedKey).latestFields);
}

QString MavlinkInspectorController::selectedGraphField() const
{
    return graphKey && selectedKey && *graphKey == *selectedKey ? graphFieldName : QString();
}

void MavlinkInspectorController::observeFrame(const MavlinkFrame &frame)
{
    if (pausedValue) {
        return;
    }

    const StreamKey key { frame.linkIdentity(), frame.systemId(), frame.componentId(), frame.messageId() };
    if (!streams.contains(key) && streams.size() >= maximumStreams) {
        // Inspection is observational. Bound memory under malformed or highly
        // dynamic traffic instead of perturbing the MAVLink receive path.
        return;
    }
    StreamState &state = streams[key];
    if (state.entry.count == 0) {
        state.entry.link = key.link;
        state.entry.systemId = key.systemId;
        state.entry.componentId = key.componentId;
        state.entry.messageId = key.messageId;
        state.entry.messageName = messageNameFor(key.messageId);
        state.firstTimestamp = frame.timestamp();
    }

    state.entry.timestamp = frame.timestamp();
    state.entry.packetSize = frame.frameLength();
    state.entry.sequence = frame.sequence();
    state.entry.mavlinkVersion = frame.mavlinkVersion();
    // Generic field decoding is comparatively expensive and only feeds the
    // selected details/graph. Keep the latest parsed message snapshot and
    // decode it on the existing 10 Hz UI cadence instead of once per packet.
    state.latestMessage = frame.message();
    state.fieldsDirty = true;
    ++state.entry.count;
    state.totalBytes += static_cast<quint64>(qMax(0, frame.frameLength()));

    const qint64 elapsedMs = state.firstTimestamp.msecsTo(state.entry.timestamp);
    if (elapsedMs > 0) {
        const double elapsedSeconds = static_cast<double>(elapsedMs) / 1000.0;
        state.entry.rateHz = static_cast<double>(state.entry.count - 1) / elapsedSeconds;
        state.entry.bytesPerSecond = static_cast<double>(state.totalBytes) / elapsedSeconds;
    }
    if (rawPackets.size() >= maximumRawPackets) {
        rawPackets.removeFirst();
    }
    rawPackets.append({ frame.timestamp(), key, state.entry.messageName, frame.frameLength(),
                        frame.mavlinkVersion(), frame.sequence(), frame.rawPacket() });
    dirty = true;
}

void MavlinkInspectorController::setPaused(bool paused)
{
    if (pausedValue == paused) {
        return;
    }
    pausedValue = paused;
    emit pausedChanged();
}

void MavlinkInspectorController::pause()
{
    setPaused(true);
}

void MavlinkInspectorController::resume()
{
    setPaused(false);
}

bool MavlinkInspectorController::selectStream(const QString &link, int systemId, int componentId, quint32 messageId)
{
    const StreamKey key { link, static_cast<quint8>(systemId), static_cast<quint8>(componentId), messageId };
    if (!streams.contains(key)) {
        return false;
    }
    if (!selectedKey || !(*selectedKey == key)) {
        selectedKey = key;
        refreshFields(streams[*selectedKey]);
        emit selectionChanged();
        emit graphStateChanged();
    }
    return true;
}

bool MavlinkInspectorController::selectRow(int row)
{
    const QModelIndex index = filterModel->index(row, 0);
    if (!index.isValid()) {
        return false;
    }
    return selectStream(index.data(MavlinkInspectorModel::LinkRole).toString(),
                        index.data(MavlinkInspectorModel::SystemIdRole).toInt(),
                        index.data(MavlinkInspectorModel::ComponentIdRole).toInt(),
                        index.data(MavlinkInspectorModel::MessageIdRole).toUInt());
}

void MavlinkInspectorController::setVehicleFilter(int systemId) { filterModel->setSystemIdFilter(systemId); }
void MavlinkInspectorController::setComponentFilter(int componentId) { filterModel->setComponentIdFilter(componentId); }
void MavlinkInspectorController::setMessageFilter(qint64 messageId) { filterModel->setMessageIdFilter(messageId); }
void MavlinkInspectorController::setLinkFilter(const QString &link) { filterModel->setLinkFilter(link); }
void MavlinkInspectorController::setSearchText(const QString &searchText) { filterModel->setSearchText(searchText); }
void MavlinkInspectorController::clearFilters() { filterModel->clearFilters(); }

bool MavlinkInspectorController::copySelectedRawHex() const
{
    const QVariantMap packet = selectedRawPacket();
    const QString hex = packet.value(QStringLiteral("hex")).toString();
    if (hex.isEmpty()) {
        return false;
    }
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(hex);
        return true;
    }
    return false;
}

void MavlinkInspectorController::clearSelection()
{
    if (selectedKey) {
        selectedKey.reset();
        emit selectionChanged();
        emit graphStateChanged();
    }
}

void MavlinkInspectorController::clear()
{
    streams.clear();
    rawPackets.clear();
    graphKey.reset();
    graphFieldName.clear();
    graphLastObservedCount = 0;
    inspectorGraphModel->reset();
    dirty = false;
    inspectorModel->clear();
    const bool hadSelection = selectedKey.has_value();
    selectedKey.reset();
    const bool statisticsWerePresent = streamCountValue != 0 || totalRxRateValue != 0.0 || totalRxBytesPerSecondValue != 0.0;
    streamCountValue = 0;
    totalRxRateValue = 0.0;
    totalRxBytesPerSecondValue = 0.0;
    if (statisticsWerePresent) {
        emit statisticsChanged();
    }
    if (hadSelection) {
        emit selectionChanged();
    }
    emit graphStateChanged();
}

void MavlinkInspectorController::refreshModel()
{
    if (!dirty) {
        return;
    }
    QVector<MavlinkInspectorEntry> entries;
    entries.reserve(streams.size());
    for (const StreamState &state : streams) {
        MavlinkInspectorEntry entry = state.entry;
        QStringList searchTerms { entry.messageName, QString::number(entry.messageId), QString::number(entry.systemId),
                                  QString::number(entry.componentId), entry.link,
                                  QStringLiteral("SYSID %1").arg(entry.systemId),
                                  QStringLiteral("COMPID %1").arg(entry.componentId) };
        // Field names come from static dialect metadata, so make them
        // searchable without eagerly decoding each live packet just for a
        // presentation filter. Values are included below once a stream has
        // been decoded for selection or graphing.
        const MavlinkMessageMetadata metadata = MavlinkMetadata::lookup(entry.messageId);
        for (const MavlinkFieldMetadata &field : metadata.fields) {
            searchTerms.append(field.name);
        }
        for (const QVariant &field : state.latestFields) {
            const QVariantMap values = field.toMap();
            searchTerms.append(values.value(QStringLiteral("name")).toString());
            searchTerms.append(values.value(QStringLiteral("value")).toString());
        }
        entry.searchText = searchTerms.join(QLatin1Char(' '));
        entries.append(std::move(entry));
    }
    std::sort(entries.begin(), entries.end(), [](const MavlinkInspectorEntry &left, const MavlinkInspectorEntry &right) {
        if (left.link != right.link) return left.link < right.link;
        if (left.systemId != right.systemId) return left.systemId < right.systemId;
        if (left.componentId != right.componentId) return left.componentId < right.componentId;
        return left.messageId < right.messageId;
    });
    inspectorModel->synchronize(entries);
    dirty = false;
    if (selectedKey && streams.contains(*selectedKey)) {
        refreshFields(streams[*selectedKey]);
    }
    if (graphKey && streams.contains(*graphKey)) {
        refreshFields(streams[*graphKey]);
    }
    appendGraphSample();

    const int newStreamCount = entries.size();
    double newTotalRxRate = 0.0;
    double newTotalRxBytesPerSecond = 0.0;
    for (const MavlinkInspectorEntry &entry : entries) {
        newTotalRxRate += entry.rateHz;
        newTotalRxBytesPerSecond += entry.bytesPerSecond;
    }
    if (streamCountValue != newStreamCount || totalRxRateValue != newTotalRxRate
            || totalRxBytesPerSecondValue != newTotalRxBytesPerSecond) {
        streamCountValue = newStreamCount;
        totalRxRateValue = newTotalRxRate;
        totalRxBytesPerSecondValue = newTotalRxBytesPerSecond;
        emit statisticsChanged();
    }
    if (selectedKey) {
        if (streams.contains(*selectedKey)) {
            emit selectionChanged();
        } else {
            selectedKey.reset();
            emit selectionChanged();
            emit graphStateChanged();
        }
    }
}

bool MavlinkInspectorController::selectGraphField(const QString &fieldName)
{
    if (!selectedKey || !streams.contains(*selectedKey)) {
        return false;
    }
    const QVariantList fields = graphableFields(streams.value(*selectedKey).latestFields);
    const auto match = std::find_if(fields.cbegin(), fields.cend(), [&fieldName](const QVariant &field) {
        return field.toMap().value(QStringLiteral("value")).toString() == fieldName;
    });
    if (match == fields.cend()) {
        return false;
    }
    graphKey = selectedKey;
    graphFieldName = fieldName;
    const StreamState &state = streams.value(*graphKey);
    inspectorGraphModel->selectSeries(graphKey->link, graphKey->systemId, graphKey->componentId,
                                      graphKey->messageId, state.entry.messageName, graphFieldName);
    graphLastObservedCount = state.entry.count;
    StreamState &mutableState = streams[*graphKey];
    refreshFields(mutableState);
    const std::optional<double> value = graphValue(mutableState.latestFields, graphFieldName);
    if (value) inspectorGraphModel->appendSample(*value);
    emit graphStateChanged();
    return true;
}

void MavlinkInspectorController::clearGraph()
{
    inspectorGraphModel->clear();
}

QVariantList MavlinkInspectorController::graphableFields(const QVariantList &fields)
{
    QVariantList result;
    const auto isNumeric = [](const QVariant &value) {
        const int type = value.metaType().id();
        return type == QMetaType::Char || type == QMetaType::SChar || type == QMetaType::UChar
                || type == QMetaType::Short || type == QMetaType::UShort || type == QMetaType::Int
                || type == QMetaType::UInt || type == QMetaType::LongLong || type == QMetaType::ULongLong
                || type == QMetaType::Float || type == QMetaType::Double;
    };
    for (const QVariant &fieldValue : fields) {
        const QVariantMap field = fieldValue.toMap();
        if (!field.value(QStringLiteral("available")).toBool()) continue;
        const QString name = field.value(QStringLiteral("name")).toString();
        const QVariant value = field.value(QStringLiteral("value"));
        if (value.metaType().id() == QMetaType::QVariantList) {
            const QVariantList array = value.toList();
            for (qsizetype index = 0; index < array.size(); ++index) {
                if (isNumeric(array.at(index))) {
                    const QString elementName = QStringLiteral("%1[%2]").arg(name).arg(index);
                    result.append(QVariantMap {{ QStringLiteral("label"), elementName }, { QStringLiteral("value"), elementName }});
                }
            }
        } else if (isNumeric(value)) {
            result.append(QVariantMap {{ QStringLiteral("label"), name }, { QStringLiteral("value"), name }});
        }
    }
    return result;
}

std::optional<double> MavlinkInspectorController::graphValue(const QVariantList &fields, const QString &fieldName)
{
    for (const QVariant &fieldValue : fields) {
        const QVariantMap field = fieldValue.toMap();
        if (!field.value(QStringLiteral("available")).toBool()) continue;
        const QString name = field.value(QStringLiteral("name")).toString();
        const QVariant value = field.value(QStringLiteral("value"));
        if (name == fieldName && value.metaType().id() != QMetaType::QString) {
            bool ok = false;
            const double numeric = value.toDouble(&ok);
            if (ok) return numeric;
        }
        if (value.metaType().id() != QMetaType::QVariantList) continue;
        const QVariantList array = value.toList();
        for (qsizetype index = 0; index < array.size(); ++index) {
            if (fieldName != QStringLiteral("%1[%2]").arg(name).arg(index)) continue;
            bool ok = false;
            const double numeric = array.at(index).toDouble(&ok);
            if (ok) return numeric;
        }
    }
    return std::nullopt;
}

void MavlinkInspectorController::appendGraphSample()
{
    if (!graphKey || !streams.contains(*graphKey)) return;
    const StreamState &state = streams.value(*graphKey);
    if (state.entry.count <= graphLastObservedCount) return;
    graphLastObservedCount = state.entry.count;
    const std::optional<double> value = graphValue(state.latestFields, graphFieldName);
    if (value) inspectorGraphModel->appendSample(*value);
}

void MavlinkInspectorController::refreshFields(StreamState &state)
{
    if (!state.fieldsDirty) {
        return;
    }
    state.latestFields = MavlinkMetadata::decodeFields(state.latestMessage);
    state.fieldsDirty = false;
}

QString MavlinkInspectorController::messageNameFor(quint32 messageId)
{
    return MavlinkMetadata::messageName(messageId);
}

QString MavlinkInspectorController::hexBytes(const QByteArray &packet)
{
    QStringList values;
    values.reserve(packet.size());
    for (const char byte : packet) {
        values.append(QStringLiteral("%1").arg(static_cast<quint8>(byte), 2, 16, QLatin1Char('0')).toUpper());
    }
    return values.join(QLatin1Char(' '));
}

QString MavlinkInspectorController::asciiBytes(const QByteArray &packet)
{
    QString result;
    result.reserve(packet.size());
    for (const char byte : packet) {
        const quint8 value = static_cast<quint8>(byte);
        result.append(value >= 0x20 && value <= 0x7e ? QChar(value) : QLatin1Char('.'));
    }
    return result;
}

QString MavlinkInspectorController::formattedPacket(const QByteArray &packet)
{
    constexpr int bytesPerLine = 16;
    QStringList lines;
    for (int offset = 0; offset < packet.size(); offset += bytesPerLine) {
        const QByteArray line = packet.mid(offset, bytesPerLine);
        QStringList hex;
        hex.reserve(line.size());
        QString ascii;
        ascii.reserve(line.size());
        for (const char byte : line) {
            const quint8 value = static_cast<quint8>(byte);
            hex.append(QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper());
            ascii.append(value >= 0x20 && value <= 0x7e ? QChar(value) : QLatin1Char('.'));
        }
        lines.append(QStringLiteral("%1  %2  %3")
                         .arg(offset, 4, 16, QLatin1Char('0')).toUpper()
                         .arg(hex.join(QLatin1Char(' ')), -47)
                         .arg(ascii));
    }
    return lines.join(QLatin1Char('\n'));
}

const MavlinkInspectorController::RawPacketRecord *MavlinkInspectorController::latestRawPacketFor(const StreamKey &key) const
{
    for (auto it = rawPackets.crbegin(); it != rawPackets.crend(); ++it) {
        if (it->key == key) {
            return &*it;
        }
    }
    return nullptr;
}

void MavlinkInspectorController::attach(Mavlink *mavlink)
{
    if (mavlink) {
        connect(mavlink, &Mavlink::frameObserved, this, &MavlinkInspectorController::observeFrame);
    }
}
