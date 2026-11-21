#include "ParameterConfigurationController.h"
#include "ParameterManager.h"
#include "mavlink/Mavlink.h"
#include "mavlink/MavlinkConsoleController.h"
#include <QtConcurrent>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStringDecoder>
#include <cmath>
#include <limits>
#include <algorithm>

namespace {
constexpr qint64 maxBytes = 16 * 1024 * 1024;
bool validName(const QString &name) { return QRegularExpression(QStringLiteral("^[!-~]{1,16}$")).match(name).hasMatch(); }
QString number(double value) { return QString::number(value, 'g', 17); }
bool decimal(const QString &text, double &value) {
    if (!QRegularExpression(QStringLiteral("^[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?$")).match(text).hasMatch()) return false;
    bool ok = false;
    value = text.toDouble(&ok);
    return ok && std::isfinite(value);
}
}

QString ParameterConfigurationController::typeLabel(int type) {
    switch (type) {
    case 1: return "UINT8"; case 2: return "INT8"; case 3: return "UINT16"; case 4: return "INT16";
    case 5: return "UINT32"; case 6: return "INT32"; case 9: return "REAL32";
    default: return QStringLiteral("Unsupported type %1").arg(type);
    }
}

QString ParameterConfigurationController::valueError(double value, int type) {
    if (!std::isfinite(value)) return "Non-finite value.";
    if (type == 9) {
        const float encoded = float(value);
        return !std::isfinite(encoded) || (value != 0 && encoded == 0) ? "Value cannot be represented as REAL32." : QString();
    }
    double low = 0, high = 0;
    switch (type) {
    case 1: high = 255; break; case 2: low = -128; high = 127; break;
    case 3: high = 65535; break; case 4: low = -32768; high = 32767; break;
    case 5: high = 4294967295.; break; case 6: low = -2147483648.; high = 2147483647.; break;
    default: return "Unsupported classic-protocol parameter type.";
    }
    return std::trunc(value) != value || value < low || value > high ? "Fractional or out-of-range integer." : QString();
}

