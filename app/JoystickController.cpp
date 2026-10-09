#include "JoystickController.h"

#include "mavlink/Mavlink.h"

#include <QDebug>
#include <QHash>
#include <QSettings>
#include <QTimer>
#include <QtMath>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {
constexpr int pollIntervalMs = 40;
constexpr int sendIntervalTicks = 1;
constexpr double detectionThreshold = 0.18;

QString settingsPrefix()
{
    return QStringLiteral("Joystick/");
}

int boundedPercent(int value, int maximum)
{
    return qBound(0, value, maximum);
}

int defaultAxisMinimum()
{
    return 0;
}

int defaultAxisMaximum()
{
    return 65535;
}

int defaultAxisCenter()
{
    return (defaultAxisMinimum() + defaultAxisMaximum()) / 2;
}

QStringList rawAxisLabels()
{
    return {
        QStringLiteral("Axis X"),
        QStringLiteral("Axis Y"),
        QStringLiteral("Axis Z"),
        QStringLiteral("Axis R"),
        QStringLiteral("Axis U"),
        QStringLiteral("Axis V")
    };
}

QString controlKey(const QString &controlName)
{
    return controlName.trimmed().toLower();
}

QString actionKey(const QString &actionName)
{
    return actionName.simplified().toLower();
}

int flightModeCustomMode(const QString &modeName)
{
    QString key = modeName.simplified().toLower();
    key.remove(QLatin1Char(' '));
    key.remove(QLatin1Char('_'));
    key.remove(QLatin1Char('-'));

    if (key == QStringLiteral("stabilize"))
        return 0;
    if (key == QStringLiteral("althold"))
        return 2;
    if (key == QStringLiteral("auto"))
        return 3;
    if (key == QStringLiteral("guided"))
        return 4;
    if (key == QStringLiteral("loiter"))
        return 5;
    if (key == QStringLiteral("rtl"))
        return 6;
    if (key == QStringLiteral("land"))
        return 9;
    if (key == QStringLiteral("poshold"))
        return 16;
    if (key == QStringLiteral("brake"))
        return 17;

    return -1;
}
}

JoystickController::JoystickController(QObject *parent)
    : QObject(parent)
    , pollTimer(new QTimer(this))
{
    resetDefaultAssignments();

    for (const QString &label : rawAxisLabels()) {
        AxisState state;
        state.label = label;
        state.minimum = defaultAxisMinimum();
        state.center = defaultAxisCenter();
        state.maximum = defaultAxisMaximum();
        axisStates.insert(label, state);
    }

    loadSettings();

    pollTimer->setInterval(pollIntervalMs);
    connect(pollTimer, &QTimer::timeout, this, &JoystickController::poll);
    pollTimer->start();

    refreshDevices();
}

QStringList JoystickController::devices() const
{
    return deviceNames;
}

QString JoystickController::selectedDevice() const
{
    return selectedDeviceName;
}

bool JoystickController::available() const
{
    return selectedDeviceId >= 0 && deviceConnectedState;
}

bool JoystickController::enabled() const
{
    return enabledState;
}

bool JoystickController::calibrating() const
{
    return calibratingState;
}

bool JoystickController::calibrated() const
{
    return calibratedState;
}

QString JoystickController::message() const
{
    return statusText;
}

QVariantMap JoystickController::status() const
{
    QVariantMap map;
    map.insert(QStringLiteral("connected"), available());
    map.insert(QStringLiteral("enabled"), enabledState);
    map.insert(QStringLiteral("calibrating"), calibratingState);
    map.insert(QStringLiteral("calibrated"), calibratedState);
    map.insert(QStringLiteral("status"), available() ? QStringLiteral("Connected") : QStringLiteral("Disconnected"));
    map.insert(QStringLiteral("axes"), selectedAxes);
    map.insert(QStringLiteral("buttons"), selectedButtons);
    map.insert(QStringLiteral("povs"), selectedHasPov ? 1 : 0);
    map.insert(QStringLiteral("calibratedButtons"), calibratedButtonCount());
    map.insert(QStringLiteral("deadzone"), deadzone);
    map.insert(QStringLiteral("expo"), expo);
    map.insert(QStringLiteral("device"), selectedDeviceName);
    map.insert(QStringLiteral("message"), statusText);
    return map;
}

QVariantMap JoystickController::liveInput() const
{
    return currentLiveInput;
}

QVariantList JoystickController::axisRanges() const
{
    QVariantList rows;
    for (const ControlAssignment &assignment : controlAssignments) {
        rows.append(axisDisplayMap(assignment));
    }
    return rows;
}

QVariantList JoystickController::assignments() const
{
    QVariantList rows;
    for (const ControlAssignment &assignment : controlAssignments) {
        QVariantMap row;
        row.insert(QStringLiteral("axis"), assignment.control);
        row.insert(QStringLiteral("icon"), assignment.icon);
        row.insert(QStringLiteral("accent"), assignment.accent);
        row.insert(QStringLiteral("binding"), assignment.binding);
        row.insert(QStringLiteral("inverted"), assignment.inverted);
        row.insert(QStringLiteral("detecting"), controlKey(assignment.control) == controlKey(pendingDetectionControl));
        rows.append(row);
    }
    return rows;
}

