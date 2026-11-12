#include "MavlinkConsoleController.h"
#include "mavlink/Mavlink.h"
#include "parameters/ParameterManager.h"
#include <QDateTime>
#include <cstring>
#include <QRegularExpression>
#include <cmath>
#include <limits>

MavlinkConsoleController::MavlinkConsoleController(Mavlink *mavlink)
    : QObject(mavlink), transport(mavlink)
{
    connect(transport, &Mavlink::parameterWriteFinished, this, [this](const QVariantMap &result) {
        if (!pendingWriteId || pendingWriteId != result.value("id").toULongLong()) return;
        const bool confirmed = result.value("outcome").toInt() == ParameterManager::Confirmed;
        append(confirmed
               ? QStringLiteral("PARAM_VALUE: %1 = %2; write confirmed by vehicle").arg(result.value("parameter").toString()).arg(result.value("confirmedValue").toDouble(), 0, 'g', 9)
               : QStringLiteral("Parameter write %1: %2").arg(result.value("outcomeName").toString(), result.value("reason").toString()),
               confirmed ? QStringLiteral("response") : QStringLiteral("warning"));
        clearPending();
    });
    poll.setInterval(500);
    startup.setInterval(4000);
    startup.setSingleShot(true);
    responseTimer.setSingleShot(true);
    responseTimer.setInterval(4000);
    connect(&responseTimer, &QTimer::timeout, this, [this] {
        append(QStringLiteral("MAVLink response timeout; command outcome is unknown. No automatic retry."), QStringLiteral("warning"));
        clearPending();
    });
    connect(&poll, &QTimer::timeout, this, [this] {
        if (available() && session) write({});
        else reset();
    });
    connect(&startup, &QTimer::timeout, this, [this] {
        stop();
        timedOut = true;
        append(QStringLiteral("No shell response. Shell unavailable or disabled on this firmware/link."), QStringLiteral("warning"));
        emit changed();
    });
    connect(transport, &Mavlink::selectedVehicleChanged, this, &MavlinkConsoleController::reset);
    connect(transport, &Mavlink::connectionStatusChanged, this, [this](bool connected) {
        if (!connected) reset();
        emit changed();
    });
    connect(transport, &Mavlink::messageReceived, this, [this](const mavlink_message_t &msg) {
        if (!transport->connected() || msg.sysid != transport->selectedSystemId()
            || msg.compid != transport->selectedComponentId()) return;
        if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
            heartbeatAge.start();
            mavlink_heartbeat_t heartbeat{};
            mavlink_msg_heartbeat_decode(&msg, &heartbeat);
            if (firmware != heartbeat.autopilot) {
                stop();

                firmware = heartbeat.autopilot;
                emit changed();
            }
            vehicleType = heartbeat.type;
        }
        if (commandMode()) {
            handleCommandResponse(msg);
            return;
        }
        if (!session || msg.msgid != MAVLINK_MSG_ID_SERIAL_CONTROL) return;
        mavlink_serial_control_t packet{};
        mavlink_msg_serial_control_decode(&msg, &packet);
        if (packet.device != SERIAL_CONTROL_DEV_SHELL || !(packet.flags & SERIAL_CONTROL_FLAG_REPLY)
            || packet.count > sizeof(packet.data)
            || (packet.target_system && packet.target_system != 255)
            || (packet.target_component && packet.target_component != MAV_COMP_ID_MISSIONPLANNER)) return;
        if (!confirmed) {
            confirmed = true;
            startup.stop();
            emit changed();
        }
        // Stateful UTF-8 decoding preserves characters split across MAVLink frames.
        const QString decoded = decoder.decode(QByteArrayView(reinterpret_cast<const char *>(packet.data), packet.count));
        QString text;
        // NSH emits ANSI color/cursor sequences. Consume CSI across frame boundaries;
        // this is a line-oriented console, not a full-screen terminal emulator.
        for (const QChar ch : decoded) {
            if (escapeState == 1) { escapeState = ch == QLatin1Char('[') ? 2 : 0; continue; }
            if (escapeState == 2) { if (ch.unicode() >= 0x40 && ch.unicode() <= 0x7e) escapeState = 0; continue; }
            if (ch.unicode() == 0x1b) { escapeState = 1; continue; }
            if (ch == QLatin1Char('\r') || ch == QChar::Null) continue;
            text += ch;
        }
        if (!text.isEmpty()) append(text, QStringLiteral("output"));
    });
}

bool MavlinkConsoleController::available() const
{
    return transport->connected() && transport->selectedSystemId() != 0
        && (firmware == MAV_AUTOPILOT_PX4 || commandMode())
        && heartbeatAge.isValid() && heartbeatAge.elapsed() < 5000;
}