ParameterConfigurationController::Document ParameterConfigurationController::parse(const QByteArray &bytes, const QString &extension) {
    Document result;
    if (extension.toLower() != "param" && extension.toLower() != "params") { result.error = "Supported extensions are .param and .params."; return result; }
    if (bytes.size() > maxBytes || bytes.contains('\0')) { result.error = "File is binary or exceeds 16 MiB."; return result; }
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString text = decoder.decode(bytes);
    if (decoder.hasError()) { result.error = "File is not valid UTF-8 text."; return result; }
    if (text.startsWith(QChar(0xfeff))) text.remove(0, 1);
    QHash<QString, QList<int>> occurrences;
    QSet<int> components;
    QSet<QString> declaredFamilies;
    const auto lines = text.split('\n');
    for (int lineNumber = 0; lineNumber < lines.size(); ++lineNumber) {
        QString line = lines[lineNumber].trimmed();
        if (line.startsWith('#')) {
            const auto header = QRegularExpression(QStringLiteral("^#\\s*(?:BonV firmware|Stack):\\s*(\\S+)"), QRegularExpression::CaseInsensitiveOption).match(line);
            if (header.hasMatch()) {
                const auto family = header.captured(1).toLower();
                if (family == "ardupilot" || family == "px4" || family == "unknown") declaredFamilies.insert(family);
                else result.error = "Unrecognized firmware declaration.";
            }
            continue;
        }
        line = line.section('#', 0, 0).trimmed();
        if (line.isEmpty()) continue;
        if (result.entries.size() >= 100000) { result.error = "More than 100,000 entries."; break; }
        QStringList fields;
        if (line.contains(',')) { fields = line.split(','); for (auto &field : fields) field = field.trimmed(); }
        else fields = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        QVariantMap row{{"line", lineNumber + 1}, {"name", fields.value(fields.size() == 5 ? 2 : 0)},
                        {"text", fields.value(fields.size() == 5 ? 3 : 1)}, {"type", -1}, {"issue", ""}};
        QString issue, kind = "Invalid";
        const QString format = fields.size() == 5 ? "QGC/PX4 typed" : fields.size() == 2 ? "ArduPilot pairs" : "";
        if (format.isEmpty() || fields.contains(QString())) issue = "Expected two name/value fields or five typed fields.";
        else if (!result.format.isEmpty() && result.format != format) issue = "Mixed parameter layouts.";
        else result.format = format;
        if (issue.isEmpty() && !validName(row.value("name").toString())) issue = "Identifier must contain 1–16 printable ASCII bytes.";
        double value = 0;
        if (issue.isEmpty() && !decimal(row.value("text").toString(), value)) issue = "Invalid or non-finite decimal value.";
        row["value"] = value;
        if (fields.size() == 5) {
            bool systemOk = false, componentOk = false, typeOk = false;
            const int system = fields[0].toInt(&systemOk), component = fields[1].toInt(&componentOk), type = fields[4].toInt(&typeOk);
            row["type"] = type;
            if (issue.isEmpty() && (!systemOk || !componentOk || system < 1 || system > 255 || component < 1 || component > 255 || !typeOk)) issue = "Invalid source IDs or type.";
            if (issue.isEmpty()) {
                components.insert((system << 8) | component);
                issue = valueError(value, type);
                if (type != 9 && (type < 1 || type > 6)) kind = "Unsupported";
            }
        }
        row["issue"] = issue;
        row["kind"] = issue.isEmpty() ? "" : kind;
        occurrences[row.value("name").toString()].append(result.entries.size());
        result.entries.append(row);
    }
    for (const auto &positions : occurrences) if (positions.size() > 1) {
        result.duplicates += positions.size() - 1;
        for (int position : positions) {
            auto row = result.entries[position].toMap(); row["issue"] = "Duplicate identifier; every occurrence is excluded."; row["kind"] = "Invalid"; result.entries[position] = row;
        }
    }
    if (components.size() > 1) for (auto &item : result.entries) {
        auto row = item.toMap();
        if (row.value("issue").toString().isEmpty()) { row["issue"] = "Multiple sources/components cannot be safely mapped to one vehicle table."; row["kind"] = "Unsupported"; item = row; }
    }
    if (declaredFamilies.size() > 1) result.error = "Conflicting firmware declarations.";
    else if (!declaredFamilies.isEmpty()) result.family = *declaredFamilies.cbegin();
    if (result.entries.isEmpty() && result.error.isEmpty()) result.error = "No parameter entries found.";
    return result;
}