QVariantList JoystickController::buttonAssignments() const
{
    QVariantList rows;
    for (int i = 0; i < buttonAssignmentsState.size(); ++i) {
        rows.append(buttonDisplayMap(buttonAssignmentsState.at(i), i));
    }
    return rows;
}

void JoystickController::refreshDevices()
{
    QVector<DeviceInfo> detected;
    QStringList names;

#ifdef Q_OS_WIN
    const UINT count = joyGetNumDevs();
    for (UINT id = 0; id < count; ++id) {
        JOYINFOEX info = {};
        info.dwSize = sizeof(JOYINFOEX);
        info.dwFlags = JOY_RETURNALL;
        if (joyGetPosEx(id, &info) != JOYERR_NOERROR)
            continue;

        JOYCAPSW caps = {};
        if (joyGetDevCapsW(id, &caps, sizeof(caps)) != JOYERR_NOERROR)
            continue;

        DeviceInfo device;
        device.id = static_cast<int>(id);
        device.name = QString::fromWCharArray(caps.szPname).trimmed();
        if (device.name.isEmpty())
            device.name = QStringLiteral("Joystick %1").arg(id + 1);
        device.axes = static_cast<int>(caps.wNumAxes);
        device.buttons = static_cast<int>(caps.wNumButtons);
        device.hasPov = (caps.wCaps & JOYCAPS_HASPOV) != 0;
        if (names.contains(device.name))
            device.name = QStringLiteral("%1 (%2)").arg(device.name).arg(device.id + 1);

        detected.append(device);
        names.append(device.name);
    }
#endif

    const bool namesChanged = names != deviceNames;
    deviceInfos = detected;
    deviceNames = names;

    if (selectedDeviceName.isEmpty() && !deviceNames.isEmpty()) {
        selectedDeviceName = deviceNames.first();
        emit selectedDeviceChanged();
    } else if (!selectedDeviceName.isEmpty() && !deviceNames.contains(selectedDeviceName)) {
        selectedDeviceName = deviceNames.isEmpty() ? QString() : deviceNames.first();
        selectedDeviceId = -1;
        emit selectedDeviceChanged();
    }

    const DeviceInfo selected = selectedDeviceInfo();
    updateDeviceStatus(selected);

    if (namesChanged)
        emit devicesChanged();

    emit statusChanged();
}

bool JoystickController::selectDevice(const QString &deviceName)
{
    if (deviceName == selectedDeviceName)
        return true;

    if (!deviceName.isEmpty() && !deviceNames.contains(deviceName)) {
        setMessage(QStringLiteral("Joystick device is not available"));
        return false;
    }

    selectedDeviceName = deviceName;
    pendingDetectionControl.clear();
    pendingButtonDetectionRow = -1;
    currentButtons = 0;
    previousButtons = 0;
    currentPov = -1;
    const DeviceInfo selected = selectedDeviceInfo();
    updateDeviceStatus(selected);
    saveSettings();

    emit selectedDeviceChanged();
    emit statusChanged();
    emit axisRangesChanged();
    emit assignmentsChanged();
    return true;
}

void JoystickController::setMavlink(QObject *mavlinkObject)
{
    Mavlink *newMavlink = qobject_cast<Mavlink *>(mavlinkObject);
    if (newMavlink == mavlink)
        return;

    mavlink = newMavlink;
    emit statusChanged();
}

bool JoystickController::beginCalibration()
{
    if (!readCurrentState()) {
        calibratedState = false;
        calibratingState = false;
        emit calibrationChanged();
        setMessage(QStringLiteral("Connect a joystick before calibration"));
        return false;
    }

    calibrationAxisBackup = axisStates;

    for (auto it = axisStates.begin(); it != axisStates.end(); ++it) {
        AxisState &state = it.value();
        if (!state.present)
            continue;
        state.minimum = state.current;
        state.maximum = state.current;
    }

    calibratingState = true;
    calibratedState = false;
    setMessage(QStringLiteral("Move every joystick axis through its full range"));
    emit calibrationChanged();
    emit axisRangesChanged();
    return true;
}