QString MavlinkConsoleController::target() const
{
    return QStringLiteral("System %1 / Component %2").arg(transport->selectedSystemId()).arg(transport->selectedComponentId());
}

QString MavlinkConsoleController::status() const
{
    if (!transport->connected()) return QStringLiteral("Disconnected");
    if (commandMode()) return available() ? QStringLiteral("Ready: supported MAVLink commands (not an unrestricted shell)")
                                         : QStringLiteral("Unavailable: vehicle heartbeat is stale");
    if (!available()) return QStringLiteral("Unavailable: firmware shell support unknown");
    if (timedOut) return QStringLiteral("Shell unavailable: no response (retry Start Shell)");
    if (session) return confirmed ? QStringLiteral("Shell connected") : QStringLiteral("Waiting for shell response…");
    return QStringLiteral("PX4 shell ready to start — operator action required");
}

bool MavlinkConsoleController::start()
{
    if (!available() || session || commandMode()) return false;
    session = true;
    timedOut = confirmed = false;
    decoder.resetState();
    escapeState = 0;
    // Only this explicit operator action sends the newline that starts PX4 NSH.
    write(QByteArrayLiteral("\n"));
    poll.start();
    startup.start();
    emit changed();
    return true;
}

void MavlinkConsoleController::stop()
{
    if (session && available() && !commandMode()) write({}, true);
    clearPending();
    session = confirmed = false;
    poll.stop();
    startup.stop();
    decoder.resetState();
    escapeState = 0;
    emit changed();
}

void MavlinkConsoleController::reset()
{
    // Never release through a changed target/link.
    session = confirmed = timedOut = false;
    firmware = -1;
    vehicleType = -1;

    clearPending();
    heartbeatAge.invalidate();
    poll.stop();
    startup.stop();
    decoder.resetState();
    escapeState = 0;
    clear();
    emit changed();
}

bool MavlinkConsoleController::send(const QString &text)
{
    if (commandMode()) {
        if (!available()) return false;
        return executeCommand(text);
    }
    if (!available() || !session || !confirmed || text.toUtf8().size() > 4096
        || text.contains(QChar::Null)) return false;
    append(text + QLatin1Char('\n'), QStringLiteral("input"));
    write(text.toUtf8() + '\n');
    return true;
}

bool MavlinkConsoleController::interrupt()
{
    if (!available() || commandMode() || !session || !confirmed) return false;
    write(QByteArray(1, '\x03'));
    append(QStringLiteral("^C\n"), QStringLiteral("input"));
    return true;
}

void MavlinkConsoleController::write(const QByteArray &bytes, bool release)
{
    if (!available() || commandMode()) return;
    qsizetype offset = 0;
    do {
        uint8_t data[70]{};
        const auto count = qMin(qsizetype(70), bytes.size() - offset);
        if (count) std::memcpy(data, bytes.constData() + offset, size_t(count));
        mavlink_message_t msg{};
        mavlink_msg_serial_control_pack(255, MAV_COMP_ID_MISSIONPLANNER, &msg,
            SERIAL_CONTROL_DEV_SHELL,
            release ? 0 : SERIAL_CONTROL_FLAG_EXCLUSIVE | SERIAL_CONTROL_FLAG_RESPOND,
            0, 0, uint8_t(count), data,
            uint8_t(transport->selectedSystemId()), uint8_t(transport->selectedComponentId()));
        transport->sendMavlinkMessage(msg);
        offset += count;
    } while (offset < bytes.size());
}

void MavlinkConsoleController::append(const QString &text, const QString &kind)
{
    // Merge adjacent output chunks without inserting artificial line breaks.
    if (kind == QLatin1String("output") && !entries.isEmpty()
        && entries.last().toMap().value("kind") == kind) {
        auto entry = entries.takeLast().toMap();
        entry["text"] = (entry.value("text").toString() + text).right(16384);
        entries.append(entry);
    } else {
        entries.append(QVariantMap{{"text", text.right(16384)}, {"kind", kind},
            {"time", QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))}});
    }
    while (entries.size() > 200) entries.removeFirst();
    emit historyChanged();
}

void MavlinkConsoleController::clear()
{
    entries.clear();
    emit historyChanged();
}

bool MavlinkConsoleController::commandMode() const
{
    return firmware == MAV_AUTOPILOT_ARDUPILOTMEGA;
}

QString MavlinkConsoleController::modeLabel() const
{
    if (commandMode()) return QStringLiteral("ArduPilot MAVLink Console");
    if (firmware == MAV_AUTOPILOT_PX4) return QStringLiteral("PX4 NSH Shell");
    return QStringLiteral("MAVLink Console");
}

