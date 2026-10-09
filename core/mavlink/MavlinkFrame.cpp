#include "MavlinkFrame.h"

#include <utility>

MavlinkFrame::MavlinkFrame(const QDateTime &timestamp, QString linkIdentity,
                           const mavlink_message_t &message, QByteArray rawPacket)
    : timestampValue(timestamp)
    , linkIdentityValue(std::move(linkIdentity))
    , messageValue(message)
    , rawPacketValue(std::move(rawPacket))
{
}

int MavlinkFrame::mavlinkVersion() const
{
    if (messageValue.magic == MAVLINK_STX_MAVLINK1) {
        return 1;
    }
    return messageValue.magic == MAVLINK_STX ? 2 : 0;
}
