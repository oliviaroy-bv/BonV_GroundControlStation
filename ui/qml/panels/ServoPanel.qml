pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.settings

Item {
    id: root

    property var mavlinkObject: null
    property bool opened: false
    property bool darkTheme: true
    property int topOffset: 60
    property int rightInset: 300
    property real anchorLeftX: anchorX
    property real anchorX: 74
    property real anchorY: topOffset + 8
    property int anchorGap: 12
    property int selectedServo: 11
    property string selectedCommand: ""
    property int selectedPwm: 0
    property int pendingServo: 0
    property int pendingPwm: 0
    property string commandState: "idle"
    property string lastCommandText: "No command selected"
    property string lastCommandTimeText: "--:--:--"
    readonly property bool vehicleConnected: mavlinkObject ? mavlinkObject.connected : false
    readonly property color panelColor: darkTheme ? "#ee09111b" : "#f8fbff"
    readonly property color cardColor: darkTheme ? "#111d2b" : "#ffffff"
    readonly property color borderColor: darkTheme ? "#25364a" : "#cfdceb"
    readonly property color textColor: darkTheme ? "#f4f8ff" : "#0f213b"
    readonly property color labelColor: darkTheme ? "#9fb5d7" : "#526680"
    readonly property color mutedColor: darkTheme ? "#7f91aa" : "#6f7f95"
    readonly property color accentColor: darkTheme ? "#00c7df" : "#008fb8"
    readonly property var servos: root.selectedServoLabels()

    Settings {
        id: servoSettings
        category: "ServoPanel"
        property string selectedChannelsJson: "[9,10,11,12,13,14]"
        property int lowPwm: 1000
        property int midPwm: 1500
        property int highPwm: 2000
    }

    anchors.fill: parent
    visible: opened
    opacity: opened ? 1 : 0
    z: 130

    Behavior on opacity { NumberAnimation { duration: 140 } }

    function open() {
        if (!vehicleConnected)
            return
        opened = true
    }

    function close() {
        opened = false
    }

    function toggle() {
        opened ? close() : open()
    }

    function servoIndex() {
        var index = servos.indexOf("S" + selectedServo)
        return Math.max(0, index)
    }

    function selectedServoNumbers() {
        try {
            var parsedChannels = JSON.parse(servoSettings.selectedChannelsJson)
            if (Array.isArray(parsedChannels)) {
                var selected = parsedChannels.filter(function(channel) {
                    return channel >= 9 && channel <= 14
                })
                if (selected.length > 0)
                    return selected
            }
        } catch (error) {
        }
        return [ 9, 10, 11, 12, 13, 14 ]
    }

    function selectedServoLabels() {
        return selectedServoNumbers().map(function(channel) {
            return "S" + channel
        })
    }

    function applyServoSettings(channelsJson, lowPwm, midPwm, highPwm) {
        servoSettings.selectedChannelsJson = channelsJson
        servoSettings.lowPwm = safePwm(lowPwm)
        servoSettings.midPwm = safePwm(midPwm)
        servoSettings.highPwm = safePwm(highPwm)
    }

    function safePwm(pwm) {
        var numericPwm = Number(pwm)
        if (!isFinite(numericPwm))
            return 1500
        return Math.max(800, Math.min(2200, numericPwm))
    }

    function commandPwm(command) {
        if (command === "LOW")
            return safePwm(servoSettings.lowPwm)
        if (command === "HIGH")
            return safePwm(servoSettings.highPwm)
        return safePwm(servoSettings.midPwm)
    }

    function commandFromPwm(pwm) {
        var lowDelta = Math.abs(pwm - servoSettings.lowPwm)
        var midDelta = Math.abs(pwm - servoSettings.midPwm)
        var highDelta = Math.abs(pwm - servoSettings.highPwm)
        if (lowDelta <= midDelta && lowDelta <= highDelta)
            return "LOW"
        if (highDelta <= midDelta && highDelta <= lowDelta)
            return "HIGH"
        return "MID"
    }

    function updateLastCommand(servoNumber, command, pwm, suffix) {
        var now = new Date()
        lastCommandTimeText = Qt.formatTime(now, "HH:mm:ss")
        lastCommandText = "S" + servoNumber + " -> " + command + " (" + pwm + " us)" + (suffix.length > 0 ? " " + suffix : "")
    }

    function markCommandRejected(servoNumber, pwm, reason) {
        commandState = "rejected"
        if (selectedServo === servoNumber && selectedPwm === pwm)
            selectedCommand = ""
        updateLastCommand(servoNumber, commandFromPwm(pwm), pwm, reason && reason.length > 0 ? reason : "rejected")
    }

    function sendCommand(command) {
        if (!vehicleConnected || !mavlinkObject) {
            commandState = "rejected"
            lastCommandText = "Connect vehicle before sending servo command"
            lastCommandTimeText = Qt.formatTime(new Date(), "HH:mm:ss")
            return
        }

        var pwm = commandPwm(command)
        selectedCommand = command
        selectedPwm = pwm
        pendingServo = selectedServo
        pendingPwm = pwm
        commandState = "pending"
        updateLastCommand(selectedServo, command, pwm, "sent")

        if (!mavlinkObject.servo(selectedServo, pwm) && commandState === "pending")
            markCommandRejected(selectedServo, pwm, "send failed")
    }

    function clearPendingCommand() {
        selectedCommand = ""
        selectedPwm = 0
        commandState = "idle"
    }

    function panelX(panelWidth) {
        var rightX = root.anchorX + root.anchorGap
        var maxRightX = root.width - root.rightInset - panelWidth - 8
        if (rightX <= maxRightX)
            return Math.max(8, rightX)

        return Math.max(8, root.anchorLeftX - root.anchorGap - panelWidth)
    }

    function panelY() {
        return Math.max(root.topOffset + 8, root.anchorY)
    }

    onVehicleConnectedChanged: {
        if (!vehicleConnected) {
            clearPendingCommand()
            close()
        }
    }

    onServosChanged: {
        if (servos.indexOf("S" + selectedServo) === -1) {
            selectedServo = Number(servos[0].substring(1))
            clearPendingCommand()
        }
    }

    Connections {
        target: root.mavlinkObject
        ignoreUnknownSignals: true

        function onServoCommandAccepted(servoNumber, pwm) {
            var command = root.commandFromPwm(pwm)
            if (root.selectedServo === servoNumber && root.selectedPwm === pwm) {
                root.selectedCommand = command
                root.commandState = "accepted"
            }
            root.updateLastCommand(servoNumber, command, pwm, "accepted")
        }

        function onServoCommandRejected(servoNumber, pwm, reason) {
            root.markCommandRejected(servoNumber, pwm, reason)
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.close()
    }

    Rectangle {
        id: panel

        width: Math.min(610, Math.max(420, root.width - root.rightInset - 120))
        height: 58
        x: root.panelX(width)
        y: root.opened ? root.panelY() : root.panelY() - 18
        radius: 6
        color: root.panelColor
        border.color: root.accentColor
        border.width: 1
        clip: true

        Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on y { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        MouseArea {
            anchors.fill: parent
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            spacing: 7

            ComboBox {
                id: servoCombo

                Layout.preferredWidth: 96
                Layout.fillHeight: true
                model: root.servos
                currentIndex: root.servoIndex()
                onActivated: function(index) {
                    root.selectedServo = Number(root.servos[index].substring(1))
                    root.clearPendingCommand()
                }

                indicator: Canvas {
                    x: servoCombo.width - width - 9
                    y: Math.round((servoCombo.height - height) / 2)
                    width: 10
                    height: 7
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle = root.textColor
                        ctx.lineWidth = 1.5
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"
                        ctx.beginPath()
                        ctx.moveTo(2, 2)
                        ctx.lineTo(5, 5.5)
                        ctx.lineTo(8, 2)
                        ctx.stroke()
                    }
                }

                contentItem: Item {
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 21
                        spacing: 6

                        ServoGlyph {
                            Layout.preferredWidth: 17
                            Layout.preferredHeight: 17
                            iconColor: root.accentColor
                        }

                        Text {
                            Layout.fillWidth: true
                            text: servoCombo.displayText
                            color: root.textColor
                            font.pixelSize: 12
                            font.bold: true
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }
                }

                background: Rectangle {
                    radius: 5
                    color: servoCombo.down ? "#1b3146" : root.cardColor
                    border.color: servoCombo.hovered || servoCombo.down ? root.accentColor : root.borderColor
                    border.width: 1
                }

                popup: Popup {
                    y: servoCombo.height + 4
                    width: servoCombo.width
                    implicitHeight: contentItem.implicitHeight + 2
                    padding: 1

                    background: Rectangle {
                        radius: 4
                        color: root.darkTheme ? "#f20a111b" : "#ffffff"
                        border.color: root.accentColor
                        border.width: 1
                    }

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: servoCombo.popup.visible ? servoCombo.delegateModel : null
                        currentIndex: servoCombo.highlightedIndex
                    }
                }

                delegate: ItemDelegate {
                    id: servoDelegate

                    required property int index
                    required property string modelData

                    width: servoCombo.width - 2
                    height: 28
                    highlighted: servoCombo.highlightedIndex === servoDelegate.index

                    contentItem: Text {
                        text: servoDelegate.modelData
                        color: servoDelegate.highlighted ? root.accentColor : root.textColor
                        font.pixelSize: 11
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 9
                    }

                    background: Rectangle {
                        color: servoDelegate.highlighted ? (root.darkTheme ? "#122536" : "#e4f6fb") : "#00000000"
                    }
                }
            }

            ServoCommandButton {
                Layout.preferredWidth: 82
                Layout.fillHeight: true
                command: "LOW"
                pwmText: root.commandPwm(command) + " us"
                arrow: "down"
                enabled: root.vehicleConnected
                active: root.selectedCommand === command
                onClicked: root.sendCommand(command)
            }

            ServoCommandButton {
                Layout.preferredWidth: 82
                Layout.fillHeight: true
                command: "MID"
                pwmText: root.commandPwm(command) + " us"
                arrow: "mid"
                enabled: root.vehicleConnected
                active: root.selectedCommand === command
                onClicked: root.sendCommand(command)
            }

            ServoCommandButton {
                Layout.preferredWidth: 82
                Layout.fillHeight: true
                command: "HIGH"
                pwmText: root.commandPwm(command) + " us"
                arrow: "up"
                enabled: root.vehicleConnected
                active: root.selectedCommand === command
                onClicked: root.sendCommand(command)
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: root.borderColor
            }

            Rectangle {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                Layout.alignment: Qt.AlignVCenter
                radius: 8
                color: root.commandState === "rejected" ? "#ff4b46"
                       : root.commandState === "pending" ? "#ffbf3d"
                       : root.commandState === "accepted" ? "#34ee62"
                       : root.borderColor

                Text {
                    anchors.centerIn: parent
                    text: root.commandState === "rejected" ? "!"
                          : root.commandState === "pending" ? "..."
                          : root.commandState === "accepted" ? "\u2713"
                          : "-"
                    color: root.commandState === "pending" ? "#2d2100"
                           : root.commandState === "idle" ? root.labelColor
                           : "#07350f"
                    font.pixelSize: 9
                    font.bold: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 0

                Text {
                    Layout.fillWidth: true
                    text: "Last Command"
                    color: root.labelColor
                    font.pixelSize: 8
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: root.lastCommandText
                    color: root.accentColor
                    font.pixelSize: 10
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: root.lastCommandTimeText
                    color: root.labelColor
                    font.pixelSize: 9
                    elide: Text.ElideRight
                }
            }

            Button {
                id: closeButton

                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
                padding: 0
                flat: true
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.text: "Close"
                onClicked: root.close()

                contentItem: Canvas {
                    id: closeIcon

                    anchors.centerIn: parent
                    width: 12
                    height: 12
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle = closeButton.hovered ? root.textColor : root.labelColor
                        ctx.lineWidth = 1.6
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"
                        ctx.beginPath()
                        ctx.moveTo(3, 8)
                        ctx.lineTo(6, 5)
                        ctx.lineTo(9, 8)
                        ctx.stroke()
                    }

                    Connections {
                        target: closeButton
                        function onHoveredChanged() { closeIcon.requestPaint() }
                        function onDownChanged() { closeIcon.requestPaint() }
                    }
                }

                background: Rectangle {
                    radius: 3
                    color: closeButton.down ? "#1a2738" : closeButton.hovered ? "#121d2b" : "#00000000"
                }
            }
        }
    }

    component ServoCommandButton: Button {
        id: commandButton

        property string command: ""
        property string pwmText: ""
        property string arrow: "mid"
        property bool active: false

        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 5

            Canvas {
                id: commandIcon

                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                Layout.alignment: Qt.AlignVCenter
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = commandButton.active || commandButton.hovered ? "#cfffff" : root.accentColor
                    ctx.lineWidth = 1.7
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    if (commandButton.arrow === "down") {
                        ctx.moveTo(8, 2)
                        ctx.lineTo(8, 14)
                        ctx.moveTo(4, 10)
                        ctx.lineTo(8, 14)
                        ctx.lineTo(12, 10)
                    } else if (commandButton.arrow === "up") {
                        ctx.moveTo(8, 14)
                        ctx.lineTo(8, 2)
                        ctx.moveTo(4, 6)
                        ctx.lineTo(8, 2)
                        ctx.lineTo(12, 6)
                    } else {
                        ctx.arc(8, 8, 4.5, 0, Math.PI * 2)
                    }
                    ctx.stroke()
                }

                Connections {
                    target: commandButton
                    function onHoveredChanged() { commandIcon.requestPaint() }
                    function onActiveChanged() { commandIcon.requestPaint() }
                    function onDownChanged() { commandIcon.requestPaint() }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 1

                Text {
                    Layout.fillWidth: true
                    text: commandButton.command
                    color: commandButton.active || commandButton.hovered ? "#ffffff" : root.textColor
                    font.pixelSize: 10
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: commandButton.pwmText
                    color: commandButton.active || commandButton.hovered ? "#d7fbff" : root.labelColor
                    font.pixelSize: 8
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
            }
        }

        background: Rectangle {
            radius: 6
            color: commandButton.active ? "#178fe8"
                  : commandButton.down ? "#193149"
                  : commandButton.hovered ? "#13283c"
                  : root.cardColor
            border.color: commandButton.active ? "#3fe0ff"
                         : commandButton.hovered ? root.accentColor
                         : root.borderColor
            border.width: 1

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                visible: commandButton.active
                color: "#9ff5ff"
                opacity: 0.7
            }
        }
    }

    component ServoGlyph: Canvas {
        property color iconColor: root.accentColor

        onIconColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = iconColor
            ctx.fillStyle = iconColor
            ctx.lineWidth = 1.8
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.strokeRect(width * 0.18, height * 0.40, width * 0.64, height * 0.32)
            ctx.beginPath()
            ctx.moveTo(width * 0.50, height * 0.40)
            ctx.lineTo(width * 0.50, height * 0.18)
            ctx.lineTo(width * 0.66, height * 0.18)
            ctx.moveTo(width * 0.33, height * 0.72)
            ctx.lineTo(width * 0.24, height * 0.88)
            ctx.moveTo(width * 0.67, height * 0.72)
            ctx.lineTo(width * 0.76, height * 0.88)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(width * 0.66, height * 0.18, width * 0.06, 0, Math.PI * 2)
            ctx.fill()
        }
    }
}