void MavlinkConsoleController::clearPending()
{
    responseTimer.stop();
    pendingMessage = pendingCommand = pendingMode = -1;
    pendingParameter.clear();
    pendingWriteId = 0;
}

bool MavlinkConsoleController::executeCommand(const QString &text)
{
    auto error = [this](const QString &message) { append(message, QStringLiteral("error")); return true; };
    if (text.size() > 256 || text.contains(QChar::Null) || text.contains('\n') || text.contains('\r'))
        return error(QStringLiteral("Enter a single command, up to 256 characters."));
    append(text + '\n', QStringLiteral("input"));
    const auto args = text.simplified().split(' ', Qt::SkipEmptyParts);
    if (args.isEmpty()) return error(QStringLiteral("Command required."));
    if (responseTimer.isActive() || pendingWriteId) return error(QStringLiteral("A command is awaiting a MAVLink response; wait for its result or timeout."));
    const QString verb = args[0].toLower();
    if ((args.size() == 1 && verb == QLatin1String("status"))
        || (args.size() == 2 && verb == QLatin1String("gps") && args[1].compare("status", Qt::CaseInsensitive) == 0)) {
        pendingMessage = verb == QLatin1String("status") ? MAVLINK_MSG_ID_HEARTBEAT : MAVLINK_MSG_ID_GPS_RAW_INT;
        pendingCommand = MAV_CMD_REQUEST_MESSAGE;
        transport->sendCommandLong(MAV_CMD_REQUEST_MESSAGE, float(pendingMessage));
    } else if (args.size() == 1 && (verb == QLatin1String("arm") || verb == QLatin1String("disarm"))) {
        if (verb == QLatin1String("arm") && !armingAllowed())
            return error(QStringLiteral("Arming blocked: complete the GCS preflight checklist using the Arm controls first."));
        pendingCommand = MAV_CMD_COMPONENT_ARM_DISARM;
        // The normal API preserves autopilot pre-arm checks; never force-arm/disarm.
        transport->armVehicle(verb == QLatin1String("arm"));
    } else if (args.size() == 2 && verb == QLatin1String("mode")) {
        const QString name = args[1].toUpper();
        if (name == QLatin1String("RTL")) {
            pendingCommand = MAV_CMD_NAV_RETURN_TO_LAUNCH;
            transport->returnToLaunch();
        } else if (name == QLatin1String("GUIDED")) {
            switch (vehicleType) {
            case MAV_TYPE_QUADROTOR: case MAV_TYPE_COAXIAL: case MAV_TYPE_HELICOPTER:
            case MAV_TYPE_HEXAROTOR: case MAV_TYPE_OCTOROTOR: case MAV_TYPE_TRICOPTER:
            case MAV_TYPE_DODECAROTOR: pendingMode = 4; break;
            case MAV_TYPE_FIXED_WING: case MAV_TYPE_VTOL_TAILSITTER_DUOROTOR: case MAV_TYPE_VTOL_TAILSITTER_QUADROTOR:
            case MAV_TYPE_VTOL_TILTROTOR: case MAV_TYPE_VTOL_FIXEDROTOR:
            case MAV_TYPE_VTOL_TAILSITTER: case MAV_TYPE_VTOL_TILTWING: pendingMode = 15; break;
            case MAV_TYPE_GROUND_ROVER: case MAV_TYPE_SURFACE_BOAT: pendingMode = 15; break;
            default: return error(QStringLiteral("GUIDED mode mapping is unavailable for this vehicle type."));
            }
            pendingMessage = MAVLINK_MSG_ID_HEARTBEAT;
            transport->setFlightMode(pendingMode);
        } else return error(QStringLiteral("Supported modes: GUIDED, RTL."));
    } else if (verb == QLatin1String("param") && args.size() >= 2) {
        const QString operation = args[1].toLower();
        if ((operation != QLatin1String("get") || args.size() != 3)
            && (operation != QLatin1String("set") || args.size() != 4))
            return error(QStringLiteral("Usage: param get NAME | param set NAME VALUE"));
        const QString name = args[2].toUpper();
        static const QRegularExpression parameterName(QStringLiteral("^[A-Z][A-Z0-9_]{0,15}$"));
        if (!parameterName.match(name).hasMatch()) return error(QStringLiteral("Invalid parameter name (1-16 ASCII letters/digits/underscores)."));
        if (operation == QLatin1String("get")) {
            if (!transport->requestParameterRead(name)) return error(QStringLiteral("Parameter read rejected by backend."));
        } else {
            bool ok = false;
            const double value = args[3].toDouble(&ok);
            if (!ok || !std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
                return error(QStringLiteral("Parameter value must be a finite MAVLink float32-compatible number."));
            const auto metadata = transport->parameterValue(name);
            if (metadata.isEmpty()) return error(QStringLiteral("Parameter is absent from the confirmed snapshot; finish synchronization first."));
            const auto accepted = transport->requestParameterWrite(name, value, metadata.value("type").toInt());
            if (!accepted.value("accepted").toBool()) return error(accepted.value("reason").toString());
            pendingWriteId = accepted.value("id").toULongLong();
        }
        pendingParameter = name;
    } else return error(QStringLiteral("Unsupported command. Supported: status; gps status; arm; disarm; mode GUIDED|RTL; param get NAME; param set NAME VALUE."));
    append(pendingWriteId
           ? QStringLiteral("Queued: %1 for %2; awaiting managed parameter-write confirmation.").arg(text.trimmed(), target())
           : QStringLiteral("TX: %1 sent to %2; awaiting MAVLink response (not shell output).").arg(text.trimmed(), target()), QStringLiteral("tx"));
    if (!pendingWriteId) responseTimer.start();
    return true;
}

void MavlinkConsoleController::handleCommandResponse(const mavlink_message_t &msg)
{
    if (msg.msgid == MAVLINK_MSG_ID_PARAM_VALUE) {
        mavlink_param_value_t value{};
        mavlink_msg_param_value_decode(&msg, &value);
        const QString name = QString::fromLatin1(value.param_id, int(strnlen(value.param_id, 16)));
        if (pendingWriteId || name != pendingParameter || name.isEmpty()) return;
        append(QStringLiteral("PARAM_VALUE: %1 = %2 (type %3)").arg(name)
               .arg(double(value.param_value), 0, 'g', 9).arg(value.param_type), QStringLiteral("response"));
        clearPending();
    } else if (msg.msgid == MAVLINK_MSG_ID_COMMAND_ACK && pendingCommand >= 0) {
        mavlink_command_ack_t ack{};
        mavlink_msg_command_ack_decode(&msg, &ack);
        if (ack.command != pendingCommand || (ack.target_system && ack.target_system != 255)
            || (ack.target_component && ack.target_component != MAV_COMP_ID_MISSIONPLANNER)) return;
        static const QStringList results{"ACCEPTED", "TEMPORARILY_REJECTED", "DENIED", "UNSUPPORTED", "FAILED", "IN_PROGRESS", "CANCELLED"};
        const QString result = ack.result < results.size() ? results[ack.result] : QString::number(ack.result);
        append(QStringLiteral("COMMAND_ACK: command %1 %2 (progress %3). This is a MAVLink acknowledgement.")
            .arg(ack.command).arg(result).arg(ack.progress), ack.result == MAV_RESULT_ACCEPTED || ack.result == MAV_RESULT_IN_PROGRESS
                ? QStringLiteral("response") : QStringLiteral("error"));
        if (ack.result == MAV_RESULT_IN_PROGRESS) responseTimer.start();
        else if (ack.result != MAV_RESULT_ACCEPTED || pendingMessage < 0) clearPending();
    } else if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT && pendingMessage == MAVLINK_MSG_ID_HEARTBEAT) {
        mavlink_heartbeat_t hb{};
        mavlink_msg_heartbeat_decode(&msg, &hb);
        if (pendingMode >= 0 && int(hb.custom_mode) != pendingMode) return;
        append(QStringLiteral("HEARTBEAT: %1; custom mode %2; system state %3; vehicle type %4%5")
            .arg((hb.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) ? QStringLiteral("ARMED") : QStringLiteral("DISARMED"))
            .arg(hb.custom_mode).arg(hb.system_status).arg(hb.type)
            .arg(pendingMode >= 0 ? QStringLiteral("; requested mode observed") : QString()), QStringLiteral("response"));
        clearPending();
    } else if (msg.msgid == MAVLINK_MSG_ID_GPS_RAW_INT && pendingMessage == MAVLINK_MSG_ID_GPS_RAW_INT) {
        mavlink_gps_raw_int_t gps{};
        mavlink_msg_gps_raw_int_decode(&msg, &gps);
        append(QStringLiteral("GPS_RAW_INT: fix type %1; satellites %2; HDOP %3; lat %4; lon %5")
            .arg(gps.fix_type).arg(gps.satellites_visible == 255 ? QStringLiteral("unknown") : QString::number(gps.satellites_visible))
            .arg(gps.eph == UINT16_MAX ? QStringLiteral("unknown") : QString::number(gps.eph / 100.0, 'f', 2))
            .arg(gps.lat / 1e7, 0, 'f', 7).arg(gps.lon / 1e7, 0, 'f', 7), QStringLiteral("response"));
        clearPending();
    }
}
