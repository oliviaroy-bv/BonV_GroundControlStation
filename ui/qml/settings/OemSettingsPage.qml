import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.settings
import "../accessories"

Item {
    id: root

    property bool open: false
    property bool darkTheme: true
    property var vehicle: null
    property QtObject palette: Palette { darkMode: root.darkTheme }
    property string activeDetailPage: ""
    property int activeSectionIndex: -1
    property bool draftPayloadButtonVisible: false
    readonly property bool payloadButtonVisible: tacticalSettings.showPayloadButton
    property bool draftVpsModuleEnabled: false
    readonly property bool vpsModuleEnabled: vpsSettings.enabled
    property bool draftServoButtonVisible: true
    property string draftServoChannelsJson: "[9,10,11,12,13,14]"
    property int draftLowPwm: 1000
    property int draftMidPwm: 1500
    property int draftHighPwm: 2000
    property int joystickCalibrationStep: 1
    property int joystickAxisCalibrationIndex: 0
    property bool joystickEnabled: backend.joystickEnabled
    property string selectedJoystickName: backend.joystickSelectedDevice
    property int joystickDeadzone: 8
    property int joystickExpo: 25
    property var joystickDevices: backend.joystickDevices
    property var joystickStatus: backend.joystickStatus
    property var joystickLiveInput: backend.joystickLiveInput
    property var joystickAxisRanges: backend.joystickAxisRanges
    property var joystickAssignments: backend.joystickAssignments
    property var joystickButtonAssignments: backend.joystickButtonAssignments
    property int rcCalibrationStep: 1
    property bool rcCalibrationActive: false
    property string rcCalibrationStatus: "Ready"
    readonly property bool joystickDeviceDetected: backend.joystickAvailable
    readonly property var joystickStepTitles: [ "Connect", "Axes", "Buttons", "Verify", "Complete" ]
    readonly property var joystickAxisControls: [
        { axis: "Roll", icon: "roll", title: "Calibrate Roll", instruction: "Move the control you want to use for Roll left and right.", guide: "Move the roll stick left and right for Roll calibration." },
        { axis: "Pitch", icon: "pitch", title: "Calibrate Pitch", instruction: "Move the control you want to use for Pitch up and down.", guide: "Move the pitch stick up and down for Pitch calibration." },
        { axis: "Throttle", icon: "throttle", title: "Calibrate Throttle", instruction: "Move the throttle control through its full range.", guide: "Move the throttle control from minimum to maximum." },
        { axis: "Yaw", icon: "yaw", title: "Calibrate Yaw", instruction: "Move the control you want to use for Yaw left and right.", guide: "Twist or move the yaw control through its full range." }
    ]
    readonly property var joystickButtonActionOptions: [ "Arm", "Disarm", "Takeoff", "Flight Mode", "Servo", "Relay", "Camera trigger", "No action" ]
    readonly property var joystickFlightModeOptions: [
        { label: "Stabilize", mode: 0 },
        { label: "Alt Hold", mode: 2 },
        { label: "Auto", mode: 3 },
        { label: "Guided", mode: 4 },
        { label: "Loiter", mode: 5 },
        { label: "RTL", mode: 6 },
        { label: "Land", mode: 9 },
        { label: "PosHold", mode: 16 },
        { label: "Brake", mode: 17 }
    ]
    readonly property var rcStepTitles: [ "Calibrate", "Center", "Save" ]
    readonly property var rcChannelLabels: [
        "Roll", "Pitch", "Throttle", "Yaw", "Flight Mode", "Tuning",
        "Aux 1", "Aux 2", "Aux 3", "Aux 4", "Aux 5", "Aux 6",
        "Aux 7", "Aux 8", "Aux 9", "Aux 10"
    ]
    readonly property int joystickDetailMargin: width < 900 ? 24 : width < 1280 ? 56 : 86
    readonly property int joystickContentMaxWidth: width < 1280 ? 1060 : 1360
    readonly property int joystickRowGap: width < 900 ? 9 : 12
    readonly property int joystickMediumGap: width < 900 ? 12 : width < 1280 ? 14 : 16
    readonly property int joystickLargeGap: width < 900 ? 14 : width < 1280 ? 18 : 20
    readonly property int joystickCardHeight: width < 900 ? 196 : width < 1280 ? 212 : 224
    readonly property int joystickControlHeight: width < 900 ? 42 : width < 1280 ? 44 : 46
    readonly property int joystickActionHeight: width < 900 ? 40 : 44
    readonly property int joystickBodyFont: width < 900 ? 12 : 13
    readonly property int joystickTitleFont: width < 900 ? 13 : 14
    readonly property int joystickAxisNameWidth: width < 900 ? 84 : 104
    readonly property int joystickPillWidth: width < 900 ? 58 : 68
    readonly property int sectionGridMargin: width < 760 ? 20 : width < 1280 ? 44 : 72
    readonly property int sectionGridGap: width < 760 ? 16 : width < 1280 ? 28 : 36
    readonly property int sectionCardHeight: width < 760 ? 104 : width < 1280 ? 110 : 116
    readonly property color pageColor: palette.background
    readonly property color panelColor: palette.panel
    readonly property color cardColor: palette.panel
    readonly property color cardHoverColor: palette.panelAlt
    readonly property color borderColor: palette.border
    readonly property color strongBorderColor: palette.accent
    readonly property color textColor: palette.text
    readonly property color mutedColor: palette.muted
    readonly property color faintColor: palette.muted
    readonly property color accentColor: palette.accent
    readonly property color successColor: palette.success
    readonly property color warningColor: palette.warning
    readonly property color dangerColor: palette.danger
    readonly property color iconTileColor: root.darkTheme ? "#20262f" : "#eef3f9"
    readonly property int columnCount: width < 760 ? 1 : width < 1040 ? 2 : 3
    property var sections: [
        { title: "Vehicle Settings", detail: "Basic vehicle configuration,\nparameters and system settings.", icon: "plane", accent: root.accentColor, bg: root.iconTileColor },
        { title: "Telemetry", detail: "Configure telemetry link,\nradio settings and data rates.", icon: "telemetry", accent: root.successColor, bg: root.iconTileColor },
        { title: "Camera", detail: "Camera settings, streaming\nand gimbal configuration.", icon: "camera", accent: "#9f8bd8", bg: root.iconTileColor },
        { title: "Servo / Payload", detail: "Configure servos, payload\nmechanisms and output channels.", icon: "servo", accent: root.warningColor, bg: root.iconTileColor },
        { title: "Joystick", detail: "Configure joystick, axis mapping\nand control settings.", icon: "joystick", accent: root.accentColor, bg: root.iconTileColor },
        { title: "RC", detail: "Radio calibration,\nchannel mapping and reversals.", icon: "rc", accent: "#d9689d", bg: root.iconTileColor },
        { title: "MAVLink", detail: "Configure MAVLink connections,\nRTK, NTRIP and data settings.", icon: "network", accent: root.accentColor, bg: root.iconTileColor },
        { title: "Tactical", detail: "Configure Payload command button\nvisibility in the command panel.", icon: "modules", accent: "#a48cd8", bg: root.iconTileColor },
        { title: "VPS", detail: "Enable visual positioning\ncontrols in the command panel.", icon: "telemetry", accent: root.accentColor, bg: root.iconTileColor },
        { title: "Safety", detail: "Failsafes, geofence, alarms\nand safety parameters.", icon: "safety", accent: root.successColor, bg: root.iconTileColor },
        { title: "User & License", detail: "License information,\nuser preferences and activation.", icon: "users", accent: root.dangerColor, bg: root.iconTileColor },
        { title: "Data Management", detail: "Logs, terrain data, maps\nand storage settings.", icon: "cloud", accent: root.accentColor, bg: root.iconTileColor },
        { title: "About", detail: "Version information,\nsystem status and diagnostics.", icon: "about", accent: root.accentColor, bg: root.iconTileColor }
    ]

    signal closeRequested()
    signal saveRequested(bool servoButtonVisible, string servoChannelsJson, int lowPwm, int midPwm, int highPwm)
    signal sectionRequested(string sectionTitle)

    Settings {
        id: servoSettings
        category: "ServoPanel"
        property bool showTopPanelButton: true
        property string selectedChannelsJson: "[9,10,11,12,13,14]"
        property int lowPwm: 1000
        property int midPwm: 1500
        property int highPwm: 2000
    }

    Settings {
        id: tacticalSettings
        category: "Tactical"
        property bool showPayloadButton: false
    }

    Settings {
        id: vpsSettings
        category: "VPS"
        property bool enabled: false
    }

    ListModel {
        id: rcCalibrationSamplesModel
    }

    visible: opacity > 0
    enabled: root.open
    opacity: root.open ? 1 : 0
    focus: root.open
    clip: true

    Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    Keys.onEscapePressed: root.closeRequested()
    Component.onCompleted: updateJoystickMavlink()
    onOpenChanged: {
        if (open) {
            activeDetailPage = ""
            activeSectionIndex = -1
            loadDraftSettings()
            refreshJoystickDevices()
            updateJoystickMavlink()
            resetRcCalibration()
            forceActiveFocus()
        }
    }
    onVehicleChanged: updateJoystickMavlink()
    onJoystickDevicesChanged: synchronizeSelectedJoystick()

    function loadDraftSettings() {
        draftPayloadButtonVisible = tacticalSettings.showPayloadButton
        draftVpsModuleEnabled = vpsSettings.enabled
        draftServoButtonVisible = servoSettings.showTopPanelButton
        draftServoChannelsJson = normalizedChannelsJson(servoSettings.selectedChannelsJson)
        draftLowPwm = safePwm(servoSettings.lowPwm, 1000)
        draftMidPwm = safePwm(servoSettings.midPwm, 1500)
        draftHighPwm = safePwm(servoSettings.highPwm, 2000)
        joystickDeadzone = root.joystickStatus.deadzone === undefined ? joystickDeadzone : Number(root.joystickStatus.deadzone)
        joystickExpo = root.joystickStatus.expo === undefined ? joystickExpo : Number(root.joystickStatus.expo)
    }

    function applyDraftSettings() {
        draftServoChannelsJson = normalizedChannelsJson(draftServoChannelsJson)
        draftLowPwm = safePwm(draftLowPwm, 1000)
        draftMidPwm = safePwm(draftMidPwm, 1500)
        draftHighPwm = safePwm(draftHighPwm, 2000)

        tacticalSettings.showPayloadButton = draftPayloadButtonVisible
        vpsSettings.enabled = draftVpsModuleEnabled
        servoSettings.showTopPanelButton = draftServoButtonVisible
        servoSettings.selectedChannelsJson = draftServoChannelsJson
        servoSettings.lowPwm = draftLowPwm
        servoSettings.midPwm = draftMidPwm
        servoSettings.highPwm = draftHighPwm
        backend.setJoystickTuning(joystickDeadzone, joystickExpo)
        backend.setJoystickEnabled(joystickEnabled)
        root.saveRequested(draftServoButtonVisible, draftServoChannelsJson, draftLowPwm, draftMidPwm, draftHighPwm)
    }

    function sectionIndex(sectionTitle) {
        for (var index = 0; index < sections.length; ++index) {
            if (sections[index].title === sectionTitle)
                return index
        }
        return -1
    }

    function sectionKey(sectionTitle) {
        return sectionTitle.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "")
    }

    function activeSection() {
        return activeSectionIndex >= 0 && activeSectionIndex < sections.length ? sections[activeSectionIndex] : null
    }

    function selectSection(sectionIndex) {
        if (sectionIndex < 0 || sectionIndex >= sections.length)
            return
        activeSectionIndex = sectionIndex
        activeDetailPage = sectionKey(sections[sectionIndex].title)
        if (activeDetailPage === "joystick") {
            joystickCalibrationStep = 1
            joystickAxisCalibrationIndex = 0
            refreshJoystickDevices()
        }
        if (activeDetailPage === "rc")
            resetRcCalibration()
        root.sectionRequested(sections[sectionIndex].title)
    }

    function detailTitle() {
        if (activeDetailPage === "joystick")
            return "Joystick Calibration"
        if (activeDetailPage === "rc")
            return "RC Calibration"
        return activeSection() ? activeSection().title : ""
    }

    function detailSubtitle() {
        if (activeDetailPage === "joystick")
            return "Connect, calibrate, verify and complete joystick setup."
        if (activeDetailPage === "rc")
            return "Calibrate transmitter min, max, trim and reverse settings."
        return activeSection() ? activeSection().detail.replace(/\n/g, " ") : ""
    }

    function setJoystickStep(step) {
        var previousStep = joystickCalibrationStep
        var nextStep = Math.max(1, Math.min(5, step))
        if (previousStep === nextStep)
            return

        if (activeDetailPage === "joystick" && previousStep === 2 && nextStep === 1)
            backend.cancelJoystickCalibration()
        if (activeDetailPage === "joystick" && previousStep === 3 && nextStep > 3 && !backend.finishJoystickCalibration())
            return

        joystickCalibrationStep = nextStep

        if (activeDetailPage === "joystick" && previousStep === 1 && nextStep === 2) {
            joystickAxisCalibrationIndex = 0
            if (!backend.beginJoystickCalibration())
                joystickCalibrationStep = previousStep
        }
    }

    function refreshJoystickDevices() {
        backend.refreshJoystickDevices()
        synchronizeSelectedJoystick()
    }

    function synchronizeSelectedJoystick() {
        if (joystickDevices.length === 0) {
            selectedJoystickName = ""
            return
        }

        if (selectedJoystickName.length === 0 || joystickDevices.indexOf(selectedJoystickName) === -1) {
            selectedJoystickName = joystickDevices[0]
            backend.selectJoystickDevice(selectedJoystickName)
        }
    }

    function updateJoystickMavlink() {
        backend.setJoystickMavlink(root.vehicle ? root.vehicle.mavlink : null)
    }

    function mavlinkObject() {
        return root.vehicle ? root.vehicle.mavlink : null
    }

    function liveRcChannels() {
        var mavlink = root.mavlinkObject()
        return mavlink && mavlink.rcChannels ? mavlink.rcChannels : []
    }

    function liveRcValue(channel) {
        var channels = root.liveRcChannels()
        for (var index = 0; index < channels.length; ++index) {
            if (Number(channels[index].channel) === channel)
                return Number(channels[index].value)
        }
        return -1
    }

    function rcParameterValue(name, fallbackValue) {
        var mavlink = root.mavlinkObject()
        if (!mavlink || !mavlink.parameterValue)
            return fallbackValue
        var parameter = mavlink.parameterValue(name)
        if (!parameter || parameter.value === undefined)
            return fallbackValue
        var value = Number(parameter.value)
        return isFinite(value) ? Math.round(value) : fallbackValue
    }

    function makeRcCalibrationRows(useLiveAsTrim) {
        var rows = []
        for (var channel = 1; channel <= 16; ++channel) {
            var current = root.liveRcValue(channel)
            var fallbackTrim = current > 0 ? current : 1500
            var trim = useLiveAsTrim ? fallbackTrim : root.rcParameterValue("RC" + channel + "_TRIM", fallbackTrim)
            rows.push({
                channel: channel,
                label: root.rcChannelLabels[channel - 1],
                value: current,
                minimum: useLiveAsTrim ? fallbackTrim : root.rcParameterValue("RC" + channel + "_MIN", current > 0 ? current : 1100),
                maximum: useLiveAsTrim ? fallbackTrim : root.rcParameterValue("RC" + channel + "_MAX", current > 0 ? current : 1900),
                trim: trim,
                reversed: root.rcParameterValue("RC" + channel + "_REVERSED", 0) > 0
            })
        }
        return rows
    }

    function setRcCalibrationRows(rows) {
        rcCalibrationSamplesModel.clear()
        for (var index = 0; index < rows.length; ++index)
            rcCalibrationSamplesModel.append(rows[index])
    }

    function rcCalibrationRows() {
        var rows = []
        for (var index = 0; index < rcCalibrationSamplesModel.count; ++index)
            rows.push(rcCalibrationSamplesModel.get(index))
        return rows
    }

    function liveRcChannelValues() {
        var values = ({})
        var channels = root.liveRcChannels()
        for (var index = 0; index < channels.length; ++index)
            values[Number(channels[index].channel)] = Number(channels[index].value)
        return values
    }

    function resetRcCalibration() {
        rcCalibrationActive = false
        rcCalibrationStep = 1
        if (root.mavlinkObject() && root.mavlinkObject().connected) {
            beginRcCalibration()
            return
        }

        rcCalibrationStatus = "Connect the vehicle to read RC channels."
        setRcCalibrationRows(makeRcCalibrationRows(false))
    }

    function updateRcLiveSamples() {
        if (rcCalibrationSamplesModel.count === 0) {
            setRcCalibrationRows(makeRcCalibrationRows(false))
            return
        }

        var values = liveRcChannelValues()
        for (var index = 0; index < rcCalibrationSamplesModel.count; ++index) {
            var row = rcCalibrationSamplesModel.get(index)
            var current = values[Number(row.channel)]
            current = current === undefined ? -1 : current
            if (Number(row.value) !== current)
                rcCalibrationSamplesModel.setProperty(index, "value", current)
            if (root.rcCalibrationActive && current > 0) {
                var minimum = Math.min(Number(row.minimum), current)
                var maximum = Math.max(Number(row.maximum), current)
                if (Number(row.minimum) !== minimum)
                    rcCalibrationSamplesModel.setProperty(index, "minimum", minimum)
                if (Number(row.maximum) !== maximum)
                    rcCalibrationSamplesModel.setProperty(index, "maximum", maximum)
            }
        }
    }

    function beginRcCalibration() {
        if (!root.mavlinkObject() || !root.mavlinkObject().connected) {
            rcCalibrationStatus = "Connect the vehicle before calibrating RC."
            return
        }
        setRcCalibrationRows(makeRcCalibrationRows(true))
        rcCalibrationActive = true
        rcCalibrationStep = 1
        rcCalibrationStatus = "Move sticks, knobs and switches to every limit."
    }

    function finishRcRangeCapture() {
        rcCalibrationActive = false
        rcCalibrationStep = 2
        rcCalibrationStatus = "Center roll, pitch and yaw. Move throttle fully down, then continue."
    }

    function captureRcTrim() {
        var values = liveRcChannelValues()
        for (var index = 0; index < rcCalibrationSamplesModel.count; ++index) {
            var row = rcCalibrationSamplesModel.get(index)
            var current = values[Number(row.channel)]
            if (current > 0)
                rcCalibrationSamplesModel.setProperty(index, "trim", current)
        }
        rcCalibrationStep = 3
        rcCalibrationStatus = "Review calibration values. Normal min/max values are usually near 1100 and 1900."
    }

    function setRcChannelReversed(channel, reversed) {
        for (var index = 0; index < rcCalibrationSamplesModel.count; ++index) {
            if (Number(rcCalibrationSamplesModel.get(index).channel) === channel) {
                rcCalibrationSamplesModel.setProperty(index, "reversed", reversed)
                break
            }
        }
    }

    function saveRcCalibration() {
        var mavlink = root.mavlinkObject()
        if (!mavlink || !mavlink.connected) {
            rcCalibrationStatus = "Connect the vehicle before saving RC calibration."
            return
        }
        if (mavlink.writeRcCalibration(root.rcCalibrationRows()))
            rcCalibrationStatus = "RC calibration written to vehicle parameters."
        else
            rcCalibrationStatus = "RC calibration write failed."
    }

    function leaveDetailPage() {
        activeSectionIndex = -1
        activeDetailPage = ""
    }

    function safePwm(value, fallbackValue) {
        var numericValue = Number(value)
        if (!isFinite(numericValue))
            return fallbackValue
        return Math.max(800, Math.min(2200, Math.round(numericValue)))
    }

    function servoChannelNumbers(channelsJson) {
        var serializedChannels = channelsJson === undefined ? root.draftServoChannelsJson : channelsJson
        try {
            var parsedChannels = JSON.parse(serializedChannels)
            if (Array.isArray(parsedChannels))
                return parsedChannels.filter(function(channel) { return channel >= 9 && channel <= 14 })
        } catch (error) {
        }
        return [ 9, 10, 11, 12, 13, 14 ]
    }

    function normalizedChannelsJson(channelsJson) {
        var channels = servoChannelNumbers(channelsJson)
        if (channels.length === 0)
            channels = [ 9 ]
        channels.sort(function(a, b) { return a - b })
        return JSON.stringify(channels)
    }

    function channelSelected(channelNumber) {
        return servoChannelNumbers().indexOf(channelNumber) !== -1
    }

    function setChannelSelected(channelNumber, selected) {
        var channels = servoChannelNumbers()
        var existingIndex = channels.indexOf(channelNumber)
        if (selected && existingIndex === -1)
            channels.push(channelNumber)
        if (!selected && existingIndex !== -1 && channels.length > 1)
            channels.splice(existingIndex, 1)
        channels.sort(function(a, b) { return a - b })
        draftServoChannelsJson = JSON.stringify(channels)
    }

    function showSection(sectionTitle) {
        selectSection(sectionIndex(sectionTitle))
    }

    function joystickStatusText(key, fallbackValue) {
        if (!root.joystickStatus || root.joystickStatus[key] === undefined || root.joystickStatus[key] === null)
            return fallbackValue
        return String(root.joystickStatus[key])
    }

    function joystickCountText(key) {
        return root.joystickDeviceDetected ? joystickStatusText(key, "--") : "--"
    }

    function joystickPwmValue(value, minimumValue, maximumValue) {
        var range = Math.max(1, Number(maximumValue) - Number(minimumValue))
        var position = Math.max(0, Math.min(1, (Number(value) - Number(minimumValue)) / range))
        return Math.round(1000 + position * 1000)
    }

    function joystickAxisOutputNormalized(axisRow, assignmentRow) {
        var value = joystickAxisNormalized(axisRow)
        if (assignmentRow && assignmentRow.inverted === true)
            value = -value
        return value
    }

    function joystickAxisOutputPwm(axisRow, assignmentRow) {
        if (!axisRow)
            return 1500

        var minValue = Number(axisRow.minimum)
        var maxValue = Number(axisRow.maximum)
        var centerValue = Number(axisRow.center)
        var currentValue = Number(axisRow.current)
        if (!isFinite(minValue) || !isFinite(maxValue) || !isFinite(centerValue) || !isFinite(currentValue))
            return 1500

        if (axisRow.axis === "Throttle") {
            var span = Math.max(1, maxValue - minValue)
            var ratio = Math.max(0, Math.min(1, (currentValue - minValue) / span))
            if (assignmentRow && assignmentRow.inverted === true)
                ratio = 1 - ratio
            return Math.round(1000 + ratio * 1000)
        }

        var normalized = joystickAxisOutputNormalized(axisRow, assignmentRow)
        return Math.max(1000, Math.min(2000, Math.round(1500 + normalized * 500)))
    }

    function currentJoystickAxisControl() {
        var index = Math.max(0, Math.min(root.joystickAxisControls.length - 1, root.joystickAxisCalibrationIndex))
        return root.joystickAxisControls[index]
    }

    function joystickAxisRow(controlName) {
        if (!root.joystickAxisRanges)
            return null

        for (var index = 0; index < root.joystickAxisRanges.length; ++index) {
            if (root.joystickAxisRanges[index].axis === controlName)
                return root.joystickAxisRanges[index]
        }
        return null
    }

    function joystickAssignmentRow(controlName) {
        if (!root.joystickAssignments)
            return null

        for (var index = 0; index < root.joystickAssignments.length; ++index) {
            if (root.joystickAssignments[index].axis === controlName)
                return root.joystickAssignments[index]
        }
        return null
    }

    function joystickAxisNormalized(axisRow) {
        if (!axisRow)
            return 0

        var minValue = Number(axisRow.minimum)
        var maxValue = Number(axisRow.maximum)
        var centerValue = Number(axisRow.center)
        var currentValue = Number(axisRow.current)
        if (!isFinite(minValue) || !isFinite(maxValue) || !isFinite(centerValue) || !isFinite(currentValue))
            return 0

        if (axisRow.axis === "Throttle") {
            var throttleSpan = Math.max(1, maxValue - minValue)
            return Math.max(-1, Math.min(1, ((currentValue - minValue) / throttleSpan) * 2 - 1))
        }

        if (currentValue >= centerValue)
            return Math.max(0, Math.min(1, (currentValue - centerValue) / Math.max(1, maxValue - centerValue)))
        return -Math.max(0, Math.min(1, (centerValue - currentValue) / Math.max(1, centerValue - minValue)))
    }

    function joystickAxisTravel(axisRow) {
        if (!axisRow)
            return 0

        var minValue = Number(axisRow.minimum)
        var maxValue = Number(axisRow.maximum)
        if (!isFinite(minValue) || !isFinite(maxValue))
            return 0
        return Math.max(0, Math.min(1, (maxValue - minValue) / 65535.0))
    }

    function normalizedButtonAction(actionName) {
        if (actionName === "Arm / Disarm")
            return "Arm"
        if (actionName === "Flight Mode Menu")
            return "Flight Mode"
        if (actionName === "Servo Payload")
            return "Servo"
        if (actionName === "RTL")
            return "Flight Mode: RTL"
        if (actionName === "Camera Trigger")
            return "Camera trigger"
        if (actionName === "No Action")
            return "No action"
        return actionName && actionName.length > 0 ? actionName : "No action"
    }

    function joystickButtonAssigned(buttonRow) {
        if (!buttonRow)
            return false

        var buttonIndex = Number(buttonRow.buttonIndex)
        return isFinite(buttonIndex)
                && buttonIndex >= 0
                && buttonRow.calibrated === true
                && normalizedButtonAction(buttonRow.action) !== "No action"
    }

    function assignedJoystickButtons(buttonRows) {
        var assignedButtons = []
        if (!buttonRows)
            return assignedButtons

        for (var index = 0; index < buttonRows.length; ++index) {
            if (joystickButtonAssigned(buttonRows[index]))
                assignedButtons.push(buttonRows[index])
        }
        return assignedButtons
    }

    function joystickAxisCalibrated(axisRow) {
        return axisRow
                && axisRow.present === true
                && axisRow.binding !== undefined
                && String(axisRow.binding).length > 0
                && axisRow.calibrated === true
    }

    function calibratedJoystickAxisCount(axisRows) {
        var count = 0
        if (!axisRows)
            return count

        for (var index = 0; index < axisRows.length; ++index) {
            if (joystickAxisCalibrated(axisRows[index]))
                ++count
        }
        return count
    }

    function calibratedJoystickButtonCount(buttonRows) {
        var count = 0
        if (!buttonRows)
            return count

        for (var index = 0; index < buttonRows.length; ++index) {
            var buttonIndex = Number(buttonRows[index].buttonIndex)
            if (isFinite(buttonIndex) && buttonIndex >= 0 && buttonRows[index].calibrated === true)
                ++count
        }
        return count
    }

    function joystickCalibrationReady() {
        return calibratedJoystickAxisCount(root.joystickAxisRanges) >= 4
                && calibratedJoystickButtonCount(root.joystickButtonAssignments) >= 2
    }

    function joystickNextEnabled() {
        if (root.joystickCalibrationStep === 1)
            return root.joystickDeviceDetected
        if (root.joystickCalibrationStep === 2)
            return joystickAxisCalibrated(root.joystickAxisRow(root.currentJoystickAxisControl().axis))
        if (root.joystickCalibrationStep === 3)
            return calibratedJoystickButtonCount(root.joystickButtonAssignments) >= 2
        return true
    }

    function joystickPrevious() {
        if (root.joystickCalibrationStep === 1) {
            root.leaveDetailPage()
            return
        }

        if (root.joystickCalibrationStep === 2 && root.joystickAxisCalibrationIndex > 0) {
            root.joystickAxisCalibrationIndex -= 1
            return
        }

        root.setJoystickStep(root.joystickCalibrationStep - 1)
    }

    function joystickNext() {
        if (root.joystickCalibrationStep === 5) {
            root.leaveDetailPage()
            return
        }

        if (root.joystickCalibrationStep === 2 && root.joystickAxisCalibrationIndex < root.joystickAxisControls.length - 1) {
            root.joystickAxisCalibrationIndex += 1
            return
        }

        root.setJoystickStep(root.joystickCalibrationStep + 1)
    }

    Connections {
        target: root.mavlinkObject()
        function onConnectionStatusChanged() {
            var mavlink = root.mavlinkObject()
            if (root.activeDetailPage === "rc" && mavlink && mavlink.connected
                    && root.rcCalibrationStep === 1 && !root.rcCalibrationActive)
                root.beginRcCalibration()
        }
        function onRcChannelsChanged() {
            root.updateRcLiveSamples()
        }
        function onParameterUpdated(name, parameter) {
            if (root.activeDetailPage === "rc" && String(name).indexOf("RC") === 0)
                root.updateRcLiveSamples()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.pageColor
    }

    Flickable {
        id: pageFlickable
        visible: root.activeSectionIndex < 0
        anchors.fill: parent
        anchors.bottomMargin: actionFooter.height
        contentWidth: width
        contentHeight: Math.max(height, pageColumn.implicitHeight + 32)
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ColumnLayout {
            id: pageColumn
            width: pageFlickable.width
            spacing: 14

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.width < 760 ? 104 : 70

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: searchBox.left
                    anchors.rightMargin: 24
                    anchors.top: parent.top
                    anchors.topMargin: 14
                    spacing: 4

                    Text {
                        Layout.fillWidth: true
                        text: "OEM Settings"
                        color: root.textColor
                        font.pixelSize: 22
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Configure system settings for your vehicle and peripherals."
                        color: root.mutedColor
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    id: searchBox
                    width: root.width < 760 ? parent.width - 20 : Math.min(286, parent.width * 0.28)
                    height: 36
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.top: parent.top
                    anchors.topMargin: root.width < 760 ? 64 : 21
                    radius: 5
                    color: root.panelColor
                    border.color: root.borderColor
                    border.width: 1

                    SearchGlyph {
                        width: 18
                        height: 18
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.faintColor
                    }

                    TextField {
                        id: searchField
                        anchors.left: parent.left
                        anchors.leftMargin: 36
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        height: 30
                        padding: 0
                        text: ""
                        placeholderText: "Search settings..."
                        placeholderTextColor: root.faintColor
                        color: root.textColor
                        font.pixelSize: 11
                        selectByMouse: true
                        background: Rectangle { color: "#00000000"; border.width: 0 }
                    }
                }
            }

            GridLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                Layout.leftMargin: root.sectionGridMargin
                Layout.rightMargin: root.sectionGridMargin
                columns: root.columnCount
                columnSpacing: root.sectionGridGap
                rowSpacing: root.sectionGridGap

                Repeater {
                    model: root.sections

                    OemSectionCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.sectionCardHeight
                        title: modelData.title
                        detail: modelData.detail
                        iconType: modelData.icon
                        accent: modelData.accent
                        iconBackground: modelData.bg
                        onClicked: root.showSection(modelData.title)
                    }
                }
            }
        }
    }

    Item {
        id: detailWorkspace
        anchors.fill: parent
        anchors.bottomMargin: actionFooter.height
        visible: root.activeSectionIndex >= 0
        opacity: visible ? 1 : 0
        z: 8

        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                id: sectionMenu
                Layout.preferredWidth: root.width < 760 ? 204 : 256
                Layout.fillHeight: true
                color: root.darkTheme ? "#07131f" : "#edf4fb"
                border.color: root.borderColor
                border.width: 1
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: 18
                    anchors.bottomMargin: 18
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            id: backToGridButton
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            padding: 0
                            flat: true
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.text: "All sections"
                            onClicked: {
                                root.activeSectionIndex = -1
                                root.activeDetailPage = ""
                            }

                            contentItem: Canvas {
                                id: backToGridIcon
                                anchors.centerIn: parent
                                width: 16
                                height: 16
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.strokeStyle = backToGridButton.hovered ? root.textColor : root.mutedColor
                                    ctx.lineWidth = 1.8
                                    ctx.lineCap = "round"
                                    ctx.lineJoin = "round"
                                    ctx.beginPath()
                                    ctx.moveTo(10, 3)
                                    ctx.lineTo(5, 8)
                                    ctx.lineTo(10, 13)
                                    ctx.stroke()
                                }

                                Connections {
                                    target: backToGridButton
                                    function onHoveredChanged() { backToGridIcon.requestPaint() }
                                    function onDownChanged() { backToGridIcon.requestPaint() }
                                }
                            }

                            background: Rectangle {
                                radius: 5
                                color: backToGridButton.down ? root.cardHoverColor
                                      : backToGridButton.hovered ? root.cardHoverColor
                                      : "#00000000"
                                border.color: backToGridButton.hovered ? root.strongBorderColor : root.borderColor
                                border.width: 1
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: "OEM Settings"
                                color: root.textColor
                                font.pixelSize: 15
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.detailTitle()
                                color: root.accentColor
                                font.pixelSize: 10
                                font.bold: true
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: root.borderColor
                    }

                    Flickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: width
                        contentHeight: sectionMenuColumn.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true

                        Column {
                            id: sectionMenuColumn
                            width: parent.width
                            spacing: 6

                            Repeater {
                                model: root.sections

                                SectionMenuButton {
                                    width: sectionMenuColumn.width
                                    title: modelData.title
                                    iconType: modelData.icon
                                    accent: modelData.accent
                                    selected: index === root.activeSectionIndex
                                    onClicked: root.selectSection(index)
                                }
                            }
                        }
                    }
                }
            }

            Flickable {
                id: detailFlickable
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                // Guided calibration and the MAVLink Inspector are fixed workspaces.
                // Their own content panes scroll, never this outer settings viewport.
                contentHeight: root.activeDetailPage === "joystick" || root.activeDetailPage === "mavlink-inspector"
                               ? height
                               : Math.max(height, detailColumn.implicitHeight + 40)
                interactive: root.activeDetailPage !== "joystick" && root.activeDetailPage !== "mavlink-inspector"
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                ColumnLayout {
                    id: detailColumn
                    width: detailFlickable.width
                    spacing: 18

                    RowLayout {
                        visible: root.activeDetailPage !== "joystick" && root.activeDetailPage !== "mavlink-inspector"
                        Layout.fillWidth: root.activeDetailPage !== "joystick" && root.activeDetailPage !== "mavlink-inspector"
                        Layout.preferredWidth: root.activeDetailPage === "joystick"
                                               ? Math.max(320, Math.min(detailFlickable.width - root.joystickDetailMargin * 2, root.joystickContentMaxWidth))
                                               : detailFlickable.width - 70
                        Layout.alignment: root.activeDetailPage === "joystick" ? Qt.AlignHCenter : Qt.AlignLeft
                        Layout.leftMargin: root.activeDetailPage === "joystick" ? root.joystickDetailMargin : 34
                        Layout.rightMargin: root.activeDetailPage === "joystick" ? root.joystickDetailMargin : 36
                        Layout.topMargin: 24
                        spacing: 16

                        Rectangle {
                            Layout.preferredWidth: 58
                            Layout.preferredHeight: 58
                            radius: 7
                            color: root.activeSection() ? root.activeSection().bg : root.iconTileColor

                            OemIcon {
                                anchors.centerIn: parent
                                width: 34
                                height: 34
                                iconType: root.activeSection() ? root.activeSection().icon : ""
                                color: root.activeSection() ? root.activeSection().accent : root.accentColor
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                Layout.fillWidth: true
                                text: root.activeSection() ? root.activeSection().title : ""
                                color: root.textColor
                                font.pixelSize: 24
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.detailSubtitle()
                                color: root.mutedColor
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Item {
                        id: activePageContainer

                        Layout.fillWidth: root.activeDetailPage !== "joystick"
                        Layout.preferredWidth: root.activeDetailPage === "joystick"
                                               ? Math.max(320, Math.min(detailFlickable.width - root.joystickDetailMargin * 2, root.joystickContentMaxWidth))
                                               : detailFlickable.width - 70
                        Layout.alignment: root.activeDetailPage === "joystick" ? Qt.AlignHCenter : Qt.AlignLeft
                        Layout.leftMargin: root.activeDetailPage === "joystick" ? root.joystickDetailMargin : 34
                        Layout.rightMargin: root.activeDetailPage === "joystick" ? root.joystickDetailMargin : 36
                        readonly property bool fitJoystickPage: root.activeDetailPage === "joystick"
                        readonly property bool fitInspectorPage: root.activeDetailPage === "mavlink-inspector"
                        // Reserve the Flickable's bottom breathing room so the
                        // scaled content never extends below the visible page.
                        readonly property real availableHeight: Math.max(1, detailFlickable.height - 40)
                        readonly property real joystickScale: !fitJoystickPage || activePageLoader.implicitHeight <= 0
                                                             ? 1.0
                                                             : Math.min(1.0, availableHeight / activePageLoader.implicitHeight)

                        Layout.preferredHeight: fitInspectorPage ? availableHeight : -1
                        implicitHeight: fitInspectorPage
                                        ? availableHeight
                                        : fitJoystickPage
                                        ? Math.ceil(activePageLoader.implicitHeight * joystickScale)
                                        : activePageLoader.implicitHeight

                        Loader {
                            id: activePageLoader
                            x: activePageContainer.fitJoystickPage
                               ? Math.round((activePageContainer.width - width * activePageContainer.joystickScale) / 2)
                               : 0
                            width: parent.width
                            height: activePageContainer.fitInspectorPage ? parent.height : implicitHeight
                            scale: activePageContainer.joystickScale
                            transformOrigin: Item.TopLeft
                            sourceComponent: root.activeDetailPage === "servo-payload"
                                             ? servoSettingsPage
                                             : root.activeDetailPage === "rc"
                                               ? rcCalibrationPage
                                             : root.activeDetailPage === "joystick"
                                               ? joystickCalibrationPage
                                             : root.activeDetailPage === "mavlink"
                                               ? mavlinkToolsPage
                                             : root.activeDetailPage === "mavlink-inspector"
                                               ? mavlinkInspectorPage
                                             : root.activeDetailPage === "mavlink-forwarding"
                                               ? mavlinkForwardingPage
                                             : root.activeDetailPage === "tactical"
                                             ? tacticalSettingsPage
                                             : root.activeDetailPage === "vps"
                                             ? vpsSettingsPage
                                             : placeholderSettingsPage
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: actionFooter

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.activeSectionIndex < 0 ? 64 : 0
        visible: height > 0
        z: 20
        color: root.darkTheme ? "#f2031321" : "#f2f3f8fd"
        border.color: root.borderColor
        border.width: 1

        RowLayout {
            anchors.right: parent.right
            anchors.rightMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            FooterButton {
                text: "CLOSE"
                onClicked: root.closeRequested()
            }

            FooterButton {
                text: "SAVE"
                active: true
                onClicked: root.applyDraftSettings()
            }
        }
    }

    Component {
        id: mavlinkToolsPage

        ColumnLayout {
            width: parent ? parent.width : 0
            spacing: 12

            Text { text: "MAVLink tools"; color: root.textColor; font.pixelSize: 16; font.bold: true }
            Text { text: "Observe traffic or copy the complete inbound MAVLink stream to a UDP endpoint."; color: root.mutedColor; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }

            Repeater {
                model: [
                    { title: "MAVLink Inspector", detail: "Live traffic, fields, graphs and raw packets", page: "mavlink-inspector" },
                    { title: "MAVLink Forwarding", detail: "Forward valid inbound wire packets to UDP", page: "mavlink-forwarding" }
                ]
                delegate: Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    padding: 12
                    onClicked: root.activeDetailPage = modelData.page
                    background: Rectangle { color: parent.hovered ? root.cardHoverColor : root.cardColor; border.color: root.borderColor; border.width: 1; radius: 5 }
                    contentItem: ColumnLayout {
                        Text { text: modelData.title; color: root.textColor; font.pixelSize: 14; font.bold: true }
                        Text { text: modelData.detail; color: root.mutedColor; font.pixelSize: 11 }
                    }
                }
            }
        }
    }

    Component {
        id: mavlinkInspectorPage

        MavlinkInspectorPage {
            width: parent ? parent.width : 0
            mavlinkObject: root.mavlinkObject()
            darkTheme: root.darkTheme
            panelColor: root.panelColor
            borderColor: root.borderColor
            textColor: root.textColor
            mutedColor: root.mutedColor
            accentColor: root.accentColor
        }
    }

    Component {
        id: mavlinkForwardingPage

        MavlinkForwardingPage {
            width: parent ? parent.width : 0
            mavlinkObject: root.mavlinkObject()
            darkTheme: root.darkTheme
            panelColor: root.panelColor
            borderColor: root.borderColor
            textColor: root.textColor
            mutedColor: root.mutedColor
            accentColor: root.accentColor
        }
    }

    Component {
        id: servoSettingsPage

        ColumnLayout {
            width: parent ? parent.width : 0
            spacing: 14

            SettingBlock {
                title: "TOP PANEL"
                detail: "Show servo button on top panel on vehicle connection"

                CheckBox {
                    id: topPanelServoCheck
                    checked: root.draftServoButtonVisible
                    onToggled: root.draftServoButtonVisible = checked

                    indicator: Rectangle {
                        implicitWidth: 20
                        implicitHeight: 20
                        radius: 4
                        color: topPanelServoCheck.checked ? root.accentColor : "#00000000"
                        border.color: topPanelServoCheck.checked ? root.accentColor : root.borderColor
                        border.width: 1

                        Canvas {
                            anchors.centerIn: parent
                            width: 12
                            height: 10
                            visible: topPanelServoCheck.checked
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                ctx.strokeStyle = root.darkTheme ? root.pageColor : "#ffffff"
                                ctx.lineWidth = 2
                                ctx.lineCap = "round"
                                ctx.lineJoin = "round"
                                ctx.beginPath()
                                ctx.moveTo(2, 5)
                                ctx.lineTo(5, 8)
                                ctx.lineTo(10, 2)
                                ctx.stroke()
                            }
                        }
                    }

                    contentItem: Text {
                        text: topPanelServoCheck.checked ? "Enabled" : "Disabled"
                        color: topPanelServoCheck.checked ? root.accentColor : root.mutedColor
                        font.pixelSize: 12
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 28
                    }
                }
            }

            SettingBlock {
                title: "DROPDOWN CHANNELS"
                detail: "Select channel numbers to be displayed on the dropdown"

                Flow {
                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: [ 9, 10, 11, 12, 13, 14 ]

                        ChannelCheck {
                            channelNumber: modelData
                            checked: root.channelSelected(modelData)
                            onToggled: root.setChannelSelected(modelData, checked)
                        }
                    }
                }
            }

            SettingBlock {
                title: "PWM PRESETS"
                detail: "Modify PWM values of low, mid and high buttons"

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    PwmEditor { label: "LOW"; value: root.draftLowPwm; onEdited: root.draftLowPwm = value }
                    PwmEditor { label: "MID"; value: root.draftMidPwm; onEdited: root.draftMidPwm = value }
                    PwmEditor { label: "HIGH"; value: root.draftHighPwm; onEdited: root.draftHighPwm = value }
                }
            }
        }
    }

    Component {
        id: tacticalSettingsPage

        ColumnLayout {
            width: parent ? parent.width : 0
            spacing: 14

            SettingBlock {
                title: "PAYLOAD COMMAND BUTTON"
                detail: "Show Payload after RTL in critical actions when a vehicle is connected."

                CheckBox {
                    id: payloadButtonCheck
                    checked: root.draftPayloadButtonVisible
                    onToggled: root.draftPayloadButtonVisible = checked

                    indicator: Rectangle {
                        implicitWidth: 20
                        implicitHeight: 20
                        radius: 4
                        color: payloadButtonCheck.checked ? root.accentColor : "#00000000"
                        border.color: payloadButtonCheck.checked ? root.accentColor : root.borderColor
                        border.width: 1

                        Canvas {
                            anchors.centerIn: parent
                            width: 12
                            height: 10
                            visible: payloadButtonCheck.checked
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                ctx.strokeStyle = root.darkTheme ? root.pageColor : "#ffffff"
                                ctx.lineWidth = 2
                                ctx.lineCap = "round"
                                ctx.lineJoin = "round"
                                ctx.beginPath()
                                ctx.moveTo(2, 5)
                                ctx.lineTo(5, 8)
                                ctx.lineTo(10, 2)
                                ctx.stroke()
                            }
                        }
                    }

                    contentItem: Text {
                        text: payloadButtonCheck.checked ? "Enabled" : "Disabled"
                        color: payloadButtonCheck.checked ? root.accentColor : root.mutedColor
                        font.pixelSize: 12
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 28
                    }
                }
            }
        }
    }

    Component {
        id: vpsSettingsPage

        ColumnLayout {
            width: parent ? parent.width : 0
            spacing: 14

            SettingBlock {
                title: "VPS MODULE"
                detail: "Show VPS controls in the command panel when a vehicle is connected."

                CheckBox {
                    id: vpsModuleCheck
                    checked: root.draftVpsModuleEnabled
                    onToggled: root.draftVpsModuleEnabled = checked
                    text: checked ? "Enabled" : "Disabled"
                    contentItem: Text {
                        text: vpsModuleCheck.text
                        color: vpsModuleCheck.checked ? root.accentColor : root.mutedColor
                        font.pixelSize: 12
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: vpsModuleCheck.indicator.width + 8
                    }
                }
            }
        }
    }

    Component {
        id: placeholderSettingsPage

        ColumnLayout {
            width: parent ? parent.width : 0
            spacing: 14

            SettingBlock {
                title: "SYSTEM PARAMETERS"
                detail: "Review and tune the selected OEM setting group."

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        text: root.activeSection() ? root.activeSection().title + " page" : "Settings page"
                        color: root.textColor
                        font.pixelSize: 13
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: "Ready"
                        color: root.successColor
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            SettingBlock {
                title: "CONFIGURATION"
                detail: "This area is reserved for detailed controls for the selected section."

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    radius: 5
                    color: root.cardColor
                    border.color: root.borderColor
                    border.width: 1

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        text: root.activeSection() ? root.activeSection().detail.replace(/\n/g, " ") : ""
                        color: root.mutedColor
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    Component {
        id: rcCalibrationPage

        ColumnLayout {
            width: parent ? parent.width : 0
            spacing: root.joystickLargeGap

            ColumnLayout {
                Layout.fillWidth: true
                spacing: root.joystickLargeGap

                JoystickStepProgress {
                    Layout.fillWidth: true
                    currentStep: root.rcCalibrationStep
                    stepTitles: root.rcStepTitles
                }

                SettingBlock {
                    title: "RADIO CALIBRATION"
                    detail: root.rcCalibrationStatus

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: root.joystickMediumGap

                        Text {
                            Layout.fillWidth: true
                            text: root.rcCalibrationStep === 1
                                  ? root.rcCalibrationActive
                                    ? "Move roll, pitch, throttle, yaw, switches and knobs through their full travel. Red ticks track observed limits."
                                    : "Turn on the transmitter, center trims, verify receiver input and keep propellers removed."
                                  : root.rcCalibrationStep === 2
                                    ? "Center all sticks except throttle. Keep throttle fully down before continuing."
                                    : "Confirm trim and reverse direction before writing values to the autopilot."
                            color: root.mutedColor
                            font.pixelSize: root.joystickBodyFont
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: root.joystickMediumGap

                            DetailMetric {
                                Layout.fillWidth: true
                                label: "Vehicle"
                                value: root.mavlinkObject() && root.mavlinkObject().connected ? "Connected" : "Disconnected"
                                success: root.mavlinkObject() && root.mavlinkObject().connected
                            }

                            DetailMetric {
                                Layout.fillWidth: true
                                label: "Channels"
                                value: String(root.liveRcChannels().length)
                                success: root.liveRcChannels().length > 0
                            }

                            DetailMetric {
                                Layout.fillWidth: true
                                label: "Capture"
                                value: root.rcCalibrationActive ? "Recording" : "Idle"
                                success: root.rcCalibrationActive
                            }
                        }
                    }
                }
            }

            SettingBlock {
                title: "CHANNEL INPUT"
                detail: root.rcCalibrationActive
                        ? "Green bars show live PWM. Red markers show the minimum and maximum captured in this calibration."
                        : "Green bars show live PWM. Start calibration to record channel limits."

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width < 1180 ? 1 : 2
                    columnSpacing: root.joystickLargeGap
                    rowSpacing: root.joystickRowGap

                    Repeater {
                        model: rcCalibrationSamplesModel

                        RcChannelCalibrationRow {
                            Layout.fillWidth: true
                            channel: model.channel
                            label: model.label
                            value: model.value
                            minimum: model.minimum
                            maximum: model.maximum
                            trim: model.trim
                            reversed: model.reversed
                            calibrationActive: root.rcCalibrationActive
                            onReverseChanged: root.setRcChannelReversed(channel, reversed)
                        }
                    }
                }
            }

            SettingBlock {
                title: "ACTIONS"
                detail: ""

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.joystickMediumGap

                    WorkflowButton {
                        text: "Reset"
                        iconType: "cancel"
                        onClicked: root.resetRcCalibration()
                    }

                    Item { Layout.fillWidth: true }

                    WorkflowButton {
                        text: root.rcCalibrationStep === 1 ? (root.rcCalibrationActive ? "Click When Done" : "Calibrate Radio")
                              : root.rcCalibrationStep === 2 ? "Centered"
                              : "Write Params"
                        iconType: root.rcCalibrationStep === 3 ? "check" : "next"
                        active: root.mavlinkObject() && root.mavlinkObject().connected
                        enabled: root.mavlinkObject() && root.mavlinkObject().connected
                        onClicked: {
                            if (root.rcCalibrationStep === 1 && !root.rcCalibrationActive)
                                root.beginRcCalibration()
                            else if (root.rcCalibrationStep === 1)
                                root.finishRcRangeCapture()
                            else if (root.rcCalibrationStep === 2)
                                root.captureRcTrim()
                            else
                                root.saveRcCalibration()
                        }
                    }
                }
            }
        }
    }

    Component {
        id: joystickCalibrationPage

        ColumnLayout {
            width: parent ? parent.width : 0
            spacing: root.joystickLargeGap

            JoystickStepProgress {
                Layout.fillWidth: true
                currentStep: root.joystickCalibrationStep
            }

            Loader {
                Layout.fillWidth: true
                Layout.topMargin: 8
                sourceComponent: root.joystickCalibrationStep === 1 ? joystickConnectStep
                                 : root.joystickCalibrationStep === 2 ? joystickCalibrateStep
                                 : root.joystickCalibrationStep === 3 ? joystickButtonCalibrationStep
                                 : root.joystickCalibrationStep === 4 ? joystickVerifyStep
                                 : joystickCompleteStep
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 12
                spacing: root.joystickLargeGap

                WorkflowButton {
                    text: root.joystickCalibrationStep === 1 ? "Cancel" : "Back"
                    iconType: root.joystickCalibrationStep === 1 ? "cancel" : "back"
                    onClicked: root.joystickPrevious()
                }

                Item { Layout.fillWidth: true }

                WorkflowButton {
                    text: root.joystickCalibrationStep === 5 ? "Finish" : "Next"
                    iconType: root.joystickCalibrationStep === 5 ? "check" : "next"
                    active: root.joystickNextEnabled()
                    enabled: root.joystickNextEnabled()
                    onClicked: root.joystickNext()
                }
            }
        }
    }

    Component {
        id: joystickConnectStep

        ColumnLayout {
            width: parent ? parent.width : 0
            spacing: root.joystickLargeGap

            SettingBlock {
                title: "Select Joystick"
                detail: "Choose the input device to calibrate and refresh the connected joystick list."

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.joystickMediumGap

                    Rectangle {
                        Layout.preferredWidth: root.width < 900 ? 68 : 76
                        Layout.preferredHeight: root.joystickControlHeight
                        radius: 7
                        color: root.iconTileColor

                        Image {
                            anchors.centerIn: parent
                            width: root.width < 900 ? 34 : 40
                            height: width
                            source: "qrc:/icons/joystick.png"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }
                    }

                    ComboBox {
                        id: joystickSelector
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.joystickControlHeight
                        enabled: root.joystickDeviceDetected
                        model: root.joystickDevices
                        currentIndex: root.joystickDeviceDetected ? Math.max(0, root.joystickDevices.indexOf(root.selectedJoystickName)) : -1
                        onActivated: {
                            if (currentText.length > 0) {
                                root.selectedJoystickName = currentText
                                backend.selectJoystickDevice(currentText)
                            }
                        }

                        contentItem: Text {
                            text: root.joystickDeviceDetected ? joystickSelector.displayText : "No joystick detected"
                            color: root.joystickDeviceDetected ? root.textColor : root.mutedColor
                            font.pixelSize: root.joystickBodyFont
                            font.bold: true
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: root.joystickMediumGap
                            elide: Text.ElideRight
                        }

                        indicator: DropDownGlyph {
                            width: 16
                            height: 16
                            x: joystickSelector.width - width - 16
                            y: Math.round((joystickSelector.height - height) / 2)
                            color: root.mutedColor
                        }

                        background: Rectangle {
                            radius: 5
                            color: root.cardColor
                            border.color: joystickSelector.hovered || joystickSelector.activeFocus ? root.strongBorderColor : root.borderColor
                            border.width: 1
                        }
                    }

                    IconOnlyButton {
                        Layout.preferredWidth: root.width < 900 ? 52 : 58
                        Layout.preferredHeight: root.joystickControlHeight
                        iconType: "refresh"
                        toolTipText: "Refresh devices"
                        onClicked: root.refreshJoystickDevices()
                    }
                }
            }

            SettingBlock {
                title: "Device Status"
                detail: root.joystickDeviceDetected ? "Connection details for the selected controller." : "Attach a joystick and refresh to start calibration."

                GridLayout {
                    Layout.fillWidth: true
                    columns: detailFlickable.width < 820 ? 1 : 2
                    columnSpacing: root.width < 900 ? 28 : 42
                    rowSpacing: root.joystickLargeGap

                    JoystickDevicePreview {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.width < 900 ? 180 : 200
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: root.joystickMediumGap

                        DetailMetric { label: "Device"; value: root.joystickDeviceDetected ? root.joystickStatusText("device", root.selectedJoystickName) : "--" }
                        DetailMetric { label: "Status"; value: root.joystickStatusText("status", "Disconnected"); success: root.joystickDeviceDetected }
                        DetailMetric { label: "Axes"; value: root.joystickCountText("axes") }
                        DetailMetric { label: "Buttons"; value: root.joystickCountText("buttons") }
                        DetailMetric { label: "POVs"; value: root.joystickCountText("povs") }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: root.borderColor
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: root.joystickMediumGap

                            CheckBox {
                                id: joystickEnableCheck
                                Layout.preferredWidth: root.width < 900 ? 168 : 184
                                Layout.preferredHeight: root.joystickControlHeight
                                checked: root.joystickEnabled
                                onToggled: {
                                    root.joystickEnabled = checked
                                    backend.setJoystickEnabled(checked)
                                }
                                padding: 0

                                indicator: Rectangle {
                                    x: 0
                                    y: Math.round((joystickEnableCheck.height - height) / 2)
                                    width: root.width < 900 ? 22 : 24
                                    height: root.width < 900 ? 22 : 24
                                    radius: 4
                                    color: joystickEnableCheck.checked ? root.accentColor : "#00000000"
                                    border.color: joystickEnableCheck.checked ? root.accentColor : root.borderColor
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        visible: joystickEnableCheck.checked
                                        text: "\u2713"
                                        color: root.darkTheme ? root.pageColor : "#ffffff"
                                        font.pixelSize: 14
                                        font.bold: true
                                    }
                                }

                                contentItem: Text {
                                    text: joystickEnableCheck.checked ? "Joystick Enabled" : "Joystick Disabled"
                                    color: joystickEnableCheck.checked ? root.accentColor : root.mutedColor
                                    font.pixelSize: root.joystickBodyFont - 1
                                    font.bold: true
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: root.width < 900 ? 32 : 36
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: joystickCalibrateStep

        RowLayout {
            width: parent ? parent.width : 0
            spacing: root.joystickLargeGap

            GuidedAxisCalibrationPanel {
                Layout.fillWidth: true
                Layout.preferredWidth: 620
            }

            ControllerGuidePanel {
                Layout.preferredWidth: root.width < 1100 ? 300 : 360
                Layout.fillHeight: true
            }
        }
    }

    Component {
        id: joystickButtonCalibrationStep

        ColumnLayout {
            width: parent ? parent.width : 0
            spacing: root.joystickLargeGap
            
            SettingBlock {
                title: "Button Calibration"
                detail: "Detect at least two joystick buttons and assign the action for each slot."

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: root.joystickRowGap

                    Text {
                        Layout.fillWidth: true
                        text: root.calibratedJoystickButtonCount(root.joystickButtonAssignments) + " / 2 required buttons detected"
                        color: root.calibratedJoystickButtonCount(root.joystickButtonAssignments) >= 2 ? root.successColor : root.mutedColor
                        font.pixelSize: root.joystickBodyFont
                        font.bold: true
                    }

                    Repeater {
                        model: root.joystickButtonAssignments

                        ButtonShortcutRow {
                            Layout.fillWidth: true
                            rowIndex: modelData.row
                            buttonName: modelData.button
                            actionName: modelData.action
                            detecting: modelData.detecting
                            pressed: modelData.pressed
                            showDivider: index < root.joystickButtonAssignments.length - 1
                        }
                    }
                }
            }
        }
    }

    Component {
        id: joystickVerifyStep

        ColumnLayout {
            width: parent ? parent.width : 0
            spacing: root.joystickLargeGap

            SettingBlock {
                title: "Assign Controls"
                detail: "Detect the input used for each flight control and set reverse direction where needed."

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: root.joystickRowGap

                    Repeater {
                        model: root.joystickAssignments

                        AssignmentRow {
                            Layout.fillWidth: true
                            axisName: modelData.axis
                            iconType: modelData.icon
                            accent: modelData.accent
                            binding: modelData.binding
                            inverted: modelData.inverted
                            detecting: modelData.detecting
                            showDivider: index < root.joystickAssignments.length - 1
                        }
                    }
                }
            }

            SettingBlock {
                id: verifyButtonsBlock

                title: "Calibrated Buttons"
                detail: "Review the detected joystick buttons that have assigned actions."

                readonly property var assignedButtons: root.assignedJoystickButtons(root.joystickButtonAssignments)

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: root.joystickRowGap

                    Text {
                        Layout.fillWidth: true
                        visible: verifyButtonsBlock.assignedButtons.length === 0
                        text: "No assigned joystick buttons yet."
                        color: root.mutedColor
                        font.pixelSize: root.joystickBodyFont
                        horizontalAlignment: Text.AlignHCenter
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        visible: verifyButtonsBlock.assignedButtons.length > 0
                        columns: detailFlickable.width < 820 ? 2 : 4
                        columnSpacing: root.joystickMediumGap
                        rowSpacing: root.joystickMediumGap

                        Repeater {
                            model: verifyButtonsBlock.assignedButtons

                            SummaryChip {
                                Layout.fillWidth: true
                                highlighted: true
                                label: modelData.button
                                value: root.normalizedButtonAction(modelData.action)
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: joystickCompleteStep

        SettingBlock {
            title: "Calibration Summary"
            detail: "Calibration is saved and joystick manual-control output is ready when enabled."

            ColumnLayout {
                Layout.fillWidth: true
                spacing: root.joystickMediumGap

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: root.width < 900 ? 58 : 64
                        Layout.preferredHeight: root.width < 900 ? 58 : 64
                        radius: width / 2
                        color: "#55f0bd"

                        Canvas {
                            anchors.centerIn: parent
                            width: root.width < 900 ? 34 : 38
                            height: root.width < 900 ? 34 : 38
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                ctx.strokeStyle = root.darkTheme ? "#06251b" : "#0a472f"
                                ctx.lineWidth = 5
                                ctx.lineCap = "round"
                                ctx.lineJoin = "round"
                                ctx.beginPath()
                                ctx.moveTo(8, 20)
                                ctx.lineTo(16, 28)
                                ctx.lineTo(30, 10)
                                ctx.stroke()
                            }
                        }

                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Joystick Calibrated"
                        color: root.textColor
                        font.pixelSize: root.width < 900 ? 20 : 22
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Your joystick is ready to use."
                        color: root.mutedColor
                        font.pixelSize: root.joystickBodyFont - 1
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: root.borderColor
                }

                SummaryChip {
                    Layout.fillWidth: true
                    label: "Joystick"
                    value: root.selectedJoystickName.length > 0 ? root.selectedJoystickName : "None"
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: root.borderColor
                }

                Text {
                    Layout.fillWidth: true
                    text: "Assigned Buttons"
                    color: root.textColor
                    font.pixelSize: root.joystickTitleFont
                    font.bold: true
                }

                Text {
                    Layout.fillWidth: true
                    visible: assignedButtonGrid.assignedButtons.length === 0
                    text: "No assigned joystick buttons yet."
                    color: root.mutedColor
                    font.pixelSize: root.joystickBodyFont
                    horizontalAlignment: Text.AlignHCenter
                }

                GridLayout {
                    id: assignedButtonGrid

                    readonly property var assignedButtons: root.assignedJoystickButtons(root.joystickButtonAssignments)

                    Layout.fillWidth: true
                    visible: assignedButtons.length > 0
                    columns: detailFlickable.width < 820 ? 2 : 4
                    columnSpacing: root.joystickMediumGap
                    rowSpacing: root.joystickMediumGap

                    Repeater {
                        model: assignedButtonGrid.assignedButtons

                        SummaryChip {
                            Layout.fillWidth: true
                            highlighted: true
                            label: modelData.button
                            value: root.normalizedButtonAction(modelData.action)
                        }
                    }
                }
            }
        }
    }

    component SectionMenuButton: Button {
        id: menuButton

        property string title: ""
        property string iconType: ""
        property color accent: root.accentColor
        property bool selected: false

        height: 44
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: Item {
            OemIcon {
                width: 20
                height: 20
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                iconType: menuButton.iconType
                color: menuButton.selected ? menuButton.accent : root.faintColor
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 42
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: menuButton.title
                color: menuButton.selected ? root.textColor : root.mutedColor
                font.pixelSize: 12
                font.bold: menuButton.selected
                elide: Text.ElideRight
            }

            Rectangle {
                width: 3
                height: parent.height - 14
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                radius: 1
                color: menuButton.accent
                visible: menuButton.selected
            }
        }

        background: Rectangle {
            radius: 6
            color: menuButton.selected ? (root.darkTheme ? "#102537" : "#dfeef8")
                  : menuButton.down ? root.cardHoverColor
                  : menuButton.hovered ? root.cardHoverColor
                  : "#00000000"
            border.color: menuButton.selected ? menuButton.accent
                         : menuButton.hovered ? root.strongBorderColor
                         : "#00000000"
            border.width: menuButton.selected || menuButton.hovered ? 1 : 0
        }
    }

    component OemSectionCard: Button {
        id: card

        property string title: ""
        property string detail: ""
        property string iconType: ""
        property color accent: root.accentColor
        property color iconBackground: "#0b3150"

        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: Item {
            Rectangle {
                id: iconTile
                width: root.width < 760 ? 50 : 58
                height: width
                x: root.width < 760 ? 16 : 20
                anchors.verticalCenter: parent.verticalCenter
                radius: 6
                color: card.iconBackground

                OemIcon {
                    anchors.centerIn: parent
                    width: root.width < 760 ? 31 : 36
                    height: width
                    iconType: card.iconType
                    color: card.accent
                }
            }

            ColumnLayout {
                anchors.left: iconTile.right
                anchors.leftMargin: root.width < 760 ? 16 : 22
                anchors.right: chevron.left
                anchors.rightMargin: root.width < 760 ? 12 : 18
                anchors.verticalCenter: parent.verticalCenter
                spacing: 7

                Text {
                    Layout.fillWidth: true
                    text: card.title
                    color: root.textColor
                    font.pixelSize: root.width < 760 ? 14 : 16
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: card.detail
                    color: root.mutedColor
                    font.pixelSize: root.width < 760 ? 12 : 13
                    lineHeight: 1.24
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }
            }

            ChevronGlyph { 
                id: chevron
                width: root.width < 760 ? 18 : 22
                height: width
                anchors.right: parent.right
                anchors.rightMargin: root.width < 760 ? 14 : 20
                anchors.verticalCenter: parent.verticalCenter
                color: card.hovered ? root.textColor : root.faintColor
            }
        }

        background: Rectangle {
            radius: 6
            color: card.down ? root.cardHoverColor
                  : card.hovered ? root.cardHoverColor
                  : root.cardColor
            border.color: card.hovered || card.down ? root.strongBorderColor : root.borderColor
            border.width: 1
        }
    }

    component SettingBlock: Rectangle {
        id: block

        property string title: ""
        property string detail: ""
        default property alias blockContent: blockContentColumn.data

        Layout.fillWidth: true
        implicitHeight: blockColumn.implicitHeight + 40
        radius: 6
        color: root.panelColor
        border.color: root.borderColor
        border.width: 1

        ColumnLayout {
            id: blockColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5

                Text {
                    Layout.fillWidth: true
                    text: block.title
                    color: root.textColor
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: block.detail
                    visible: block.detail.length > 0
                    color: root.mutedColor
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                id: blockContentColumn
                Layout.fillWidth: true
                spacing: 12
            }
        }
    }

    component FooterButton: Button {
        id: footerButton

        property bool active: false

        Layout.preferredWidth: 86
        Layout.preferredHeight: 34
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: Text {
            text: footerButton.text
            color: footerButton.active ? root.accentColor : root.textColor
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 5
            color: footerButton.down ? root.cardHoverColor
                  : footerButton.hovered ? root.cardHoverColor
                  : root.panelColor
            border.color: footerButton.active ? root.accentColor
                         : footerButton.hovered ? root.strongBorderColor
                         : root.borderColor
            border.width: 1
        }
    }

    component WorkflowButton: Button {
        id: workflowButton

        property bool active: false
        property string iconType: "next"

        Layout.preferredWidth: root.width < 900 ? 112 : 132
        Layout.preferredHeight: root.joystickActionHeight
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: Item {
            Row {
                anchors.centerIn: parent
                spacing: 8

                Canvas {
                    id: workflowLeadingIcon
                    width: root.width < 900 ? 15 : 17
                    height: width
                    visible: workflowButton.iconType !== "next" && workflowButton.iconType !== "none"
                    anchors.verticalCenter: parent.verticalCenter
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle = workflowButton.active ? "#ffffff" : root.textColor
                        ctx.fillStyle = ctx.strokeStyle
                        ctx.lineWidth = 1.8
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"

                        if (workflowButton.iconType === "back") {
                            ctx.beginPath()
                            ctx.moveTo(10, 3)
                            ctx.lineTo(5, 8)
                            ctx.lineTo(10, 13)
                            ctx.moveTo(5, 8)
                            ctx.lineTo(14, 8)
                            ctx.stroke()
                        } else if (workflowButton.iconType === "cancel") {
                            ctx.beginPath()
                            ctx.moveTo(4, 4)
                            ctx.lineTo(12, 12)
                            ctx.moveTo(12, 4)
                            ctx.lineTo(4, 12)
                            ctx.stroke()
                        } else if (workflowButton.iconType === "check") {
                            ctx.beginPath()
                            ctx.arc(8, 8, 6, 0, Math.PI * 2)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(5, 8)
                            ctx.lineTo(7.2, 10.2)
                            ctx.lineTo(11.5, 5.6)
                            ctx.stroke()
                        }
                    }

                    Connections {
                        target: workflowButton
                        function onHoveredChanged() { workflowLeadingIcon.requestPaint() }
                        function onActiveChanged() { workflowLeadingIcon.requestPaint() }
                        function onIconTypeChanged() { workflowLeadingIcon.requestPaint() }
                    }
                }

                Text {
                    text: workflowButton.text
                    color: workflowButton.active ? "#ffffff" : root.textColor
                    font.pixelSize: root.joystickBodyFont
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }

                Canvas {
                    id: workflowNextIcon
                    width: root.width < 900 ? 15 : 17
                    height: width
                    visible: workflowButton.iconType === "next"
                    anchors.verticalCenter: parent.verticalCenter
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle = workflowButton.active ? "#ffffff" : root.mutedColor
                        ctx.lineWidth = 1.8
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"
                        ctx.beginPath()
                        ctx.moveTo(6, 3)
                        ctx.lineTo(11, 8)
                        ctx.lineTo(6, 13)
                        ctx.moveTo(2, 8)
                        ctx.lineTo(11, 8)
                        ctx.stroke()
                    }

                    Connections {
                        target: workflowButton
                        function onHoveredChanged() { workflowNextIcon.requestPaint() }
                        function onActiveChanged() { workflowNextIcon.requestPaint() }
                        function onIconTypeChanged() { workflowNextIcon.requestPaint() }
                    }
                }
            }
        }

        background: Rectangle {
            radius: 6
            color: workflowButton.active
                   ? (workflowButton.down ? "#0d7fd6" : workflowButton.hovered ? "#23b4ff" : "#129ef7")
                   : !workflowButton.enabled ? (root.darkTheme ? "#0a1724" : "#e6edf4")
                   : workflowButton.down ? root.cardHoverColor
                   : workflowButton.hovered ? root.cardHoverColor
                   : root.panelColor
            border.color: workflowButton.active ? "#31caff"
                         : workflowButton.hovered ? root.strongBorderColor
                         : root.borderColor
            border.width: 1
        }
    }

    component IconOnlyButton: Button {
        id: iconButton

        property string iconType: "refresh"
        property string toolTipText: ""

        padding: 0
        flat: true
        hoverEnabled: true
        ToolTip.visible: hovered && toolTipText.length > 0
        ToolTip.text: toolTipText

        contentItem: Item {
            Canvas {
                id: iconOnlyCanvas
                anchors.centerIn: parent
                width: 22
                height: 22
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = iconButton.hovered ? root.textColor : root.accentColor
                    ctx.lineWidth = 1.8
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"

                    if (iconButton.iconType === "refresh") {
                        ctx.beginPath()
                        ctx.arc(11, 11, 7, Math.PI * 0.15, Math.PI * 1.55)
                        ctx.moveTo(4, 11)
                        ctx.lineTo(1.8, 8.4)
                        ctx.moveTo(4, 11)
                        ctx.lineTo(6.8, 8.8)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.arc(11, 11, 7, Math.PI * 1.15, Math.PI * 2.55)
                        ctx.moveTo(18, 11)
                        ctx.lineTo(20.2, 13.6)
                        ctx.moveTo(18, 11)
                        ctx.lineTo(15.2, 13.2)
                        ctx.stroke()
                    }
                }

                Connections {
                    target: iconButton
                    function onHoveredChanged() { iconOnlyCanvas.requestPaint() }
                    function onDownChanged() { iconOnlyCanvas.requestPaint() }
                }
            }
        }

        background: Rectangle {
            radius: 6
            color: iconButton.down ? root.cardHoverColor
                  : iconButton.hovered ? root.cardHoverColor
                  : root.panelColor
            border.color: iconButton.hovered ? root.strongBorderColor : root.borderColor
            border.width: 1
        }
    }

    component JoystickStepProgress: Item {
        id: stepper

        property int currentStep: 1
        property var stepTitles: root.joystickStepTitles

        implicitHeight: root.width < 900 ? 52 : 58

        RowLayout {
            anchors.fill: parent
            spacing: root.joystickRowGap

            Repeater {
                model: stepper.stepTitles

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.joystickRowGap

                    Rectangle {
                        Layout.preferredWidth: root.width < 900 ? 34 : 40
                        Layout.preferredHeight: root.width < 900 ? 34 : 40
                        radius: width / 2
                        color: index + 1 < stepper.currentStep ? "#24c8ff"
                              : index + 1 === stepper.currentStep ? "#188ff7"
                              : root.darkTheme ? "#122c45" : "#dce9f7"
                        border.color: index + 1 <= stepper.currentStep ? "#34d4ff" : root.borderColor
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: index + 1 < stepper.currentStep ? "\u2713" : String(index + 1)
                            color: index + 1 <= stepper.currentStep ? "#ffffff" : root.mutedColor
                            font.pixelSize: root.width < 900 ? 14 : 16
                            font.bold: true
                        }
                    }

                    Text {
                        Layout.preferredWidth: root.width < 900 ? 74 : 88
                        text: modelData
                        color: index + 1 <= stepper.currentStep ? root.textColor : root.mutedColor
                        font.pixelSize: root.joystickBodyFont
                        font.bold: index + 1 === stepper.currentStep
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 2
                        visible: index < stepper.stepTitles.length - 1
                        color: index + 1 < stepper.currentStep ? root.accentColor : root.borderColor
                    }
                }
            }
        }
    }

    component DetailMetric: RowLayout {
        property string label: ""
        property string value: ""
        property bool success: false

        Layout.fillWidth: true
        spacing: root.joystickRowGap

        Text {
            Layout.preferredWidth: root.width < 900 ? 92 : 112
            text: label
            color: root.mutedColor
            font.pixelSize: root.joystickBodyFont
            font.bold: true
        }

        Rectangle {
            Layout.preferredWidth: root.width < 900 ? 18 : 20
            Layout.preferredHeight: root.width < 900 ? 18 : 20
            radius: width / 2
            visible: success
            color: root.successColor

            Text {
                anchors.centerIn: parent
                text: "\u2713"
                color: root.darkTheme ? root.pageColor : "#ffffff"
                font.pixelSize: 12
                font.bold: true
            }
        }

        Text {
            Layout.fillWidth: true
            text: value
            color: success ? root.successColor : root.textColor
            font.pixelSize: root.joystickBodyFont + 1
            font.bold: true
            elide: Text.ElideRight
        }
    }

    component GuidedAxisCalibrationPanel: SettingBlock {
        id: guidedPanel

        readonly property var axisControl: root.currentJoystickAxisControl()
        readonly property var axisRow: root.joystickAxisRow(axisControl.axis)
        readonly property var assignmentRow: root.joystickAssignmentRow(axisControl.axis)
        readonly property real normalizedValue: root.joystickAxisNormalized(axisRow)
        readonly property real outputNormalizedValue: root.joystickAxisOutputNormalized(axisRow, assignmentRow)
        readonly property int pwmValue: root.joystickAxisOutputPwm(axisRow, assignmentRow)
        readonly property bool axisReady: root.joystickAxisCalibrated(axisRow)
        readonly property string bindingText: assignmentRow && assignmentRow.binding && assignmentRow.binding.length > 0 ? assignmentRow.binding : "Not detected"

        title: "Step " + (root.joystickAxisCalibrationIndex + 1) + " of " + root.joystickAxisControls.length
        detail: axisControl.title

        ColumnLayout {
            Layout.fillWidth: true
            spacing: root.joystickMediumGap

            RowLayout {
                Layout.fillWidth: true
                spacing: root.joystickRowGap

                AxisGlyph {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    iconType: guidedPanel.axisControl.icon
                    color: guidedPanel.axisRow && guidedPanel.axisRow.accent ? guidedPanel.axisRow.accent : root.accentColor
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: guidedPanel.axisControl.title
                        color: root.textColor
                        font.pixelSize: root.width < 900 ? 18 : 21
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: guidedPanel.axisControl.instruction
                        color: root.mutedColor
                        font.pixelSize: root.joystickBodyFont
                        wrapMode: Text.WordWrap
                    }
                }
            }

            AxisCalibrationProgress {
                Layout.fillWidth: true
                axisName: guidedPanel.axisControl.axis
                accent: guidedPanel.axisRow && guidedPanel.axisRow.accent ? guidedPanel.axisRow.accent : root.accentColor
                normalizedValue: guidedPanel.outputNormalizedValue
                rawNormalizedValue: guidedPanel.normalizedValue
                pwmValue: guidedPanel.pwmValue
                inverted: guidedPanel.assignmentRow && guidedPanel.assignmentRow.inverted === true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                radius: 5
                color: guidedPanel.axisReady ? (root.darkTheme ? "#0c3b2e" : "#dff8eb") : root.cardColor
                border.color: guidedPanel.axisReady ? root.successColor : root.borderColor
                border.width: 1

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    text: guidedPanel.axisReady ? "Axis calibrated" : "Move this control through its full range"
                    color: guidedPanel.axisReady ? root.successColor : root.mutedColor
                    font.pixelSize: root.joystickBodyFont
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: root.borderColor
            }

            Text {
                Layout.fillWidth: true
                text: "Assigned Input"
                color: root.textColor
                font.pixelSize: root.joystickTitleFont
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: root.joystickMediumGap

                WorkflowButton {
                    Layout.preferredWidth: 128
                    text: assignmentRow && assignmentRow.detecting ? "Move Axis" : "Detect Input"
                    iconType: "none"
                    active: assignmentRow && assignmentRow.detecting
                    onClicked: backend.beginJoystickAxisDetection(guidedPanel.axisControl.axis)
                }

                SummaryChip {
                    Layout.fillWidth: true
                    highlighted: guidedPanel.bindingText !== "Not detected"
                    label: "Detected Input"
                    value: guidedPanel.bindingText
                }

                CheckBox {
                    id: guidedInvertCheck
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: root.joystickActionHeight
                    text: "Invert"
                    checked: assignmentRow ? assignmentRow.inverted : false
                    onToggled: backend.setJoystickAxisInverted(guidedPanel.axisControl.axis, checked)
                    padding: 0

                    indicator: Rectangle {
                        x: 0
                        y: Math.round((guidedInvertCheck.height - height) / 2)
                        width: 22
                        height: 22
                        radius: 4
                        color: guidedInvertCheck.checked ? root.accentColor : "#00000000"
                        border.color: guidedInvertCheck.checked ? root.accentColor : root.borderColor
                        border.width: 1
                    }

                    contentItem: Text {
                        text: "Invert PWM Output"
                        color: root.mutedColor
                        font.pixelSize: root.joystickBodyFont - 1
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 30
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }

    component ControllerGuidePanel: SettingBlock {
        id: guidePanel

        readonly property var axisControl: root.currentJoystickAxisControl()

        title: "Controller Guide"
        detail: "Configured sticks and live inputs."

        ColumnLayout {
            Layout.fillWidth: true
            spacing: root.joystickMediumGap

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.width < 1100 ? 155 : 190

                Image {
                    anchors.fill: parent
                    source: "qrc:/models/JoystickModel.png"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                ControllerMovementOverlay {
                    anchors.fill: parent
                    axisName: guidePanel.axisControl.axis
                    accent: guidePanel.axisControl.axis === "Roll" ? "#ff5b68"
                            : guidePanel.axisControl.axis === "Pitch" ? "#2397ff"
                            : guidePanel.axisControl.axis === "Throttle" ? "#49f08b"
                            : "#b963ef"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                radius: 5
                color: root.darkTheme ? "#073654" : "#dff4ff"
                border.color: root.accentColor
                border.width: 1

                Text {
                    anchors.fill: parent
                    anchors.margins: 10
                    text: guidePanel.axisControl.guide
                    color: root.textColor
                    font.pixelSize: root.joystickBodyFont - 1
                    font.bold: true
                    wrapMode: Text.WordWrap
                    verticalAlignment: Text.AlignVCenter
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: root.joystickRowGap
                rowSpacing: root.joystickRowGap

                Repeater {
                    model: root.joystickAxisControls

                    SummaryChip {
                        Layout.fillWidth: true
                        highlighted: root.joystickAxisCalibrated(root.joystickAxisRow(modelData.axis))
                        label: modelData.axis
                        value: root.joystickAssignmentRow(modelData.axis) && root.joystickAssignmentRow(modelData.axis).binding ? root.joystickAssignmentRow(modelData.axis).binding : "--"
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: "Live Inputs (Normalized)"
                color: root.textColor
                font.pixelSize: root.joystickTitleFont
                font.bold: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Repeater {
                    model: root.joystickAxisControls

                    LiveInputRow {
                        Layout.fillWidth: true
                        label: modelData.axis
                        value: root.joystickAxisNormalized(root.joystickAxisRow(modelData.axis))
                        accent: root.joystickAxisRow(modelData.axis) && root.joystickAxisRow(modelData.axis).accent ? root.joystickAxisRow(modelData.axis).accent : root.accentColor
                    }
                }
            }
        }
    }

    component LiveInputRow: RowLayout {
        id: liveRow

        property string label: ""
        property real value: 0
        property color accent: root.accentColor

        spacing: 8

        Text {
            Layout.preferredWidth: 70
            text: liveRow.label
            color: root.mutedColor
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 10
            radius: 2
            color: root.darkTheme ? "#17283b" : "#d9e7f2"
            clip: true

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                x: parent.width / 2
                width: Math.max(1, Math.abs(liveRow.value) * parent.width / 2)
                height: parent.height
                color: liveRow.accent
                transformOrigin: Item.Left
                scale: liveRow.value < 0 ? -1 : 1
            }

            Rectangle {
                width: 1
                height: parent.height
                anchors.horizontalCenter: parent.horizontalCenter
                color: root.textColor
                opacity: 0.45
            }
        }

        Text {
            Layout.preferredWidth: 42
            text: liveRow.value.toFixed(2)
            color: root.textColor
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
        }
    }

    component AxisCalibrationProgress: Item {
        id: axisProgress

        property string axisName: ""
        property color accent: root.accentColor
        property real normalizedValue: 0
        property real rawNormalizedValue: 0
        property int pwmValue: 1500
        property bool inverted: false

        implicitHeight: root.width < 900 ? 86 : 94

        Rectangle {
            id: axisProgressTrack
            anchors.left: parent.left
            anchors.right: pwmReadout.left
            anchors.rightMargin: root.joystickMediumGap
            anchors.top: parent.top
            anchors.topMargin: 8
            height: root.width < 900 ? 20 : 24
            radius: 3
            color: root.darkTheme ? "#10263a" : "#ddebf7"
            border.color: root.borderColor
            border.width: 1
            clip: true

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                x: parent.width / 2
                width: Math.max(2, Math.abs(axisProgress.normalizedValue) * parent.width / 2)
                height: parent.height
                color: axisProgress.accent
                opacity: 0.82
                transformOrigin: Item.Left
                scale: axisProgress.normalizedValue < 0 ? -1 : 1
            }

            Rectangle {
                width: 3
                height: parent.height
                anchors.horizontalCenter: parent.horizontalCenter
                color: root.textColor
                opacity: 0.58
            }
        }

        Rectangle {
            id: axisProgressThumb
            width: 4
            height: axisProgressTrack.height + 6
            radius: 1
            x: axisProgressTrack.x + axisProgressTrack.width / 2 + axisProgress.normalizedValue * axisProgressTrack.width / 2 - width / 2
            y: axisProgressTrack.y - 3
            color: "#ffffff"
            opacity: 0.9
        }

        Rectangle {
            id: pwmReadout
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 0
            width: root.width < 900 ? 72 : 82
            height: root.width < 900 ? 38 : 44
            radius: 5
            color: root.cardColor
            border.color: root.borderColor
            border.width: 1

            Column {
                anchors.centerIn: parent
                spacing: 1

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: axisProgress.pwmValue
                    color: root.textColor
                    font.pixelSize: root.joystickTitleFont
                    font.bold: true
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "PWM"
                    color: root.mutedColor
                    font.pixelSize: 9
                    font.bold: true
                }
            }
        }

        Text {
            anchors.left: axisProgressTrack.left
            anchors.top: axisProgressTrack.bottom
            anchors.topMargin: 6
            text: "-1.0"
            color: root.mutedColor
            font.pixelSize: 10
        }

        Text {
            anchors.horizontalCenter: axisProgressTrack.horizontalCenter
            anchors.top: axisProgressTrack.bottom
            anchors.topMargin: 6
            text: "0.0"
            color: root.mutedColor
            font.pixelSize: 10
        }

        Text {
            anchors.right: axisProgressTrack.right
            anchors.top: axisProgressTrack.bottom
            anchors.topMargin: 6
            text: "1.0"
            color: root.mutedColor
            font.pixelSize: 10
        }

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            text: axisProgress.axisName + " output " + axisProgress.normalizedValue.toFixed(2)
                  + (axisProgress.inverted ? " (inverted from raw " + axisProgress.rawNormalizedValue.toFixed(2) + ")" : "")
            color: axisProgress.inverted ? root.warningColor : root.mutedColor
            font.pixelSize: root.width < 900 ? 10 : 11
            elide: Text.ElideRight
        }
    }

    component ControllerMovementOverlay: Canvas {
        id: movementOverlay

        property string axisName: ""
        property color accent: root.accentColor

        opacity: 0.98
        onAxisNameChanged: requestPaint()
        onAccentChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            var imageWidth = Math.min(width, height * 1.9)
            var imageHeight = imageWidth / 1.9
            if (imageHeight > height) {
                imageHeight = height
                imageWidth = imageHeight * 1.9
            }
            var ox = (width - imageWidth) / 2
            var oy = (height - imageHeight) / 2

            var leftStickX = ox + imageWidth * 0.34
            var rightStickX = ox + imageWidth * 0.64
            var stickY = oy + imageHeight * 0.64
            var targetX = axisName === "Yaw" ? leftStickX : rightStickX
            var targetY = stickY

            if (axisName === "Throttle") {
                targetX = rightStickX
                targetY = oy + imageHeight * 0.55
            }

            ctx.strokeStyle = movementOverlay.accent
            ctx.fillStyle = movementOverlay.accent
            ctx.lineWidth = 4
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.shadowColor = movementOverlay.accent
            ctx.shadowBlur = 14

            ctx.beginPath()
            ctx.arc(targetX, targetY, imageWidth * 0.075, 0, Math.PI * 2)
            ctx.stroke()

            function arrow(fromX, fromY, toX, toY) {
                var angle = Math.atan2(toY - fromY, toX - fromX)
                ctx.beginPath()
                ctx.moveTo(fromX, fromY)
                ctx.lineTo(toX, toY)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(toX, toY)
                ctx.lineTo(toX - Math.cos(angle - 0.55) * 13, toY - Math.sin(angle - 0.55) * 13)
                ctx.lineTo(toX - Math.cos(angle + 0.55) * 13, toY - Math.sin(angle + 0.55) * 13)
                ctx.closePath()
                ctx.fill()
            }

            if (axisName === "Pitch" || axisName === "Throttle") {
                arrow(targetX, targetY - imageHeight * 0.02, targetX, targetY - imageHeight * 0.22)
                arrow(targetX, targetY + imageHeight * 0.02, targetX, targetY + imageHeight * 0.22)
            } else {
                arrow(targetX - imageWidth * 0.02, targetY, targetX - imageWidth * 0.17, targetY)
                arrow(targetX + imageWidth * 0.02, targetY, targetX + imageWidth * 0.17, targetY)
            }
        }
    }

    component AxisRangeRow: Item {
        id: axisRangeRow

        property string axisName: ""
        property string iconType: ""
        property color accent: root.accentColor
        property int minimumValue: 1000
        property int centerValue: 1500
        property int maximumValue: 2000
        property int currentValue: 1500
        readonly property real rangeSpan: Math.max(1, maximumValue - minimumValue)
        readonly property real progressValue: Math.max(0, Math.min(1, (currentValue - minimumValue) / rangeSpan))
        readonly property int pwmValue: root.joystickPwmValue(currentValue, minimumValue, maximumValue)

        implicitHeight: root.width < 900 ? 58 : 64

        RowLayout {
            anchors.fill: parent
            spacing: root.joystickRowGap

            AxisGlyph {
                Layout.preferredWidth: root.width < 900 ? 28 : 32
                Layout.preferredHeight: root.width < 900 ? 28 : 32
                iconType: axisRangeRow.iconType
                color: axisRangeRow.accent
            }

            Text {
                Layout.preferredWidth: root.joystickAxisNameWidth
                text: axisRangeRow.axisName
                color: root.textColor
                font.pixelSize: root.joystickBodyFont
                font.bold: true
                elide: Text.ElideRight
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: root.width < 900 ? 24 : 28
                radius: height / 2
                color: root.darkTheme ? "#10263a" : "#ddebf7"
                border.color: root.borderColor
                border.width: 1
                clip: true

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width * axisRangeRow.progressValue
                    radius: parent.radius
                    color: axisRangeRow.accent
                    opacity: 0.72
                }

                Rectangle {
                    width: 4
                    height: parent.height
                    anchors.verticalCenter: parent.verticalCenter
                    x: Math.max(0, Math.min(parent.width - width, parent.width * ((axisRangeRow.centerValue - axisRangeRow.minimumValue) / axisRangeRow.rangeSpan) - width / 2))
                    radius: 1
                    color: root.textColor
                    opacity: 0.38
                }

                Text {
                    anchors.fill: parent
                    text: "PWM " + axisRangeRow.pwmValue
                    color: "#ffffff"
                    font.pixelSize: root.width < 900 ? 11 : 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

        }
    }

    component TuningSlider: RowLayout {
        id: tuningSlider

        property string label: ""
        property string suffix: ""
        property int from: 0
        property int to: 100
        property int value: 0
        signal edited(int value)

        Layout.fillWidth: true
        spacing: root.joystickMediumGap

        Text {
            Layout.preferredWidth: root.width < 900 ? 96 : 116
            text: tuningSlider.label
            color: root.textColor
            font.pixelSize: root.joystickBodyFont
            font.bold: true
            elide: Text.ElideRight
        }

        Slider {
            id: slider
            Layout.fillWidth: true
            from: tuningSlider.from
            to: tuningSlider.to
            stepSize: 1
            value: tuningSlider.value
            onMoved: tuningSlider.edited(Math.round(value))

            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: slider.availableWidth
                height: root.width < 900 ? 7 : 8
                radius: height / 2
                color: root.darkTheme ? "#10263a" : "#dbe9f6"

                Rectangle {
                    width: slider.visualPosition * parent.width
                    height: parent.height
                    radius: 3
                    color: root.accentColor
                }
            }

            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: root.width < 900 ? 22 : 24
                height: root.width < 900 ? 22 : 24
                radius: width / 2
                color: root.textColor
                border.color: root.accentColor
                border.width: 2
            }
        }

        Rectangle {
            Layout.preferredWidth: root.width < 900 ? 64 : 70
            Layout.preferredHeight: root.width < 900 ? 34 : 38
            radius: 5
            color: root.cardColor
            border.color: root.borderColor
            border.width: 1

            Text {
                anchors.fill: parent
                text: tuningSlider.value + tuningSlider.suffix
                color: root.accentColor
                font.pixelSize: root.joystickBodyFont
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    component ButtonActionSelector: Control {
        id: actionSelector

        property int rowIndex: -1
        property string actionName: ""
        property bool pressed: false
        property bool flightModeHovered: false
        readonly property string displayName: root.normalizedButtonAction(actionName)

        Layout.fillWidth: true
        Layout.preferredHeight: root.joystickActionHeight
        padding: 0
        hoverEnabled: true

        contentItem: Item {
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.right: selectorGlyph.left
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: actionSelector.displayName
                color: root.textColor
                font.pixelSize: root.joystickBodyFont
                font.bold: true
                elide: Text.ElideRight
            }

            DropDownGlyph {
                id: selectorGlyph
                width: 14
                height: 14
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                color: root.mutedColor
            }
        }

        background: Rectangle {
            radius: 5
            color: actionSelector.hovered || actionSelector.pressed || actionPopup.opened ? root.cardHoverColor
                  : root.cardColor
            border.color: actionSelector.hovered || actionPopup.opened ? root.strongBorderColor : root.borderColor
            border.width: 1
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: actionPopup.open()
        }

        Popup {
            id: actionPopup
            x: 0
            y: actionSelector.height + 4
            padding: 0
            modal: false
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
            onClosed: actionSelector.flightModeHovered = false

            contentItem: Rectangle {
                implicitWidth: actionSelector.flightModeHovered
                               ? (root.width < 900 ? 360 : 428)
                               : (root.width < 900 ? 180 : 206)
                implicitHeight: actionColumn.implicitHeight + 12
                radius: 6
                color: root.panelColor
                border.color: root.borderColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 6

                    ColumnLayout {
                        id: actionColumn
                        Layout.preferredWidth: root.width < 900 ? 168 : 194
                        spacing: 2

                        Repeater {
                            model: root.joystickButtonActionOptions

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.width < 900 ? 30 : 34
                                radius: 4
                                color: optionMouse.containsMouse || actionSelector.displayName === modelData
                                       ? root.cardHoverColor
                                       : "transparent"

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    anchors.right: modeChevron.left
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData
                                    color: root.textColor
                                    font.pixelSize: root.width < 900 ? 11 : 12
                                    font.bold: actionSelector.displayName === modelData
                                    elide: Text.ElideRight
                                }

                                Text {
                                    id: modeChevron
                                    width: 14
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: ">"
                                    visible: modelData === "Flight Mode"
                                    color: root.mutedColor
                                    font.pixelSize: root.width < 900 ? 12 : 13
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                MouseArea {
                                    id: optionMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onEntered: actionSelector.flightModeHovered = modelData === "Flight Mode"
                                    onClicked: {
                                        backend.setJoystickButtonAction(actionSelector.rowIndex, modelData)
                                        actionPopup.close()
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.fillHeight: true
                        visible: actionSelector.flightModeHovered
                        color: root.borderColor
                    }

                    ColumnLayout {
                        Layout.preferredWidth: root.width < 900 ? 168 : 214
                        visible: actionSelector.flightModeHovered
                        spacing: 2

                        Repeater {
                            model: root.joystickFlightModeOptions

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.width < 900 ? 30 : 34
                                radius: 4
                                readonly property string flightModeAction: "Flight Mode: " + modelData.label
                                color: modeMouse.containsMouse || actionSelector.displayName === flightModeAction
                                       ? root.cardHoverColor
                                       : "transparent"

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    anchors.right: parent.right
                                    anchors.rightMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.label
                                    color: root.textColor
                                    font.pixelSize: root.width < 900 ? 11 : 12
                                    font.bold: actionSelector.displayName === flightModeAction
                                    elide: Text.ElideRight
                                }

                                MouseArea {
                                    id: modeMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onEntered: actionSelector.flightModeHovered = true
                                    onClicked: {
                                        backend.setJoystickButtonAction(actionSelector.rowIndex, flightModeAction)
                                        actionPopup.close()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component ButtonShortcutRow: Item {
        id: shortcutRow

        property string buttonName: ""
        property string actionName: ""
        property int rowIndex: -1
        property bool detecting: false
        property bool pressed: false
        property bool showDivider: false

        implicitHeight: root.width < 900 ? 64 : 68

        RowLayout {
            anchors.fill: parent
            spacing: root.joystickMediumGap

            Text {
                Layout.preferredWidth: root.width < 900 ? 96 : 112
                text: shortcutRow.buttonName
                color: root.textColor
                font.pixelSize: root.joystickBodyFont
                font.bold: true
                elide: Text.ElideRight
            }

            ButtonActionSelector {
                rowIndex: shortcutRow.rowIndex
                actionName: shortcutRow.actionName
                pressed: shortcutRow.pressed
            }

            WorkflowButton {
                Layout.preferredWidth: root.width < 900 ? 104 : 112
                Layout.preferredHeight: root.joystickActionHeight
                text: shortcutRow.detecting ? "Press" : "Detect"
                iconType: "none"
                active: shortcutRow.detecting || shortcutRow.pressed
                onClicked: backend.beginJoystickButtonDetection(shortcutRow.rowIndex)
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: root.borderColor
            visible: shortcutRow.showDivider
        }
    }

    component SummaryChip: Rectangle {
        id: summaryChip

        property string label: ""
        property string value: ""
        property bool highlighted: false

        Layout.preferredHeight: root.width < 900 ? 56 : 64
        radius: 6
        color: summaryChip.highlighted ? (root.darkTheme ? "#103728" : "#dff7eb") : root.cardColor
        border.color: summaryChip.highlighted ? root.successColor : root.borderColor
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.topMargin: 10
            anchors.bottomMargin: 10
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: summaryChip.label
                color: root.mutedColor
                font.pixelSize: root.width < 900 ? 10 : 11
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: summaryChip.value
                color: summaryChip.highlighted ? root.successColor : root.textColor
                font.pixelSize: root.joystickBodyFont
                font.bold: true
                elide: Text.ElideRight
            }
        }
    }

    component JoystickDevicePreview: Canvas {
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var cx = width / 2
            var baseY = height * 0.72
            ctx.strokeStyle = root.borderColor
            ctx.fillStyle = root.darkTheme ? "#172434" : "#dfeaf5"
            ctx.lineWidth = 2
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            ctx.beginPath()
            ctx.ellipse(cx, baseY, width * 0.34, height * 0.13, 0, 0, Math.PI * 2)
            ctx.fill()
            ctx.stroke()

            ctx.fillStyle = root.darkTheme ? "#0f1825" : "#f4f8fc"
            ctx.beginPath()
            ctx.moveTo(cx - width * 0.22, baseY)
            ctx.lineTo(cx - width * 0.13, baseY - height * 0.16)
            ctx.lineTo(cx + width * 0.18, baseY - height * 0.18)
            ctx.lineTo(cx + width * 0.29, baseY - height * 0.02)
            ctx.lineTo(cx + width * 0.18, baseY + height * 0.07)
            ctx.lineTo(cx - width * 0.20, baseY + height * 0.07)
            ctx.closePath()
            ctx.fill()
            ctx.stroke()

            ctx.fillStyle = root.darkTheme ? "#303946" : "#94a4b5"
            ctx.beginPath()
            ctx.ellipse(cx - width * 0.06, baseY - height * 0.33, width * 0.07, height * 0.29, -0.18, 0, Math.PI * 2)
            ctx.fill()
            ctx.stroke()

            ctx.fillStyle = root.accentColor
            ctx.beginPath()
            ctx.arc(cx - width * 0.05, baseY - height * 0.58, width * 0.055, 0, Math.PI * 2)
            ctx.fill()

            ctx.strokeStyle = root.accentColor
            ctx.beginPath()
            ctx.arc(cx + width * 0.16, baseY - height * 0.10, width * 0.05, 0, Math.PI * 2)
            ctx.arc(cx + width * 0.25, baseY - height * 0.08, width * 0.035, 0, Math.PI * 2)
            ctx.stroke()
        }
    }

    component StickRangeCard: Rectangle {
        id: stickCard

        property string title: ""
        property real xValue: 0.5
        property real yValue: 0.5
        onXValueChanged: stickCanvas.requestPaint()
        onYValueChanged: stickCanvas.requestPaint()

        Layout.minimumWidth: root.width < 900 ? 180 : 190
        implicitHeight: root.joystickCardHeight
        radius: 6
        color: root.cardColor
        border.color: root.borderColor
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.width < 900 ? 14 : 16
            spacing: root.joystickRowGap

            Text {
                Layout.fillWidth: true
                text: stickCard.title
                color: root.textColor
                font.pixelSize: root.joystickTitleFont
                font.bold: true
                elide: Text.ElideRight
            }

            Canvas {
                id: stickCanvas
                Layout.fillWidth: true
                Layout.fillHeight: true
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    var size = Math.min(width, height) - 8
                    var left = (width - size) / 2
                    var top = (height - size) / 2
                    var cx = left + size / 2
                    var cy = top + size / 2
                    ctx.strokeStyle = root.borderColor
                    ctx.lineWidth = 1.5
                    ctx.beginPath()
                    ctx.arc(cx, cy, size * 0.43, 0, Math.PI * 2)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(cx, top + size * 0.08)
                    ctx.lineTo(cx, top + size * 0.92)
                    ctx.moveTo(left + size * 0.08, cy)
                    ctx.lineTo(left + size * 0.92, cy)
                    ctx.stroke()
                    ctx.fillStyle = root.accentColor
                    ctx.beginPath()
                    ctx.arc(left + size * stickCard.xValue, top + size * stickCard.yValue, root.width < 900 ? 9 : 11, 0, Math.PI * 2)
                    ctx.fill()
                }
            }
        }
    }

    component TriggerRangeCard: Rectangle {
        id: triggerCard

        property string title: ""
        property real leftValue: 0.0
        property real rightValue: 0.0

        Layout.minimumWidth: root.width < 900 ? 180 : 190
        implicitHeight: root.joystickCardHeight
        radius: 6
        color: root.cardColor
        border.color: root.borderColor
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.width < 900 ? 14 : 16
            spacing: root.joystickRowGap

            Text {
                Layout.fillWidth: true
                text: triggerCard.title
                color: root.textColor
                font.pixelSize: root.joystickTitleFont
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: root.width < 900 ? 22 : 26

                TriggerColumn { Layout.fillWidth: true; label: "LT"; value: triggerCard.leftValue }
                TriggerColumn { Layout.fillWidth: true; label: "RT"; value: triggerCard.rightValue }
            }
        }
    }

    component TriggerColumn: Item {
        id: triggerColumn

        property string label: ""
        property real value: 0.5

        implicitWidth: root.width < 900 ? 54 : 58
        implicitHeight: root.width < 900 ? 154 : 170

        Rectangle {
            width: root.width < 900 ? 18 : 20
            height: parent.height - 32
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            radius: 6
            color: root.darkTheme ? "#17334e" : "#d5e8f7"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: parent.height * triggerColumn.value
                radius: 6
                color: root.accentColor
            }

            Rectangle {
                width: root.width < 900 ? 22 : 24
                height: root.width < 900 ? 22 : 24
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height * (1 - triggerColumn.value) - height / 2
                radius: 9
                color: "#ffffff"
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            text: triggerColumn.label
            color: root.textColor
            font.pixelSize: root.joystickBodyFont - 1
            font.bold: true
        }
    }

    component DpadRangeCard: Rectangle {
        id: dpadCard

        property string title: ""
        property int pov: -1

        Layout.minimumWidth: root.width < 900 ? 180 : 190
        implicitHeight: root.joystickCardHeight
        radius: 6
        color: root.cardColor
        border.color: root.borderColor
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.width < 900 ? 14 : 16
            spacing: root.joystickRowGap

            Text {
                Layout.fillWidth: true
                text: dpadCard.title
                color: root.textColor
                font.pixelSize: root.joystickTitleFont
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Repeater {
                    model: [
                        { label: "^", x: 0.50, y: 0.20, active: dpadCard.pov >= 31500 || (dpadCard.pov >= 0 && dpadCard.pov <= 4500) },
                        { label: "<", x: 0.28, y: 0.50, active: dpadCard.pov >= 22500 && dpadCard.pov <= 31500 },
                        { label: ">", x: 0.72, y: 0.50, active: dpadCard.pov >= 4500 && dpadCard.pov <= 13500 },
                        { label: "v", x: 0.50, y: 0.80, active: dpadCard.pov >= 13500 && dpadCard.pov <= 22500 }
                    ]

                    Rectangle {
                        width: root.width < 900 ? 46 : 54
                        height: root.width < 900 ? 46 : 54
                        radius: width / 2
                        x: parent.width * modelData.x - width / 2
                        y: parent.height * modelData.y - height / 2
                        color: modelData.active ? root.accentColor : root.darkTheme ? "#132b43" : "#dcecf8"
                        border.color: modelData.active ? "#34d4ff" : root.borderColor
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: modelData.active ? "#ffffff" : root.textColor
                            font.pixelSize: root.width < 900 ? 19 : 22
                            font.bold: true
                        }
                    }
                }

                Rectangle {
                    width: root.width < 900 ? 18 : 20
                    height: root.width < 900 ? 18 : 20
                    radius: width / 2
                    anchors.centerIn: parent
                    color: root.accentColor
                }
            }
        }
    }

    component CalibrationHint: Rectangle {
        property string text: ""

        implicitHeight: root.width < 900 ? 46 : 50
        radius: 6
        color: root.darkTheme ? "#09372f" : "#dcf7ec"
        border.color: root.successColor
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: root.joystickRowGap

            Rectangle {
                Layout.preferredWidth: root.width < 900 ? 20 : 24
                Layout.preferredHeight: root.width < 900 ? 20 : 24
                radius: width / 2
                color: root.successColor

                Text {
                    anchors.centerIn: parent
                    text: "\u2713"
                    color: root.darkTheme ? root.pageColor : "#ffffff"
                    font.pixelSize: 14
                    font.bold: true
                }
            }

            Text {
                Layout.fillWidth: true
                text: parent.parent.text
                color: root.successColor
                font.pixelSize: root.joystickBodyFont
                font.bold: true
                elide: Text.ElideRight
            }
        }
    }

    component AssignmentRow: Item {
        id: assignmentRow

        property string axisName: ""
        property string iconType: ""
        property color accent: root.accentColor
        property string binding: ""
        property bool inverted: false
        property bool detecting: false
        property bool showDivider: false

        implicitHeight: root.width < 900 ? 66 : 72

        RowLayout {
            anchors.fill: parent
            spacing: root.joystickMediumGap

            AxisGlyph {
                Layout.preferredWidth: root.width < 900 ? 34 : 42
                Layout.preferredHeight: root.width < 900 ? 34 : 42
                iconType: assignmentRow.iconType
                color: assignmentRow.accent
            }

            Text {
                Layout.preferredWidth: root.width < 900 ? 92 : 116
                text: assignmentRow.axisName
                color: root.textColor
                font.pixelSize: root.joystickTitleFont
                font.bold: true
                elide: Text.ElideRight
            }

            WorkflowButton {
                Layout.preferredWidth: root.width < 900 ? 104 : 124
                Layout.preferredHeight: root.joystickActionHeight
                text: assignmentRow.detecting ? "Move Axis" : "Detect"
                iconType: "none"
                active: assignmentRow.detecting
                onClicked: backend.beginJoystickAxisDetection(assignmentRow.axisName)
            }

            Rectangle {
                Layout.preferredWidth: root.width < 900 ? 20 : 24
                Layout.preferredHeight: root.width < 900 ? 20 : 24
                radius: width / 2
                color: root.successColor

                Text {
                    anchors.centerIn: parent
                    text: "\u2713"
                    color: root.darkTheme ? root.pageColor : "#ffffff"
                    font.pixelSize: 14
                    font.bold: true
                }
            }

            Text {
                Layout.fillWidth: true
                text: assignmentRow.binding
                color: root.textColor
                font.pixelSize: root.joystickBodyFont
                elide: Text.ElideRight
            }

            CheckBox {
                id: invertCheck
                Layout.preferredWidth: root.width < 900 ? 92 : 112
                Layout.preferredHeight: root.joystickActionHeight
                text: "Invert"
                checked: assignmentRow.inverted
                onToggled: backend.setJoystickAxisInverted(assignmentRow.axisName, checked)
                padding: 0

                indicator: Rectangle {
                    x: 0
                    y: Math.round((invertCheck.height - height) / 2)
                    width: root.width < 900 ? 20 : 24
                    height: root.width < 900 ? 20 : 24
                    radius: 4
                    color: invertCheck.checked ? root.accentColor : "#00000000"
                    border.color: invertCheck.checked ? root.accentColor : root.borderColor
                    border.width: 1
                }

                contentItem: Text {
                    text: invertCheck.text
                    color: root.mutedColor
                    font.pixelSize: root.joystickBodyFont
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: root.width < 900 ? 30 : 36
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: root.borderColor
            visible: assignmentRow.showDivider
        }
    }

    component SummaryAxis: Item {
        id: summaryAxis

        property string axisName: ""
        property string iconType: ""
        property color accent: root.accentColor
        property string binding: ""
        property bool showDivider: false

        implicitHeight: root.width < 900 ? 82 : 96

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: root.joystickRowGap

            AxisGlyph {
                Layout.preferredWidth: root.width < 900 ? 30 : 34
                Layout.preferredHeight: root.width < 900 ? 30 : 34
                iconType: summaryAxis.iconType
                color: summaryAxis.accent
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: summaryAxis.axisName
                    color: root.textColor
                    font.pixelSize: root.joystickBodyFont
                    font.bold: true
                    elide: Text.ElideRight
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.joystickRowGap

                    Rectangle {
                        Layout.preferredWidth: root.width < 900 ? 18 : 22
                        Layout.preferredHeight: root.width < 900 ? 18 : 22
                        radius: width / 2
                        color: root.successColor

                        Text {
                            anchors.centerIn: parent
                            text: "\u2713"
                            color: root.darkTheme ? root.pageColor : "#ffffff"
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: summaryAxis.binding
                        color: root.textColor
                        font.pixelSize: root.width < 900 ? 12 : 13
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: root.borderColor
            visible: summaryAxis.showDivider
        }
    }

    component RcChannelCalibrationRow: Rectangle {
        id: rcRow

        property int channel: 1
        property string label: ""
        property int value: -1
        property int minimum: 1100
        property int maximum: 1900
        property int trim: 1500
        property bool reversed: false
        property bool calibrationActive: false
        signal reverseChanged(int channel, bool reversed)

        readonly property int safeValue: value > 0 ? value : trim
        readonly property real valuePosition: Math.max(0, Math.min(1, (safeValue - 800) / 1400))
        readonly property real minimumPosition: Math.max(0, Math.min(1, (minimum - 800) / 1400))
        readonly property real maximumPosition: Math.max(0, Math.min(1, (maximum - 800) / 1400))
        readonly property real trimPosition: Math.max(0, Math.min(1, (trim - 800) / 1400))

        implicitHeight: calibrationActive ? (root.width < 900 ? 118 : 126) : (root.width < 900 ? 92 : 104)
        radius: 6
        color: root.cardColor
        border.color: calibrationActive ? root.strongBorderColor : root.borderColor
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    Layout.preferredWidth: 92
                    text: "CH" + rcRow.channel + "  " + rcRow.label
                    color: root.textColor
                    font.pixelSize: root.joystickBodyFont
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.preferredWidth: 58
                    text: rcRow.value > 0 ? rcRow.value + " us" : "--"
                    color: rcRow.value > 0 ? root.successColor : root.mutedColor
                    font.pixelSize: root.joystickBodyFont - 1
                    font.bold: true
                    horizontalAlignment: Text.AlignRight
                }

                CheckBox {
                    id: rcReverseCheck
                    Layout.preferredWidth: 98
                    Layout.preferredHeight: 26
                    visible: rcRow.channel <= 4
                    enabled: root.rcCalibrationStep === 3
                    checked: rcRow.reversed
                    onToggled: rcRow.reverseChanged(rcRow.channel, checked)
                    padding: 0

                    indicator: Rectangle {
                        x: 0
                        y: Math.round((rcReverseCheck.height - height) / 2)
                        width: 18
                        height: 18
                        radius: 4
                        color: rcReverseCheck.checked ? root.warningColor : "#00000000"
                        border.color: rcReverseCheck.checked ? root.warningColor : root.borderColor
                        border.width: 1
                    }

                    contentItem: Text {
                        text: "Reverse"
                        color: rcReverseCheck.checked ? root.warningColor : root.mutedColor
                        font.pixelSize: 11
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 26
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 28

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: 10
                    radius: 5
                    color: root.darkTheme ? "#071019" : "#e8f0f7"
                    border.color: root.borderColor
                    border.width: 1
                }

                Rectangle {
                    x: Math.round(parent.width * rcRow.valuePosition) - width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    width: 10
                    height: 22
                    radius: 4
                    color: rcRow.value > 0 ? root.successColor : root.mutedColor
                }

                Rectangle {
                    visible: rcRow.calibrationActive
                    x: Math.round(parent.width * rcRow.minimumPosition)
                    anchors.verticalCenter: parent.verticalCenter
                    width: 2
                    height: 28
                    color: root.dangerColor
                }

                Rectangle {
                    visible: rcRow.calibrationActive
                    x: Math.round(parent.width * rcRow.maximumPosition)
                    anchors.verticalCenter: parent.verticalCenter
                    width: 2
                    height: 28
                    color: root.dangerColor
                }

                Rectangle {
                    x: Math.round(parent.width * rcRow.trimPosition)
                    anchors.verticalCenter: parent.verticalCenter
                    width: 2
                    height: 20
                    color: root.accentColor
                }
            }

            RowLayout {
                visible: rcRow.calibrationActive
                Layout.fillWidth: true
                spacing: 8

                Text { Layout.fillWidth: true; text: "Min " + rcRow.minimum; color: root.dangerColor; font.pixelSize: 10; font.bold: true }
                Text { Layout.fillWidth: true; text: "Trim " + rcRow.trim; color: root.accentColor; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                Text { Layout.fillWidth: true; text: "Max " + rcRow.maximum; color: root.dangerColor; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignRight }
            }
        }
    }

    component AxisGlyph: Canvas {
        property string iconType: ""
        property color color: root.accentColor

        onIconTypeChanged: requestPaint()
        onColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = color
            ctx.fillStyle = color
            ctx.lineWidth = 3
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            var cx = width / 2
            var cy = height / 2

            if (iconType === "roll") {
                ctx.beginPath()
                ctx.moveTo(width * 0.14, cy)
                ctx.lineTo(width * 0.86, cy)
                ctx.moveTo(width * 0.14, cy)
                ctx.lineTo(width * 0.32, height * 0.32)
                ctx.moveTo(width * 0.14, cy)
                ctx.lineTo(width * 0.32, height * 0.68)
                ctx.moveTo(width * 0.86, cy)
                ctx.lineTo(width * 0.68, height * 0.32)
                ctx.moveTo(width * 0.86, cy)
                ctx.lineTo(width * 0.68, height * 0.68)
                ctx.stroke()
                return
            }

            if (iconType === "pitch") {
                ctx.beginPath()
                ctx.moveTo(cx, height * 0.14)
                ctx.lineTo(cx, height * 0.86)
                ctx.moveTo(cx, height * 0.14)
                ctx.lineTo(width * 0.32, height * 0.32)
                ctx.moveTo(cx, height * 0.14)
                ctx.lineTo(width * 0.68, height * 0.32)
                ctx.moveTo(cx, height * 0.86)
                ctx.lineTo(width * 0.32, height * 0.68)
                ctx.moveTo(cx, height * 0.86)
                ctx.lineTo(width * 0.68, height * 0.68)
                ctx.stroke()
                return
            }

            if (iconType === "throttle") {
                ctx.beginPath()
                ctx.moveTo(cx, height * 0.12)
                ctx.lineTo(width * 0.24, height * 0.40)
                ctx.moveTo(cx, height * 0.12)
                ctx.lineTo(width * 0.76, height * 0.40)
                ctx.moveTo(cx, height * 0.38)
                ctx.lineTo(width * 0.24, height * 0.66)
                ctx.moveTo(cx, height * 0.38)
                ctx.lineTo(width * 0.76, height * 0.66)
                ctx.moveTo(cx, height * 0.64)
                ctx.lineTo(width * 0.24, height * 0.92)
                ctx.moveTo(cx, height * 0.64)
                ctx.lineTo(width * 0.76, height * 0.92)
                ctx.stroke()
                return
            }

            if (iconType === "yaw") {
                ctx.beginPath()
                ctx.arc(cx, cy, width * 0.30, Math.PI * 0.25, Math.PI * 1.85)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(width * 0.74, height * 0.22)
                ctx.lineTo(width * 0.86, height * 0.42)
                ctx.lineTo(width * 0.62, height * 0.39)
                ctx.stroke()
            }
        }
    }

    component ChannelCheck: CheckBox {
        id: channelCheck

        property int channelNumber: 9

        width: 58
        height: 30
        padding: 0
        hoverEnabled: true

        indicator: Rectangle {
            x: 0
            y: 0
            width: channelCheck.width
            height: channelCheck.height
            radius: 5
            color: channelCheck.checked ? (root.darkTheme ? "#133d30" : "#dcf7ec")
                  : channelCheck.hovered ? root.cardHoverColor
                  : root.cardColor
            border.color: channelCheck.checked ? root.successColor
                         : channelCheck.hovered ? root.strongBorderColor
                         : root.borderColor
            border.width: 1
        }

        contentItem: Text {
            text: "S" + channelCheck.channelNumber
            color: channelCheck.checked ? root.successColor : root.mutedColor
            font.pixelSize: 12
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component PwmEditor: RowLayout {
        id: pwmEditor

        property string label: ""
        property int value: 1500
        signal edited(int value)

        Layout.fillWidth: true
        spacing: 10

        Text {
            Layout.preferredWidth: 48
            text: pwmEditor.label
            color: root.textColor
            font.pixelSize: 12
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }

        SpinBox {
            id: pwmSpinBox
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            from: 800
            to: 2200
            stepSize: 25
            editable: true
            value: pwmEditor.value
            onValueModified: pwmEditor.edited(value)

            contentItem: TextInput {
                text: pwmSpinBox.textFromValue(pwmSpinBox.value, pwmSpinBox.locale)
                color: root.textColor
                selectionColor: root.accentColor
                selectedTextColor: root.darkTheme ? root.pageColor : "#ffffff"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                readOnly: !pwmSpinBox.editable
                validator: pwmSpinBox.validator
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                font.pixelSize: 12
            }

            background: Rectangle {
                radius: 5
                color: root.cardColor
                border.color: pwmSpinBox.activeFocus ? root.strongBorderColor : root.borderColor
                border.width: 1
            }
        }

        Text {
            Layout.preferredWidth: 18
            text: "us"
            color: root.mutedColor
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
        }
    }

    component SearchGlyph: Canvas {
        property color color: "#ffffff"
        onColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = color
            ctx.lineWidth = 1.6
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.arc(width * 0.43, height * 0.42, width * 0.25, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(width * 0.62, height * 0.62)
            ctx.lineTo(width * 0.82, height * 0.82)
            ctx.stroke()
        }
    }

    component ChevronGlyph: Canvas {
        property color color: "#ffffff"
        onColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = color
            ctx.lineWidth = 1.6
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(width * 0.38, height * 0.24)
            ctx.lineTo(width * 0.64, height * 0.50)
            ctx.lineTo(width * 0.38, height * 0.76)
            ctx.stroke()
        }
    }

    component DropDownGlyph: Canvas {
        property color color: "#ffffff"
        onColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = color
            ctx.lineWidth = 2
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.beginPath()
            ctx.moveTo(width * 0.24, height * 0.36)
            ctx.lineTo(width * 0.50, height * 0.64)
            ctx.lineTo(width * 0.76, height * 0.36)
            ctx.stroke()
        }
    }

    component OemIcon: Canvas {
        id: iconCanvas

        property string iconType: ""
        property color color: "#ffffff"

        onIconTypeChanged: requestPaint()
        onColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            var cx = w / 2
            var cy = h / 2
            ctx.reset()
            ctx.strokeStyle = color
            ctx.fillStyle = color
            ctx.lineWidth = 2.6
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (iconType === "plane") {
                ctx.beginPath()
                ctx.moveTo(cx, h * 0.10)
                ctx.lineTo(w * 0.62, h * 0.88)
                ctx.lineTo(cx, h * 0.72)
                ctx.lineTo(w * 0.38, h * 0.88)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(w * 0.18, h * 0.48)
                ctx.lineTo(w * 0.82, h * 0.48)
                ctx.stroke()
                return
            }

            if (iconType === "telemetry") {
                ctx.beginPath(); ctx.arc(cx, cy, w * 0.08, 0, Math.PI * 2); ctx.fill()
                for (var r = 0; r < 3; ++r) {
                    var radius = w * (0.18 + r * 0.13)
                    ctx.beginPath(); ctx.arc(cx, cy, radius, -Math.PI * 0.38, Math.PI * 0.38); ctx.stroke()
                    ctx.beginPath(); ctx.arc(cx, cy, radius, Math.PI * 0.62, Math.PI * 1.38); ctx.stroke()
                }
                ctx.beginPath(); ctx.moveTo(cx, cy + 4); ctx.lineTo(cx, h * 0.86); ctx.stroke()
                return
            }

            if (iconType === "camera") {
                ctx.strokeRect(w * 0.18, h * 0.32, w * 0.64, h * 0.42)
                ctx.strokeRect(w * 0.30, h * 0.22, w * 0.18, h * 0.10)
                ctx.beginPath(); ctx.arc(cx, h * 0.53, w * 0.13, 0, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.arc(w * 0.70, h * 0.38, w * 0.03, 0, Math.PI * 2); ctx.fill()
                return
            }

            if (iconType === "servo") {
                ctx.beginPath(); ctx.arc(cx, cy, w * 0.18, 0, Math.PI * 2); ctx.stroke()
                for (var i = 0; i < 8; ++i) {
                    var a = i * Math.PI / 4
                    ctx.beginPath()
                    ctx.moveTo(cx + Math.cos(a) * w * 0.27, cy + Math.sin(a) * h * 0.27)
                    ctx.lineTo(cx + Math.cos(a) * w * 0.39, cy + Math.sin(a) * h * 0.39)
                    ctx.stroke()
                }
                return
            }

            if (iconType === "joystick") {
                ctx.beginPath(); ctx.arc(w * 0.35, h * 0.62, w * 0.08, 0, Math.PI * 2); ctx.fill()
                ctx.beginPath(); ctx.arc(w * 0.65, h * 0.62, w * 0.08, 0, Math.PI * 2); ctx.fill()
                ctx.beginPath()
                ctx.moveTo(w * 0.20, h * 0.70)
                ctx.quadraticCurveTo(w * 0.27, h * 0.35, cx, h * 0.42)
                ctx.quadraticCurveTo(w * 0.73, h * 0.35, w * 0.80, h * 0.70)
                ctx.stroke()
                ctx.beginPath(); ctx.moveTo(w * 0.44, h * 0.45); ctx.lineTo(w * 0.32, h * 0.45); ctx.moveTo(w * 0.38, h * 0.39); ctx.lineTo(w * 0.38, h * 0.51); ctx.stroke()
                return
            }

            if (iconType === "rc") {
                ctx.strokeRect(w * 0.20, h * 0.28, w * 0.60, h * 0.48)
                ctx.beginPath(); ctx.arc(w * 0.36, h * 0.58, w * 0.09, 0, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.arc(w * 0.64, h * 0.58, w * 0.09, 0, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(cx, h * 0.28); ctx.lineTo(cx, h * 0.12); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(w * 0.31, h * 0.40); ctx.lineTo(w * 0.43, h * 0.40); ctx.moveTo(w * 0.37, h * 0.34); ctx.lineTo(w * 0.37, h * 0.46); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(w * 0.58, h * 0.40); ctx.lineTo(w * 0.70, h * 0.40); ctx.stroke()
                return
            }

            if (iconType === "gcs") {
                ctx.strokeRect(w * 0.16, h * 0.24, w * 0.68, h * 0.44)
                ctx.beginPath()
                ctx.moveTo(cx, h * 0.68)
                ctx.lineTo(cx, h * 0.82)
                ctx.moveTo(w * 0.35, h * 0.82)
                ctx.lineTo(w * 0.65, h * 0.82)
                ctx.stroke()
                return
            }

            if (iconType === "network") {
                ctx.strokeRect(w * 0.42, h * 0.14, w * 0.16, h * 0.16)
                ctx.strokeRect(w * 0.18, h * 0.68, w * 0.16, h * 0.16)
                ctx.strokeRect(w * 0.42, h * 0.68, w * 0.16, h * 0.16)
                ctx.strokeRect(w * 0.66, h * 0.68, w * 0.16, h * 0.16)
                ctx.beginPath()
                ctx.moveTo(cx, h * 0.30)
                ctx.lineTo(cx, h * 0.55)
                ctx.moveTo(w * 0.26, h * 0.68)
                ctx.lineTo(w * 0.26, h * 0.55)
                ctx.lineTo(w * 0.74, h * 0.55)
                ctx.lineTo(w * 0.74, h * 0.68)
                ctx.moveTo(cx, h * 0.55)
                ctx.lineTo(cx, h * 0.68)
                ctx.stroke()
                return
            }

            if (iconType === "modules") {
                ctx.beginPath()
                ctx.moveTo(w * 0.32, h * 0.18); ctx.lineTo(w * 0.32, h * 0.82)
                ctx.moveTo(w * 0.68, h * 0.18); ctx.lineTo(w * 0.68, h * 0.82)
                ctx.stroke()
                ctx.beginPath(); ctx.arc(w * 0.32, h * 0.62, w * 0.09, 0, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.arc(w * 0.68, h * 0.40, w * 0.09, 0, Math.PI * 2); ctx.stroke()
                return
            }

            if (iconType === "safety") {
                ctx.beginPath()
                ctx.moveTo(cx, h * 0.12)
                ctx.lineTo(w * 0.78, h * 0.26)
                ctx.lineTo(w * 0.72, h * 0.62)
                ctx.quadraticCurveTo(cx, h * 0.84, w * 0.28, h * 0.62)
                ctx.lineTo(w * 0.22, h * 0.26)
                ctx.closePath()
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(w * 0.36, h * 0.50)
                ctx.lineTo(w * 0.47, h * 0.60)
                ctx.lineTo(w * 0.66, h * 0.38)
                ctx.stroke()
                return
            }

            if (iconType === "users") {
                ctx.beginPath(); ctx.arc(cx, h * 0.32, w * 0.13, 0, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.arc(w * 0.26, h * 0.42, w * 0.10, 0, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.arc(w * 0.74, h * 0.42, w * 0.10, 0, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.arc(cx, h * 0.84, w * 0.28, Math.PI, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.arc(w * 0.26, h * 0.84, w * 0.19, Math.PI, Math.PI * 1.72); ctx.stroke()
                ctx.beginPath(); ctx.arc(w * 0.74, h * 0.84, w * 0.19, Math.PI * 1.28, Math.PI * 2); ctx.stroke()
                return
            }

            if (iconType === "cloud") {
                ctx.beginPath()
                ctx.moveTo(w * 0.22, h * 0.64)
                ctx.quadraticCurveTo(w * 0.14, h * 0.48, w * 0.32, h * 0.46)
                ctx.quadraticCurveTo(w * 0.38, h * 0.20, w * 0.62, h * 0.34)
                ctx.quadraticCurveTo(w * 0.84, h * 0.34, w * 0.82, h * 0.60)
                ctx.quadraticCurveTo(w * 0.80, h * 0.74, w * 0.62, h * 0.74)
                ctx.lineTo(w * 0.28, h * 0.74)
                ctx.quadraticCurveTo(w * 0.14, h * 0.74, w * 0.22, h * 0.64)
                ctx.stroke()
                return
            }

            if (iconType === "about") {
                ctx.beginPath(); ctx.arc(cx, cy, w * 0.34, 0, Math.PI * 2); ctx.fill()
                ctx.fillStyle = root.darkTheme ? root.pageColor : "#ffffff"
                ctx.font = "bold " + Math.round(w * 0.50) + "px sans-serif"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                ctx.fillText("i", cx, cy + h * 0.02)
            }
        }
    }
}
