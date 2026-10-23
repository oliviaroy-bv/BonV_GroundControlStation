import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.settings
import QtQuick.Window
import "../settings"

Rectangle {
    id: root

    property var mavlinkObject: null
    property var vehicle: null
    property var videoObject: null
    property string homeDistanceText: "-- m"
    property string flightTimeText: "--:--"
    property string flightModeText: "--"
    property string batteryText: "-- %"
    property string satellitesText: "--"
    property string mapViewMode: "satellite"
    property string currentRtspLink: ""
    property string currentCameraName: ""
    property bool darkTheme: true
    property bool armed: false
    property bool vpsModuleEnabled: false
    property bool vpsVioMarker: false
    property bool vpsVprMarker: false
    property bool vpsFusedMarker: false
    signal vpsMarkerChanged(string marker, bool enabled)
    property bool oemSettingsOpen: false
    property var commLinks: []
    property var cameraLinks: []
    property string connectedCommAddress: ""
    property string connectedCameraAddress: ""
    property string pendingCommAddress: ""
    property string pendingCameraAddress: ""
    property bool screenRecording: false
    property string screenRecordingFilePath: ""
    readonly property bool vehicleConnected: root.mavlinkObject ? root.mavlinkObject.connected : false
    readonly property int commLinkQuality: root.vehicle ? root.vehicle.commLinkQuality : -1
    readonly property string commStatusText: !root.vehicleConnected ? "NO LINK"
                                                 : root.commLinkQuality < 0 ? "CONNECTED"
                                                 : root.commLinkQuality >= 85 ? "STRONG " + root.commLinkQuality + "%"
                                                 : root.commLinkQuality >= 55 ? "FAIR " + root.commLinkQuality + "%"
                                                 : "WEAK " + root.commLinkQuality + "%"
    readonly property bool cameraConnected: root.videoObject && root.videoObject.streaming
    readonly property string camStatusText: root.cameraConnected ? "STRONG" : "NO LINK"
    readonly property int gpsFixType: root.vehicle ? root.vehicle.gpsFix : 0
    readonly property bool gpsHasFix: root.vehicleConnected && root.gpsFixType >= 2
    readonly property string gpsStatusText: root.vehicleConnected ? root.gpsFixStatus(root.gpsFixType).toUpperCase() : "NO GPS"
    readonly property bool batteryKnown: root.vehicleConnected && root.vehicle && root.vehicle.battery >= 0
    readonly property int batteryPercent: root.batteryKnown ? Math.round(root.vehicle.battery) : 0
    readonly property color batteryChargeColor: !root.batteryKnown ? root.inactiveColor : root.batteryPercent >= 60 ? "#2aae58" : root.batteryPercent >= 40 ? "#f7a21b" : "#ff4b46"
    readonly property color chromeColor: root.darkTheme ? "#f2070b11" : "#f8fbff"
    readonly property color panelColor: root.darkTheme ? "#00000000" : "#ffffff"
    readonly property color hoverColor: root.darkTheme ? "#111b29" : "#eef5fc"
    readonly property color downColor: root.darkTheme ? "#182638" : "#ddeaf8"
    readonly property color borderColor: root.darkTheme ? "#1c2636" : "#cfdceb"
    readonly property color textColor: root.darkTheme ? "#f7fbff" : "#0f213b"
    readonly property color mutedColor: root.darkTheme ? "#8291ad" : "#526680"
    readonly property color inactiveColor: root.darkTheme ? "#6f7c91" : "#75869d"
    readonly property color accentColor: root.darkTheme ? "#00c7df" : "#008fb8"
    readonly property color actionIconColor: root.darkTheme ? "#9bb3db" : "#486382"

    signal cameraLinkRequested(string rtspLink, string cameraName)
    signal mapViewModeRequested(string mapViewMode)
    signal generateMissionRequested()
    signal preflightChecklistRequested()
    signal motorTestRequested()
    signal servoPanelRequested()
    signal statusMessageRequested(string message)
    signal themeModeRequested(bool darkTheme)
    signal screenRecordingRequested()
    signal oemSettingsRequested()
    signal oemSettingsCloseRequested()
    signal preflightChecklistEnabledChanged(bool enabled)

    width: parent ? parent.width : 0
    height: 60
    color: root.chromeColor
    border.color: root.borderColor
    border.width: 1
    z: 100

    Settings {
        id: persistedLinkSettings
        category: "SettingsWindowLinks"
        property string commLinksJson: ""
        property string cameraLinksJson: ""
        onCommLinksJsonChanged: root.loadCommLinks()
        onCameraLinksJsonChanged: root.loadCameraLinks()
    }

    Settings {
        id: servoPanelSettings
        category: "ServoPanel"
        property bool showTopPanelButton: true
    }

    function applyServoSettings(showTopPanelButton) {
        servoPanelSettings.showTopPanelButton = showTopPanelButton
    }

    function resetConnectionStates(links) {
        return links.map(function(link) {
            return {
                name: link.name,
                address: link.address,
                protocol: link.protocol || "UDP",
                baudRate: link.baudRate || 57600,
                connected: root.mavlinkObject && root.mavlinkObject.connected && link.address === root.connectedCommAddress
            }
        })
    }

    function resetCameraConnectionStates(links) {
        var activeAddress = root.connectedCameraAddress.length > 0 ? root.connectedCameraAddress : root.currentRtspLink
        return links.map(function(link) {
            return {
                name: link.name,
                address: link.address,
                connected: root.cameraConnected && activeAddress.length > 0 && link.address === activeAddress
            }
        })
    }

    function syncActiveCameraLink() {
        if (!root.cameraConnected || root.currentRtspLink.length === 0) {
            if (!root.cameraConnected) {
                root.connectedCameraAddress = ""
                root.pendingCameraAddress = ""
                settingsWindow.setCameraLinkConnectionState("", false)
            }
            root.loadCameraLinks()
            return
        }

        root.connectedCameraAddress = root.currentRtspLink
        root.pendingCameraAddress = ""
        root.loadCameraLinks()
        settingsWindow.setCameraLinkConnectionState(root.connectedCameraAddress, true)
    }

    function loadCommLinks() {
        if (persistedLinkSettings.commLinksJson.length === 0) {
            commLinks = []
            return
        }

        try {
            var parsedLinks = JSON.parse(persistedLinkSettings.commLinksJson)
            commLinks = Array.isArray(parsedLinks) ? resetConnectionStates(parsedLinks) : []
        } catch (error) {
            commLinks = []
        }
    }

    function loadCameraLinks() {
        if (persistedLinkSettings.cameraLinksJson.length === 0) {
            cameraLinks = []
            return
        }

        try {
            var parsedLinks = JSON.parse(persistedLinkSettings.cameraLinksJson)
            cameraLinks = Array.isArray(parsedLinks) ? resetCameraConnectionStates(parsedLinks) : []
        } catch (error) {
            cameraLinks = []
        }
    }

    function markSelectedCommLink(linkIndex) {
        connectedCommAddress = linkIndex >= 0 && commLinks[linkIndex] ? commLinks[linkIndex].address : ""
        pendingCommAddress = ""
        commLinks = commLinks.map(function(link, index) {
            return { name: link.name, address: link.address, protocol: link.protocol || "UDP", baudRate: link.baudRate || 57600, connected: index === linkIndex }
        })
        settingsWindow.setCommLinkConnectionState(linkIndex, linkIndex >= 0)
    }

    function markSelectedCameraLink(linkIndex) {
        connectedCameraAddress = linkIndex >= 0 && cameraLinks[linkIndex] ? cameraLinks[linkIndex].address : ""
        pendingCameraAddress = ""
        cameraLinks = cameraLinks.map(function(link, index) {
            return { name: link.name, address: link.address, connected: index === linkIndex }
        })
    }

    function connectCommLink(linkIndex) {
        var commLink = commLinks[linkIndex]
        if (!commLink || !root.mavlinkObject) return

        if (commLink.connected) {
            markSelectedCommLink(-1)
            if (root.mavlinkObject.connected) root.mavlinkObject.disconnectFromVehicle()
            root.statusMessageRequested("Disconnected from " + commLink.name)
            return
        }

        var protocol = commLink.protocol || "UDP"
        if (protocol === "Serial") {
            if (root.mavlinkObject.connected) root.mavlinkObject.disconnectFromVehicle()
            pendingCommAddress = commLink.address
            var serialConnectionStarted = root.mavlinkObject.connectToVehicleProtocol("Serial", commLink.address, 0, commLink.baudRate || 57600)
            if (!serialConnectionStarted)
                pendingCommAddress = ""
            root.statusMessageRequested(serialConnectionStarted ? "Connecting to " + commLink.name : "Connection failed: " + commLink.name)
            return
        }

        var addressParts = commLink.address.split(":")
        if (addressParts.length < 2) return
        var port = parseInt(addressParts[addressParts.length - 1])
        var host = addressParts.slice(0, addressParts.length - 1).join(":")
        if (host.length === 0 || isNaN(port)) {
            pendingCommAddress = ""
            return
        }

        if (root.mavlinkObject.connected) root.mavlinkObject.disconnectFromVehicle()
        pendingCommAddress = commLink.address
        var networkConnectionStarted = root.mavlinkObject.connectToVehicleProtocol(protocol, host, port, 0)
        if (!networkConnectionStarted)
            pendingCommAddress = ""
        root.statusMessageRequested(networkConnectionStarted ? "Connecting to " + commLink.name : "Connection failed: " + commLink.name)
    }

    function connectCameraLink(linkIndex) {
        var cameraLink = cameraLinks[linkIndex]
        if (!cameraLink) return

        if (cameraLink.connected) {
            markSelectedCameraLink(-1)
            root.cameraLinkRequested("", "")
            root.statusMessageRequested("Disconnected from " + cameraLink.name)
            return
        }

        pendingCameraAddress = cameraLink.address
        root.cameraLinkRequested(cameraLink.address, cameraLink.name)
        root.statusMessageRequested("Connecting to camera " + cameraLink.name)
    }

    function cancelPendingCommLink() {
        if (pendingCommAddress.length === 0)
            return
        if (root.mavlinkObject)
            root.mavlinkObject.disconnectFromVehicle()
        markSelectedCommLink(-1)
        root.statusMessageRequested("Connection cancelled")
    }

    function cancelPendingCameraLink() {
        if (pendingCameraAddress.length === 0)
            return
        pendingCameraAddress = ""
        connectedCameraAddress = ""
        root.cameraLinkRequested("", "")
        root.loadCameraLinks()
        settingsWindow.setCameraLinkConnectionState("", false)
        root.statusMessageRequested("Camera connection cancelled")
    }

    function gpsFixStatus(fixType) {
        switch (Number(fixType)) {
        case 1: return "No fix"
        case 2: return "2D fix"
        case 3: return "3D fix"
        case 4: return "DGPS"
        case 5: return "RTK float"
        case 6: return "RTK fixed"
        case 7: return "Static"
        case 8: return "PPP"
        case 0:
        default:
            return "No GPS"
        }
    }

    function validTelemetryValue(value) {
        return value !== undefined && value !== null && Number(value) >= 0
    }

    function telemetryNumberText(value, decimals, suffix) {
        if (!root.validTelemetryValue(value))
            return "--"
        var text = Number(value).toFixed(decimals)
        return suffix && suffix.length > 0 ? text + " " + suffix : text
    }

    function telemetryIntText(value) {
        if (!root.validTelemetryValue(value))
            return "--"
        return Number(value).toFixed(0)
    }

    function coordinateText(value) {
        if (value === undefined || value === null || !isFinite(Number(value)))
            return "--"
        return Number(value).toFixed(6)
    }

    Component.onCompleted: {
        loadCommLinks()
        loadCameraLinks()
        syncActiveCameraLink()
    }

    onCurrentRtspLinkChanged: syncActiveCameraLink()
    onCameraConnectedChanged: syncActiveCameraLink()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 0

        StatusTile {
            Layout.preferredWidth: 112
            iconType: "comm"
            label: "COMM LINK"
            value: root.commStatusText
            active: root.vehicleConnected
            onClicked: {
                commLinksPopover.opened ? commLinksPopover.close() : commLinksPopover.open()
                root.statusMessageRequested(commLinksPopover.opened ? "Communication links opened" : "Communication links closed")
            }
        }

        StatusTile {
            Layout.preferredWidth: 112
            iconType: "camera"
            label: "CAMERA LINK"
            value: root.camStatusText
            active: root.cameraConnected
            onClicked: {
                cameraLinksPopover.opened ? cameraLinksPopover.close() : cameraLinksPopover.open()
                root.statusMessageRequested(cameraLinksPopover.opened ? "Camera links opened" : "Camera links closed")
            }
        }

        StatusTile {
            id: gpsStatusTile
            Layout.preferredWidth: 134
            iconType: "gps"
            label: "GPS"
            value: root.gpsStatusText
            active: root.gpsHasFix
            onClicked: {
                var p = gpsStatusTile.mapToItem(Overlay.overlay, 0, gpsStatusTile.height)
                gpsDetailsPopup.x = Math.max(8, Math.min(Overlay.overlay.width - gpsDetailsPopup.width - 8, Math.round(p.x)))
                gpsDetailsPopup.y = Math.round(p.y + 6)
                gpsDetailsPopup.opened ? gpsDetailsPopup.close() : gpsDetailsPopup.open()
                root.statusMessageRequested(gpsDetailsPopup.opened ? "GPS details opened" : "GPS details closed")
            }
        }
        // BatteryTile { Layout.preferredWidth: 166 }
        StatusTile { Layout.preferredWidth: 122; iconType: "home"; label: "HOME DIST"; value: root.homeDistanceText; active: true }
        StatusTile { Layout.preferredWidth: 108; iconType: "time"; label: "FLT TIME"; value: root.flightTimeText; active: true }
        StatusTile { Layout.preferredWidth: 150; iconType: "mode"; label: "FLT MODE"; value: root.flightModeText; active: root.vehicleConnected }
        StatusTile { Layout.preferredWidth: 136; iconType: "armed"; label: "STATE"; value: root.armed ? "ARMED" : "DISARMED"; active: root.armed }
        BatteryTile { Layout.preferredWidth: 166 }

        Item { Layout.fillWidth: true }

        TopActionButton {
            visible: root.vehicleConnected
            Layout.preferredWidth: visible ? 58 : 0
            iconType: "checklist"
            labelText: "Checklist"
            toolTipText: "Preflight Checklist"
            onClicked: {
                root.statusMessageRequested("Preflight checklist opened")
                root.preflightChecklistRequested()
            }
        }

        TopActionButton {
            visible: root.vehicleConnected
            Layout.preferredWidth: visible ? 58 : 0
            iconType: "motor"
            labelText: "Motor"
            toolTipText: "Motor Test"
            onClicked: {
                root.statusMessageRequested("Motor test opened")
                root.motorTestRequested()
            }
        }

        Button {
            id: servoPanelButton

            visible: root.vehicleConnected && servoPanelSettings.showTopPanelButton
            Layout.preferredWidth: visible ? 58 : 0
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignVCenter
            padding: 0
            flat: true
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: "Servo Control"

            contentItem: ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                spacing: 1

                MiniGlyph {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.alignment: Qt.AlignHCenter
                    iconType: "servo"
                    iconColor: root.accentColor
                }

                Text {
                    Layout.fillWidth: true
                    text: "Servo"
                    color: root.accentColor
                    font.pixelSize: 10
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            background: Rectangle {
                radius: 4
                color: servoPanelButton.down ? root.downColor : servoPanelButton.hovered ? root.hoverColor : root.panelColor
                border.color: servoPanelButton.hovered || servoPanelButton.down ? root.accentColor : root.borderColor
                border.width: 1
            }

            onClicked: {
                root.statusMessageRequested("Servo control opened")
                root.servoPanelRequested()
            }
        }

        TopActionButton {
            Layout.preferredWidth: 58
            Layout.leftMargin: 8
            iconType: root.screenRecording ? "screenStop" : "screenRecord"
            iconColor: root.screenRecording ? "#ff4b46" : root.accentColor
            labelText: "REC"
            active: root.screenRecording
            toolTipText: root.screenRecording
                         ? (root.screenRecordingFilePath.length > 0 ? root.screenRecordingFilePath : "Stop screen recording")
                         : "Start screen recording"
            onClicked: root.screenRecordingRequested()
        }

        Button {
            id: topPanelThemeButton

            Layout.preferredWidth: 82
            Layout.preferredHeight: 32
            Layout.alignment: Qt.AlignVCenter
            Layout.leftMargin: 8
            padding: 0
            flat: true
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: root.darkTheme ? "Switch to light theme" : "Switch to dark theme"

            contentItem: RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 11
                anchors.rightMargin: 10
                spacing: 7

                MiniGlyph {
                    Layout.preferredWidth: 15
                    Layout.preferredHeight: 15
                    Layout.alignment: Qt.AlignVCenter
                    iconType: root.darkTheme ? "sun" : "moon"
                    iconColor: root.darkTheme ? "#ff9a16" : "#0d6fb8"
                }

                Text {
                    Layout.fillWidth: true
                    text: root.darkTheme ? "LIGHT" : "DARK"
                    color: root.darkTheme ? "#9bb3db" : "#425b7a"
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            background: Rectangle {
                radius: 0
                color: topPanelThemeButton.down ? root.downColor : topPanelThemeButton.hovered ? root.hoverColor : root.panelColor
                border.color: topPanelThemeButton.hovered || topPanelThemeButton.down ? (root.darkTheme ? "#38506f" : "#8fb2d8") : root.borderColor
                border.width: 1
            }

            onClicked: {
                var nextDarkTheme = !root.darkTheme
                root.themeModeRequested(nextDarkTheme)
                root.statusMessageRequested((nextDarkTheme ? "Dark" : "Light") + " theme selected")
            }
        }

        Divider {
            Layout.preferredHeight: 38
            Layout.leftMargin: 14
            Layout.rightMargin: 14
        }
        Button {
            id: topPanelSettingsButton

            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            Layout.alignment: Qt.AlignVCenter
            padding: 0
            flat: true
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: "Settings"

            contentItem: Item {
                MiniGlyph {
                    anchors.centerIn: parent
                    width: 19
                    height: 19
                    iconType: "settings"
                    iconColor: root.actionIconColor
                }
            }

            background: Rectangle {
                radius: 4
                color: topPanelSettingsButton.down ? root.downColor : topPanelSettingsButton.hovered ? root.hoverColor : root.panelColor
                border.color: topPanelSettingsButton.hovered || topPanelSettingsButton.down ? (root.darkTheme ? "#2e415c" : "#9ab7d7") : root.borderColor
                border.width: 1
            }

            onClicked: {
                if (root.oemSettingsOpen) {
                    root.statusMessageRequested("OEM settings closed")
                    root.oemSettingsCloseRequested()
                    return
                }
                root.statusMessageRequested("Settings opened")
                settingsWindow.openAtTopRight()
            }
        }
    }
    Popup {
        id: commLinksPopover
        parent: Overlay.overlay
        width: 310
        height: commLinksColumn.implicitHeight + 16
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside

        onAboutToShow: {
            var p = root.mapToItem(Overlay.overlay, 12, root.height)
            x = p.x
            y = p.y + 6
        }

        background: Rectangle {
            color: root.darkTheme ? "#ee090f18" : "#ffffff"
            border.color: root.accentColor
            border.width: 1
        }

        contentItem: Column {
            id: commLinksColumn
            width: parent.width
            padding: 8
            spacing: 4

            Repeater {
                model: root.commLinks
                delegate: Button {
                    id: commLinkButton
                    readonly property bool connecting: !modelData.connected
                                                    && root.pendingCommAddress === modelData.address
                    width: commLinksColumn.width - 16
                    height: 42
                    flat: true
                    padding: 0
                    hoverEnabled: true

                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 9
                            Layout.preferredHeight: 9
                            radius: 5
                            color: modelData.connected ? "#ff4b46" : "#20d86c"
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text { text: modelData.name; color: root.textColor; font.pixelSize: 12; font.bold: true }
                            Text {
                                text: (modelData.protocol || "UDP") === "Serial" ? modelData.address + " @ " + (modelData.baudRate || 57600) : modelData.address
                                color: root.mutedColor
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 6

                            ConnectingSpinner {
                                visible: commLinkButton.connecting
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                                spinnerColor: root.accentColor
                            }

                            Text {
                                text: modelData.connected ? "DISCONNECT" : (commLinkButton.connecting ? "CONNECTING" : "CONNECT")
                                color: modelData.connected ? "#ff6964" : "#35dc79"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            CancelConnectionButton {
                                visible: commLinkButton.connecting
                                Layout.preferredWidth: 22
                                Layout.preferredHeight: 22
                                onClicked: root.cancelPendingCommLink()
                            }
                        }
                    }

                    background: Rectangle {
                        color: commLinkButton.hovered ? root.hoverColor : "#00000000"
                        border.color: modelData.connected ? (root.darkTheme ? "#663a2424" : "#efc2c2") : root.borderColor
                        border.width: 1
                    }

                    onClicked: if (!connecting) root.connectCommLink(index)
                }
            }
        }
    }

    Popup {
        id: cameraLinksPopover
        parent: Overlay.overlay
        width: 330
        height: cameraLinksColumn.implicitHeight + 16
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside

        onAboutToShow: {
            var p = root.mapToItem(Overlay.overlay, 124, root.height)
            x = p.x
            y = p.y + 6
        }

        background: Rectangle {
            color: root.darkTheme ? "#ee090f18" : "#ffffff"
            border.color: root.accentColor
            border.width: 1
        }

        contentItem: Column {
            id: cameraLinksColumn
            width: parent.width
            padding: 8
            spacing: 4

            Repeater {
                model: root.cameraLinks
                delegate: Button {
                    id: cameraLinkButton
                    readonly property bool connecting: !modelData.connected
                                                    && root.pendingCameraAddress === modelData.address
                    width: cameraLinksColumn.width - 16
                    height: 42
                    flat: true
                    padding: 0
                    hoverEnabled: true

                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 9
                            Layout.preferredHeight: 9
                            radius: 5
                            color: modelData.connected ? "#ff4b46" : "#20d86c"
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text { text: modelData.name; color: root.textColor; font.pixelSize: 12; font.bold: true }
                            Text {
                                text: (modelData.protocol || "UDP") === "Serial" ? modelData.address + " @ " + (modelData.baudRate || 57600) : modelData.address
                                color: root.mutedColor
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 6

                            ConnectingSpinner {
                                visible: cameraLinkButton.connecting
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                                spinnerColor: root.accentColor
                            }

                            Text {
                                text: modelData.connected ? "DISCONNECT" : (cameraLinkButton.connecting ? "CONNECTING" : "CONNECT")
                                color: modelData.connected ? "#ff6964" : "#35dc79"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            CancelConnectionButton {
                                visible: cameraLinkButton.connecting
                                Layout.preferredWidth: 22
                                Layout.preferredHeight: 22
                                onClicked: root.cancelPendingCameraLink()
                            }
                        }
                    }

                    background: Rectangle {
                        color: cameraLinkButton.hovered ? root.hoverColor : "#00000000"
                        border.color: modelData.connected ? (root.darkTheme ? "#663a2424" : "#efc2c2") : root.borderColor
                        border.width: 1
                    }

                    onClicked: if (!connecting) root.connectCameraLink(index)
                }
            }

            Button {
                id: manageCameraLinksButton
                width: cameraLinksColumn.width - 16
                height: 34
                flat: true
                padding: 0
                hoverEnabled: true

                contentItem: Text {
                    text: "MANAGE CAMERA LINKS"
                    color: root.accentColor
                    font.pixelSize: 10
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: manageCameraLinksButton.hovered ? root.hoverColor : "#00000000"
                    border.color: root.borderColor
                    border.width: 1
                }

                onClicked: {
                    cameraLinksPopover.close()
                    settingsWindow.activeCategory = "camera"
                    settingsWindow.openAtTopRight()
                }
            }
        }
    }

    Popup {
        id: batteryDetailsPopup
        parent: Overlay.overlay
        width: 240
        height: batteryDetailsContent.implicitHeight + 28
        padding: 14
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside

        background: Rectangle {
            color: root.darkTheme ? "#f20b1018" : "#ffffff"
            border.color: root.accentColor
            border.width: 1
        }

        contentItem: ColumnLayout {
            id: batteryDetailsContent
            width: batteryDetailsPopup.width - 28
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: "BATTERY"
                color: root.accentColor
                font.pixelSize: 13
                font.bold: true
            }

            DetailRow {
                label: "Charge"
                value: root.batteryKnown ? root.batteryPercent + " %" : "--"
            }

            DetailRow {
                label: "Voltage"
                value: root.vehicleConnected && root.vehicle && root.vehicle.batteryVoltage >= 0 ? Number(root.vehicle.batteryVoltage).toFixed(2) + " V" : "--"
            }

            DetailRow {
                label: "Current"
                value: root.vehicleConnected && root.vehicle && root.vehicle.batteryCurrent >= 0 ? Number(root.vehicle.batteryCurrent).toFixed(2) + " A" : "--"
            }
        }
    }

    Popup {
        id: gpsDetailsPopup
        parent: Overlay.overlay
        width: 310
        height: gpsDetailsContent.implicitHeight + 28
        padding: 14
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside

        background: Rectangle {
            color: root.darkTheme ? "#f20b1018" : "#ffffff"
            border.color: root.accentColor
            border.width: 1
        }

        contentItem: ColumnLayout {
            id: gpsDetailsContent
            width: gpsDetailsPopup.width - 28
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: "GPS"
                color: root.accentColor
                font.pixelSize: 13
                font.bold: true
            }

            DetailRow {
                label: "Status"
                value: root.vehicleConnected ? root.gpsFixStatus(root.gpsFixType) : "No telemetry"
            }

            DetailRow {
                label: "Satellites"
                value: root.vehicle ? root.telemetryIntText(root.vehicle.satellites) : "--"
            }

            // DetailRow {
            //     label: "Latitude"
            //     value: root.vehicleConnected && root.vehicle ? root.coordinateText(root.vehicle.latitude) : "--"
            // }

            // DetailRow {
            //     label: "Longitude"
            //     value: root.vehicleConnected && root.vehicle ? root.coordinateText(root.vehicle.longitude) : "--"
            // }

            // DetailRow {
            //     label: "GPS altitude"
            //     value: root.vehicle ? root.telemetryNumberText(root.vehicle.gpsAltitude, 1, "m") : "--"
            // }

            // DetailRow {
            //     label: "Relative altitude"
            //     value: root.vehicleConnected && root.vehicle ? root.telemetryNumberText(root.vehicle.altitude, 1, "m") : "--"
            // }

            DetailRow {
                label: "HDOP"
                value: root.vehicle ? root.telemetryNumberText(root.vehicle.gpsHdop, 2, "") : "--"
            }

            DetailRow {
                label: "VDOP"
                value: root.vehicle ? root.telemetryNumberText(root.vehicle.gpsVdop, 2, "") : "--"
            }

            DetailRow {
                label: "Horizontal accuracy"
                value: root.vehicle ? root.telemetryNumberText(root.vehicle.gpsHorizontalAccuracy, 2, "m") : "--"
            }

            DetailRow {
                label: "Vertical accuracy"
                value: root.vehicle ? root.telemetryNumberText(root.vehicle.gpsVerticalAccuracy, 2, "m") : "--"
            }

            // DetailRow {
            //     label: "Ground speed"
            //     value: root.vehicle ? root.telemetryNumberText(root.vehicle.gpsGroundSpeed, 1, "m/s") : "--"
            // }

            // DetailRow {
            //     label: "Course"
            //     value: root.vehicle ? root.telemetryNumberText(root.vehicle.gpsCourse, 1, "deg") : "--"
            // }

            // DetailRow {
            //     label: "Speed accuracy"
            //     value: root.vehicle ? root.telemetryNumberText(root.vehicle.gpsSpeedAccuracy, 2, "m/s") : "--"
            // }

            // DetailRow {
            //     label: "Heading accuracy"
            //     value: root.vehicle ? root.telemetryNumberText(root.vehicle.gpsHeadingAccuracy, 2, "deg") : "--"
            // }
        }
    }

    Connections {
        target: root.mavlinkObject
        function onConnectionStatusChanged(connected) {
            if (!connected) root.markSelectedCommLink(-1)
            else {
                var linkIndex = root.commLinks.findIndex(function(link) { return link.connected })
                if (linkIndex < 0 && settingsWindow && settingsWindow.activeCommLinkIndex >= 0)
                    linkIndex = settingsWindow.activeCommLinkIndex
                if (linkIndex < 0) {
                    linkIndex = root.commLinks.findIndex(function(link) {
                        return link.address === root.pendingCommAddress
                    })
                }
                root.markSelectedCommLink(linkIndex)
                settingsWindow.setCommLinkConnectionState(linkIndex, linkIndex >= 0)
            }
        }
    }

    Connections {
        target: root.videoObject
        function onStreamingChanged() {
            root.syncActiveCameraLink()
        }
    }

    SettingsWindow {
        id: settingsWindow
        transientParent: root.Window.window
        mavlinkObject: root.mavlinkObject
        vpsModuleEnabled: root.vpsModuleEnabled
        vehicleArmed: root.armed
        vpsVioMarker: root.vpsVioMarker
        vpsVprMarker: root.vpsVprMarker
        vpsFusedMarker: root.vpsFusedMarker
        onVpsMarkerChanged: function(marker, enabled) { root.vpsMarkerChanged(marker, enabled) }
        videoObject: root.videoObject
        darkTheme: root.darkTheme
        mapViewMode: root.mapViewMode
        onCommLinksSaved: function(commLinks) {
            root.commLinks = commLinks
            var selectedLink = null
            for (var i = 0; i < commLinks.length; i++) {
                if (commLinks[i].connected) {
                    selectedLink = commLinks[i]
                    break
                }
            }
            root.connectedCommAddress = selectedLink ? selectedLink.address : ""
        }

        onCameraLinksSaved: function(cameraLinks) {
            root.cameraLinks = cameraLinks
        }
        onPreflightChecklistEnabledChanged: function(enabled) {
            root.preflightChecklistEnabledChanged(enabled)
        }
        onCameraLinkRequested: function(rtspLink, cameraName) {
            root.cameraLinkRequested(rtspLink, cameraName)
        }
        onMapViewModeRequested: function(mapViewMode) {
            root.mapViewMode = mapViewMode
            root.mapViewModeRequested(mapViewMode)
        }
        onOemSettingsRequested: {
            root.statusMessageRequested("OEM settings opened")
            root.oemSettingsRequested()
        }
    }

    component CancelConnectionButton: Button {
        id: cancelButton
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: Canvas {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 3
            width: 14
            height: 14
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = cancelButton.hovered ? "#ff6964" : root.mutedColor
                ctx.lineWidth = 1.8
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.moveTo(3, 3)
                ctx.lineTo(11, 11)
                ctx.moveTo(11, 3)
                ctx.lineTo(3, 11)
                ctx.stroke()
            }
        }

        background: Rectangle {
            radius: width / 2
            color: cancelButton.hovered ? (root.darkTheme ? "#442f1518" : "#ffe7e7") : "#00000000"
        }
    }

    component ConnectingSpinner: Item {
        id: spinner
        property color spinnerColor: root.accentColor
        implicitWidth: 16
        implicitHeight: 16

        onSpinnerColorChanged: spinnerCanvas.requestPaint()

        Canvas {
            id: spinnerCanvas
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = spinner.spinnerColor
                ctx.lineWidth = 2
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.arc(width / 2, height / 2, 5.5, Math.PI * 0.15, Math.PI * 1.55)
                ctx.stroke()
            }
        }

        RotationAnimator on rotation {
            running: spinner.visible
            from: 0
            to: 360
            duration: 800
            loops: Animation.Infinite
        }
    }

    component Divider: Rectangle {
        Layout.preferredWidth: 1
        color: root.borderColor
    }



    component TopActionButton: Button {
        id: actionButton
        property string iconType: ""
        property color iconColor: root.accentColor
        property string toolTipText: ""
        property string labelText: ""
        property bool active: false

        Layout.preferredHeight: labelText.length > 0 ? 48 : 38
        Layout.alignment: Qt.AlignVCenter
        padding: 0
        flat: true
        hoverEnabled: true
        ToolTip.visible: hovered
        ToolTip.text: toolTipText

        contentItem: Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: actionButton.labelText.length > 0 ? 4 : 0
                anchors.bottomMargin: actionButton.labelText.length > 0 ? 4 : 0
                spacing: 1

                MiniGlyph {
                    Layout.preferredWidth: actionButton.labelText.length > 0 ? 20 : 19
                    Layout.preferredHeight: actionButton.labelText.length > 0 ? 20 : 19
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                    iconType: actionButton.iconType
                    iconColor: actionButton.iconColor
                }

                Text {
                    visible: actionButton.labelText.length > 0
                    Layout.fillWidth: true
                    text: actionButton.labelText
                    color: actionButton.iconColor
                    font.pixelSize: 9
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }

        background: Rectangle {
            radius: 4
            color: actionButton.active ? (root.darkTheme ? "#281116" : "#fff0f1")
                  : actionButton.down ? root.downColor
                  : actionButton.hovered ? root.hoverColor
                  : root.panelColor
            border.color: actionButton.active ? "#ff4b46"
                         : actionButton.hovered || actionButton.down ? actionButton.iconColor
                         : root.panelColor
            border.width: 1
        }
    }
    component BatteryTile: Button {
        id: batteryTile
        Layout.fillHeight: true
        flat: true
        padding: 0
        hoverEnabled: true
        onClicked: {
            var p = batteryValueText.mapToItem(Overlay.overlay, 0, batteryValueText.height)
            batteryDetailsPopup.x = Math.max(8, Math.min(Overlay.overlay.width - batteryDetailsPopup.width - 8, Math.round(p.x + batteryValueText.width / 2 - batteryDetailsPopup.width / 2)))
            batteryDetailsPopup.y = Math.round(p.y + 6)
            batteryDetailsPopup.open()
        }

        contentItem: RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 10
            spacing: 10

            MiniGlyph {
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                Layout.alignment: Qt.AlignVCenter
                iconType: "battery"
                iconColor: root.batteryKnown ? root.accentColor : root.inactiveColor
            }

            Rectangle {
                Layout.preferredWidth: 82
                Layout.preferredHeight: 14
                Layout.alignment: Qt.AlignVCenter
                radius: 3
                color: root.darkTheme ? "#071017" : "#eef4fb"
                border.color: root.batteryKnown ? (root.darkTheme ? "#2a415b" : "#b7c8dd") : root.borderColor
                border.width: 1

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: 1
                    width: root.batteryKnown ? Math.max(0, (parent.width - 2) * Math.min(100, root.batteryPercent) / 100) : 0
                    radius: 2
                    color: root.batteryChargeColor
                }
            }

            Text {
                id: batteryValueText
                Layout.preferredWidth: 52
                text: root.batteryText
                color: root.batteryChargeColor
                font.pixelSize: 12
                font.bold: true
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                Layout.alignment: Qt.AlignVCenter
            }
        }

        background: Rectangle {
            color: batteryTile.down ? root.downColor : batteryTile.hovered ? root.hoverColor : root.panelColor
            border.color: root.borderColor
            border.width: 1
        }
    }

    component DetailRow: RowLayout {
        property string label: ""
        property string value: ""
        Layout.fillWidth: true
        spacing: 10

        Text {
            Layout.fillWidth: true
            text: label
            color: root.mutedColor
            font.pixelSize: 11
            font.bold: true
        }

        Text {
            text: value
            color: root.textColor
            font.pixelSize: 12
            font.bold: true
            horizontalAlignment: Text.AlignRight
        }
    }
    component StatusTile: Button {
        id: tile
        property string iconType: ""
        property string label: ""
        property string value: ""
        property bool active: false

        Layout.fillHeight: true
        flat: true
        padding: 0
        hoverEnabled: true

        contentItem: RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 10
            spacing: 8

            MiniGlyph {
                Layout.preferredWidth: 17
                Layout.preferredHeight: 17
                Layout.alignment: Qt.AlignVCenter
                iconType: tile.iconType
                iconColor: root.accentColor
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 1

                Text {
                    Layout.fillWidth: true
                    text: tile.label
                    color: root.mutedColor
                    font.pixelSize: 9
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: tile.value
                    color: tile.active ? root.textColor : root.inactiveColor
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                }
            }
        }

        background: Rectangle {
            color: tile.down ? root.downColor : tile.hovered ? root.hoverColor : root.panelColor
            border.color: root.borderColor
            border.width: 1
        }
    }

    component MiniGlyph: Canvas {
        property string iconType: ""
        property color iconColor: "#00c7df"

        onIconTypeChanged: requestPaint()
        onIconColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            var cx = w / 2
            var cy = h / 2
            ctx.clearRect(0, 0, w, h)
            ctx.strokeStyle = iconColor
            ctx.fillStyle = iconColor
            ctx.lineWidth = Math.max(1.4, Math.min(w, h) * 0.09)
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (iconType === "comm") {
                for (var i = 0; i < 4; i++) ctx.fillRect(w * (0.18 + i * 0.18), h * (0.72 - i * 0.15), w * 0.09, h * (0.16 + i * 0.15))
                ctx.beginPath(); ctx.arc(w * 0.10, h * 0.72, w * 0.05, 0, Math.PI * 2); ctx.fill()
            } else if (iconType === "camera") {
                ctx.strokeRect(w * 0.18, h * 0.30, w * 0.64, h * 0.46)
                ctx.beginPath(); ctx.arc(cx, h * 0.53, w * 0.14, 0, Math.PI * 2); ctx.stroke()
                ctx.strokeRect(w * 0.30, h * 0.20, w * 0.20, h * 0.10)
            } else if (iconType === "gps") {
                ctx.beginPath(); ctx.moveTo(cx, h * 0.12); ctx.lineTo(w * 0.78, h * 0.78); ctx.lineTo(cx, h * 0.62); ctx.lineTo(w * 0.22, h * 0.78); ctx.closePath(); ctx.stroke()
            } else if (iconType === "altitude") {
                ctx.beginPath(); ctx.moveTo(cx, h * 0.16); ctx.lineTo(cx, h * 0.84); ctx.moveTo(cx, h * 0.16); ctx.lineTo(w * 0.28, h * 0.40); ctx.moveTo(cx, h * 0.16); ctx.lineTo(w * 0.72, h * 0.40); ctx.stroke()
            } else if (iconType === "speed") {
                ctx.beginPath(); ctx.arc(cx, h * 0.62, w * 0.32, Math.PI, Math.PI * 2); ctx.moveTo(cx, h * 0.62); ctx.lineTo(w * 0.70, h * 0.42); ctx.stroke()
            } else if (iconType === "wind") {
                ctx.beginPath(); ctx.moveTo(w * 0.15, h * 0.35); ctx.lineTo(w * 0.76, h * 0.35); ctx.quadraticCurveTo(w * 0.93, h * 0.35, w * 0.82, h * 0.50); ctx.moveTo(w * 0.25, h * 0.62); ctx.lineTo(w * 0.70, h * 0.62); ctx.stroke()
            } else if (iconType === "heading") {
                ctx.beginPath(); ctx.arc(cx, cy, w * 0.34, 0, Math.PI * 2); ctx.moveTo(cx, cy); ctx.lineTo(w * 0.70, h * 0.30); ctx.stroke()
            } else if (iconType === "time") {
                ctx.beginPath(); ctx.arc(cx, cy, w * 0.34, 0, Math.PI * 2); ctx.moveTo(cx, cy); ctx.lineTo(cx, h * 0.28); ctx.moveTo(cx, cy); ctx.lineTo(w * 0.68, h * 0.62); ctx.stroke()
            } else if (iconType === "battery") {
                ctx.strokeRect(w * 0.12, h * 0.34, w * 0.68, h * 0.34); ctx.fillRect(w * 0.82, h * 0.44, w * 0.08, h * 0.14)
            } else if (iconType === "home") {
                ctx.beginPath(); ctx.moveTo(w * 0.18, h * 0.50); ctx.lineTo(cx, h * 0.18); ctx.lineTo(w * 0.82, h * 0.50); ctx.moveTo(w * 0.30, h * 0.46); ctx.lineTo(w * 0.30, h * 0.82); ctx.lineTo(w * 0.70, h * 0.82); ctx.lineTo(w * 0.70, h * 0.46); ctx.stroke()
            } else if (iconType === "mode") {
                ctx.beginPath(); ctx.arc(cx, cy, w * 0.32, Math.PI * 0.78, Math.PI * 2.22); ctx.moveTo(cx, cy); ctx.lineTo(w * 0.70, h * 0.34); ctx.stroke()
            } else if (iconType === "armed") {
                ctx.strokeRect(w * 0.26, h * 0.44, w * 0.48, h * 0.36)
                ctx.beginPath()
                ctx.moveTo(w * 0.36, h * 0.44)
                ctx.lineTo(w * 0.36, h * 0.32)
                ctx.quadraticCurveTo(w * 0.36, h * 0.16, cx, h * 0.16)
                ctx.quadraticCurveTo(w * 0.64, h * 0.16, w * 0.64, h * 0.32)
                ctx.lineTo(w * 0.64, h * 0.44)
                ctx.stroke()
            } else if (iconType === "checklist") {
                ctx.strokeRect(w * 0.22, h * 0.16, w * 0.56, h * 0.68)
                ctx.beginPath(); ctx.moveTo(w * 0.34, h * 0.36); ctx.lineTo(w * 0.42, h * 0.44); ctx.lineTo(w * 0.58, h * 0.28); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(w * 0.34, h * 0.62); ctx.lineTo(w * 0.42, h * 0.70); ctx.lineTo(w * 0.62, h * 0.50); ctx.stroke()
            } else if (iconType === "motor") {
                ctx.beginPath(); ctx.arc(cx, cy, w * 0.14, 0, Math.PI * 2); ctx.fill()
                for (var k = 0; k < 4; k++) {
                    var a2 = k * Math.PI / 2 + Math.PI / 4
                    ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(cx + Math.cos(a2) * w * 0.38, cy + Math.sin(a2) * h * 0.38); ctx.stroke()
                    ctx.beginPath(); ctx.arc(cx + Math.cos(a2) * w * 0.38, cy + Math.sin(a2) * h * 0.38, w * 0.08, 0, Math.PI * 2); ctx.stroke()
                }
            } else if (iconType === "servo") {
                ctx.strokeRect(w * 0.18, h * 0.38, w * 0.64, h * 0.34)
                ctx.beginPath()
                ctx.moveTo(cx, h * 0.38)
                ctx.lineTo(cx, h * 0.18)
                ctx.lineTo(w * 0.66, h * 0.18)
                ctx.moveTo(w * 0.32, h * 0.72)
                ctx.lineTo(w * 0.24, h * 0.88)
                ctx.moveTo(w * 0.68, h * 0.72)
                ctx.lineTo(w * 0.76, h * 0.88)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(w * 0.66, h * 0.18, w * 0.06, 0, Math.PI * 2)
                ctx.fill()
            } else if (iconType === "screenRecord") {
                ctx.strokeRect(w * 0.15, h * 0.20, w * 0.70, h * 0.48)
                ctx.beginPath(); ctx.moveTo(w * 0.38, h * 0.82); ctx.lineTo(w * 0.62, h * 0.82); ctx.moveTo(cx, h * 0.68); ctx.lineTo(cx, h * 0.82); ctx.stroke()
                ctx.beginPath(); ctx.arc(w * 0.70, h * 0.44, w * 0.10, 0, Math.PI * 2); ctx.fill()
            } else if (iconType === "screenStop") {
                ctx.strokeRect(w * 0.15, h * 0.20, w * 0.70, h * 0.48)
                ctx.beginPath(); ctx.moveTo(w * 0.38, h * 0.82); ctx.lineTo(w * 0.62, h * 0.82); ctx.moveTo(cx, h * 0.68); ctx.lineTo(cx, h * 0.82); ctx.stroke()
                ctx.fillRect(w * 0.62, h * 0.36, w * 0.18, h * 0.18)
            } else if (iconType === "sun") {
                ctx.beginPath(); ctx.arc(cx, cy, w * 0.20, 0, Math.PI * 2); ctx.stroke()
                for (var s = 0; s < 8; s++) {
                    var sunAngle = s * Math.PI / 4
                    ctx.beginPath()
                    ctx.moveTo(cx + Math.cos(sunAngle) * w * 0.32, cy + Math.sin(sunAngle) * h * 0.32)
                    ctx.lineTo(cx + Math.cos(sunAngle) * w * 0.44, cy + Math.sin(sunAngle) * h * 0.44)
                    ctx.stroke()
                }
            } else if (iconType === "moon") {
                ctx.beginPath()
                ctx.arc(cx, cy, w * 0.36, Math.PI * 0.62, Math.PI * 1.55, false)
                ctx.quadraticCurveTo(w * 0.62, h * 0.52, w * 0.55, h * 0.15)
                ctx.quadraticCurveTo(w * 0.82, h * 0.42, w * 0.55, h * 0.78)
                ctx.quadraticCurveTo(w * 0.38, h * 0.88, cx, h * 0.86)
                ctx.stroke()
            } else if (iconType === "settings") {
                ctx.beginPath(); ctx.arc(cx, cy, w * 0.26, 0, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.arc(cx, cy, w * 0.08, 0, Math.PI * 2); ctx.stroke()
                for (var j = 0; j < 8; j++) {
                    var angle = j * Math.PI / 4
                    var inner = w * 0.34
                    var outer = w * 0.43
                    ctx.beginPath()
                    ctx.moveTo(cx + Math.cos(angle) * inner, cy + Math.sin(angle) * inner)
                    ctx.lineTo(cx + Math.cos(angle) * outer, cy + Math.sin(angle) * outer)
                    ctx.stroke()
                }
            }
        }
    }
}
