pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Column {
    id: root
    objectName: "commLinkDiagnosticsRow"
    property var linkRecord: ({})
    property var diagnostics: ({})
    property string endpoint: ""
    property bool connectionActive: false
    property bool connecting: false
    property bool expanded: false
    property bool darkTheme: true
    property color textColor: "#e5edf5"
    property color mutedColor: "#91a1b4"
    property color accentColor: "#00bdd6"
    property color hoverColor: "#18212c"
    property color borderColor: "#1c2636"
    readonly property string linkKey: (linkRecord.protocol || "UDP").toUpperCase() + ":" + linkRecord.address
        + ((linkRecord.protocol || "UDP").toUpperCase() === "SERIAL" ? ":" + (linkRecord.baudRate || 57600) : "")
    readonly property bool attributed: endpoint.length > 0 && endpoint === linkKey
    readonly property bool canExpand: connectionActive && attributed
    readonly property var linkData: attributed ? diagnostics : ({})
    signal connectionRequested()
    signal cancelRequested()
    spacing: 4
    function value(key, unit) {
        var result = linkData[key]
        return result === undefined || result < 0 ? "N/A" : result + (unit || "")
    }
    function toggleDiagnostics() { if (canExpand) expanded = !expanded }
    onLinkKeyChanged: expanded = false
    onCanExpandChanged: if (!canExpand) expanded = false
    RowLayout {
        width: root.width
        spacing: 0
        Button {
            id: selectButton
            objectName: "linkSelection"
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            flat: true
            padding: 0
            hoverEnabled: true
            onClicked: if (!root.connecting) root.connectionRequested()
            background: Rectangle {
                color: selectButton.hovered ? root.hoverColor : "transparent"
                border.color: root.canExpand ? (root.darkTheme ? "#663a2424" : "#efc2c2") : root.borderColor
                border.width: 1
            }
            contentItem: RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 10
                Rectangle {
                    Layout.preferredWidth: 9; Layout.preferredHeight: 9
                    radius: 5
                    color: root.canExpand ? "#ff4b46" : "#20d86c"
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Text { Layout.fillWidth: true; text: root.linkRecord.name || "Link"; color: root.textColor; font.pixelSize: 12; font.bold: true; elide: Text.ElideRight }
                    Text {
                        Layout.fillWidth: true
                        text: (root.linkRecord.protocol || "UDP").toUpperCase() === "SERIAL"
                            ? root.linkRecord.address + " @ " + (root.linkRecord.baudRate || 57600) : root.linkRecord.address || ""
                        color: root.mutedColor; font.pixelSize: 10; elide: Text.ElideRight
                    }
                }
                BusyIndicator { visible: root.connecting; running: visible; Layout.preferredWidth: 16; Layout.preferredHeight: 16 }
                Text {
                    text: root.canExpand ? "DISCONNECT" : root.connecting ? "CONNECTING" : "CONNECT"
                    color: root.canExpand ? "#ff6964" : "#35dc79"; font.pixelSize: 10; font.bold: true
                }
                ToolButton {
                    objectName: "cancelLink"
                    visible: root.connecting
                    Layout.preferredWidth: 22; Layout.preferredHeight: 22
                    text: "\u00d7"
                    onClicked: root.cancelRequested()
                    Accessible.name: "Cancel connection"
                }
            }
        }
        ToolButton {
            objectName: "expandDiagnostics"
            Layout.preferredWidth: 22; Layout.preferredHeight: 42
            enabled: root.canExpand
            opacity: enabled ? 1 : 0.3
            text: root.expanded ? "\u2303" : "\u2304"
            onClicked: root.toggleDiagnostics()
            Accessible.name: root.expanded ? "Collapse RSSI" : "Expand RSSI"
            ToolTip.visible: hovered
            ToolTip.text: enabled ? Accessible.name : "Connect this link to view RSSI"
        }
    }
    Column {
        objectName: "rssiDetails"
        width: root.width - 20
        x: 10
        visible: root.expanded && root.canExpand
        spacing: 4
        Repeater {
            model: [["Local RSSI (raw)", "rssi"], ["Remote RSSI (raw)", "remoteRssi"]]
            delegate: RowLayout {
                id: metric
                required property var modelData
                width: parent.width
                Text { Layout.fillWidth: true; text: metric.modelData[0]; color: root.mutedColor; font.pixelSize: 11 }
                Text { text: root.value(metric.modelData[1], ""); color: root.textColor; font.pixelSize: 11 }
            }
        }
    }
}