QVariantList ParameterConfigurationController::compareEntries(const Document &document, const QVariantMap &snapshot, int encoding, const QString &family) {
    QVariantList rows;
    QSet<QString> present;
    for (const auto &entry : document.entries) {
        auto row = entry.toMap();
        const auto name = row.value("name").toString(); present.insert(name);
        const auto current = snapshot.value(name).toMap();
        const int type = current.isEmpty() ? row.value("type").toInt() : current.value("type").toInt();
        QString status = row.value("kind").toString(), detail = row.value("issue").toString();
        if (!document.error.isEmpty()) { status = "Invalid"; detail = document.error; }
        if (status.isEmpty()) {
            if (document.family != "unknown" && family != "unknown" && document.family != family) {
                status = "Unsupported"; detail = "File and vehicle firmware families are incompatible.";
            } else if (current.isEmpty()) status = "Missing from vehicle";
            else if (row.value("type").toInt() != -1 && row.value("type").toInt() != type) status = "Type mismatch";
            else {
                detail = valueError(row.value("value").toDouble(), type);
                if (!detail.isEmpty()) status = type != 9 && (type < 1 || type > 6) ? "Unsupported" : "Invalid";
                else if (encoding == int(ParameterManager::NumericCast) && type != 9 && double(float(row.value("value").toDouble())) != row.value("value").toDouble()) {
                    status = "Unsupported"; detail = "Integer cannot be represented exactly by this vehicle's numeric encoding.";
                }
                else {
                    const double encoded = type == 9 ? double(float(row.value("value").toDouble())) : row.value("value").toDouble();
                    status = encoded == current.value("value").toDouble() ? "Identical" : "Different";
                }
            }
        }
        row["current"] = current.value("value"); row["file"] = row.value("text");
        row["actualType"] = type; row["typeLabel"] = type == -1 ? "Unspecified" : typeLabel(type);
        row["status"] = status; row["detail"] = detail; row["eligible"] = status == "Different";
        row["selected"] = false; row["outcome"] = ""; row["attempts"] = 0;
        rows.append(row);
    }
    for (auto it = snapshot.cbegin(); it != snapshot.cend(); ++it) if (!present.contains(it.key())) {
        const auto record = it.value().toMap();
        rows.append(QVariantMap{{"name", it.key()}, {"current", record.value("value")}, {"actualType", record.value("type")},
            {"typeLabel", typeLabel(record.value("type").toInt())}, {"status", "Missing from file"}, {"selected", false}, {"eligible", false}});
    }
    std::stable_sort(rows.begin(), rows.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value("name").toString() < b.toMap().value("name").toString(); });
    return rows;
}

QByteArray ParameterConfigurationController::serialize(const QVariantMap &snapshot, const QVariantMap &provenance, const QString &family, bool stale) {
    QByteArray bytes = "# BonV firmware: " + family.toUtf8() + "\n# Snapshot: " + (stale ? QByteArray("retained/stale") : QByteArray("current")) + "\n";
    bytes += family == "ardupilot" ? "# Name Value\n" : "# Vehicle-Id Component-Id Name Value Type\n";
    for (auto it = snapshot.cbegin(); it != snapshot.cend(); ++it) {
        const auto record = it.value().toMap();
        if (!validName(it.key()) || !valueError(record.value("value").toDouble(), record.value("type").toInt()).isEmpty()) return {};
        if (family != "ardupilot") bytes += QByteArray::number(provenance.value("systemId").toInt()) + '\t' + QByteArray::number(provenance.value("componentId").toInt()) + '\t';
        bytes += it.key().toLatin1() + '\t' + number(record.value("value").toDouble()).toLatin1();
        if (family != "ardupilot") bytes += '\t' + QByteArray::number(record.value("type").toInt());
        bytes += '\n';
    }
    return bytes;
}

ParameterConfigurationController::ParameterConfigurationController(Mavlink *link, ParameterManager *parameters)
    : QAbstractListModel(link), transport(link), manager(parameters) {
    proxy.setSourceModel(this); proxy.setFilterRole(StatusRole);
    submitTimer.setSingleShot(true); submitTimer.setInterval(50);
    connect(&submitTimer, &QTimer::timeout, this, &ParameterConfigurationController::submitNext);
    connect(manager, &ParameterManager::snapshotPublished, this, [this] { snapshotFamily = liveFamily(); invalidate(); emit changed(); });
    connect(manager, &ParameterManager::snapshotChanged, this, [this] { invalidate(); });
    connect(manager, &ParameterManager::statusChanged, this, [this] {
        if (operation && !currentSource()) { stopping = true; markRemaining("Not submitted: vehicle disconnected or synchronization generation changed."); if (!pendingId) finishOperation(); }
        if (!manager || !manager->parametersReady()) compared = false;
        emit changed();
    });
    connect(transport, &Mavlink::parameterWriteFinished, this, &ParameterConfigurationController::finishedWrite);
    connect(&importer, &QFutureWatcher<Document>::finished, this, [this] {
        document = importer.result(); working = false; compared = false; applicationRows.clear(); submitted = 0; replaceRows({});
        statusMessage = document.error.isEmpty() ? "Imported locally. Compare before selecting writes; no vehicle values or drafts were changed." : document.error;
        emit changed();
    });
    connect(&comparer, &QFutureWatcher<QVariantList>::finished, this, [this] {
        working = false;
        if (!manager || comparisonRevision != manager->snapshotRevision() || !currentSource()) { compared = false; statusMessage = "Snapshot changed during comparison. Compare again."; emit changed(); return; }
        auto rows = comparer.result();
        replaceRows(rows); compared = true; comparedFamily = liveFamily();
        statusMessage = "Comparison ready. Select only reviewed valid differences."; emit changed();
    });
    connect(&exporter, &QFutureWatcher<QString>::finished, this, [this] {
        working = false; const auto error = exporter.result();
        statusMessage = error.isEmpty() ? "Configuration file written successfully." : error; emit changed();
    });
}

