#pragma once

#include <QVector>
#include <QString>
#include <QVariantList>
#include <QtGlobal>

extern "C" {
#include "common/mavlink.h"
}

struct MavlinkFieldMetadata {
    QString name;
    QString units;
    QString printFormat;
    int type = 0;
    unsigned arrayLength = 0;
    unsigned wireOffset = 0;
};

struct MavlinkMessageMetadata {
    quint32 messageId = 0;
    QString name;
    QVector<MavlinkFieldMetadata> fields;
};

// Reads the generated metadata compiled for the active MAVLink dialect. No
// external schema and no message-ID switch table is maintained by BonV GCS.
namespace MavlinkMetadata {
MavlinkMessageMetadata lookup(quint32 messageId);
QString messageName(quint32 messageId);
// Decodes the payload using the generated dialect field descriptions. This is
// deliberately metadata-driven: Vehicle telemetry state is never involved.
QVariantList decodeFields(const mavlink_message_t &message);
}
