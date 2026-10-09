#include "MavlinkMessageMetadata.h"

extern "C" {
#include "common/mavlink.h"
}

#include <algorithm>
#include <cstring>
#include <iterator>
#include <QtEndian>

namespace {

const mavlink_message_info_t *messageInfoFor(quint32 messageId)
{
    static const mavlink_message_info_t messageInfo[] = MAVLINK_MESSAGE_INFO;
    const auto begin = std::begin(messageInfo);
    const auto end = std::end(messageInfo);
    const auto found = std::lower_bound(begin, end, messageId,
        [](const mavlink_message_info_t &info, quint32 id) { return info.msgid < id; });
    return found != end && found->msgid == messageId ? found : nullptr;
}

QString typeName(mavlink_message_type_t type)
{
    switch (type) {
    case MAVLINK_TYPE_CHAR: return QStringLiteral("char");
    case MAVLINK_TYPE_UINT8_T: return QStringLiteral("uint8_t");
    case MAVLINK_TYPE_INT8_T: return QStringLiteral("int8_t");
    case MAVLINK_TYPE_UINT16_T: return QStringLiteral("uint16_t");
    case MAVLINK_TYPE_INT16_T: return QStringLiteral("int16_t");
    case MAVLINK_TYPE_UINT32_T: return QStringLiteral("uint32_t");
    case MAVLINK_TYPE_INT32_T: return QStringLiteral("int32_t");
    case MAVLINK_TYPE_UINT64_T: return QStringLiteral("uint64_t");
    case MAVLINK_TYPE_INT64_T: return QStringLiteral("int64_t");
    case MAVLINK_TYPE_FLOAT: return QStringLiteral("float");
    case MAVLINK_TYPE_DOUBLE: return QStringLiteral("double");
    }
    return QStringLiteral("unknown");
}

int typeSize(mavlink_message_type_t type)
{
    switch (type) {
    case MAVLINK_TYPE_CHAR:
    case MAVLINK_TYPE_UINT8_T:
    case MAVLINK_TYPE_INT8_T: return 1;
    case MAVLINK_TYPE_UINT16_T:
    case MAVLINK_TYPE_INT16_T: return 2;
    case MAVLINK_TYPE_UINT32_T:
    case MAVLINK_TYPE_INT32_T:
    case MAVLINK_TYPE_FLOAT: return 4;
    case MAVLINK_TYPE_UINT64_T:
    case MAVLINK_TYPE_INT64_T:
    case MAVLINK_TYPE_DOUBLE: return 8;
    }
    return 0;
}

QVariant scalarValue(const char *data, mavlink_message_type_t type)
{
    switch (type) {
    case MAVLINK_TYPE_CHAR: return QString(QChar::fromLatin1(*data));
    case MAVLINK_TYPE_UINT8_T: return static_cast<uint>(*reinterpret_cast<const quint8 *>(data));
    case MAVLINK_TYPE_INT8_T: return static_cast<int>(*reinterpret_cast<const qint8 *>(data));
    case MAVLINK_TYPE_UINT16_T: return static_cast<uint>(qFromLittleEndian<quint16>(data));
    case MAVLINK_TYPE_INT16_T: return static_cast<int>(qFromLittleEndian<qint16>(data));
    case MAVLINK_TYPE_UINT32_T: return qFromLittleEndian<quint32>(data);
    case MAVLINK_TYPE_INT32_T: return qFromLittleEndian<qint32>(data);
    case MAVLINK_TYPE_UINT64_T: return QVariant::fromValue(qFromLittleEndian<quint64>(data));
    case MAVLINK_TYPE_INT64_T: return QVariant::fromValue(qFromLittleEndian<qint64>(data));
    case MAVLINK_TYPE_FLOAT: {
        const quint32 bits = qFromLittleEndian<quint32>(data);
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    case MAVLINK_TYPE_DOUBLE: {
        const quint64 bits = qFromLittleEndian<quint64>(data);
        double value = 0.0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    }
    return {};
}

} // namespace

MavlinkMessageMetadata MavlinkMetadata::lookup(quint32 messageId)
{
    MavlinkMessageMetadata metadata;
    metadata.messageId = messageId;
    const mavlink_message_info_t *info = messageInfoFor(messageId);
    if (!info) {
        metadata.name = QStringLiteral("UNKNOWN");
        return metadata;
    }

    metadata.name = QString::fromLatin1(info->name);
    metadata.fields.reserve(static_cast<qsizetype>(info->num_fields));
    for (unsigned index = 0; index < info->num_fields; ++index) {
        const mavlink_field_info_t &field = info->fields[index];
        metadata.fields.append({
            QString::fromLatin1(field.name),
            {}, // The bundled C metadata does not carry units.
            QString::fromLatin1(field.print_format ? field.print_format : ""),
            static_cast<int>(field.type),
            field.array_length,
            field.wire_offset,
        });
    }
    return metadata;
}

QString MavlinkMetadata::messageName(quint32 messageId)
{
    return lookup(messageId).name;
}

QVariantList MavlinkMetadata::decodeFields(const mavlink_message_t &message)
{
    QVariantList result;
    const mavlink_message_info_t *info = messageInfoFor(message.msgid);
    if (!info) {
        return result;
    }

    const char *payload = _MAV_PAYLOAD(&message);
    for (unsigned index = 0; index < info->num_fields; ++index) {
        const mavlink_field_info_t &field = info->fields[index];
        const int size = typeSize(field.type);
        const unsigned count = field.array_length == 0 ? 1U : field.array_length;
        QVariantMap decoded {
            { QStringLiteral("name"), QString::fromLatin1(field.name) },
            { QStringLiteral("type"), typeName(field.type) },
            { QStringLiteral("arrayLength"), field.array_length },
        };
        const quint64 endOffset = static_cast<quint64>(field.wire_offset)
                + static_cast<quint64>(size) * count;
        if (size <= 0 || endOffset > message.len) {
            decoded.insert(QStringLiteral("available"), false);
            result.append(decoded);
            continue;
        }

        decoded.insert(QStringLiteral("available"), true);
        const char *fieldData = payload + field.wire_offset;
        if (field.array_length == 0) {
            decoded.insert(QStringLiteral("value"), scalarValue(fieldData, field.type));
        } else if (field.type == MAVLINK_TYPE_CHAR) {
            const QByteArray bytes(fieldData, static_cast<int>(field.array_length));
            const int terminator = bytes.indexOf('\0');
            decoded.insert(QStringLiteral("value"), QString::fromLatin1(bytes.constData(), terminator < 0 ? bytes.size() : terminator));
        } else {
            QVariantList values;
            values.reserve(static_cast<qsizetype>(field.array_length));
            for (unsigned element = 0; element < field.array_length; ++element) {
                values.append(scalarValue(fieldData + element * size, field.type));
            }
            decoded.insert(QStringLiteral("value"), values);
        }
        result.append(decoded);
    }
    return result;
}