bool JoystickController::finishCalibration()
{
    if (!readCurrentState()) {
        calibratingState = false;
        calibratedState = false;
        emit calibrationChanged();
        setMessage(QStringLiteral("Joystick disconnected during calibration"));
        return false;
    }

    for (auto it = axisStates.begin(); it != axisStates.end(); ++it) {
        AxisState &state = it.value();
        if (!state.present)
            continue;

        const int defaultSpan = qMax(1, state.defaultMaximum - state.defaultMinimum);
        if (state.maximum - state.minimum < defaultSpan / 6) {
            state.minimum = state.defaultMinimum;
            state.maximum = state.defaultMaximum;
        }
        state.center = qBound(state.minimum, state.current, state.maximum);
    }

    QStringList missingControls;
    if (!requiredAssignmentsAvailable(&missingControls)) {
        axisStates = calibrationAxisBackup;
        calibrationAxisBackup.clear();
        calibratingState = false;
        calibratedState = false;
        setMessage(QStringLiteral("Assign these joystick controls: %1").arg(missingControls.join(QStringLiteral(", "))));
        emit calibrationChanged();
        emit axisRangesChanged();
        return false;
    }

    if (!minimumButtonsCalibrated()) {
        axisStates = calibrationAxisBackup;
        calibrationAxisBackup.clear();
        calibratingState = false;
        calibratedState = false;
        setMessage(QStringLiteral("Detect at least 2 joystick buttons before continuing"));
        emit calibrationChanged();
        emit axisRangesChanged();
        emit buttonAssignmentsChanged();
        return false;
    }

    calibratingState = false;
    calibratedState = true;
    calibrationAxisBackup.clear();
    setMessage(QStringLiteral("Joystick calibration saved"));
    emit calibrationChanged();
    emit axisRangesChanged();
    saveSettings();
    return true;
}

void JoystickController::cancelCalibration()
{
    if (!calibratingState)
        return;

    calibratingState = false;
    if (!calibrationAxisBackup.isEmpty()) {
        axisStates = calibrationAxisBackup;
        calibrationAxisBackup.clear();
        emit axisRangesChanged();
    }
    setMessage(QStringLiteral("Joystick calibration cancelled"));
    emit calibrationChanged();
}

void JoystickController::beginAxisDetection(const QString &controlName)
{
    if (!assignmentForControl(controlName)) {
        setMessage(QStringLiteral("Unknown joystick control"));
        return;
    }

    if (!readCurrentState(&detectionBaseline)) {
        setMessage(QStringLiteral("Connect a joystick before detecting an axis"));
        return;
    }

    pendingDetectionControl = controlName;
    setMessage(QStringLiteral("Move the %1 control").arg(controlName));
    emit assignmentsChanged();
}

void JoystickController::beginButtonDetection(int row)
{
    if (row < 0 || row >= buttonAssignmentsState.size()) {
        setMessage(QStringLiteral("Unknown joystick button slot"));
        return;
    }

    if (!readCurrentState()) {
        setMessage(QStringLiteral("Connect a joystick before detecting a button"));
        return;
    }

    pendingButtonDetectionRow = row;
    detectionButtonsBaseline = currentButtons;
    setMessage(QStringLiteral("Press a joystick button for Button %1").arg(row + 1));
    emit buttonAssignmentsChanged();
}

void JoystickController::setAxisInverted(const QString &controlName, bool inverted)
{
    ControlAssignment *assignment = assignmentForControl(controlName);
    if (!assignment || assignment->inverted == inverted)
        return;

    assignment->inverted = inverted;
    saveSettings();
    emit assignmentsChanged();
    emit axisRangesChanged();
}

void JoystickController::setButtonAction(int row, const QString &actionName)
{
    if (row < 0 || row >= buttonAssignmentsState.size())
        return;

    ButtonAssignment &assignment = buttonAssignmentsState[row];
    if (assignment.action == actionName)
        return;

    assignment.action = actionName;
    saveSettings();
    emit buttonAssignmentsChanged();
}

void JoystickController::setTuning(int deadzonePercent, int expoPercent)
{
    const int nextDeadzone = boundedPercent(deadzonePercent, 40);
    const int nextExpo = boundedPercent(expoPercent, 80);
    if (deadzone == nextDeadzone && expo == nextExpo)
        return;

    deadzone = nextDeadzone;
    expo = nextExpo;
    saveSettings();
    emit axisRangesChanged();
}

void JoystickController::setEnabled(bool enabled)
{
    if (enabledState == enabled)
        return;

    if (enabled && (!available() || !calibratedState || !requiredAssignmentsAvailable() || !minimumButtonsCalibrated())) {
        setMessage(QStringLiteral("Complete joystick calibration before enabling control"));
        return;
    }

    enabledState = enabled;
    saveSettings();
    emit enabledChanged();
    emit statusChanged();

    if (!enabledState && mavlink) {
        mavlink->sendManualControl(0, 0, 0, 0, 0);
        mavlink->sendRcChannelsOverride(0, 0, 0, 0);
    }

    setMessage(enabledState ? QStringLiteral("Joystick control enabled")
                            : QStringLiteral("Joystick control disabled"));
}

