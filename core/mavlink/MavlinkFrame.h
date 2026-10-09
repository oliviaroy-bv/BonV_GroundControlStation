#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

extern "C" {
#include "common/mavlink.h"
}

// A value snapshot produced at the receive/parser boundary. The parser's
// working storage is never exposed; both the MAVLink message and raw bytes are
// copied into this object before it is emitted to observers.
class MavlinkFrame
{
public:
    MavlinkFrame() = default;
    MavlinkFrame(const QDateTime &timestamp, QString linkIdentity,
                 const mavlink_message_t &message, QByteArray rawPacket);

    const QDateTime &timestamp() const { return timestampValue; }
    const QString &linkIdentity() const { return linkIdentityValue; }
    quint8 systemId() const { return messageValue.sysid; }
    quint8 componentId() const { return messageValue.compid; }
    quint32 messageId() const { return messageValue.msgid; }
    quint8 sequence() const { return messageValue.seq; }
    quint8 payloadLength() const { return messageValue.len; }
    int frameLength() const { return rawPacketValue.size(); }
    int mavlinkVersion() const;
    const QByteArray &rawPacket() const { return rawPacketValue; }
    const mavlink_message_t &message() const { return messageValue; }

private:
    QDateTime timestampValue;
    QString linkIdentityValue;
    mavlink_message_t messageValue = {};
    QByteArray rawPacketValue;
};

Q_DECLARE_METATYPE(MavlinkFrame)