int ParameterConfigurationController::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : comparisonRows.size(); }
QHash<int, QByteArray> ParameterConfigurationController::roleNames() const {
    return {{NameRole,"parameterName"},{CurrentRole,"currentValue"},{FileRole,"fileValue"},{TypeRole,"parameterType"},{StatusRole,"comparisonStatus"},
        {DetailRole,"detail"},{EligibleRole,"eligible"},{SelectedRole,"chosen"},{OutcomeRole,"outcome"},{AttemptsRole,"attempts"},{IdRole,"transactionId"},{SourceRowRole,"sourceRow"}};
}
QVariant ParameterConfigurationController::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= comparisonRows.size()) return {};
    if (role == SourceRowRole) return index.row();
    static const QHash<int, QString> keys{{NameRole,"name"},{CurrentRole,"current"},{FileRole,"file"},{TypeRole,"typeLabel"},{StatusRole,"status"},
        {DetailRole,"detail"},{EligibleRole,"eligible"},{SelectedRole,"selected"},{OutcomeRole,"outcome"},{AttemptsRole,"attempts"},{IdRole,"id"}};
    return comparisonRows[index.row()].toMap().value(keys.value(role));
}
void ParameterConfigurationController::replaceRows(QVariantList rows) { beginResetModel(); comparisonRows = std::move(rows); selectedTotal = 0; outcomeCounts.clear(); endResetModel(); }
void ParameterConfigurationController::setRow(int position, const QVariantMap &row) {
    const auto before = comparisonRows[position].toMap();
    selectedTotal += int(row.value("selected").toBool()) - int(before.value("selected").toBool());
    const auto oldOutcome = before.value("outcome").toString(), newOutcome = row.value("outcome").toString();
    if (oldOutcome != newOutcome) { if (!oldOutcome.isEmpty()) --outcomeCounts[oldOutcome]; if (!newOutcome.isEmpty()) ++outcomeCounts[newOutcome]; }
    comparisonRows[position] = row; emit dataChanged(index(position), index(position));
}
bool ParameterConfigurationController::canCompare() const { return !working && !operation && manager && manager->hasCompleteSnapshot() && !document.entries.isEmpty(); }
void ParameterConfigurationController::setFilter(const QString &value) {
    rowFilter = value;
    proxy.setFilterRegularExpression(value == "All" ? "" : value == "Missing" ? "^Missing" : value == "Unsupported" ? "^(Unsupported|Type mismatch)$" : "^" + QRegularExpression::escape(value) + "$");
    emit changed();
}
int ParameterConfigurationController::selectedCount() const { return selectedTotal; }
QString ParameterConfigurationController::liveFamily() const {
    auto *console = qobject_cast<MavlinkConsoleController *>(transport->console());
    return console && console->commandMode() ? "ardupilot" : console && console->modeLabel() == "PX4 NSH Shell" ? "px4" : "unknown";
}
QString ParameterConfigurationController::compatibility() const {
    const auto family = manager && manager->parametersReady() ? liveFamily() : snapshotFamily;
    if (document.family != "unknown" && family != "unknown" && document.family != family) return "Incompatible firmware families — application blocked.";
    return document.family == "unknown" || family == "unknown" ? "Unverified firmware identity — explicit acknowledgement required." : "Matching firmware family: " + family;
}
QVariantMap ParameterConfigurationController::importSummary() const {
    int valid = 0, invalid = 0, unsupported = 0;
    for (const auto &item : document.entries) { const auto row = item.toMap(); if (row.value("kind") == "Unsupported") ++unsupported; else if (!row.value("issue").toString().isEmpty()) ++invalid; else ++valid; }
    if (!document.error.isEmpty()) { valid = 0; unsupported = 0; invalid = document.entries.size(); }
    return {{"total",document.entries.size()},{"valid",valid},{"invalid",invalid},{"unsupported",unsupported},{"duplicates",document.duplicates},{"format",document.format},{"firmware",document.family},{"error",document.error}};
}
QVariantMap ParameterConfigurationController::progress() const {
    QVariantMap result{{"total",applicationRows.size()},{"submitted",submitted},{"pending",pendingId ? 1 : 0}};
    for (const auto &key : {"Confirmed","Rejected","TimedOut","Cancelled","TransportError","InvalidRequest","Not submitted"}) result[key] = outcomeCounts.value(key);
    return result;
}
void ParameterConfigurationController::importFile(const QUrl &url) {
    if (working || operation) return;
    if (!url.isLocalFile()) { statusMessage = "Select a local file."; emit changed(); return; }
    working = true; emit changed(); const auto path = url.toLocalFile();
    importer.setFuture(QtConcurrent::run([path] {
        QFile file(path); Document result;
        if (!file.open(QIODevice::ReadOnly)) { result.error = file.errorString(); return result; }
        const auto bytes = file.read(maxBytes + 1);
        if (file.error() != QFileDevice::NoError) { result.error = file.errorString(); return result; }
        return parse(bytes, QFileInfo(path).suffix());
    }));
}
void ParameterConfigurationController::exportFile(const QUrl &url, bool acknowledgeStale, bool acknowledgeUnknown) {
    if (working || operation) return;
    if (!manager) return;
    const bool stale = !manager->parametersReady() || !transport->connected();
    const auto family = manager->parametersReady() ? liveFamily() : snapshotFamily;
    const auto expected = family == "ardupilot" ? "param" : "params";
    if (!manager->hasCompleteSnapshot() || !url.isLocalFile() || QFileInfo(url.toLocalFile()).suffix().toLower() != expected
        || (stale && !acknowledgeStale) || (family == "unknown" && !acknowledgeUnknown)) {
        statusMessage = QStringLiteral("Export needs a complete snapshot, a local .%1 file, and acknowledgement for stale/unknown sources.").arg(expected); emit changed(); return;
    }
    const auto snapshot = manager->parameters(); const auto provenance = manager->snapshotProvenance(); const auto path = url.toLocalFile();
    working = true; emit changed();
    exporter.setFuture(QtConcurrent::run([snapshot, provenance, family, stale, path]() -> QString {
        const auto bytes = serialize(snapshot, provenance, family, stale);
        if (snapshot.isEmpty() || bytes.isEmpty()) return "No valid complete snapshot to export.";
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return file.errorString();
        if (file.write(bytes) != bytes.size()) { file.cancelWriting(); return file.errorString(); }
        if (!file.commit()) return file.errorString();
        return {};
    }));
}
bool ParameterConfigurationController::currentSource() const {
    return manager && transport->connected() && manager->parametersReady() && manager->generation() == sourceGeneration
        && transport->connectionGeneration() == sourceConnection
        && manager->snapshotProvenance().value("sourceConnectionGeneration").toULongLong() == sourceConnection;
}
void ParameterConfigurationController::compare() {
    if (!canCompare()) return;
    sourceGeneration = manager->generation(); sourceConnection = transport->connectionGeneration(); comparisonRevision = manager->snapshotRevision();
    if (!currentSource()) { statusMessage = "Compare requires a connected vehicle with a current ready snapshot."; emit changed(); return; }
    working = true; compared = false; applicationRows.clear(); submitted = 0; emit changed();
    const auto staged = document; const auto snapshot = manager->parameters(); const auto encoding = manager->snapshotProvenance().value("encoding").toInt(); const auto family = liveFamily();
    comparer.setFuture(QtConcurrent::run([staged, snapshot, encoding, family] { return compareEntries(staged, snapshot, encoding, family); }));
}
void ParameterConfigurationController::invalidate() {
    if (!manager || (!operation && manager->snapshotRevision() != comparisonRevision)) compared = false;
    if (operation && !currentSource()) { stopping = true; markRemaining("Not submitted: source changed. Compare again before continuing."); if (!pendingId) finishOperation(); }
    emit changed();
}
void ParameterConfigurationController::selectRow(int position, bool selected) {
    if (!compared || working || operation || position < 0 || position >= comparisonRows.size()) return;
    auto row = comparisonRows[position].toMap(); row["selected"] = selected && row.value("eligible").toBool(); setRow(position,row); emit changed();
}
void ParameterConfigurationController::selectAll(bool selected) {
    if (working || operation || !compared) return;
    selectedTotal = 0;
    for (auto &item : comparisonRows) { auto row = item.toMap(); row["selected"] = selected && row.value("eligible").toBool(); selectedTotal += row.value("selected").toBool(); item = row; }
    if (!comparisonRows.isEmpty()) emit dataChanged(index(0),index(comparisonRows.size()-1),{SelectedRole});
    emit changed();
}
void ParameterConfigurationController::applySelected(const QStringList &draftNames, bool acknowledgeUnknown, bool replaceDraftConflicts) {
    if (working || operation || !compared || !currentSource() || manager->pendingWriteCount() || comparisonRevision != manager->snapshotRevision() || !selectedCount()) {
        statusMessage = "Application blocked: compare a current snapshot, select valid differences and finish other parameter writes first."; emit changed(); return;
    }
    const auto family = liveFamily();
    if (!document.error.isEmpty() || (document.family != "unknown" && family != "unknown" && document.family != family)
        || ((family == "unknown" || document.family == "unknown") && !acknowledgeUnknown) || family != comparedFamily) {
        statusMessage = "Firmware compatibility is unverified or incompatible. Review the file and compare again."; emit changed(); return;
    }
    for (const auto &item : comparisonRows) if (item.toMap().value("selected").toBool()
        && draftNames.contains(item.toMap().value("name").toString()) && !replaceDraftConflicts) {
        statusMessage = "Selected file values conflict with local drafts. Explicitly confirm applying file values over those proposals, or deselect them. Draft text will be preserved."; emit changed(); return;
    }
    applicationRows.clear(); cursor = 0; submitted = 0; stopping = false; outcomeCounts.clear();
    for (int i = 0; i < comparisonRows.size(); ++i) if (comparisonRows[i].toMap().value("selected").toBool()) applicationRows.append(i);
    operation = true; statusMessage = "Applying individual writes; this operation is not atomic."; emit changed(); submitTimer.start();
}
void ParameterConfigurationController::markRemaining(const QString &reason) {
    submitTimer.stop();
    for (int i = cursor; i < applicationRows.size(); ++i) { auto row = comparisonRows[applicationRows[i]].toMap(); row["outcome"] = "Not submitted"; row["detail"] = reason; setRow(applicationRows[i],row); }
    cursor = applicationRows.size();
}
void ParameterConfigurationController::stop() {
    if (!operation) return;
    stopping = true; markRemaining("Not submitted: user stopped further submissions. A pending backend write continues until its real result.");
    statusMessage = "Further submissions stopped. Waiting for any pending backend result."; if (!pendingId) finishOperation(); emit changed();
}
void ParameterConfigurationController::submitNext() {
    if (!operation || pendingId) return;
    if (stopping || cursor >= applicationRows.size()) { finishOperation(); return; }
    if (!currentSource() || liveFamily() != comparedFamily || manager->pendingWriteCount()) { stopping = true; markRemaining("Not submitted: source changed or another parameter write is active. Compare again."); finishOperation(); return; }
    const int position = applicationRows[cursor++]; auto row = comparisonRows[position].toMap();
    const auto name = row.value("name").toString(); const auto current = transport->parameterValue(name); float wire = 0;
    const double value = row.value("value").toDouble(); const int type = row.value("actualType").toInt();
    if (!row.value("eligible").toBool() || current.isEmpty() || current.value("type").toInt() != type
        || current.value("value") != row.value("current") || !manager->prepareWrite(name,value,quint8(type),wire)) {
        row["outcome"] = "InvalidRequest"; row["detail"] = "Parameter value/type changed or is no longer representable. Compare again."; setRow(position,row);
        stopping = true; markRemaining("Not submitted: revalidation failed. Compare again."); finishOperation(); return;
    }
    row["writeProvenance"] = manager->snapshotProvenance();
    const auto request = transport->requestParameterWrite(name,value,type);
    ++submitted; row["id"] = request.value("id");
    if (!request.value("accepted").toBool()) {
        row["outcome"] = "InvalidRequest"; row["detail"] = request.value("reason"); setRow(position,row);
        stopping = true; markRemaining("Not submitted: backend refused a request."); finishOperation(); return;
    }
    pendingId = request.value("id").toULongLong(); pendingRow = position; row["outcome"] = "Pending"; setRow(position,row); emit changed();
}
void ParameterConfigurationController::finishedWrite(const QVariantMap &result) {
    if (!operation || !pendingId || result.value("id").toULongLong() != pendingId || pendingRow < 0) return;
    auto row = comparisonRows[pendingRow].toMap();
    if (result.value("parameter") != row.value("name") || result.value("generation").toULongLong() != sourceGeneration
        || result.value("type").toInt() != row.value("actualType").toInt()) return;
    const auto provenance = row.value("writeProvenance").toMap();
    if (result.value("connectionGeneration") != provenance.value("connectionGeneration")
        || result.value("systemId").toInt() != provenance.value("systemId").toInt()
        || result.value("componentId").toInt() != provenance.value("componentId").toInt()) return;
    row["outcome"] = result.value("outcomeName"); row["detail"] = result.value("reason"); row["attempts"] = result.value("attempts"); row["confirmedValue"] = result.value("confirmedValue");
    if (currentSource()) {
        const auto current = transport->parameterValue(row.value("name").toString());
        if (current.value("type").toInt() == row.value("actualType").toInt()) row["current"] = current.value("value");
    }
    if (result.value("hasConfirmedValue").toBool()) row["detail"] = row.value("detail").toString() + " Vehicle reported " + number(result.value("confirmedValue").toDouble()) + ".";
    if (result.value("outcomeName") == "TimedOut") row["detail"] = "Confirmation was not received; the vehicle may have applied this value. " + row.value("detail").toString();
    setRow(pendingRow,row); pendingId = 0; pendingRow = -1;
    if (!currentSource()) { stopping = true; markRemaining("Not submitted: connection or generation changed."); }
    if (stopping || cursor >= applicationRows.size()) finishOperation(); else submitTimer.start(); emit changed();
}
void ParameterConfigurationController::finishOperation() {
    submitTimer.stop(); operation = false; compared = false;
    const auto counts = progress(); const int confirmed = counts.value("Confirmed").toInt(), total = applicationRows.size();
    statusMessage = confirmed == total && total > 0 ? "All selected writes confirmed. Compare again before another operation."
        : confirmed > 0 ? "Partial success. Review every result; compare again before retrying."
        : "No writes confirmed. Review failures and unsubmitted entries; confirmation timeouts are uncertain. Compare again before retrying.";
    emit changed();
}