void JoystickController::poll()
{
    if (++deviceRefreshTick >= 50) {
        deviceRefreshTick = 0;
        refreshDevices();
    }

    const bool hadDevice = deviceConnectedState;
    readCurrentState();

    if (hadDevice != deviceConnectedState) {
        if (hadDevice && !deviceConnectedState && mavlink) {
            mavlink->sendManualControl(0, 0, 0, 0, 0);
            mavlink->sendRcChannelsOverride(0, 0, 0, 0);
        }
        setMessage(deviceConnectedState ? QStringLiteral("Joystick connected")
                                        : QStringLiteral("Joystick disconnected"));
        emit statusChanged();
        emit axisRangesChanged();
    }

    if (!pendingDetectionControl.isEmpty() && deviceConnectedState) {
        QString bestAxis;
        double bestMotion = 0.0;

        for (const QString &label : rawAxisLabels()) {
            const AxisState state = axisState(label);
            if (!state.present || !detectionBaseline.contains(label))
                continue;

            const double span = qMax(1, state.defaultMaximum - state.defaultMinimum);
            const double motion = qAbs(state.current - detectionBaseline.value(label).toInt()) / span;
            if (motion > bestMotion) {
                bestMotion = motion;
                bestAxis = label;
            }
        }

        if (!bestAxis.isEmpty() && bestMotion >= detectionThreshold) {
            if (ControlAssignment *assignment = assignmentForControl(pendingDetectionControl)) {
                for (ControlAssignment &otherAssignment : controlAssignments) {
                    if (controlKey(otherAssignment.control) != controlKey(assignment->control)
                            && otherAssignment.binding == bestAxis) {
                        otherAssignment.binding.clear();
                    }
                }
                assignment->binding = bestAxis;
                pendingDetectionControl.clear();
                setMessage(QStringLiteral("%1 assigned to %2").arg(assignment->control, bestAxis));
                saveSettings();
                emit assignmentsChanged();
                emit axisRangesChanged();
            }
        }
    }

    if (pendingButtonDetectionRow >= 0 && deviceConnectedState) {
        const quint32 pressedButtons = currentButtons & ~detectionButtonsBaseline;
        if (pressedButtons != 0) {
            for (int button = 0; button < 32; ++button) {
                if ((pressedButtons & (quint32(1) << button)) == 0)
                    continue;

                buttonAssignmentsState[pendingButtonDetectionRow].button = button;
                buttonAssignmentsState[pendingButtonDetectionRow].calibrated = true;
                setMessage(QStringLiteral("Button %1 assigned").arg(button + 1));
                pendingButtonDetectionRow = -1;
                saveSettings();
                emit buttonAssignmentsChanged();
                break;
            }
        }
    }

    if (calibratingState) {
        for (auto it = axisStates.begin(); it != axisStates.end(); ++it) {
            AxisState &state = it.value();
            if (!state.present)
                continue;
            state.minimum = qMin(state.minimum, state.current);
            state.maximum = qMax(state.maximum, state.current);
        }
        emit axisRangesChanged();
    }

    static int sendTick = 0;
    if (++sendTick >= sendIntervalTicks) {
        sendTick = 0;
        sendManualControl();
    }

    handleButtonActions();
    previousButtons = currentButtons;
    emitLiveStateChanged();
}

void JoystickController::loadSettings()
{
    QSettings settings;
    const QString prefix = settingsPrefix();

    enabledState = settings.value(prefix + QStringLiteral("enabled"), false).toBool();
    selectedDeviceName = settings.value(prefix + QStringLiteral("selectedDevice")).toString();
    deadzone = boundedPercent(settings.value(prefix + QStringLiteral("deadzone"), deadzone).toInt(), 40);
    expo = boundedPercent(settings.value(prefix + QStringLiteral("expo"), expo).toInt(), 80);
    calibratedState = settings.value(prefix + QStringLiteral("calibrated"), false).toBool();

    for (ControlAssignment &assignment : controlAssignments) {
        const QString key = controlKey(assignment.control);
        assignment.binding = settings.value(prefix + key + QStringLiteral("/binding"), assignment.binding).toString();
        assignment.inverted = settings.value(prefix + key + QStringLiteral("/inverted"), assignment.inverted).toBool();
    }

    for (int i = 0; i < buttonAssignmentsState.size(); ++i) {
        ButtonAssignment &assignment = buttonAssignmentsState[i];
        const QString rowKey = QStringLiteral("button%1/").arg(i);
        const QString buttonKey = prefix + rowKey + QStringLiteral("button");
        assignment.button = qBound(-1, settings.value(buttonKey, assignment.button).toInt(), 31);
        assignment.action = settings.value(prefix + rowKey + QStringLiteral("action"), assignment.action).toString();
        assignment.calibrated = settings.value(prefix + rowKey + QStringLiteral("calibrated"),
                                               settings.contains(buttonKey) && assignment.button >= 0).toBool();
    }

    for (auto it = axisStates.begin(); it != axisStates.end(); ++it) {
        AxisState &state = it.value();
        const QString key = state.label;
        state.minimum = settings.value(prefix + key + QStringLiteral("/minimum"), state.minimum).toInt();
        state.center = settings.value(prefix + key + QStringLiteral("/center"), state.center).toInt();
        state.maximum = settings.value(prefix + key + QStringLiteral("/maximum"), state.maximum).toInt();
    }
}

