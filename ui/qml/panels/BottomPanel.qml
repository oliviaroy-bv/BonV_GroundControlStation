import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Qt.labs.settings

Rectangle {
    id: root

    property var mavlinkObject: null
    property var vehicle: null
    property string flightModeText: "--"
    property bool darkTheme: true
    readonly property bool vehicleConnected: root.mavlinkObject ? root.mavlinkObject.connected : false
    readonly property bool vehicleArmed: root.vehicle ? root.vehicle.armed : false
    readonly property bool hasMissionPlan: root.mavlinkObject && root.mavlinkObject.missionUploaded
    readonly property bool hasLoadedMissionPlan: root.mavlinkObject && (root.mavlinkObject.missionUploaded || root.mavlinkObject.missionWaypoints.length > 0)
    readonly property color chromeColor: root.darkTheme ? "#f2070b11" : "#ffffff"
    readonly property color borderColor: root.darkTheme ? "#223145" : "#cfdceb"
    readonly property color panelColor: root.darkTheme ? "#0b1018" : "#ffffff"
    readonly property color hoverColor: root.darkTheme ? "#141e2d" : "#eef5fc"
    readonly property color downColor: root.darkTheme ? "#152334" : "#ddeaf8"
    readonly property color textColor: root.darkTheme ? "#f3f7ff" : "#0f213b"
    readonly property color mutedColor: root.darkTheme ? "#8196b7" : "#526680"
    readonly property color disabledColor: root.darkTheme ? "#465371" : "#c8d2e0"
    property bool preflightChecklistPassed: false
    property bool preflightChecklistOpen: false
    property bool armAfterChecklist: false
    property bool armConfirmationVisible: false
    property string armConfirmationAction: "arm"
    property bool flightModeExpanded: false
    signal generateMissionRequested()
    signal graphRequested()
    signal statusMessageRequested(string message)

    width: parent ? parent.width : 0
    height: 72
    color: root.chromeColor
    border.color: root.borderColor
    border.width: 1

    Settings {
        id: persistedGeneralSettings
        category: "General"
        property bool preflightChecklistEnabled: true

        onPreflightChecklistEnabledChanged: {
            if (preflightChecklistEnabled) {
                root.preflightChecklistPassed = false
            } else {
                preflightChecklistDialog.close()
            }
            root.armAfterChecklist = false
            root.cancelArmConfirmation()
        }
    }

    Connections {
        target: root.mavlinkObject
        function onConnectionStatusChanged(connected) {
            if (!connected) {
                root.preflightChecklistPassed = false
                root.armAfterChecklist = false
                root.cancelArmConfirmation()
                preflightChecklistDialog.close()
            }
        }
    }

    function selectFlightMode(customMode) {
        if (root.mavlinkObject && root.vehicleConnected) {
            root.mavlinkObject.setFlightMode(customMode)
            root.flightModeExpanded = false
            root.statusMessageRequested("Flight mode change requested")
        }
    }

    function requestTakeoff() {
        if (!root.mavlinkObject || !root.vehicleConnected || !root.vehicleArmed)
            return

        takeoffAltitudeField.text = "10"
        takeoffAltitudeDialog.open()
        root.statusMessageRequested("Enter takeoff altitude")
    }

    function confirmTakeoffAltitude() {
        var altitude = Number(takeoffAltitudeField.text)
        if (!isFinite(altitude) || altitude <= 0)
            return

        takeoffAltitudeDialog.close()
        root.mavlinkObject.takeoff(altitude)
        root.statusMessageRequested("Takeoff requested")
    }

    onVehicleArmedChanged: {
        if ((vehicleArmed && root.armConfirmationAction === "arm")
                || (!vehicleArmed && root.armConfirmationAction === "disarm")) {
            root.cancelArmConfirmation()
        }
    }

    function requestArmToggle() {
        if (!root.mavlinkObject)
            return

        if (root.vehicleArmed) {
            root.armConfirmationAction = "disarm"
            root.armConfirmationVisible = true
            root.statusMessageRequested("Confirm disarm request")
            return
        }

        if (persistedGeneralSettings.preflightChecklistEnabled && !root.preflightChecklistPassed) {
            root.openPreflightChecklist(true)
            root.statusMessageRequested("Complete preflight checklist before arming")
            return
        }

        root.armConfirmationAction = "arm"
        root.armConfirmationVisible = true
        root.statusMessageRequested("Confirm arm request")
    }

    function cancelArmConfirmation() {
        root.armConfirmationVisible = false
    }

    function setPreflightChecklistEnabled(enabled) {
        persistedGeneralSettings.preflightChecklistEnabled = enabled
    }

    function confirmArmRequest() {
        if (!root.mavlinkObject)
            return

        root.cancelArmConfirmation()
        if (root.armConfirmationAction === "disarm") {
            root.mavlinkObject.armVehicle(false)
            root.statusMessageRequested("Disarm requested")
            return
        }

        root.performArmRequest()
    }

    function performArmRequest() {
        if (!root.mavlinkObject)
            return

        if (!persistedGeneralSettings.preflightChecklistEnabled || root.preflightChecklistPassed) {
            root.mavlinkObject.armVehicle(true)
            root.statusMessageRequested("Arm requested")
            return
        }

        root.openPreflightChecklist(true)
        root.statusMessageRequested("Complete preflight checklist before arming")
    }

    function openPreflightChecklist(armAfterConfirm) {
        root.armAfterChecklist = armAfterConfirm
        preflightChecklistDialog.open()
        root.statusMessageRequested("Preflight checklist opened")
    }

    function confirmPreflightChecklist() {
        if (persistedGeneralSettings.preflightChecklistEnabled && !preflightChecklistDialog.checklistComplete) {
            root.statusMessageRequested("Complete all checklist items before arming")
            return
        }

        root.preflightChecklistPassed = true
                root.cancelArmConfirmation()
                preflightChecklistDialog.close()
                root.flightModeExpanded = false
        if (root.armAfterChecklist && root.mavlinkObject && root.vehicleConnected) {
            root.mavlinkObject.armVehicle(true)
            root.statusMessageRequested("Checklist confirmed. Arm requested")
        } else {
            root.statusMessageRequested("Preflight checklist confirmed")
        }
        root.armAfterChecklist = false
    }

    Rectangle {
        visible: root.armConfirmationVisible
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 2
        color: root.armConfirmationAction === "disarm" ? "#ff4b46" : "#1fc969"
        z: 2
    }

    RowLayout {
        visible: root.armConfirmationVisible
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        spacing: 14

        Item { Layout.fillWidth: true }

        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: 10

            Text {
                text: "!"
                color: root.armConfirmationAction === "disarm" ? "#ff4b46" : "#1fc969"
                font.pixelSize: 20
                font.bold: true
                verticalAlignment: Text.AlignVCenter
            }

            ColumnLayout {
                spacing: 1
                Text {
                    text: root.armConfirmationAction === "disarm" ? "DISARM AIRCRAFT" : "ARM AIRCRAFT"
                    color: root.armConfirmationAction === "disarm" ? "#ff4b46" : "#1fc969"
                    font.pixelSize: 12
                    font.bold: true
                    font.letterSpacing: 0.0
                }
                Text {
                    text: root.armConfirmationAction === "disarm" ? "Confirm motors should stop and aircraft should disarm." : "Ensure the area is clear. Motors will become active immediately."
                    color: root.mutedColor
                    font.pixelSize: 10
                }
            }
        }

        Item { Layout.fillWidth: true }

        ConfirmActionButton {
            Layout.preferredWidth: 86
            text: "CANCEL"
            onClicked: root.cancelArmConfirmation()
        }

        ConfirmActionButton {
            Layout.preferredWidth: 92
            text: "CONFIRM"
            active: true
            onClicked: root.confirmArmRequest()
        }
    }
    RowLayout {
        visible: !root.armConfirmationVisible
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        spacing: 20

        ControlGroup {
            title: "MISSION CONTROLS"
            buttons: [
                { label: "PLAN", accent: "#7e8da8", action: "plan", enabled: true },
                { label: "UPLOAD", accent: "#7e8da8", action: "upload", enabled: true },
                { label: "CLEAR", accent: "#ff8a00", action: "clear", enabled: root.hasLoadedMissionPlan }
            ].concat(root.hasLoadedMissionPlan ? [
                { label: "GRAPH", accent: "#00c7df", action: "graph", enabled: true }
            ] : [])
            onActionRequested: function(action) {
                if (action === "plan") {
                    root.statusMessageRequested("Mission planner requested")
                    root.generateMissionRequested()
                } else if (action === "upload") {
                    missionUploadDialog.open()
                    root.statusMessageRequested("Select mission file")
                } else if (action === "clear") {
                    if (root.mavlinkObject) {
                        root.mavlinkObject.clearMission()
                        root.statusMessageRequested("Mission plan cleared")
                    }
                } else if (action === "graph") {
                    root.graphRequested()
                }
            }
        }

        Separator {}

        FlightModeControlGroup {
            expanded: root.flightModeExpanded
            currentModeText: root.flightModeText
            vehicleConnected: root.vehicleConnected
            vehicleArmed: root.vehicleArmed
            onExpansionRequested: root.flightModeExpanded = !root.flightModeExpanded
            onTakeoffRequested: root.requestTakeoff()
            onModeRequested: function(customMode) { root.selectFlightMode(customMode) }
        }

        Separator {}

        ControlGroup {
            title: "CRITICAL ACTIONS"
            dangerTitle: true
            buttons: [
                { label: "ARM", accent: "#1fc969", action: "arm", enabled: root.vehicleConnected && !root.vehicleArmed },
                { label: "DISARM", accent: "#ff4b46", fill: "#261016", hoverFill: "#35141b", action: "disarm", enabled: root.vehicleConnected && root.vehicleArmed },
                { label: "LAND", accent: "#9d2bd4", action: "land", enabled: root.vehicleConnected },
                { label: "RTL", accent: "#ff8a00", action: "rtl", enabled: root.vehicleConnected }
            ]
            onActionRequested: function(action) {
                if (!root.mavlinkObject) return
                if (action === "arm") root.requestArmToggle()
                else if (action === "disarm") root.requestArmToggle()
                else if (action === "land") {
                    root.mavlinkObject.land()
                    root.statusMessageRequested("Land requested")
                } else if (action === "rtl") {
                    root.mavlinkObject.returnToLaunch()
                    root.statusMessageRequested("Return to launch requested")
                }
            }
        }
    }

    FileDialog {
        id: missionUploadDialog
        title: "Select mission waypoint file"
        currentFolder: (typeof backend !== "undefined" && backend.missionsDirectoryUrl)
                       ? backend.missionsDirectoryUrl()
                       : StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        nameFilters: [ "Waypoint files (*.waypoints *.txt)", "All files (*)" ]
        fileMode: FileDialog.OpenFile
        onAccepted: if (root.mavlinkObject) {
            root.statusMessageRequested("Mission upload requested")
            root.mavlinkObject.uploadMissionFile(selectedFile)
        }
    }

    Popup {
        id: takeoffAltitudeDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        width: 320
        height: takeoffAltitudeContent.implicitHeight + 36
        padding: 18
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside

        onAboutToShow: {
            x = Math.round((parent.width - width) / 2)
            y = Math.round((parent.height - height) / 2)
        }

        background: Rectangle {
            color: root.darkTheme ? "#f20b1018" : "#ffffff"
            border.color: root.darkTheme ? "#00aeca" : "#008fb8"
            border.width: 1
        }

        contentItem: ColumnLayout {
            id: takeoffAltitudeContent
            width: takeoffAltitudeDialog.width - 36
            spacing: 12
            Text {
                Layout.fillWidth: true
                text: "Enter takeoff altitude"
                color: root.textColor
                font.pixelSize: 14
                font.bold: true
            }
            TextField {
                id: takeoffAltitudeField
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                text: "10"
                validator: DoubleValidator { bottom: 1; top: 100; decimals: 1 }
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                color: root.textColor
                font.pixelSize: 13
                selectByMouse: true
                placeholderText: "Altitude in meters"
                placeholderTextColor: root.mutedColor
                background: Rectangle {
                    color: root.darkTheme ? "#070c13" : "#f8fbff"
                    border.color: takeoffAltitudeField.activeFocus ? (root.darkTheme ? "#00aeca" : "#008fb8") : root.borderColor
                    border.width: 1
                }
                Keys.onReturnPressed: root.confirmTakeoffAltitude()
                Keys.onEnterPressed: root.confirmTakeoffAltitude()
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                DialogButton { text: "CANCEL"; onClicked: takeoffAltitudeDialog.close() }
                DialogButton { text: "TAKEOFF"; accent: "#00c7df"; onClicked: root.confirmTakeoffAltitude() }
            }
        }
    }

    Popup {
        id: preflightChecklistDialog
        parent: Overlay.overlay
        modal: true
        focus: true
        width: Math.min(724, parent ? parent.width - 48 : 724)
        height: Math.min(708, parent ? parent.height - 144 : 708)
        x: parent ? Math.round((parent.width - width) / 2) : 0
        y: parent ? Math.round((parent.height - height) / 2) : 0
        padding: 0
        closePolicy: Popup.CloseOnEscape

        readonly property bool checklistComplete: hardwareCheck.checked && batteryCheck.checked && sensorsCheck.checked && gpsCheck.checked && radioCheck.checked && motorsCheck.checked && missionCheck.checked && soundCheck.checked && payloadCheck.checked && weatherCheck.checked && areaCheck.checked
        readonly property int requiredCount: 11
        readonly property int completedCount: (hardwareCheck.checked ? 1 : 0) + (batteryCheck.checked ? 1 : 0) + (sensorsCheck.checked ? 1 : 0) + (gpsCheck.checked ? 1 : 0) + (radioCheck.checked ? 1 : 0) + (motorsCheck.checked ? 1 : 0) + (missionCheck.checked ? 1 : 0) + (soundCheck.checked ? 1 : 0) + (payloadCheck.checked ? 1 : 0) + (weatherCheck.checked ? 1 : 0) + (areaCheck.checked ? 1 : 0)

        onOpened: root.preflightChecklistOpen = true
        onClosed: root.preflightChecklistOpen = false

        onAboutToShow: {
            hardwareCheck.expanded = false
            batteryCheck.expanded = false
            sensorsCheck.expanded = false
            gpsCheck.expanded = false
            radioCheck.expanded = false
            motorsCheck.expanded = false
            missionCheck.expanded = false
            soundCheck.expanded = false
            payloadCheck.expanded = false
            weatherCheck.expanded = false
            areaCheck.expanded = false
        }

        Overlay.modal: Rectangle {
            color: "#c8000308"
        }

        background: Rectangle {
            radius: 0
            color: "#0b1018"
            border.color: "#435273"
            border.width: 1
        }

        contentItem: Item {
            anchors.fill: parent

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    color: "#090e16"
                    border.color: "#1c2b45"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 22
                        anchors.rightMargin: 18
                        spacing: 16

                        MiniChecklistIcon {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 18
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "PREFLIGHT CHECKLIST"
                            color: "#f6f8ff"
                            font.pixelSize: 16
                            font.bold: true
                            font.letterSpacing: 0
                            verticalAlignment: Text.AlignVCenter
                        }

                        Rectangle {
                            Layout.preferredWidth: 112
                            Layout.preferredHeight: 24
                            color: "#07202d"
                            border.color: "#006f86"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: preflightChecklistDialog.completedCount + " / " + preflightChecklistDialog.requiredCount + " Required"
                                color: "#00d7f2"
                                font.pixelSize: 10
                                font.bold: true
                                font.family: "Consolas"
                            }
                        }

                        Button {
                            id: closeButton
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            padding: 0
                            flat: true
                            hoverEnabled: true
                            contentItem: Text {
                                text: "X"
                                color: closeButton.hovered ? "#ffffff" : "#71809c"
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { color: closeButton.hovered ? "#141f30" : "transparent" }
                            onClicked: {
                                root.armAfterChecklist = false
                                root.cancelArmConfirmation()
                                preflightChecklistDialog.close()
                            }
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        id: checklistColumn
                        width: preflightChecklistDialog.width - 18
                        spacing: 0

                        ChecklistSection {
                            title: "INITIAL CHECKS"
                            completed: (hardwareCheck.checked ? 1 : 0) + (batteryCheck.checked ? 1 : 0) + (sensorsCheck.checked ? 1 : 0) + (gpsCheck.checked ? 1 : 0) + (radioCheck.checked ? 1 : 0)
                            total: 5
                        }
                        ChecklistItem { id: hardwareCheck; title: "Hardware"; detail: "Props mounted and secured?" }
                        ChecklistItem { id: batteryCheck; title: "Battery"; detail: "Battery connector firmly plugged?" }
                        ChecklistItem { id: sensorsCheck; title: "Sensors"; detail: "Sensor status is passed." }
                        ChecklistItem { id: gpsCheck; title: "GPS"; detail: "GPS lock and telemetry are verified." }
                        ChecklistItem { id: radioCheck; title: "Radio Control"; detail: "Receiving signal. Perform range test and confirm." }

                        ChecklistSection {
                            title: "ARM CHECKS"
                            completed: (motorsCheck.checked ? 1 : 0) + (missionCheck.checked ? 1 : 0) + (soundCheck.checked ? 1 : 0)
                            total: 3
                        }
                        ChecklistItem { id: motorsCheck; title: "Motors"; detail: "Propellers free? Throttle up gently. Working properly?" }
                        ChecklistItem { id: missionCheck; title: "Mission"; detail: "Confirm mission is valid: waypoints valid, no terrain collision." }
                        ChecklistItem { id: soundCheck; title: "Sound output"; detail: "BV GCS audio and system audio output enabled." }

                        ChecklistSection {
                            title: "LAUNCH PREP"
                            completed: (payloadCheck.checked ? 1 : 0) + (weatherCheck.checked ? 1 : 0) + (areaCheck.checked ? 1 : 0)
                            total: 3
                        }
                        ChecklistItem { id: payloadCheck; title: "Payload"; detail: "Configured and started? Payload lid closed?" }
                        ChecklistItem { id: weatherCheck; title: "Wind & weather"; detail: "OK for your platform?" }
                        ChecklistItem { id: areaCheck; title: "Flight area"; detail: "Launch area and path free of obstacles and people?" }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    color: "#0b1018"
                    border.color: "#1c2b45"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 22
                        anchors.rightMargin: 22
                        spacing: 12

                        Text {
                            Layout.fillWidth: true
                            text: (preflightChecklistDialog.requiredCount - preflightChecklistDialog.completedCount) + " required items remaining"
                            color: "#7da6ca"
                            font.pixelSize: 12
                        }

                        Button {
                            id: skipChecklistButton
                            Layout.preferredWidth: 156
                            Layout.preferredHeight: 34
                            visible: !persistedGeneralSettings.preflightChecklistEnabled
                            text: "SKIP CHECKLIST"
                            padding: 0
                            flat: true
                            hoverEnabled: true
                            contentItem: Text {
                                text: skipChecklistButton.text
                                color: "#d7deea"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: skipChecklistButton.hovered ? "#152236" : "#111a2a"
                                border.color: "#33425d"
                            }
                            onClicked: root.confirmPreflightChecklist()
                        }

                        DialogButton {
                            text: root.armAfterChecklist ? "CONFIRM AND ARM" : "CONFIRM CHECKLIST"
                            accent: "#92a0b8"
                            enabled: preflightChecklistDialog.checklistComplete
                            Layout.preferredWidth: 188
                            onClicked: root.confirmPreflightChecklist()
                        }
                    }
                }
            }
        }
    }

    component MiniChecklistIcon: Item {
        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: 2
            color: "transparent"
            border.color: "#00d7f2"
            border.width: 1
        }
        Rectangle { x: 5; y: 0; width: 6; height: 4; color: "#0b1018"; border.color: "#00d7f2"; border.width: 1 }
        Repeater {
            model: [5, 9, 13]
            Rectangle { x: 5; y: modelData; width: 7; height: 1; color: "#00d7f2" }
        }
    }

    component ChecklistSection: Item {
        id: sectionRoot
        property string title: ""
        property int completed: 0
        property int total: 0

        Layout.fillWidth: true
        Layout.preferredHeight: 30

        Rectangle {
            anchors.fill: parent
            color: "#090e16"
            border.color: "#1e2d48"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: sectionRoot.title
                    color: "#00d7f2"
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 0
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    text: sectionRoot.completed + "/" + sectionRoot.total
                    color: "#6e99c5"
                    font.pixelSize: 10
                    font.family: "Consolas"
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    component ChecklistItem: Item {
        id: checklistItem
        property string title: ""
        property string detail: ""
        property bool checked: false
        property bool expanded: false

        Layout.fillWidth: true
        Layout.preferredHeight: expanded ? 102 : 40

        Rectangle {
            anchors.fill: parent
            color: checklistItem.checked ? "#071511" : rowHover.containsMouse ? "#101824" : "#0b1018"
            border.color: "#1b2940"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40

                    MouseArea {
                        id: rowHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: checklistItem.expanded = !checklistItem.expanded
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 22
                        anchors.rightMargin: 22
                        spacing: 14

                        Button {
                            id: rowCheckButton
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            Layout.alignment: Qt.AlignVCenter
                            padding: 0
                            flat: true
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.text: checklistItem.checked ? "Mark incomplete" : "Mark complete"

                            contentItem: Rectangle {
                                radius: width / 2
                                color: checklistItem.checked ? "#2fc86b" : rowCheckButton.hovered ? "#132338" : "transparent"
                                border.color: checklistItem.checked ? "#2fc86b" : rowCheckButton.hovered ? "#7da6ca" : "#496487"
                                border.width: 1

                                Canvas {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    visible: checklistItem.checked
                                    onVisibleChanged: requestPaint()
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        ctx.strokeStyle = "#061018"
                                        ctx.lineWidth = 2
                                        ctx.lineCap = "round"
                                        ctx.lineJoin = "round"
                                        ctx.beginPath()
                                        ctx.moveTo(width * 0.15, height * 0.55)
                                        ctx.lineTo(width * 0.42, height * 0.82)
                                        ctx.lineTo(width * 0.88, height * 0.18)
                                        ctx.stroke()
                                    }
                                }
                            }

                            onClicked: {
                                checklistItem.checked = !checklistItem.checked
                                checklistItem.expanded = true
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: checklistItem.title
                            color: checklistItem.checked ? "#2fc86b" : "#dce3ef"
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        Canvas {
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            visible: checklistItem.checked
                            onVisibleChanged: requestPaint()
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                ctx.strokeStyle = "#2fc86b"
                                ctx.lineWidth = 2
                                ctx.lineCap = "round"
                                ctx.lineJoin = "round"
                                ctx.beginPath()
                                ctx.moveTo(width * 0.20, height * 0.52)
                                ctx.lineTo(width * 0.42, height * 0.75)
                                ctx.lineTo(width * 0.82, height * 0.25)
                                ctx.stroke()
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 19
                            visible: !checklistItem.checked
                            color: "#101827"
                            border.color: "#3b4f73"
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "REQ"
                                color: "#9cb0cc"
                                font.pixelSize: 9
                                font.bold: true
                            }
                        }

                        Text {
                            id: expandIndicator
                            Layout.preferredWidth: 16
                            text: checklistItem.expanded ? "^" : "v"
                            color: rowHover.containsMouse ? "#c9d7eb" : "#7c8da8"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 52
                    Layout.rightMargin: 24
                    Layout.bottomMargin: 12
                    spacing: 10
                    visible: checklistItem.expanded

                    Text {
                        Layout.fillWidth: true
                        text: checklistItem.detail
                        color: "#7f8ba0"
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }

                    CheckBox {
                        id: completionBox
                        checked: checklistItem.checked
                        text: checklistItem.checked ? "Completed" : "Mark as completed"
                        spacing: 10
                        hoverEnabled: true

                        indicator: Rectangle {
                            implicitWidth: 14
                            implicitHeight: 14
                            radius: 1
                            color: completionBox.checked ? "#00d7f2" : completionBox.hovered ? "#dfe8f4" : "#f4f6fb"
                            border.color: completionBox.checked ? "#00d7f2" : completionBox.hovered ? "#00d7f2" : "#c8d1df"
                            border.width: 1

                            Canvas {
                                anchors.fill: parent
                                anchors.margins: 3
                                visible: completionBox.checked
                                onVisibleChanged: requestPaint()
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.strokeStyle = "#061018"
                                    ctx.lineWidth = 2
                                    ctx.lineCap = "round"
                                    ctx.lineJoin = "round"
                                    ctx.beginPath()
                                    ctx.moveTo(width * 0.10, height * 0.55)
                                    ctx.lineTo(width * 0.40, height * 0.82)
                                    ctx.lineTo(width * 0.88, height * 0.18)
                                    ctx.stroke()
                                }
                            }
                        }

                        contentItem: Text {
                            text: completionBox.text
                            color: completionBox.checked ? "#2fc86b" : "#d5dbe7"
                            font.pixelSize: 11
                            font.bold: true
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: completionBox.indicator.width + completionBox.spacing
                        }

                        onToggled: checklistItem.checked = checked
                    }
                }
            }
        }
    }

    component DialogButton: Button {
        id: dialogButton
        property color accent: "#7e8da8"
        Layout.preferredWidth: 86
        Layout.preferredHeight: 34
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: Text {
            text: dialogButton.text
            color: dialogButton.enabled ? dialogButton.accent : root.disabledColor
            font.pixelSize: 12
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            color: dialogButton.down ? (root.darkTheme ? Qt.darker(dialogButton.accent, 3.0) : "#e1edf8") : dialogButton.hovered ? root.hoverColor : root.panelColor
            border.color: dialogButton.enabled ? dialogButton.accent : root.borderColor
            border.width: 1
        }
    }
    component ConfirmActionButton: Button {
        id: confirmButton
        property bool active: false
        Layout.preferredHeight: 34
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: Text {
            text: confirmButton.text
            color: !confirmButton.enabled ? root.disabledColor : confirmButton.active ? root.textColor : root.mutedColor
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            color: !confirmButton.enabled ? (root.darkTheme ? "#182132" : "#f3f6fb") : confirmButton.down ? root.downColor : confirmButton.hovered ? root.hoverColor : confirmButton.active ? (root.darkTheme ? "#1b2740" : "#eaf3ff") : root.panelColor
            border.color: !confirmButton.enabled ? root.borderColor : confirmButton.active ? "#1fc969" : root.borderColor
            border.width: 1
        }
    }
    component Separator: Rectangle {
        Layout.preferredWidth: 1
        Layout.fillHeight: true
        color: "#1c2638"
    }

    component FlightModeControlGroup: ColumnLayout {
        id: modeGroup
        property bool expanded: false
        property bool vehicleConnected: false
        property bool vehicleArmed: false
        property string currentModeText: "--"
        readonly property var modes: [
            { label: "AUTO", customMode: 3, accent: "#00c7df" },
            { label: "GUIDED", customMode: 4, accent: "#13d7a3" },
            { label: "ALT HOLD", customMode: 2, accent: "#36a3ff" },
            { label: "LOITER", customMode: 5, accent: "#25c767" },
            { label: "STABILIZE", customMode: 0, accent: "#f2c94c" }
        ]
        readonly property string normalizedCurrentMode: currentModeText.trim().toUpperCase().replace("_", " ")
        readonly property string compactModeLabel: normalizedCurrentMode.length > 0 && normalizedCurrentMode !== "--"
                                                 ? normalizedCurrentMode : "FLIGHT MODE"
        signal expansionRequested()
        signal modeRequested(int customMode)
        signal takeoffRequested()

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: expanded ? 562 : 152
        spacing: 7

        Text {
            text: "FLIGHT CONTROLS"
            color: root.mutedColor
            font.pixelSize: 10
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Row {
            id: modeRow
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: 48
            spacing: modeGroup.expanded ? 26 : 0

            FlightModeButton {
                width: modeGroup.expanded ? 72 : 0
                opacity: modeGroup.expanded ? 1 : 0
                visible: width > 0
                label: "TAKEOFF"
                accent: "#00c7df"
                enabled: modeGroup.vehicleConnected && modeGroup.vehicleArmed
                onClicked: modeGroup.takeoffRequested()

                Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
            }

            Repeater {
                model: modeGroup.modes
                delegate: FlightModeButton {
                    readonly property bool selected: modelData.label === modeGroup.normalizedCurrentMode
                    width: modeGroup.expanded ? 72 : 0
                    opacity: modeGroup.expanded ? 1 : 0
                    visible: width > 0
                    label: modelData.label
                    accent: modelData.accent
                    enabled: modeGroup.vehicleConnected
                    active: selected
                    onClicked: modeGroup.modeRequested(modelData.customMode)

                    Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
                }
            }

            FlightModeButton {
                id: compactModeButton
                width: modeGroup.expanded ? 0 : 152
                opacity: modeGroup.expanded ? 0 : 1
                visible: width > 0
                label: modeGroup.compactModeLabel
                accent: "#00c7df"
                enabled: modeGroup.vehicleConnected
                active: true
                chevron: true
                expanded: modeGroup.expanded
                onClicked: modeGroup.expansionRequested()

                Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
            }
        }
    }

    component FlightModeButton: Button {
        id: modeButton
        property string label: ""
        property color accent: "#00c7df"
        property bool active: false
        property bool chevron: false
        property bool expanded: false
        height: 48
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: RowLayout {
            anchors.fill: parent
            anchors.leftMargin: chevron ? 12 : 4
            anchors.rightMargin: chevron ? 10 : 4
            spacing: 7

            Text {
                Layout.fillWidth: true
                text: modeButton.label
                color: !modeButton.enabled ? root.disabledColor : modeButton.active ? root.textColor : modeButton.accent
                font.pixelSize: 12
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            Text {
                visible: modeButton.chevron
                text: modeButton.expanded ? "⌃" : "⌄"
                color: !modeButton.enabled ? root.disabledColor : modeButton.accent
                font.pixelSize: 15
                font.bold: true
                verticalAlignment: Text.AlignVCenter
            }
        }

        background: Rectangle {
            color: !modeButton.enabled ? (root.darkTheme ? "#0b1018" : "#f3f6fb")
                  : modeButton.down ? (root.darkTheme ? Qt.darker(modeButton.accent, 3.0) : "#e1edf8")
                  : modeButton.active ? (root.darkTheme ? "#07546a" : "#bdeffd")
                  : modeButton.hovered ? root.hoverColor
                  : root.panelColor
            // In expanded mode every flight command retains its own colored
            // outline. The active command alone receives the shaded fill.
            border.color: !modeButton.enabled ? root.borderColor : modeButton.active ? "#48e5ff" : modeButton.accent
            border.width: 2

            Rectangle {
                anchors.fill: parent
                anchors.margins: 3
                visible: modeButton.active && modeButton.enabled
                color: "transparent"
                border.color: root.darkTheme ? "#6627dff8" : "#55008fb8"
                border.width: 1
            }
        }
    }

    component ControlGroup: ColumnLayout {
        id: group
        property string title: ""
        property bool dangerTitle: false
        property int buttonWidth: 72
        property int buttonSpacing: 26
        property var buttons: []
        signal actionRequested(string action)

        Layout.fillWidth: true
        Layout.preferredWidth: group.buttons.length * group.buttonWidth + Math.max(0, group.buttons.length - 1) * group.buttonSpacing
        Layout.fillHeight: true
        spacing: 7

        Text {
            text: group.title
            color: group.dangerTitle ? "#ff3b30" : root.mutedColor
            font.pixelSize: 10
            font.bold: true
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: group.buttonSpacing
            Repeater {
                model: group.buttons
                delegate: Button {
                    Layout.preferredWidth: group.buttonWidth
                    Layout.preferredHeight: 48
                    text: modelData.label
                    enabled: modelData.enabled
                    flat: true
                    padding: 0
                    hoverEnabled: true

                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? modelData.accent : root.disabledColor
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: !parent.enabled ? (root.darkTheme ? "#0b1018" : "#f3f6fb") : parent.down ? (root.darkTheme ? Qt.darker(modelData.accent, 3.0) : "#e1edf8") : parent.hovered ? (root.darkTheme ? (modelData.hoverFill || root.hoverColor) : root.hoverColor) : (root.darkTheme ? (modelData.fill || root.panelColor) : root.panelColor)
                        border.color: parent.enabled ? modelData.accent : root.borderColor
                        border.width: 2
                        radius: 0
                    }

                    onClicked: group.actionRequested(modelData.action)
                }
            }
        }
    }
}
