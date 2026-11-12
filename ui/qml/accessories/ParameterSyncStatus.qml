import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Presentation only. State numbers follow ParameterManager::SyncState and
// Mavlink::ConnectionState; readiness and validated progress come from C++.
ColumnLayout {
    id: root
    property var vehicle: null
    property var mavlinkObject: vehicle ? vehicle.mavlink : null
    readonly property var manager: vehicle ? vehicle.parameterManager : null
    property bool compact: false
    property bool darkTheme: true
    readonly property bool connected: !!mavlinkObject && mavlinkObject.connected
    readonly property bool ready: connected && !!manager && manager.parametersReady
    readonly property int syncStateValue: manager ? manager.syncState : 0
    readonly property bool active: connected && (syncStateValue === 1 || syncStateValue === 2)
    readonly property int progressValue: !connected || !manager ? 0 : ready ? manager.progress : Math.min(99, manager.progress)
    readonly property bool retained: !!manager && manager.hasCompleteSnapshot && !ready
    readonly property string connectionText: connected ? "Vehicle connected"
        : mavlinkObject && mavlinkObject.connectionState === 1 ? "Connecting - awaiting heartbeat" : "No vehicle connected"
    readonly property string statusText: !connected ? (retained ? "Parameters stale - retained snapshot" : "Parameters unavailable")
        : ready ? "Parameters ready"
        : syncStateValue === 1 ? "Loading parameters - " + progressValue + "%"
        : syncStateValue === 2 ? "Recovering parameters - " + progressValue + "%"
        : syncStateValue === 4 ? "Parameter synchronization failed" : "Parameters unavailable"
    readonly property string countText: connected && manager && manager.expectedParameterCount > 0 ? manager.receivedParameterCount + " / " + manager.expectedParameterCount
        + " validated" + (syncStateValue === 2 ? " - " + Math.max(0, manager.expectedParameterCount - manager.receivedParameterCount) + " missing" : "") : ""
    readonly property string detailText: [connectionText, statusText, countText,
        connected && syncStateValue === 4 && manager ? manager.syncError : "",
        connected && retained ? "Previous snapshot is stale; configuration writes unavailable." : ""].filter(function(text) { return text.length > 0 }).join("\n")
    readonly property color statusColor: !connected ? (darkTheme ? "#94a3b8" : "#526680")
        : ready ? "#70b997" : syncStateValue === 4 ? "#d98e88" : syncStateValue === 2 ? "#c7ad78" : "#79afcc"
    spacing: 2
    implicitWidth: compact ? 180 : 300
    Accessible.name: detailText
    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered
    ToolTip.text: detailText
    RowLayout {
        Layout.fillWidth: true
        Label {
            Layout.fillWidth: true
            text: root.compact ? root.statusText : root.connectionText + " - " + root.statusText
            color: root.statusColor
            font.pixelSize: root.compact ? 10 : 12
            elide: root.compact ? Text.ElideRight : Text.ElideNone
            wrapMode: root.compact ? Text.NoWrap : Text.Wrap
        }
        ToolButton {
            objectName: "parameterRetry"
            text: "Retry"
            visible: root.connected && root.syncStateValue === 4
            Accessible.name: "Retry parameter synchronization"
            onClicked: root.mavlinkObject.requestParameters()
        }
    }
    Label {
        Layout.fillWidth: true
        visible: root.compact && (root.active || (root.connected && root.syncStateValue === 4))
        text: root.active ? root.countText : root.manager ? root.manager.syncError : ""
        color: root.statusColor
        font.pixelSize: 9
        elide: Text.ElideRight
    }
    ProgressBar {
        Layout.fillWidth: true
        visible: root.active
        from: 0; to: 100; value: root.progressValue
        implicitHeight: 4
        Accessible.name: root.statusText
    }
    Label {
        Layout.fillWidth: true
        visible: !root.compact && (root.countText.length > 0 || root.retained || (root.connected && root.syncStateValue === 4))
        text: [root.countText, root.connected && root.syncStateValue === 4 && root.manager ? root.manager.syncError : "",
            root.retained ? "Retained snapshot is stale; configuration writes unavailable." : ""].filter(function(text) { return text.length > 0 }).join("\n")
        color: root.statusColor
        font.pixelSize: 11
        wrapMode: Text.Wrap
    }
}
