import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var mavlinkObject: null
    property var vehicle: null
    property bool opened: false
    property int motorCount: vehicle ? Math.max(1, vehicle.motorCount) : 4
    property int frameClass: 1
    property int frameType: 1
    property bool frameConfigurationAvailable: false
    property int throttlePercent: 10
    property int runtimeSeconds: 2
    property bool propellersRemoved: false
    property string motorTestStatus: "Ready"
    property string pendingMotorTestMode: ""
    property int pendingFirstMotor: 1
    property int pendingMotorCount: 1
    property int pendingDurationSeconds: 0
    property bool motorTestStartedByController: false

    anchors.fill: parent
    visible: opened
    z: 140

    function open() {
        refreshFrameConfiguration()
        opened = true
    }

    function parameterNumber(name, fallbackValue) {
        if (!mavlinkObject || !mavlinkObject.parameterValue)
            return fallbackValue

        var parameter = mavlinkObject.parameterValue(name)
        return parameter && parameter.value !== undefined
                ? Math.round(Number(parameter.value))
                : fallbackValue
    }

    function refreshFrameConfiguration() {
        if (!mavlinkObject || !mavlinkObject.connected)
            return

        var frameClassParameter = mavlinkObject.parameterValue("FRAME_CLASS")
        if (!frameClassParameter || frameClassParameter.value === undefined)
            return

        frameClass = parameterNumber("FRAME_CLASS", 1)
        frameType = parameterNumber("FRAME_TYPE", 1)
        frameConfigurationAvailable = true
    }

    function frameClassName() {
        var names = {
            1: "QUAD", 2: "HEXA", 3: "OCTA", 4: "X8", 5: "Y6",
            6: "HELI", 7: "TRI", 8: "SINGLE", 9: "COAX", 10: "TAILSITTER",
            12: "DODECAHEXA", 14: "DECA", 15: "CUSTOM"
        }
        return names[frameClass] || (vehicle && vehicle.vehicleType ? vehicle.vehicleType : "MULTIROTOR")
    }

    function frameTypeName() {
        var names = {
            0: "PLUS", 1: "X", 2: "V", 3: "H", 4: "V-TAIL", 5: "A-TAIL",
            6: "PLUS REV", 12: "BF X", 13: "DJI X", 14: "CW X", 15: "I",
            16: "NYT PLUS", 17: "NYT X", 18: "BF X REV", 19: "Y4"
        }
        return names[frameType] || "CUSTOM"
    }

    function close() {
        stopAll()
        opened = false
    }

    function controlsEnabled() {
        return propellersRemoved && mavlinkObject && mavlinkObject.connected
    }

    function requestMotorTest(mode, firstMotor, count, durationSeconds) {
        pendingMotorTestMode = mode
        pendingFirstMotor = firstMotor
        pendingMotorCount = count
        pendingDurationSeconds = durationSeconds
        motorTestStartedByController = false
        motorTestStatus = "Awaiting flight controller motor-test start..."
        motorTestConfirmationTimer.restart()
    }

    function startConfirmedMotors() {
        motorTestConfirmationTimer.stop()
        motorTestStartedByController = true
        motorTestStatus = "Flight controller started motor test"

        var firstIndex = Math.max(0, pendingFirstMotor - 1)
        var count = pendingMotorTestMode === "all" ? pendingMotorCount : 1
        for (var i = 0; i < count && firstIndex + i < motorVisuals.count; i++)
            motorVisuals.itemAt(firstIndex + i).spin(pendingDurationSeconds)
    }

    function stopMotorVisuals() {
        for (var i = 0; i < motorVisuals.count; i++)
            motorVisuals.itemAt(i).stopSpin()
    }

    function startMotor(motor) {
        if (!controlsEnabled())
            return

        requestMotorTest("single", motor, 1, runtimeSeconds)
        mavlinkObject.testMotor(motor, throttlePercent, runtimeSeconds)
    }

    function runAll() {
        if (!controlsEnabled())
            return

        stopMotorVisuals()

        requestMotorTest("all", 1, motorCount, runtimeSeconds)
        mavlinkObject.runAllMotorTest(throttlePercent, runtimeSeconds, motorCount)
    }

    function runSequential() {
        if (!controlsEnabled())
            return

        stopMotorVisuals()

        requestMotorTest("sequential", 1, motorCount, runtimeSeconds)
        mavlinkObject.runSequentialMotorTest(throttlePercent, runtimeSeconds, motorCount)
    }

    function stopAll() {
        motorTestConfirmationTimer.stop()
        pendingMotorTestMode = ""
        motorTestStartedByController = false
        motorTestStatus = "Motor test stopped"
        stopMotorVisuals()

        if (mavlinkObject)
            mavlinkObject.stopAllMotorTests(motorCount)
    }

    Connections {
        target: root.mavlinkObject
        ignoreUnknownSignals: true

        function onMotorTestCommandAccepted(mode, firstMotor, motorCount, durationSeconds) {
            if (!root.opened || !root.motorTestStartedByController)
                return

            if (mode === "sequential") {
                var motorIndex = Math.max(0, firstMotor - 1)
                if (motorIndex < motorVisuals.count)
                    motorVisuals.itemAt(motorIndex).spin(durationSeconds)
            }
        }

        function onMotorTestCommandRejected(mode, firstMotor, motorCount, reason) {
            motorTestConfirmationTimer.stop()
            motorTestStartedByController = false
            motorTestStatus = "Motor test rejected: " + reason
            stopMotorVisuals()
        }

        function onParameterUpdated(name, parameter) {
            if (name === "FRAME_CLASS" || name === "FRAME_TYPE")
                root.refreshFrameConfiguration()
        }

        function onConnectionStatusChanged(connected) {
            if (connected)
                root.refreshFrameConfiguration()
        }
    }

    Connections {
        target: root.vehicle
        ignoreUnknownSignals: true

        function onStatusTextReceived(message) {
            if (!root.opened || root.pendingMotorTestMode.length === 0)
                return

            var normalizedMessage = message.toLowerCase()
            if (normalizedMessage.indexOf("starting motor test") >= 0) {
                root.startConfirmedMotors()
                return
            }

            var motorTestBlocked = normalizedMessage.indexOf("motor test") >= 0
                    || normalizedMessage.indexOf("prearm:") >= 0
                    || normalizedMessage.indexOf("safety") >= 0
                    || normalizedMessage.indexOf("rc not calibrated") >= 0
                    || normalizedMessage.indexOf("vehicle not landed") >= 0
            if (motorTestBlocked) {
                motorTestConfirmationTimer.stop()
                motorTestStartedByController = false
                motorTestStatus = message
                stopMotorVisuals()
            }
        }
    }

    Timer {
        id: motorTestConfirmationTimer
        interval: 1500
        repeat: false
        onTriggered: {
            if (!root.motorTestStartedByController && root.pendingMotorTestMode.length > 0)
                root.motorTestStatus = "No motor-test start confirmation from flight controller"
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#99000000"

        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    Rectangle {
        id: panel

        width: Math.min(620, root.width - 80)
        height: Math.min(500, root.height - 80)
        radius: 6
        anchors.centerIn: parent
        color: "#ee0c1720"
        border.color: "#355a78"
        border.width: 1

        MouseArea {
            anchors.fill: parent
        }

        Text {
            id: titleText

            anchors.top: parent.top
            anchors.topMargin: 16
            anchors.horizontalCenter: parent.horizontalCenter
            text: "MOTOR TEST"
            color: "#dce7ef"
            font.pixelSize: 15
            font.bold: true
        }

        Text {
            id: motorCountText

            anchors.top: titleText.bottom
            anchors.topMargin: 6
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Motor Count: " + root.motorCount + "  |  " + root.frameClassName() + " " + root.frameTypeName()
            color: "#58d4ff"
            font.pixelSize: 12
        }

        Button {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 12
            anchors.rightMargin: 12
            width: 22
            height: 22
            padding: 0
            flat: true
            text: "\u00D7"
            font.pixelSize: 14
            onClicked: root.close()

            contentItem: Text {
                text: parent.text
                color: "#aeb8c2"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Rectangle {
            id: warningBanner

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: motorCountText.bottom
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 22
            height: 30
            radius: 3
            color: "#3c2a12"
            border.color: "#a66e18"

            Text {
                anchors.centerIn: parent
                text: "\u26A0  REMOVE PROPELLERS BEFORE TESTING!"
                color: "#ffbf3d"
                font.pixelSize: 10
                font.bold: true
            }
        }

        CheckBox {
            id: propellerCheck

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: warningBanner.bottom
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 12
            height: 34
            text: "Propellers Removed"
            checked: root.propellersRemoved
            onCheckedChanged: root.propellersRemoved = checked

            indicator: Rectangle {
                implicitWidth: 18
                implicitHeight: 18
                x: Math.round((propellerCheck.width - propellerCheck.contentItem.implicitWidth) / 2 - width - 14)
                y: Math.round((propellerCheck.height - height) / 2)
                radius: 3
                color: propellerCheck.checked ? "#109cbd" : "#122634"
                border.color: "#13d8ff"

                Text {
                    anchors.centerIn: parent
                    text: propellerCheck.checked ? "\u2713" : ""
                    color: "white"
                    font.pixelSize: 12
                    font.bold: true
                }
            }

            contentItem: Text {
                text: propellerCheck.text
                color: propellerCheck.checked ? "#6de6ff" : "#7b8d98"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 3
                color: "#102935"
                border.color: propellerCheck.checked ? "#0fc7e6" : "#1a7b8a"
            }
        }

        Text {
            id: motorTestStatusText

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: propellerCheck.bottom
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 6
            height: 16
            text: root.motorTestStatus
            color: root.motorTestStartedByController ? "#6de6ff"
                  : root.motorTestStatus.indexOf("rejected") >= 0
                    || root.motorTestStatus.indexOf("No motor-test") >= 0 ? "#ff9a94" : "#aebdca"
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Item {
            id: controlsArea

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: motorTestStatusText.bottom
            anchors.bottom: parent.bottom
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 22
            anchors.bottomMargin: 24
            enabled: root.controlsEnabled()

            ColumnLayout {
                anchors.fill: parent
                spacing: 14

                GridLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 78
                    columns: 3
                    columnSpacing: 18
                    rowSpacing: 10

                    Text {
                        Layout.preferredWidth: 100
                        text: "Throttle"
                        color: controlsArea.enabled ? "#cfdbe4" : "#61717c"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                    }

                    Slider {
                        id: throttleSlider

                        Layout.fillWidth: true
                        from: 1
                        to: 20
                        stepSize: 1
                        value: root.throttlePercent
                        onMoved: root.throttlePercent = Math.round(value)
                    }

                    Text {
                        Layout.preferredWidth: 48
                        text: root.throttlePercent + "%"
                        color: "#47cfff"
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    Text {
                        Layout.preferredWidth: 100
                        text: "Runtime (s):"
                        color: controlsArea.enabled ? "#cfdbe4" : "#61717c"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Button {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 28
                            text: "-"
                            onClicked: root.runtimeSeconds = Math.max(1, root.runtimeSeconds - 1)
                        }
                        Rectangle {
                            Layout.preferredWidth: 56
                            Layout.preferredHeight: 28
                            color: "#1b2630"
                            Text {
                                anchors.centerIn: parent
                                text: root.runtimeSeconds
                                color: "white"
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }
                        Button {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 28
                            text: "+"
                            onClicked: root.runtimeSeconds = Math.min(30, root.runtimeSeconds + 1)
                        }
                        Text {
                            Layout.leftMargin: 18
                            text: "seconds"
                            color: controlsArea.enabled ? "#cfdbe4" : "#61717c"
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                        }
                        Item { Layout.fillWidth: true }
                    }

                    Item { Layout.preferredWidth: 48 }
                }

                Item {
                    id: droneModel

                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 300
                    Layout.preferredHeight: 170
                    Layout.fillWidth: false

                    function motorLabel(index) {
                        return "M" + (index + 1)
                    }

                    function motorDefinitions() {
                        // Values mirror ArduPilot's FRAME_CLASS/FRAME_TYPE motor-test order.
                        if (root.frameClass === 1 && root.motorCount === 4 && root.frameType === 0)
                            return [ { angle: 0, direction: "CW" }, { angle: 90, direction: "CCW" }, { angle: 180, direction: "CW" }, { angle: -90, direction: "CCW" } ]
                        if (root.frameClass === 1 && root.motorCount === 4 && root.frameType === 6)
                            return [ { angle: 0, direction: "CCW" }, { angle: 90, direction: "CW" }, { angle: 180, direction: "CCW" }, { angle: -90, direction: "CW" } ]
                        if (root.frameClass === 1 && root.motorCount === 4 && (root.frameType === 3 || root.frameType === 18))
                            return [ { angle: 45, direction: "CW" }, { angle: 135, direction: "CCW" }, { angle: -135, direction: "CW" }, { angle: -45, direction: "CCW" } ]
                        if (root.frameClass === 1 && root.motorCount === 4)
                            return [ { angle: 45, direction: "CCW" }, { angle: 135, direction: "CW" }, { angle: -135, direction: "CCW" }, { angle: -45, direction: "CW" } ]
                        if (root.frameClass === 2 && root.motorCount === 6 && root.frameType === 0)
                            return [ { angle: 0, direction: "CW" }, { angle: 60, direction: "CCW" }, { angle: 120, direction: "CW" }, { angle: 180, direction: "CCW" }, { angle: -120, direction: "CW" }, { angle: -60, direction: "CCW" } ]
                        if (root.frameClass === 2 && root.motorCount === 6)
                            return [ { angle: 30, direction: "CCW" }, { angle: 90, direction: "CW" }, { angle: 150, direction: "CCW" }, { angle: -150, direction: "CW" }, { angle: -90, direction: "CCW" }, { angle: -30, direction: "CW" } ]
                        if (root.frameClass === 3 && root.motorCount === 8 && root.frameType === 0)
                            return [ { angle: 0, direction: "CW" }, { angle: 45, direction: "CCW" }, { angle: 90, direction: "CW" }, { angle: 135, direction: "CCW" }, { angle: 180, direction: "CW" }, { angle: -135, direction: "CCW" }, { angle: -90, direction: "CW" }, { angle: -45, direction: "CCW" } ]
                        if (root.frameClass === 3 && root.motorCount === 8)
                            return [ { angle: 22.5, direction: "CW" }, { angle: 67.5, direction: "CCW" }, { angle: 112.5, direction: "CW" }, { angle: 157.5, direction: "CCW" }, { angle: -157.5, direction: "CW" }, { angle: -112.5, direction: "CCW" }, { angle: -67.5, direction: "CW" }, { angle: -22.5, direction: "CCW" } ]

                        var motors = []
                        var count = Math.max(1, root.motorCount)
                        var offset = root.frameType === 0 ? 0 : 180 / count
                        for (var i = 0; i < count; i++)
                            motors.push({ angle: offset + i * 360 / count, direction: i % 2 === 0 ? "CCW" : "CW" })
                        return motors
                    }

                    function motorDefinition(index) {
                        var motors = motorDefinitions()
                        return motors[index] || { angle: 0, direction: "--" }
                    }

                    function motorX(index) {
                        return width / 2 + Math.sin(motorDefinition(index).angle * Math.PI / 180) * (root.motorCount <= 4 ? 96 : 108)
                    }

                    function motorY(index) {
                        return height / 2 - Math.cos(motorDefinition(index).angle * Math.PI / 180) * (root.motorCount <= 4 ? 58 : 66)
                    }

                    function motorPositionLabel(index) {
                        var angle = motorDefinition(index).angle
                        if (Math.abs(angle) < 15) return "FRONT"
                        if (Math.abs(Math.abs(angle) - 180) < 15) return "REAR"
                        if (angle > 0) return Math.abs(angle) < 90 ? "FRONT L" : "REAR L"
                        return Math.abs(angle) < 90 ? "FRONT R" : "REAR R"
                    }

                    function motorDirection(index) {
                        return motorDefinition(index).direction
                    }

                    function isHFrame() {
                        return root.frameType === 3
                    }

                    Text {
                        anchors.top: parent.top
                        anchors.topMargin: 2
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "FRONT"
                        color: "#58d4ff"
                        font.pixelSize: 9
                        font.bold: true
                    }

                    Canvas {
                        anchors.top: parent.top
                        anchors.topMargin: 14
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 16
                        height: 9

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.fillStyle = "#58d4ff"
                            ctx.beginPath()
                            ctx.moveTo(width / 2, 0)
                            ctx.lineTo(width, height)
                            ctx.lineTo(0, height)
                            ctx.closePath()
                            ctx.fill()
                        }
                    }

                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            var cx = width / 2
                            var cy = height / 2
                            var armStopInset = 38
                            ctx.clearRect(0, 0, width, height)

                            ctx.save()
                            ctx.shadowColor = "#90000000"
                            ctx.shadowBlur = 14
                            ctx.shadowOffsetY = 6
                            ctx.lineCap = "round"
                            ctx.lineWidth = 13
                            ctx.strokeStyle = "#152235"

                            for (var i = 0; i < root.motorCount; i++) {
                                var endX = droneModel.motorX(i)
                                var endY = droneModel.motorY(i)
                                ctx.beginPath()
                                ctx.moveTo(cx, cy)
                                ctx.lineTo(endX, endY)
                                ctx.stroke()
                            }

                            ctx.shadowBlur = 0
                            ctx.lineWidth = 5
                            ctx.strokeStyle = "#263a55"
                            for (var j = 0; j < root.motorCount; j++) {
                                var hEndX = droneModel.motorX(j)
                                var hEndY = droneModel.motorY(j)
                                ctx.beginPath()
                                ctx.moveTo(cx, cy - 1)
                                ctx.lineTo(hEndX, hEndY - 1)
                                ctx.stroke()
                            }

                            if (droneModel.isHFrame() && root.motorCount === 4) {
                                ctx.lineWidth = 4
                                ctx.strokeStyle = "#355170"
                                ctx.beginPath()
                                ctx.moveTo(cx - 72, cy - 44)
                                ctx.lineTo(cx - 72, cy + 44)
                                ctx.moveTo(cx + 72, cy - 44)
                                ctx.lineTo(cx + 72, cy + 44)
                                ctx.moveTo(cx - 72, cy)
                                ctx.lineTo(cx + 72, cy)
                                ctx.stroke()
                            }
                            ctx.restore()
                        }
                    }

                    Rectangle {
                        id: flightController

                        anchors.centerIn: parent
                        width: 42
                        height: 34
                        radius: 5
                        color: "#102033"
                        border.color: "#254263"
                        border.width: 1

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: 1
                            radius: 1
                            color: "#46698f"
                            opacity: 0.8
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "FC"
                            color: "#18d9ff"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Repeater {
                        id: motorVisuals

                        model: root.motorCount

                        delegate: MotorVisual {
                            x: droneModel.motorX(index) - width / 2
                            y: droneModel.motorY(index) - height / 2
                            label: droneModel.motorLabel(index)
                            subLabel: droneModel.motorPositionLabel(index) + " " + droneModel.motorDirection(index)
                            rotationDirection: droneModel.motorDirection(index)
                            motorNumber: index + 1
                            enabled: droneModel.enabled
                            onClicked: root.startMotor(motorNumber)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    spacing: 10

                    TestButton {
                        Layout.fillWidth: true
                        label: "All"
                        accent: "#1aa9ef"
                        onClicked: root.runAll()
                    }
                    TestButton {
                        Layout.fillWidth: true
                        label: "Sequential"
                        accent: "#39bfff"
                        onClicked: root.runSequential()
                    }
                    TestButton {
                        Layout.fillWidth: true
                        label: "Stop All"
                        accent: "#ff4a4f"
                        onClicked: root.stopAll()
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: !root.controlsEnabled()
                z: 10
                radius: 4
                color: "#78000000"
                border.color: "#221018"
                border.width: 1

                MouseArea {
                    anchors.fill: parent
                }
            }
        }
    }

    component TestButton: Button {
        property string label: ""
        property color accent: "#39bfff"

        Layout.preferredHeight: 32
        text: label
        font.pixelSize: 11

        contentItem: Text {
            text: parent.text
            color: parent.enabled ? parent.accent : "#61717c"
            font: parent.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 2
            color: parent.down ? "#1f3544" : "#10202b"
            border.color: parent.enabled ? parent.accent : "#30414e"
            opacity: parent.enabled ? 1.0 : 0.6
        }
    }

    component MotorVisual: Item {
        id: motorRoot

        property string label: ""
        property string subLabel: ""
        property string rotationDirection: "CW"
        property int motorNumber: 1
        signal clicked()

        width: 58
        height: 58

        function spin(seconds) {
            spinAnimation.duration = Math.max(1, seconds) * 1000
            spinAnimation.restart()
        }

        function stopSpin() {
            spinAnimation.stop()
            propeller.rotation = 0
        }

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: motorMouseArea.containsMouse ? "#172845" : "#111c30"
            border.color: spinAnimation.running ? "#28d3ff" : "#2b4165"
            border.width: 1

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: width / 2
                color: "transparent"
                border.color: "#253a5b"
                border.width: 1
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 7
                height: 1
                radius: 1
                color: "#5c79a6"
                opacity: 0.9
            }
        }

        Item {
            id: propeller
            anchors.centerIn: parent
            width: 44
            height: 44
            opacity: spinAnimation.running ? 0.42 : 0.16

            Rectangle {
                anchors.centerIn: parent
                width: 4
                height: 32
                radius: 2
                color: "#7da0bc"
            }
            Rectangle {
                anchors.centerIn: parent
                width: 32
                height: 4
                radius: 2
                color: "#7da0bc"
            }

            RotationAnimation on rotation {
                id: spinAnimation
                from: 0
                to: motorRoot.rotationDirection === "CCW" ? -1440 : 1440
                duration: 2000
                running: false
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 11
            text: motorRoot.label
            color: "#c9d8f1"
            font.pixelSize: 15
            font.bold: true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 31
            text: motorRoot.subLabel
            color: "#667da2"
            font.pixelSize: 9
            font.bold: true
        }

        MouseArea {
            id: motorMouseArea
            anchors.fill: parent
            enabled: motorRoot.enabled
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: motorRoot.clicked()
        }
    }
}
