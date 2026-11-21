pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "commLinkDiagnosticsRow"
    property var linkRecord: ({})
    property var diagnostics: ({})
    property var qualityDiagnostics: ({})
    property string endpoint: ""
    property bool connectionActive: false
    property bool connecting: false
    property bool expanded: false
    property bool darkTheme: true
    property color textColor: "#e5edf5"
    property color mutedColor: "#91a1b4"
    property color accentColor: "#00cfe8"
    property color hoverColor: "#18212c"
    property color borderColor: "#1c2636"
    readonly property string linkKey: (linkRecord.protocol || "UDP").toUpperCase() + ":" + linkRecord.address
        + ((linkRecord.protocol || "UDP").toUpperCase() === "SERIAL" ? ":" + (linkRecord.baudRate || 57600) : "")
    readonly property bool attributed: endpoint.length > 0 && endpoint === linkKey
    readonly property bool canExpand: connectionActive && attributed
    readonly property var linkData: attributed ? diagnostics : ({})
    readonly property var qualityData: attributed && (qualityDiagnostics.connectionGeneration === undefined
        || diagnostics.connectionGeneration === undefined || qualityDiagnostics.connectionGeneration === diagnostics.connectionGeneration) ? qualityDiagnostics : ({})
    readonly property color goodColor: darkTheme ? "#26e47f" : "#138946"
    readonly property color dangerColor: darkTheme ? "#ff353d" : "#c82533"
    readonly property var qualityScore: qualityData.qualityEstimate !== undefined ? qualityData.qualityEstimate : linkData.quality
    readonly property string sequenceText: !qualityData.confidence || qualityData.confidence === "unavailable" ? "N/A"
        : qualityData.confidence.indexOf("limited") === 0 ? "Limited" : "Measured"
    readonly property string continuityText: qualityData.availableMetrics && qualityData.availableMetrics.indexOf("sequence continuity estimate") >= 0 ? "Fresh"
        : qualityData.staleMetrics && qualityData.staleMetrics.indexOf("sequence (stale or insufficient evidence)") >= 0 ? "Limited" : "N/A"
    signal connectionRequested()
    signal cancelRequested()
    implicitHeight: expanded && canExpand ? 180 : 54
    height: implicitHeight
    radius: 8; border.width: 1
    color: root.darkTheme ? "#80101d28" : "#f5f9fc"
    border.color: root.canExpand ? (root.darkTheme ? "#1c6070" : "#78b8c3") : root.borderColor
    function value(key, unit) {
        var result = linkData[key]
        return result === undefined || result < 0 ? "N/A" : result + (unit || "")
    }
    function toggleDiagnostics() { if (canExpand) expanded = !expanded }
    onLinkKeyChanged: expanded = false
    onCanExpandChanged: if (!canExpand) expanded = false
    RowLayout {
        anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: 16; anchors.rightMargin: 8
        height: 54; spacing: 12
        Rectangle {
            objectName: "linkConnectionIndicator"
            Layout.preferredWidth: 10; Layout.preferredHeight: 10; radius: 5
            color: root.canExpand ? root.goodColor : root.connecting ? "#d8a94d" : "#747d86"
        }
        ColumnLayout {
            Layout.fillWidth: true; spacing: 2
            Text { Layout.fillWidth: true; text: root.linkRecord.name || "Link"; color: root.textColor; font.family: "Segoe UI"; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight }
            Text {
                Layout.fillWidth: true
                text: (root.linkRecord.protocol || "UDP").toUpperCase() === "SERIAL"
                    ? root.linkRecord.address + " @ " + (root.linkRecord.baudRate || 57600) : root.linkRecord.address || ""
                color: root.mutedColor; font.family: "Segoe UI"; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight
            }
        }
        Button {
            id: selectButton
            objectName: "linkSelection"
            Layout.preferredHeight: 28
            Layout.preferredWidth: root.canExpand ? 92 : root.connecting ? 102 : 74
            enabled: !root.connecting
            padding: 6; hoverEnabled: true
            onClicked: root.connectionRequested()
            Accessible.name: text + " " + (root.linkRecord.name || "link")
            text: root.canExpand ? "DISCONNECT" : root.connecting ? "CONNECTING" : "CONNECT"
            contentItem: Text {
                text: selectButton.text; color: root.canExpand ? root.dangerColor : root.goodColor
                font.family: "Segoe UI"; font.pixelSize: 10; font.bold: true
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 4; color: selectButton.hovered ? root.hoverColor : "transparent"
                border.color: root.canExpand ? root.dangerColor : root.darkTheme ? "#12633d" : "#75b68e"
            }
        }
        ToolButton {
            objectName: "cancelLink"; visible: root.connecting
            Layout.preferredWidth: 28; Layout.preferredHeight: 28
            onClicked: root.cancelRequested(); Accessible.name: "Cancel connection"
            contentItem: LinkDiagnosticIcon { name: "close"; color: root.mutedColor }
        }
        ToolButton {
            id: expandButton
            objectName: "expandDiagnostics"
            Layout.preferredWidth: 30; Layout.preferredHeight: 30
            enabled: root.canExpand; padding: 6; hoverEnabled: true
            onClicked: root.toggleDiagnostics()
            Accessible.name: root.expanded ? "Collapse link diagnostics" : "Expand link diagnostics"
            ToolTip.visible: hovered
            ToolTip.text: enabled ? Accessible.name : "Connect this link to view diagnostics"
            contentItem: LinkDiagnosticIcon {
                name: root.expanded ? "up" : "down"
                color: expandButton.enabled ? root.textColor : root.mutedColor
                opacity: expandButton.enabled ? 1 : 0.4
            }
            background: Rectangle {
                radius: 5; color: root.darkTheme ? "#8010202b" : "#eaf1f6"
                border.color: expandButton.enabled ? root.accentColor : root.borderColor
                opacity: expandButton.enabled ? 1 : 0.6
            }
        }
    }
    Column {
        objectName: "rssiDetails"
        x: 14; y: 60; width: parent.width - 28
        visible: root.expanded && root.canExpand; spacing: 8
        Row {
            width: parent.width; spacing: 8
            MetricCard { width: (parent.width - 16) / 3; label: "Link Quality"; iconName: "signal"; metric: root.qualityScore === undefined || root.qualityScore < 0 ? "N/A" : root.qualityScore + " / 100"; valueColor: root.goodColor; explanation: "Estimated link quality, not a calibrated signal-strength measurement." }
            MetricCard { width: (parent.width - 16) / 3; label: "Sequence"; iconName: "layers"; metric: root.sequenceText; explanation: root.qualityData.confidence || "Sequence evidence unavailable" }
            MetricCard { width: (parent.width - 16) / 3; label: "Continuity"; iconName: "pulse"; metric: root.continuityText; valueColor: root.continuityText === "Fresh" ? root.goodColor : root.textColor; explanation: "Freshness of the backend's sequence continuity evidence." }
        }
        Row {
            width: parent.width; spacing: 8
            MetricCard { width: (parent.width - 8) / 2; label: "Median RTT"; iconName: "clock"; metric: root.value("latencyMs", " ms"); explanation: "Median round-trip time from MAVLink PING responses." }
            MetricCard { width: (parent.width - 8) / 2; label: "Raw RSSI"; iconName: "radio"; metric: root.value("rssi", ""); explanation: "Uncalibrated device-dependent raw RSSI, not dBm or percent." }
        }
    }
    component MetricCard: Rectangle {
        id: card
        property string label: ""
        property string iconName: "signal"
        property string metric: "N/A"
        property string explanation: ""
        property color valueColor: root.textColor
        height: 52; radius: 6
        color: root.darkTheme ? "#c010202a" : "#e9f1f6"
        border.color: root.darkTheme ? "#142d39" : "#d3e1e9"
        RowLayout {
            anchors.fill: parent; anchors.margins: 9; spacing: 7
            LinkDiagnosticIcon { name: card.iconName; color: root.accentColor; Layout.preferredWidth: 18; Layout.preferredHeight: 18 }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 2
                Text { text: card.label; color: root.accentColor; font.family: "Segoe UI"; font.pixelSize: 10; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: card.metric; color: card.valueColor; font.family: "Segoe UI"; font.pixelSize: 12; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
            }
        }
        HoverHandler { id: hover }
        ToolTip.visible: hover.hovered; ToolTip.text: card.explanation
        Accessible.name: label + ": " + metric
    }
}
