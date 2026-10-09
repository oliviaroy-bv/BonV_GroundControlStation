pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var mavlinkObject: null
    readonly property var forwarder: mavlinkObject ? mavlinkObject.forwarder : null
    property bool darkTheme: true
    property color panelColor: darkTheme ? "#171a20" : "#ffffff"
    property color borderColor: darkTheme ? "#343c48" : "#ccd6e4"
    property color textColor: darkTheme ? "#f0f3f6" : "#101722"
    property color mutedColor: darkTheme ? "#778391" : "#66758a"
    property color accentColor: darkTheme ? "#34c3d6" : "#008fb0"
    property string draftHost: forwarder ? forwarder.host : "127.0.0.1"
    property int draftPort: forwarder ? forwarder.port : 14550
    property bool draftEnabled: forwarder ? forwarder.enabled : false
    property string validationError: ""

    implicitHeight: content.implicitHeight

    function bytesText(value) {
        var number = Number(value)
        if (!isFinite(number)) return "0 B"
        if (number >= 1048576) return (number / 1048576).toFixed(1) + " MB"
        if (number >= 1024) return (number / 1024).toFixed(1) + " KB"
        return number.toFixed(0) + " B"
    }
    function applyForwarding() {
        validationError = ""
        if (draftEnabled && draftHost.trim().length === 0) {
            validationError = "Destination host is required"
            return
        }
        if (draftEnabled && (draftPort < 1 || draftPort > 65535)) {
            validationError = "UDP port must be between 1 and 65535"
            return
        }
        if (!forwarder || !forwarder.apply(draftEnabled, draftHost.trim(), draftPort))
            validationError = forwarder ? forwarder.lastError : "Forwarder is unavailable"
    }

    Connections {
        target: root.forwarder
        function onConfigurationChanged() {
            root.draftHost = root.forwarder.host
            root.draftPort = root.forwarder.port
            root.draftEnabled = root.forwarder.enabled
        }
    }

    ColumnLayout {
        id: content
        width: parent.width
        spacing: 10

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 72; radius: 4
            color: root.panelColor; border.color: root.borderColor; border.width: 1
            RowLayout {
                anchors.fill: parent; anchors.margins: 14
                ColumnLayout { Layout.fillWidth: true; spacing: 2
                    Text { text: "MAVLink Forwarding"; color: root.textColor; font.pixelSize: 18; font.bold: true }
                    Text { text: "One-way GCS → UDP copy of received MAVLink wire packets"; color: root.mutedColor; font.pixelSize: 11 }
                }
                Text { text: root.forwarder ? root.forwarder.state : "DISABLED"; color: root.forwarder && root.forwarder.state === "ERROR" ? "#e06c75" : root.accentColor; font.pixelSize: 11; font.bold: true }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: settingsColumn.implicitHeight + 28; radius: 4
            color: root.panelColor; border.color: root.borderColor; border.width: 1
            ColumnLayout { id: settingsColumn; anchors.fill: parent; anchors.margins: 14; spacing: 10
                RowLayout { Layout.fillWidth: true
                    Text { text: "Forwarding"; color: root.textColor; font.pixelSize: 13; font.bold: true; Layout.fillWidth: true }
                    Switch { checked: root.draftEnabled; onToggled: root.draftEnabled = checked }
                }
                TextField { Layout.fillWidth: true; text: root.draftHost; placeholderText: "Destination host or IP"; color: root.textColor; onTextEdited: root.draftHost = text; background: Rectangle { color: root.darkTheme ? "#11161b" : "#f7f9fc"; border.color: root.borderColor; border.width: 1; radius: 3 } }
                SpinBox { Layout.preferredWidth: 150; from: 1; to: 65535; value: root.draftPort; editable: true; onValueModified: root.draftPort = value }
                Text { visible: root.validationError.length > 0 || (root.forwarder && root.forwarder.lastError.length > 0); text: root.validationError.length > 0 ? root.validationError : root.forwarder.lastError; color: "#e06c75"; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Button { text: "APPLY"; Layout.alignment: Qt.AlignRight; onClicked: root.applyForwarding() }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 94; radius: 4
            color: root.panelColor; border.color: root.borderColor; border.width: 1
            RowLayout { anchors.fill: parent; anchors.margins: 14; spacing: 0
                Stat { Layout.fillWidth: true; label: "FORWARDED"; value: root.forwarder ? String(root.forwarder.forwardedPackets) : "0" }
                Stat { Layout.fillWidth: true; label: "BYTES"; value: root.forwarder ? root.bytesText(root.forwarder.forwardedBytes) : "0 B" }
                Stat { Layout.fillWidth: true; label: "FAILED"; value: root.forwarder ? String(root.forwarder.failedPackets) : "0" }
                Stat { Layout.fillWidth: true; label: "RATE"; value: root.forwarder ? Number(root.forwarder.forwardRate).toFixed(1) + " pkt/s" : "0.0 pkt/s" }
            }
        }
        Text { text: "Direction: GCS → UDP. All valid inbound MAVLink packets are forwarded unchanged."; color: root.mutedColor; font.pixelSize: 11 }
    }

    component Stat: ColumnLayout {
        id: stat
        property string label: ""; property string value: ""
        Text { text: stat.label; color: root.mutedColor; font.pixelSize: 10; font.bold: true; Layout.alignment: Qt.AlignHCenter }
        Text { text: stat.value; color: root.textColor; font.pixelSize: 15; font.bold: true; Layout.alignment: Qt.AlignHCenter }
    }
}