void JoystickController::saveSettings() const
{
    QSettings settings;
    const QString prefix = settingsPrefix();

    settings.setValue(prefix + QStringLiteral("enabled"), enabledState);
    settings.setValue(prefix + QStringLiteral("selectedDevice"), selectedDeviceName);
    settings.setValue(prefix + QStringLiteral("deadzone"), deadzone);
    settings.setValue(prefix + QStringLiteral("expo"), expo);
    settings.setValue(prefix + QStringLiteral("calibrated"), calibratedState);

    for (const ControlAssignment &assignment : controlAssignments) {
        const QString key = controlKey(assignment.control);
        settings.setValue(prefix + key + QStringLiteral("/binding"), assignment.binding);
        settings.setValue(prefix + key + QStringLiteral("/inverted"), assignment.inverted);
    }

    for (int i = 0; i < buttonAssignmentsState.size(); ++i) {
        const ButtonAssignment &assignment = buttonAssignmentsState.at(i);
        const QString rowKey = QStringLiteral("button%1/").arg(i);
        settings.setValue(prefix + rowKey + QStringLiteral("button"), assignment.button);
        settings.setValue(prefix + rowKey + QStringLiteral("action"), assignment.action);
        settings.setValue(prefix + rowKey + QStringLiteral("calibrated"), assignment.calibrated);
    }

    for (auto it = axisStates.constBegin(); it != axisStates.constEnd(); ++it) {
        const AxisState &state = it.value();
        const QString key = state.label;
        settings.setValue(prefix + key + QStringLiteral("/minimum"), state.minimum);
        settings.setValue(prefix + key + QStringLiteral("/center"), state.center);
        settings.setValue(prefix + key + QStringLiteral("/maximum"), state.maximum);
    }
}

void JoystickController::setMessage(const QString &newMessage)
{
    if (statusText == newMessage)
        return;

    statusText = newMessage;
    emit messageChanged();
    emit statusChanged();
}

bool JoystickController::readCurrentState(QVariantMap *state)
{
    const DeviceInfo selected = selectedDeviceInfo();
    updateDeviceStatus(selected);

    if (selected.id < 0) {
        deviceConnectedState = false;
        currentButtons = 0;
        currentPov = -1;
        currentLiveInput = QVariantMap();
        return false;
    }

#ifdef Q_OS_WIN
    JOYINFOEX info = {};
    info.dwSize = sizeof(JOYINFOEX);
    info.dwFlags = JOY_RETURNALL;
    if (joyGetPosEx(static_cast<UINT>(selected.id), &info) != JOYERR_NOERROR) {
        deviceConnectedState = false;
        currentButtons = 0;
        currentPov = -1;
        currentLiveInput = QVariantMap();
        return false;
    }

    JOYCAPSW caps = {};
    if (joyGetDevCapsW(static_cast<UINT>(selected.id), &caps, sizeof(caps)) != JOYERR_NOERROR) {
        deviceConnectedState = false;
        currentButtons = 0;
        currentPov = -1;
        currentLiveInput = QVariantMap();
        return false;
    }

    updateAxisFromRaw(QStringLiteral("Axis X"), static_cast<int>(info.dwXpos), caps.wXmin, caps.wXmax, true);
    updateAxisFromRaw(QStringLiteral("Axis Y"), static_cast<int>(info.dwYpos), caps.wYmin, caps.wYmax, true);
    updateAxisFromRaw(QStringLiteral("Axis Z"), static_cast<int>(info.dwZpos), caps.wZmin, caps.wZmax, (caps.wCaps & JOYCAPS_HASZ) != 0);
    updateAxisFromRaw(QStringLiteral("Axis R"), static_cast<int>(info.dwRpos), caps.wRmin, caps.wRmax, (caps.wCaps & JOYCAPS_HASR) != 0);
    updateAxisFromRaw(QStringLiteral("Axis U"), static_cast<int>(info.dwUpos), caps.wUmin, caps.wUmax, (caps.wCaps & JOYCAPS_HASU) != 0);
    updateAxisFromRaw(QStringLiteral("Axis V"), static_cast<int>(info.dwVpos), caps.wVmin, caps.wVmax, (caps.wCaps & JOYCAPS_HASV) != 0);

    currentButtons = static_cast<quint32>(info.dwButtons) | povButtons(static_cast<int>(info.dwPOV));
    currentPov = info.dwPOV == JOY_POVCENTERED ? -1 : static_cast<int>(info.dwPOV);
    deviceConnectedState = true;
#else
    deviceConnectedState = false;
    currentButtons = 0;
    currentPov = -1;
    currentLiveInput = QVariantMap();
    return false;
#endif

    QVariantMap live;
    for (const QString &label : rawAxisLabels()) {
        const AxisState rawAxis = axisState(label);
        if (rawAxis.present)
            live.insert(label, rawAxis.current);
    }
    live.insert(QStringLiteral("leftX"), (normalizeSigned({QString(), QString(), QString(), QStringLiteral("Axis X"), false}) + 1000) / 2000.0);
    live.insert(QStringLiteral("leftY"), (normalizeSigned({QString(), QString(), QString(), QStringLiteral("Axis Y"), false}) + 1000) / 2000.0);
    live.insert(QStringLiteral("rightX"), (normalizeSigned({QString(), QString(), QString(), QStringLiteral("Axis U"), false}) + 1000) / 2000.0);
    live.insert(QStringLiteral("rightY"), (normalizeSigned({QString(), QString(), QString(), QStringLiteral("Axis V"), false}) + 1000) / 2000.0);
    live.insert(QStringLiteral("lt"), qBound(0.0, axisState(QStringLiteral("Axis Z")).current / 65535.0, 1.0));
    live.insert(QStringLiteral("rt"), qBound(0.0, axisState(QStringLiteral("Axis R")).current / 65535.0, 1.0));
    live.insert(QStringLiteral("buttons"), currentButtons);
    live.insert(QStringLiteral("pov"), currentPov);

    currentLiveInput = live;
    if (state)
        *state = live;

    return true;
}

