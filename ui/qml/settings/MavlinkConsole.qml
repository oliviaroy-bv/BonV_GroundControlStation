pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Popup {
    id: root
    objectName: "mavlinkConsole"
    property var controller: null
    property bool armingAllowed: false
    Binding {
        target: root.controller
        property: "armingAllowed"
        value: root.armingAllowed
        when: root.controller !== null
    }
    property real mapRightInset: 300
    property real missionBottomInset: 100
    property bool minimized: false
    property bool detached: false
    property bool paused: false
    property real panelX: -1
    property real panelY: -1
    property real panelWidth: -1
    property real panelHeight: -1
    readonly property bool vehicleConnected: controller && controller.status !== "Disconnected"
    readonly property string firmware: controller ? controller.modeLabel : "No vehicle"
    readonly property string targetLabel: controller ? controller.target.replace("System ", "Sys:").replace(" / Component ", " Comp:") : "No target"
    parent: Overlay.overlay
    x: Math.max(8, Math.min(parent.width - width - 8,
                           panelX >= 0 ? panelX : (parent.width - mapRightInset - width) * 0.61))
    y: Math.max(8, Math.min(parent.height - height - 8,
                           panelY >= 0 ? panelY : parent.height - missionBottomInset - height))
    width: Math.min(parent.width - 16, Math.max(430,
                    panelWidth > 0 ? panelWidth : Math.min(940, (parent.width - mapRightInset) * 0.72)))
    height: minimized ? 64 : Math.min(parent.height - 16, Math.max(230,
                    panelHeight > 0 ? panelHeight : Math.min(354, parent.height - missionBottomInset - 100)))
    padding: 0
    modal: false
    closePolicy: Popup.CloseOnEscape
    onClosed: if (!detached && controller) controller.stop()
    background: Item {}

    function submit() {
        if (controller && controller.send(command.text)) command.clear()
    }
    function popOut() {
        detached = true
        var point = dockHost.mapToGlobal(0, 0)
        floating.x = point.x + Math.min(160, width / 4)
        floating.y = Math.max(0, point.y - 80)
        close()
        floating.show()
        floating.raise()
        floating.requestActivate()
    }
    function dock() {
        floating.hide()
        detached = false
        open()
    }
    function closeConsole() {
        if (detached) floating.close()
        else close()
    }

    contentItem: Item { id: dockHost }
    Window {
        id: floating
        objectName: "mavlinkConsoleWindow"
        title: "BonV GCS - MAVLink Console"
        width: 520
        height: 360
        minimumWidth: 430
        minimumHeight: 260
        color: "#101820"
        flags: Qt.Window
        onClosing: {
            root.detached = false
            if (root.controller) root.controller.stop()
        }
    }

    Rectangle {
        id: surface
        parent: root.detached ? floating.contentItem : dockHost
        anchors.fill: parent
        radius: root.detached ? 0 : 16
        color: "#ed121b24"
        border.color: root.detached ? "#57d7e7" : "#8191a1"
        border.width: 1
        MouseArea {
            id: moveHandle
            objectName: "consoleMoveHandle"
            x: 8; y: 8
            width: Math.max(40, parent.width - 200)
            height: 46
            z: 2
            enabled: !root.detached
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            property point origin
            property real startX: 0
            property real startY: 0
            onPressed: function(mouse) {
                origin = mapToGlobal(mouse.x, mouse.y)
                startX = root.x
                startY = root.y
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                var point = mapToGlobal(mouse.x, mouse.y)
                root.panelX = Math.max(8, Math.min(root.parent.width - root.width - 8, startX + point.x - origin.x))
                root.panelY = Math.max(8, Math.min(root.parent.height - root.height - 8, startY + point.y - origin.y))
            }
        }
        Repeater {
            model: root.detached || root.minimized ? [] : [1, 2, 4, 8, 5, 6, 9, 10]
            delegate: MouseArea {
                id: resizeHandle
                required property int modelData
                objectName: "consoleResize" + modelData
                readonly property bool horizontal: (modelData & 3) !== 0
                readonly property bool vertical: (modelData & 12) !== 0
                width: horizontal ? (vertical ? 16 : 7) : surface.width - 32
                height: vertical ? (horizontal ? 16 : 7) : surface.height - 32
                x: (modelData & 1) ? 0 : (modelData & 2) ? surface.width - width : 16
                y: (modelData & 4) ? 0 : (modelData & 8) ? surface.height - height : 16
                z: 4
                cursorShape: !horizontal ? Qt.SizeVerCursor : !vertical ? Qt.SizeHorCursor
                             : modelData === 5 || modelData === 10 ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                property point origin
                property rect initial
                onPressed: function(mouse) {
                    origin = mapToGlobal(mouse.x, mouse.y)
                    initial = Qt.rect(root.x, root.y, root.width, root.height)
                }
                onPositionChanged: function(mouse) {
                    if (!pressed) return
                    var point = mapToGlobal(mouse.x, mouse.y)
                    var dx = point.x - origin.x, dy = point.y - origin.y
                    var left = initial.x, top = initial.y
                    var right = initial.x + initial.width, bottom = initial.y + initial.height
                    var minWidth = Math.min(430, root.parent.width - 16)
                    var minHeight = Math.min(230, root.parent.height - 16)
                    if (modelData & 1) left = Math.max(8, Math.min(right - minWidth, left + dx))
                    if (modelData & 2) right = Math.min(root.parent.width - 8, Math.max(left + minWidth, right + dx))
                    if (modelData & 4) top = Math.max(8, Math.min(bottom - minHeight, top + dy))
                    if (modelData & 8) bottom = Math.min(root.parent.height - 8, Math.max(top + minHeight, bottom + dy))
                    root.panelX = left
                    root.panelY = top
                    root.panelWidth = right - left
                    root.panelHeight = bottom - top
                }
            }
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            color: "transparent"
            radius: 12
            border.color: "#30404e"
            visible: !root.detached
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: root.detached ? 12 : 16
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Rectangle {
                    visible: !root.detached && surface.width > 480
                    implicitWidth: 24; implicitHeight: 24; radius: 5; color: "#445563"
                    Text { anchors.centerIn: parent; text: ">_"; color: "white"; font.family: "Consolas"; font.bold: true }
                }
                Text {
                    visible: !root.detached && surface.width > 600
                    text: "MAVLink Console"; color: "#f0f4f8"; font.bold: true; font.pixelSize: 14
                }
                Rectangle {
                    Layout.preferredWidth: Math.min(connectionText.implicitWidth + 22,
                                                   Math.max(80, surface.width - (root.detached ? 180 : 225)))
                    Layout.preferredHeight: 28
                    radius: 11
                    color: root.vehicleConnected ? "#172e25" : "#30251b"
                    border.color: root.vehicleConnected ? "#3cae69" : "#b58c55"
                    Text {
                        id: connectionText
                        anchors.centerIn: parent
                        width: parent.width - 22
                        elide: Text.ElideRight
                        text: (root.vehicleConnected ? "Connected (" : "Offline (") + root.targetLabel + ")"
                        color: root.vehicleConnected ? "#61db87" : "#e8b779"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
                Text {
                    text: "|  " + root.firmware
                    visible: surface.width > 480
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    elide: Text.ElideRight
                    color: "#dde6f0"
                    font.pixelSize: 12
                }
                Item { Layout.fillWidth: true }
                ConsoleButton {
                    objectName: "popOutConsoleButton"
                    glyphName: root.detached ? "dock" : "popout"
                    hint: root.detached ? "Return to Main Window" : "Open in New Window"
                    onClicked: root.detached ? root.dock() : root.popOut()
                }
                ConsoleButton { glyphName: "trash"; hint: "Clear"; onClicked: if (root.controller) root.controller.clear() }
                ConsoleButton { glyphName: root.paused ? "play" : "pause"; hint: root.paused ? "Resume auto-scroll" : "Pause auto-scroll"; onClicked: root.paused = !root.paused }
                ConsoleButton { objectName: "closeConsoleButton"; visible: !root.detached; glyphName: "close"; hint: "Close"; onClicked: root.closeConsole() }
            }
            Text {
                Layout.fillWidth: true
                visible: !root.minimized && root.controller && root.controller.commandMode
                text: "ArduPilot MAVLink Console - supported MAVLink commands, not an unrestricted shell."
                color: "#8faabf"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
            Rectangle {
                visible: !root.minimized
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#b3091017"
                radius: 8
                border.color: "#344451"
                ListView {
                    id: output
                    objectName: "consoleOutput"
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    model: root.controller ? root.controller.history : []
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        contentItem: Rectangle { implicitWidth: 4; radius: 2; color: "#758799"; opacity: 0.85 }
                    }
                    onModelChanged: if (!root.paused) Qt.callLater(function() { output.positionViewAtEnd() })
                    delegate: RowLayout {
                        id: entry
                        required property var modelData
                        width: output.width - 10
                        spacing: 12
                        Text { text: "[" + entry.modelData.time + "]"; color: "#89949e"; font.family: "Consolas"; font.pixelSize: 12; Layout.alignment: Qt.AlignTop }
                        Text {
                            Layout.preferredWidth: 44
                            Layout.alignment: Qt.AlignTop
                            text: entry.modelData.kind === "warning" ? "WARN" : entry.modelData.kind === "error" ? "ERR" : entry.modelData.kind === "output" ? "OUT" : "INFO"
                            color: entry.modelData.kind === "warning" ? "#ffc46e" : entry.modelData.kind === "error" ? "#ff7777" : entry.modelData.kind === "output" ? "#eef3f7" : "#00e5f5"
                            font.family: "Consolas"; font.pixelSize: 12
                        }
                        TextEdit {
                            Layout.fillWidth: true
                            text: (entry.modelData.kind === "input" ? "> " : "") + entry.modelData.text
                            readOnly: true
                            selectByMouse: true
                            textFormat: TextEdit.PlainText
                            wrapMode: TextEdit.WrapAnywhere
                            font.family: "Consolas"
                            font.pixelSize: 12
                            color: entry.modelData.kind === "input" ? "#f6d37c" : "#edf3f8"
                        }
                    }
                }
                Text {
                    anchors.fill: parent
                    anchors.margins: 14
                    visible: output.count === 0
                    text: root.controller ? root.controller.status : "No autopilot connected"
                    wrapMode: Text.Wrap
                    color: "#8fa2b3"
                    font.family: "Consolas"
                    font.pixelSize: 12
                }
            }
            RowLayout {
                visible: !root.minimized
                Layout.fillWidth: true
                spacing: 4
                TextField {
                    id: command
                    objectName: "consoleCommandInput"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    maximumLength: 4096
                    leftPadding: 14
                    color: "#f0f5fa"
                    placeholderTextColor: "#b6c3cf"
                    placeholderText: root.controller && root.controller.commandMode ? ">  status, gps status, arm, mode GUIDED, param get NAME..."
                        : root.controller && root.controller.running ? ">  Type command..." : ">  " + (root.controller ? root.controller.status : "Shell unavailable")
                    font.pixelSize: 13
                    selectByMouse: true
                    onAccepted: root.submit()
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_C && (event.modifiers & Qt.ControlModifier)
                                && selectedText.length === 0 && root.controller && !root.controller.commandMode) {
                            event.accepted = root.controller.interrupt()
                        }
                    }
                    background: Rectangle { radius: 5; color: "#26313d"; border.color: command.activeFocus ? "#25cfdf" : "#8293a7"; border.width: 1 }
                }
                ConsoleButton {
                    glyphName: "send"
                    hint: "Send command"
                    accent: true
                    implicitWidth: 42
                    implicitHeight: 40
                    enabled: root.controller && root.controller.running
                    onClicked: root.submit()
                }
                ConsoleButton {
                    visible: root.controller && root.controller.available && !root.controller.running
                    glyphName: "play"
                    hint: "Start Shell (explicitly initialize session)"
                    onClicked: root.controller.start()
                }
            }
        }
    }
    Connections {
        target: root.controller
        function onChanged() { if (!root.controller.available) command.clear() }
    }
    component ConsoleButton: Button {
        id: action
        property string glyphName: ""
        property string hint: ""
        property bool accent: false
        implicitWidth: surface.width < 480 ? 30 : 38
        implicitHeight: 38
        padding: 0
        hoverEnabled: true
        ToolTip.visible: hovered
        ToolTip.text: hint
        background: Rectangle {
            radius: 5
            color: action.accent ? (action.hovered ? "#35deeb" : "#08bbcf") : action.hovered ? "#3b4c5b" : "#26313d"
            border.color: action.accent ? "#368ea1" : "#3d4c5b"
            opacity: action.enabled ? 1 : 0.45
        }
        contentItem: Canvas {
            id: glyph
            property string symbol: action.glyphName
            onSymbolChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = action.accent ? "#073c49" : "#eef3f8"
                ctx.fillStyle = ctx.strokeStyle
                ctx.lineWidth = 2
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                var cx = width / 2, cy = height / 2
                if (symbol === "close") {
                    ctx.beginPath(); ctx.moveTo(cx-4,cy-4); ctx.lineTo(cx+4,cy+4); ctx.moveTo(cx+4,cy-4); ctx.lineTo(cx-4,cy+4); ctx.stroke()
                } else if (symbol === "pause") {
                    ctx.fillRect(cx-5,cy-6,3,12); ctx.fillRect(cx+2,cy-6,3,12)
                } else if (symbol === "send" || symbol === "play") {
                    ctx.beginPath(); ctx.moveTo(cx-5,cy-7); ctx.lineTo(cx+7,cy); ctx.lineTo(cx-5,cy+7); ctx.lineTo(cx-2,cy); ctx.closePath(); ctx.fill()
                } else if (symbol === "trash") {
                    ctx.strokeRect(cx-5,cy-4,10,12); ctx.beginPath(); ctx.moveTo(cx-7,cy-6); ctx.lineTo(cx+7,cy-6); ctx.moveTo(cx-2,cy-9); ctx.lineTo(cx+2,cy-9); ctx.moveTo(cx-2,cy-1); ctx.lineTo(cx-2,cy+5); ctx.moveTo(cx+2,cy-1); ctx.lineTo(cx+2,cy+5); ctx.stroke()
                } else {
                    ctx.beginPath(); ctx.moveTo(cx-1,cy-7); ctx.lineTo(cx-7,cy-7); ctx.lineTo(cx-7,cy+7); ctx.lineTo(cx+7,cy+7); ctx.lineTo(cx+7,cy+1); ctx.moveTo(cx-1,cy+1); ctx.lineTo(cx+7,cy-7); ctx.moveTo(cx+1,cy-7); ctx.lineTo(cx+7,cy-7); ctx.lineTo(cx+7,cy-1); ctx.stroke()
                }
            }
        }
    }
}