void JoystickController::updateDeviceStatus(const DeviceInfo &device)
{
    selectedDeviceId = device.id;
    selectedAxes = device.axes;
    selectedHasPov = device.hasPov && device.buttons <= 28;
    selectedButtons = qMin(32, device.buttons + (selectedHasPov ? 4 : 0));
}

void JoystickController::updateAxisFromRaw(const QString &label, int value, int minimum, int maximum, bool present)
{
    AxisState state = axisState(label);
    state.label = label;
    state.defaultMinimum = minimum;
    state.defaultMaximum = qMax(minimum + 1, maximum);
    state.present = present;
    if (present)
        state.current = qBound(state.defaultMinimum, value, state.defaultMaximum);
    axisStates.insert(label, state);
}

JoystickController::AxisState JoystickController::axisState(const QString &label) const
{
    return axisStates.value(label, AxisState{label, defaultAxisMinimum(), defaultAxisMaximum(),
                                             defaultAxisMinimum(), defaultAxisCenter(), defaultAxisMaximum(),
                                             defaultAxisCenter(), false});
}

int JoystickController::normalizeSigned(const ControlAssignment &assignment) const
{
    const AxisState state = axisState(assignment.binding);
    if (!state.present)
        return 0;

    const int lowSpan = qMax(1, state.center - state.minimum);
    const int highSpan = qMax(1, state.maximum - state.center);
    double value = 0.0;
    if (state.current >= state.center)
        value = (state.current - state.center) / static_cast<double>(highSpan);
    else
        value = -((state.center - state.current) / static_cast<double>(lowSpan));

    int scaled = qRound(qBound(-1.0, value, 1.0) * 1000.0);
    if (assignment.inverted)
        scaled = -scaled;
    return applyTuning(scaled);
}

int JoystickController::normalizeThrottle(const ControlAssignment &assignment) const
{
    const AxisState state = axisState(assignment.binding);
    if (!state.present)
        return 0;

    const double span = qMax(1, state.maximum - state.minimum);
    double ratio = (state.current - state.minimum) / span;
    ratio = qBound(0.0, ratio, 1.0);
    if (assignment.inverted)
        ratio = 1.0 - ratio;
    return qRound(ratio * 1000.0);
}

int JoystickController::applyTuning(int value) const
{
    const int sign = value < 0 ? -1 : 1;
    double magnitude = qAbs(value) / 1000.0;
    const double deadzoneRatio = deadzone / 100.0;
    if (magnitude <= deadzoneRatio)
        return 0;

    magnitude = (magnitude - deadzoneRatio) / qMax(0.001, 1.0 - deadzoneRatio);
    const double expoRatio = expo / 100.0;
    const double curved = ((1.0 - expoRatio) * magnitude) + (expoRatio * magnitude * magnitude * magnitude);
    return sign * qRound(qBound(0.0, curved, 1.0) * 1000.0);
}

int JoystickController::signedControlToPwm(int value) const
{
    return qBound(1000, 1500 + qRound(qBound(-1000, value, 1000) * 0.5), 2000);
}

int JoystickController::throttleControlToPwm(int value) const
{
    return qBound(1000, 1000 + qBound(0, value, 1000), 2000);
}

int JoystickController::buttonsLow16() const
{
    return static_cast<int>(currentButtons & 0xffff);
}

int JoystickController::buttonsHigh16() const
{
    return static_cast<int>((currentButtons >> 16) & 0xffff);
}

int JoystickController::totalButtonCount() const
{
    return qMin(32, selectedButtons);
}

int JoystickController::calibratedButtonCount() const
{
    int count = 0;
    for (const ButtonAssignment &assignment : buttonAssignmentsState) {
        if (assignment.calibrated && assignment.button >= 0 && assignment.button < 32)
            ++count;
    }
    return count;
}

quint32 JoystickController::povButtons(int pov) const
{
    if (!selectedHasPov || pov < 0 || selectedButtons <= 0)
        return 0;

    const int firstPovButton = selectedButtons - 4;
    if (firstPovButton < 0 || firstPovButton > 28)
        return 0;

    quint32 buttons = 0;
    if (pov >= 31500 || pov <= 4500)
        buttons |= quint32(1) << firstPovButton;
    if (pov >= 4500 && pov <= 13500)
        buttons |= quint32(1) << (firstPovButton + 1);
    if (pov >= 13500 && pov <= 22500)
        buttons |= quint32(1) << (firstPovButton + 2);
    if (pov >= 22500 && pov <= 31500)
        buttons |= quint32(1) << (firstPovButton + 3);
    return buttons;
}

QString JoystickController::buttonLabel(int buttonIndex) const
{
    if (selectedHasPov) {
        const int firstPovButton = selectedButtons - 4;
        if (buttonIndex == firstPovButton)
            return QStringLiteral("D-Pad Up");
        if (buttonIndex == firstPovButton + 1)
            return QStringLiteral("D-Pad Right");
        if (buttonIndex == firstPovButton + 2)
            return QStringLiteral("D-Pad Down");
        if (buttonIndex == firstPovButton + 3)
            return QStringLiteral("D-Pad Left");
    }

    return QStringLiteral("Button %1").arg(buttonIndex + 1);
}

bool JoystickController::requiredAssignmentsAvailable(QStringList *missingControls) const
{
    QStringList missing;
    for (const ControlAssignment &assignment : controlAssignments) {
        if (assignment.binding.isEmpty()) {
            missing.append(assignment.control);
            continue;
        }

        const AxisState state = axisState(assignment.binding);
        if (!state.present || state.maximum <= state.minimum)
            missing.append(assignment.control);
    }

    if (missingControls)
        *missingControls = missing;
    return missing.isEmpty();
}

bool JoystickController::minimumButtonsCalibrated() const
{
    return calibratedButtonCount() >= 2;
}

void JoystickController::handleButtonActions()
{
    if (!enabledState || !deviceConnectedState || !mavlink)
        return;

    const quint32 newlyPressedButtons = currentButtons & ~previousButtons;
    if (newlyPressedButtons == 0)
        return;

    for (const ButtonAssignment &assignment : buttonAssignmentsState) {
        if (!assignment.calibrated || assignment.button < 0 || assignment.button >= 32)
            continue;
        if ((newlyPressedButtons & (quint32(1) << assignment.button)) == 0)
            continue;

        executeButtonAction(assignment);
    }
}

void JoystickController::executeButtonAction(const ButtonAssignment &assignment)
{
    const QString action = assignment.action.trimmed();
    const QString normalizedAction = actionKey(action);
    if (!mavlink || normalizedAction == QStringLiteral("no action"))
        return;

    if (!mavlink->connected()) {
        setMessage(QStringLiteral("Vehicle is not connected"));
        return;
    }

    if (normalizedAction == QStringLiteral("arm")) {
        mavlink->armVehicle(true);
        setMessage(QStringLiteral("Joystick button sent Arm"));
    } else if (normalizedAction == QStringLiteral("disarm")) {
        mavlink->armVehicle(false);
        setMessage(QStringLiteral("Joystick button sent Disarm"));
    } else if (normalizedAction == QStringLiteral("takeoff")) {
        mavlink->takeoff();
        setMessage(QStringLiteral("Joystick button sent Takeoff"));
    } else if (normalizedAction == QStringLiteral("rtl")) {
        mavlink->returnToLaunch();
        setMessage(QStringLiteral("Joystick button sent RTL"));
    } else if (normalizedAction == QStringLiteral("arm / disarm")) {
        const QObject *vehicle = mavlink->parent();
        const bool arm = vehicle ? !vehicle->property("armed").toBool() : true;
        mavlink->armVehicle(arm);
        setMessage(arm ? QStringLiteral("Joystick button sent Arm")
                       : QStringLiteral("Joystick button sent Disarm"));
    } else if (normalizedAction == QStringLiteral("flight mode") || normalizedAction == QStringLiteral("flight mode menu")) {
        setMessage(QStringLiteral("Choose a flight mode for this joystick button"));
    } else if (normalizedAction.startsWith(QStringLiteral("flight mode:"))) {
        const int separator = action.indexOf(QLatin1Char(':'));
        const QString modeName = separator >= 0 ? action.mid(separator + 1).trimmed() : QString();
        const int customMode = flightModeCustomMode(modeName);
        if (customMode < 0) {
            setMessage(QStringLiteral("Unknown joystick flight mode: %1").arg(modeName));
            return;
        }
        mavlink->setFlightMode(customMode);
        setMessage(QStringLiteral("Joystick button set flight mode %1").arg(modeName));
    } else if (normalizedAction == QStringLiteral("camera trigger")) {
        setMessage(QStringLiteral("Camera trigger action is not connected yet"));
    } else if (normalizedAction == QStringLiteral("servo") || normalizedAction == QStringLiteral("servo payload")) {
        setMessage(QStringLiteral("Servo action is not configured yet"));
    } else if (normalizedAction == QStringLiteral("relay")) {
        setMessage(QStringLiteral("Relay action is not configured yet"));
    }
}

void JoystickController::sendManualControl()
{
    if (!enabledState || !calibratedState || !deviceConnectedState || !mavlink)
        return;

    const ControlAssignment *roll = assignmentForControl(QStringLiteral("Roll"));
    const ControlAssignment *pitch = assignmentForControl(QStringLiteral("Pitch"));
    const ControlAssignment *throttle = assignmentForControl(QStringLiteral("Throttle"));
    const ControlAssignment *yaw = assignmentForControl(QStringLiteral("Yaw"));
    if (!roll || !pitch || !throttle || !yaw)
        return;

    const int x = normalizeSigned(*pitch);
    const int y = normalizeSigned(*roll);
    const int z = normalizeThrottle(*throttle);
    const int r = normalizeSigned(*yaw);

    mavlink->sendManualControl(x, y, z, r, buttonsLow16(), buttonsHigh16());
    mavlink->sendRcChannelsOverride(signedControlToPwm(y),
                                     signedControlToPwm(x),
                                     throttleControlToPwm(z),
                                     signedControlToPwm(r));
}

QVariantMap JoystickController::axisDisplayMap(const ControlAssignment &assignment) const
{
    const AxisState state = axisState(assignment.binding);
    QVariantMap row;
    row.insert(QStringLiteral("axis"), assignment.control);
    row.insert(QStringLiteral("icon"), assignment.icon);
    row.insert(QStringLiteral("accent"), assignment.accent);
    row.insert(QStringLiteral("binding"), assignment.binding);
    row.insert(QStringLiteral("inverted"), assignment.inverted);
    row.insert(QStringLiteral("minimum"), state.minimum);
    row.insert(QStringLiteral("center"), state.center);
    row.insert(QStringLiteral("maximum"), state.maximum);
    row.insert(QStringLiteral("current"), state.current);
    row.insert(QStringLiteral("present"), state.present);
    const int defaultSpan = qMax(1, state.defaultMaximum - state.defaultMinimum);
    row.insert(QStringLiteral("calibrated"), state.present && (state.maximum - state.minimum) >= defaultSpan / 6);
    return row;
}

QVariantMap JoystickController::buttonDisplayMap(const ButtonAssignment &assignment, int row) const
{
    QVariantMap map;
    const bool calibrated = assignment.calibrated && assignment.button >= 0;
    map.insert(QStringLiteral("row"), row);
    map.insert(QStringLiteral("button"), calibrated ? buttonLabel(assignment.button) : QStringLiteral("Button %1").arg(row + 1));
    map.insert(QStringLiteral("buttonIndex"), calibrated ? assignment.button : -1);
    map.insert(QStringLiteral("calibrated"), calibrated);
    map.insert(QStringLiteral("action"), assignment.action);
    map.insert(QStringLiteral("detecting"), row == pendingButtonDetectionRow);
    map.insert(QStringLiteral("pressed"), calibrated && (currentButtons & (quint32(1) << assignment.button)) != 0);
    return map;
}

JoystickController::ControlAssignment *JoystickController::assignmentForControl(const QString &controlName)
{
    const QString key = controlKey(controlName);
    for (ControlAssignment &assignment : controlAssignments) {
        if (controlKey(assignment.control) == key)
            return &assignment;
    }
    return nullptr;
}

const JoystickController::ControlAssignment *JoystickController::assignmentForControl(const QString &controlName) const
{
    const QString key = controlKey(controlName);
    for (const ControlAssignment &assignment : controlAssignments) {
        if (controlKey(assignment.control) == key)
            return &assignment;
    }
    return nullptr;
}

JoystickController::DeviceInfo JoystickController::selectedDeviceInfo() const
{
    for (const DeviceInfo &device : deviceInfos) {
        if (device.name == selectedDeviceName)
            return device;
    }
    return {};
}

void JoystickController::resetDefaultAssignments()
{
    controlAssignments = {
        { QStringLiteral("Roll"), QStringLiteral("roll"), QStringLiteral("#ff5b68"), QStringLiteral("Axis X"), false },
        { QStringLiteral("Pitch"), QStringLiteral("pitch"), QStringLiteral("#2397ff"), QStringLiteral("Axis Y"), true },
        { QStringLiteral("Throttle"), QStringLiteral("throttle"), QStringLiteral("#49f08b"), QStringLiteral("Axis Z"), false },
        { QStringLiteral("Yaw"), QStringLiteral("yaw"), QStringLiteral("#b963ef"), QStringLiteral("Axis R"), false }
    };

    buttonAssignmentsState = {
        { -1, QStringLiteral("No action"), false },
        { -1, QStringLiteral("No action"), false },
        { -1, QStringLiteral("No action"), false },
        { -1, QStringLiteral("No action"), false },
        { -1, QStringLiteral("No action"), false },
        { -1, QStringLiteral("No action"), false },
        { -1, QStringLiteral("No action"), false },
        { -1, QStringLiteral("No action"), false }
    };
}

void JoystickController::emitLiveStateChanged()
{
    emit liveInputChanged();
    if (!calibratingState)
        emit axisRangesChanged();
    emit buttonAssignmentsChanged();
}
