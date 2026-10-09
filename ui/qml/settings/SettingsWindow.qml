import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Qt.labs.settings
import QtQuick.Window

Window {
    id: root

    width: transientParent ? transientParent.width : 1280
    height: transientParent ? transientParent.height : 720
    visible: false
    modality: Qt.ApplicationModal
    color: "transparent"
    flags: Qt.Dialog | Qt.FramelessWindowHint
    title: "Settings"

    property string activeCategory: "comm"
    property bool commAddModalOpen: false
    property bool cameraAddModalOpen: false
    property bool oemPasswordModalOpen: false
    property int editingLinkIndex: -1
    property int oemCopyrightClickCount: 0
    property string oemPasswordErrorText: ""
    readonly property bool editingLink: editingLinkIndex >= 0
    property string mapViewMode: "satellite"
    property var mavlinkObject: null
    property var videoObject: null
    property string pendingCommAddress: ""
    property string pendingCameraAddress: ""
    readonly property var serialPortOptions: root.mavlinkObject && root.mavlinkObject.serialPorts.length > 0 ? root.mavlinkObject.serialPorts : ["None available"]
    readonly property bool serialPortsAvailable: root.mavlinkObject && root.mavlinkObject.serialPorts.length > 0
    readonly property var serialBaudRates: [9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600]
    property int panelWidth: 600
    property bool drawerOpen: false
    property bool darkTheme: true
    readonly property var rasterManager: typeof rasterOverlayManager !== "undefined" ? rasterOverlayManager : null
    readonly property var tileCache: typeof tileCacheManager !== "undefined" ? tileCacheManager : null
    readonly property var appBackend: typeof backend !== "undefined" ? backend : null
    readonly property color overlayColor: root.darkTheme ? "#b0000308" : "#550f213b"
    readonly property color panelColor: root.darkTheme ? "#f10a0f18" : "#ffffff"
    readonly property color railColor: root.darkTheme ? "#0b111b" : "#f1f6fb"
    readonly property color cardColor: root.darkTheme ? "#0d121c" : "#ffffff"
    readonly property color inputColor: root.darkTheme ? "#070c13" : "#f8fbff"
    readonly property color hoverColor: root.darkTheme ? "#111b28" : "#eef5fc"
    readonly property color selectedColor: root.darkTheme ? "#0b2a34" : "#dff4f8"
    readonly property color borderColor: root.darkTheme ? "#253047" : "#cfdceb"
    readonly property color strongBorderColor: root.darkTheme ? "#00aeca" : "#008fb8"
    readonly property color textColor: root.darkTheme ? "#e8edf4" : "#0f213b"
    readonly property color labelColor: root.darkTheme ? "#8b96aa" : "#526680"
    readonly property color mutedColor: root.darkTheme ? "#77849a" : "#6f7f95"
    readonly property color disabledColor: root.darkTheme ? "#465371" : "#b5c3d4"
    readonly property color accentColor: root.darkTheme ? "#00c7df" : "#008fb8"

    signal cameraLinkRequested(string rtspLink, string cameraName)
    signal mapViewModeRequested(string mapViewMode)
    signal oemSettingsRequested()
    signal commLinksSaved(var commLinks)
    signal cameraLinksSaved(var cameraLinks)
    signal preflightChecklistEnabledChanged(bool enabled)
    property var cameraLinks: []
    property var commLinks: []

    Settings {
        id: persistedMapSettings
        category: "Map"
        property string viewMode: "satellite"
        onViewModeChanged: root.mapViewMode = viewMode === "street" ? "street" : "satellite"
    }

    Settings {
        id: persistedGeneralSettings
        category: "General"
        property bool preflightChecklistEnabled: true
        property bool flightControllerLoggingEnabled: true
    }

    Settings {
        id: persistedLogSettings
        category: "Log"
        property bool autoTelemetryLoggingEnabled: true
    }

    Settings {
        id: persistedLinkSettings
        category: "SettingsWindowLinks"
        property string cameraLinksJson: ""
        property string commLinksJson: ""
    }

    FileDialog {
        id: rasterImportDialog
        title: "Import GeoTIFF raster map"
        nameFilters: [ "GeoTIFF files (*.tif *.tiff)", "All files (*)" ]
        fileMode: FileDialog.OpenFile
        onAccepted: if (root.rasterManager) root.rasterManager.importGeoTiff(selectedFile)
    }

    onActiveCategoryChanged: {
        root.closeLinkEditor()
    }

    Component.onCompleted: {
        root.mapViewMode = persistedMapSettings.viewMode === "street" ? "street" : "satellite"
        cameraLinks = loadPersistedLinks(persistedLinkSettings.cameraLinksJson)
        commLinks = loadPersistedLinks(persistedLinkSettings.commLinksJson)
        applyTelemetryLoggingSetting()
        applyFlightControllerLoggingSetting()
        if (root.mavlinkObject) {
            root.mavlinkObject.setTelemetryLogDirectory(root.telemetryLogStorageLocation())
            root.mavlinkObject.setFlightControllerLogDirectory(root.ardupilotLogStorageLocation())
        }
    }

    onMavlinkObjectChanged: {
        applyTelemetryLoggingSetting()
        applyFlightControllerLoggingSetting()
        if (root.mavlinkObject) {
            root.mavlinkObject.setTelemetryLogDirectory(root.telemetryLogStorageLocation())
            root.mavlinkObject.setFlightControllerLogDirectory(root.ardupilotLogStorageLocation())
        }
    }

    function setMapViewMode(mode) {
        var nextMode = mode === "street" ? "street" : "satellite"
        root.mapViewMode = nextMode
        persistedMapSettings.viewMode = nextMode
        root.mapViewModeRequested(nextMode)
    }

    function mapCacheSizeText(bytes) {
        if (!isFinite(bytes) || bytes <= 0)
            return "0 MB"
        if (bytes >= 1024 * 1024 * 1024)
            return (bytes / (1024 * 1024 * 1024)).toFixed(2) + " GB"
        return (bytes / (1024 * 1024)).toFixed(1) + " MB"
    }

    function selectedMapStyleUrl() {
        return root.mapViewMode === "street"
                ? "https://api.maptiler.com/maps/streets/style.json?key=KtcPHyoVykbWCHELF8E8"
                : "https://api.maptiler.com/maps/hybrid/style.json?key=KtcPHyoVykbWCHELF8E8"
    }

    function logStorageLocation() {
        if (root.appBackend && root.appBackend.logsDirectory)
            return root.appBackend.logsDirectory()
        return root.mavlinkObject ? root.mavlinkObject.telemetryLogDirectory : ""
    }

    function telemetryLogStorageLocation() {
        if (root.appBackend && root.appBackend.telemetryLogDirectory)
            return root.appBackend.telemetryLogDirectory()
        return root.mavlinkObject ? root.mavlinkObject.telemetryLogDirectory : ""
    }

    function ardupilotLogStorageLocation() {
        if (root.appBackend && root.appBackend.ardupilotLogDirectory)
            return root.appBackend.ardupilotLogDirectory()
        return root.mavlinkObject ? root.mavlinkObject.flightControllerLogDirectory : ""
    }

    function applyTelemetryLoggingSetting() {
        if (!root.mavlinkObject)
            return
        root.mavlinkObject.setTelemetryLoggingEnabled(persistedLogSettings.autoTelemetryLoggingEnabled)
    }

    function applyFlightControllerLoggingSetting() {
        if (!root.mavlinkObject)
            return
        root.mavlinkObject.setFlightControllerLoggingEnabled(persistedGeneralSettings.flightControllerLoggingEnabled)
    }

    function loadPersistedLinks(savedValue) {
        if (savedValue.length === 0)
            return []
        try {
            var parsedLinks = JSON.parse(savedValue)
            return Array.isArray(parsedLinks) ? resetConnectionStates(parsedLinks) : []
        } catch (error) {
            return []
        }
    }

    function resetConnectionStates(links) {
        return links.map(function(link) {
            return { name: link.name, address: link.address, protocol: link.protocol || "UDP", baudRate: link.baudRate || 57600, connected: false }
        })
    }

    function persistedLinks(links) {
        return links.map(function(link) {
            return { name: link.name, address: link.address, protocol: link.protocol || "UDP", baudRate: link.baudRate || 57600, connected: false }
        })
    }

    // function saveCameraLinks() {
    //     persistedLinkSettings.cameraLinksJson = JSON.stringify(persistedLinks(cameraLinks))
    //     root.cameraLinksSaved(root.cameraLinks)
    // }

    function saveCameraLinks() {
        persistedLinkSettings.cameraLinksJson =
                JSON.stringify(persistedLinks(cameraLinks))

        root.cameraLinksSaved(root.cameraLinks)
    }

    // function saveCommLinks() {
    //     persistedLinkSettings.commLinksJson = JSON.stringify(persistedLinks(commLinks))
    //     root.commLinksSaved(root.commLinks)
    // }

    function saveCommLinks() {
        persistedLinkSettings.commLinksJson =
                JSON.stringify(persistedLinks(commLinks))

        root.commLinksSaved(root.commLinks)
    }

    function setCommLinkConnectionState(address, connected) {
        commLinks = commLinks.map(function(link) {
            return {
                name: link.name,
                address: link.address,
                protocol: link.protocol || "UDP",
                baudRate: link.baudRate || 57600,
                connected: connected && link.address === address
            }
        })
    }

    function setCameraLinkConnectionState(address, connected) {
        cameraLinks = cameraLinks.map(function(link) {
            return {
                name: link.name,
                address: link.address,
                protocol: link.protocol || "UDP",
                baudRate: link.baudRate || 57600,
                connected: connected && link.address === address
            }
        })
    }

    function markSelectedLink(links, linkIndex) {
        return links.map(function(link, index) {
            return { name: link.name, address: link.address, protocol: link.protocol || "UDP", baudRate: link.baudRate || 57600, connected: index === linkIndex }
        })
    }

    function closeLinkEditor() {
        commAddModalOpen = false
        cameraAddModalOpen = false
        editingLinkIndex = -1
    }

    function handleOemCopyrightClick() {
        oemCopyrightClickCount += 1
        if (oemCopyrightClickCount < 3)
            return
        oemCopyrightClickCount = 0
        openOemPasswordPrompt()
    }

    function openOemPasswordPrompt() {
        oemPasswordErrorText = ""
        oemPasswordModalOpen = true
        oemPasswordField.text = ""
        oemPasswordField.forceActiveFocus()
    }

    function closeOemPasswordPrompt() {
        oemPasswordModalOpen = false
        oemPasswordErrorText = ""
        oemPasswordField.text = ""
    }

    function verifyOemPassword() {
        if (oemPasswordField.text === "BonVOEM@0211") {
            closeOemPasswordPrompt()
            closeWithSlide()
            root.oemSettingsRequested()
            return
        }
        oemPasswordErrorText = "Incorrect OEM password"
        oemPasswordField.selectAll()
        oemPasswordField.forceActiveFocus()
    }

    function openAddCameraLink() {
        editingLinkIndex = -1
        resetCameraAddForm()
        cameraAddModalOpen = true
    }

    function openAddCommLink() {
        editingLinkIndex = -1
        refreshSerialPorts()
        resetCommAddForm()
        commAddModalOpen = true
    }

    function refreshSerialPorts() {
        if (root.mavlinkObject && root.mavlinkObject.refreshSerialPorts)
            root.mavlinkObject.refreshSerialPorts()
    }

    function protocolIndex(protocol) {
        if (protocol === "TCP")
            return 1
        if (protocol === "Serial")
            return 2
        return 0
    }

    function baudRateIndex(baudRate) {
        var index = root.serialBaudRates.indexOf(baudRate)
        return index >= 0 ? index : root.serialBaudRates.indexOf(57600)
    }

    function editLink(linkIndex) {
        editingLinkIndex = linkIndex
        if (root.activeCategory === "camera") {
            var cameraLink = cameraLinks[linkIndex]
            if (!cameraLink) {
                closeLinkEditor()
                return
            }
            cameraNameField.text = cameraLink.name
            cameraRtspField.text = cameraLink.address
            cameraAddModalOpen = true
            return
        }

        if (root.activeCategory === "comm") {
            var commLink = commLinks[linkIndex]
            if (!commLink) {
                closeLinkEditor()
                return
            }
            refreshSerialPorts()
            commNameField.text = commLink.name
            commProtocolBox.currentIndex = protocolIndex(commLink.protocol || "UDP")
            if ((commLink.protocol || "UDP") === "Serial") {
                var serialIndex = root.serialPortOptions.indexOf(commLink.address)
                commSerialPortBox.currentIndex = serialIndex >= 0 ? serialIndex : 0
                commBaudRateBox.currentIndex = baudRateIndex(commLink.baudRate || 57600)
            } else {
                var linkAddress = commLink.address.indexOf("://") >= 0 ? commLink.address.split("://")[1] : commLink.address
                var addressParts = linkAddress.split(":")
                commPortField.text = addressParts.length > 1 ? addressParts[addressParts.length - 1] : ""
                commIpField.text = addressParts.length > 1 ? addressParts.slice(0, addressParts.length - 1).join(":") : linkAddress
            }
            commAddModalOpen = true
        }
    }

    function removeLink(linkIndex) {
        if (root.activeCategory === "camera") {
            cameraLinks = cameraLinks.filter(function(_link, index) { return index !== linkIndex })
            saveCameraLinks()
        } else if (root.activeCategory === "comm") {
            commLinks = commLinks.filter(function(_link, index) { return index !== linkIndex })
            saveCommLinks()
        }
    }

    function connectLink(linkIndex) {
        if (root.activeCategory === "camera") {
            var cameraLink = cameraLinks[linkIndex]
            if (!cameraLink)
                return
            if (cameraLink.connected) {
                cameraLinks = markSelectedLink(cameraLinks, -1)
                saveCameraLinks()
                root.cameraLinkRequested("", "")
                return
            }
            pendingCameraAddress = cameraLink.address
            root.cameraLinkRequested(cameraLink.address, cameraLink.name)
            return
        }

        var commLink = commLinks[linkIndex]
        if (!commLink || !root.mavlinkObject)
            return
        if (commLink.connected) {
            commLinks = markSelectedLink(commLinks, -1)
            saveCommLinks()
            if (root.mavlinkObject.connected)
                root.mavlinkObject.disconnectFromVehicle()
            return
        }

        var protocol = commLink.protocol || "UDP"
        if (protocol === "Serial") {
            refreshSerialPorts()
            if (!root.serialPortsAvailable)
                return
            if (root.mavlinkObject.connected)
                root.mavlinkObject.disconnectFromVehicle()
            pendingCommAddress = commLink.address
            var serialConnectionStarted = root.mavlinkObject.connectToVehicleProtocol("Serial", commLink.address, 0, commLink.baudRate || 57600)
            if (!serialConnectionStarted)
                pendingCommAddress = ""
            return
        }

        var linkAddress = commLink.address.indexOf("://") >= 0 ? commLink.address.split("://")[1] : commLink.address
        var addressParts = linkAddress.split(":")
        if (addressParts.length < 2)
            return
        var port = parseInt(addressParts[addressParts.length - 1])
        var host = addressParts.slice(0, addressParts.length - 1).join(":")
        if (host.length === 0 || isNaN(port)) {
            pendingCommAddress = ""
            return
        }
        if (root.mavlinkObject.connected)
            root.mavlinkObject.disconnectFromVehicle()
        pendingCommAddress = commLink.address
        var networkConnectionStarted = root.mavlinkObject.connectToVehicleProtocol(protocol, host, port, 0)
        if (!networkConnectionStarted)
            pendingCommAddress = ""
    }

    function cancelPendingConnection() {
        if (root.activeCategory === "camera") {
            if (pendingCameraAddress.length === 0)
                return
            pendingCameraAddress = ""
            setCameraLinkConnectionState("", false)
            root.cameraLinkRequested("", "")
            return
        }

        if (pendingCommAddress.length === 0)
            return
        pendingCommAddress = ""
        setCommLinkConnectionState("", false)
        if (root.mavlinkObject)
            root.mavlinkObject.disconnectFromVehicle()
    }

    function resetCommAddForm() {
        commNameField.text = ""
        commProtocolBox.currentIndex = 0
        commIpField.text = ""
        commPortField.text = ""
        commSerialPortBox.currentIndex = 0
        commBaudRateBox.currentIndex = baudRateIndex(57600)
    }

    function resetCameraAddForm() {
        cameraNameField.text = ""
        cameraRtspField.text = ""
    }

    function saveCameraLinkFromForm() {
        var linkName = cameraNameField.text.trim()
        var rtspLink = cameraRtspField.text.trim()
        if (linkName.length === 0 || rtspLink.length === 0)
            return

        if (root.editingLink) {
            cameraLinks = cameraLinks.map(function(link, index) {
                return index === root.editingLinkIndex
                        ? { name: linkName, address: rtspLink, protocol: link.protocol || "UDP", connected: link.connected }
                        : link
            })
        } else {
            cameraLinks = cameraLinks.concat([{ name: linkName, address: rtspLink, connected: false }])
        }
        saveCameraLinks()
        closeLinkEditor()
    }

    function saveCommLinkFromForm() {
        var linkName = commNameField.text.trim()
        var protocol = commProtocolBox.currentText
        var address = ""
        var baudRate = 57600

        if (linkName.length === 0)
            return

        if (protocol === "Serial") {
            if (!root.serialPortsAvailable)
                return
            address = commSerialPortBox.currentText
            baudRate = parseInt(commBaudRateBox.currentText)
        } else {
            var ipAddress = commIpField.text.trim()
            var port = commPortField.text.trim()
            if (ipAddress.length === 0 || port.length === 0)
                return
            address = ipAddress + ":" + port
        }

        if (root.editingLink) {
            commLinks = commLinks.map(function(link, index) {
                return index === root.editingLinkIndex
                        ? { name: linkName, address: address, protocol: protocol, baudRate: baudRate, connected: link.connected }
                        : link
            })
        } else {
            commLinks = commLinks.concat([{ name: linkName, address: address, protocol: protocol, baudRate: baudRate, connected: false }])
        }
        saveCommLinks()
        closeLinkEditor()
    }

    function positionAsOverlay() {
        if (!transientParent)
            return
        x = transientParent.x
        y = transientParent.y
        width = transientParent.width
        height = transientParent.height
    }

    function openAtTopRight() {
        if (visible && drawerOpen) {
            closeWithSlide()
            return
        }
        closeDelay.stop()
        positionAsOverlay()
        drawerOpen = false
        show()
        raise()
        drawerOpen = true
    }

    function closeWithSlide() {
        drawerOpen = false
        closeDelay.restart()
    }

    onVisibleChanged: {
        if (visible)
            positionAsOverlay()
        else
            drawerOpen = false
    }

    Timer {
        id: closeDelay
        interval: 220
        repeat: false
        onTriggered: root.close()
    }

    Rectangle {
        id: dimOverlay
        anchors.fill: parent
        color: root.overlayColor
        opacity: root.drawerOpen ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        MouseArea {
            anchors.fill: parent
            onClicked: root.closeWithSlide()
        }
    }

    Rectangle {
        id: panel
        width: Math.min(root.panelWidth, root.width)
        height: root.height
        x: root.drawerOpen ? root.width - width : root.width
        y: 0
        z: 2
        color: root.panelColor
        border.color: root.borderColor
        border.width: 1
        clip: true

        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

        Rectangle {
            id: sideRail
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 72
            color: root.railColor
            border.color: root.borderColor
            border.width: 1

            Column {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 0

                SideButton { iconType: "gear"; selected: false; label: ""; height: 56; enabled: false }
                SideButton { iconType: "general"; label: "GEN"; selected: root.activeCategory === "general"; onClicked: root.activeCategory = "general" }
                SideButton { iconType: "comm"; label: "COMM"; selected: root.activeCategory === "comm"; onClicked: root.activeCategory = "comm" }
                SideButton { iconType: "camera"; label: "CAM"; selected: root.activeCategory === "camera"; onClicked: root.activeCategory = "camera" }
                SideButton { iconType: "log"; label: "LOG"; selected: root.activeCategory === "log"; onClicked: root.activeCategory = "log" }
                SideButton { iconType: "maps"; label: "MAPS"; selected: root.activeCategory === "maps"; onClicked: root.activeCategory = "maps" }
            }
        }

        Text {
            id: titleText
            anchors.left: sideRail.right
            anchors.leftMargin: 24
            anchors.top: parent.top
            anchors.topMargin: 22
            text: root.activeCategory === "general" ? "GENERAL SETTINGS"
                  : root.activeCategory === "comm" ? "COMMUNICATION LINKS"
                  : root.activeCategory === "camera" ? "CAMERA LINKS"
                  : root.activeCategory === "log" ? "LOG SETTINGS"
                  : "MAP SETTINGS"
            color: root.accentColor
            font.pixelSize: 14
            font.bold: true
            font.letterSpacing: 1.2
        }

        Button {
            id: closeButton
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 22
            anchors.topMargin: 18
            width: 22
            height: 22
            padding: 0
            flat: true
            text: "x"
            contentItem: Text {
                text: closeButton.text
                color: root.labelColor
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle { color: closeButton.hovered ? root.hoverColor : "#00000000" }
            onClicked: root.closeWithSlide()
        }

        Rectangle {
            anchors.left: sideRail.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 56
            height: 1
            color: root.borderColor
        }

        Flickable {
            id: linksView
            visible: root.activeCategory === "comm" || root.activeCategory === "camera"
            anchors.left: sideRail.right
            anchors.leftMargin: 24
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.top: parent.top
            anchors.topMargin: 78
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            contentWidth: width
            contentHeight: linksColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: linksColumn
                width: linksView.width
                spacing: 10

                Repeater {
                    model: root.activeCategory === "camera" ? root.cameraLinks : root.commLinks
                    delegate: LinkCard {
                        width: linksColumn.width
                        linkIndex: index
                        linkName: modelData.name
                        linkAddress: modelData.address
                        linkProtocol: root.activeCategory === "comm" ? modelData.protocol || "UDP" : ""
                        linkBaudRate: modelData.baudRate || 57600
                        connected: modelData.connected
                    }
                }

                AddDashedButton {
                    width: linksColumn.width
                    label: root.activeCategory === "camera" ? "ADD CAMERA" : "ADD LINK"
                    onClicked: {
                        if (root.activeCategory === "camera")
                            root.openAddCameraLink()
                        else
                            root.openAddCommLink()
                    }
                }
            }
        }

        Item {
            id: generalPanel
            visible: root.activeCategory === "general"
            anchors.left: sideRail.right
            anchors.leftMargin: 24
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.top: parent.top
            anchors.topMargin: 78
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 82
                    color: root.cardColor
                    border.color: root.borderColor
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 12
                        spacing: 16

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 4
                            Text { Layout.fillWidth: true; text: "PREFLIGHT CHECKLIST"; color: root.textColor; font.pixelSize: 13; font.bold: true }
                            Text { Layout.fillWidth: true; text: "Require checklist confirmation before arming"; color: root.mutedColor; font.pixelSize: 11; elide: Text.ElideRight }
                        }

                        Switch {
                            id: preflightChecklistSwitch
                            checked: persistedGeneralSettings.preflightChecklistEnabled
                            onToggled: {
                                persistedGeneralSettings.preflightChecklistEnabled = checked
                                root.preflightChecklistEnabledChanged(checked)
                            }
                            indicator: Rectangle {
                                implicitWidth: 46
                                implicitHeight: 24
                                radius: 12
                                color: preflightChecklistSwitch.checked ? (root.darkTheme ? "#06363f" : "#dff4f8") : (root.darkTheme ? "#141c29" : "#eef4fb")
                                border.color: preflightChecklistSwitch.checked ? root.accentColor : root.borderColor
                                Rectangle {
                                    x: preflightChecklistSwitch.checked ? parent.width - width - 3 : 3
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 18
                                    height: 18
                                    radius: 9
                                    color: preflightChecklistSwitch.checked ? root.accentColor : root.labelColor
                                    Behavior on x { NumberAnimation { duration: 120 } }
                                }
                            }
                            contentItem: Text {
                                text: preflightChecklistSwitch.checked ? "ON" : "OFF"
                                color: preflightChecklistSwitch.checked ? root.accentColor : root.labelColor
                                font.pixelSize: 12
                                font.bold: true
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 56
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Button {
                    id: oemCopyrightButton

                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredHeight: 28
                    padding: 0
                    flat: true
                    hoverEnabled: true
                    onClicked: root.handleOemCopyrightClick()

                    contentItem: Text {
                        text: "Copyright 2026 BonV Aero. All rights reserved."
                        color: oemCopyrightButton.hovered ? root.labelColor : root.mutedColor
                        opacity: oemCopyrightButton.hovered ? 1 : 0.72
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: "#00000000"
                        border.width: 0
                    }
                }
            }
        }
        Item {
            id: logPanel
            visible: root.activeCategory === "log"
            anchors.left: sideRail.right
            anchors.leftMargin: 24
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.top: parent.top
            anchors.topMargin: 78
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24

            ColumnLayout {
                anchors.fill: parent
                spacing: 14

                MapSettingRow {
                    title: "AUTO TELEMETRY LOGGING"
                    detail: persistedLogSettings.autoTelemetryLoggingEnabled
                            ? "Record and save telemetry when connected"
                            : "Telemetry logging is disabled"
                    control: Switch {
                        id: telemetryLoggingSwitch
                        checked: persistedLogSettings.autoTelemetryLoggingEnabled
                        onToggled: {
                            persistedLogSettings.autoTelemetryLoggingEnabled = checked
                            root.applyTelemetryLoggingSetting()
                        }
                        indicator: Rectangle {
                            implicitWidth: 46
                            implicitHeight: 24
                            radius: 12
                            color: telemetryLoggingSwitch.checked ? (root.darkTheme ? "#06363f" : "#dff4f8") : (root.darkTheme ? "#141c29" : "#eef4fb")
                            border.color: telemetryLoggingSwitch.checked ? root.accentColor : root.borderColor
                            Rectangle {
                                x: telemetryLoggingSwitch.checked ? parent.width - width - 3 : 3
                                anchors.verticalCenter: parent.verticalCenter
                                width: 18
                                height: 18
                                radius: 9
                                color: telemetryLoggingSwitch.checked ? root.accentColor : root.labelColor
                                Behavior on x { NumberAnimation { duration: 120 } }
                            }
                        }
                        contentItem: Text {
                            text: telemetryLoggingSwitch.checked ? "ON" : "OFF"
                            color: telemetryLoggingSwitch.checked ? root.accentColor : root.labelColor
                            font.pixelSize: 12
                            font.bold: true
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 56
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: "LOG STORAGE LOCATION"
                        color: root.textColor
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        color: root.inputColor
                        border.color: root.borderColor
                        border.width: 1

                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            text: root.logStorageLocation()
                            color: root.textColor
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideMiddle
                        }
                    }
                }

                MapSettingRow {
                    title: "FLIGHT CONTROLLER LOGS"
                    detail: persistedGeneralSettings.flightControllerLoggingEnabled
                            ? "Save flight controller messages to a log file"
                            : "Flight controller message logging is disabled"
                    control: Switch {
                        id: flightControllerLoggingSwitch
                        checked: persistedGeneralSettings.flightControllerLoggingEnabled
                        onToggled: {
                            persistedGeneralSettings.flightControllerLoggingEnabled = checked
                            root.applyFlightControllerLoggingSetting()
                        }
                        indicator: Rectangle {
                            implicitWidth: 46
                            implicitHeight: 24
                            radius: 12
                            color: flightControllerLoggingSwitch.checked ? (root.darkTheme ? "#06363f" : "#dff4f8") : (root.darkTheme ? "#141c29" : "#eef4fb")
                            border.color: flightControllerLoggingSwitch.checked ? root.accentColor : root.borderColor
                            Rectangle {
                                x: flightControllerLoggingSwitch.checked ? parent.width - width - 3 : 3
                                anchors.verticalCenter: parent.verticalCenter
                                width: 18
                                height: 18
                                radius: 9
                                color: flightControllerLoggingSwitch.checked ? root.accentColor : root.labelColor
                                Behavior on x { NumberAnimation { duration: 120 } }
                            }
                        }
                        contentItem: Text {
                            text: flightControllerLoggingSwitch.checked ? "ON" : "OFF"
                            color: flightControllerLoggingSwitch.checked ? root.accentColor : root.labelColor
                            font.pixelSize: 12
                            font.bold: true
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 56
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
        Item {
            id: mapsPanel
            visible: root.activeCategory === "maps"
            anchors.left: sideRail.right
            anchors.leftMargin: 24
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.top: parent.top
            anchors.topMargin: 78
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                MapSettingRow {
                    title: "BASE MAP VIEW"
                    detail: root.mapViewMode === "street" ? "Street map is active" : "Satellite map is active"
                    control: RowLayout {
                        spacing: 8
                        SmallActionButton { text: "STREET"; active: root.mapViewMode === "street"; onClicked: root.setMapViewMode("street") }
                        SmallActionButton { text: "SATELLITE"; active: root.mapViewMode !== "street"; onClicked: root.setMapViewMode("satellite") }
                    }
                }

                MapSettingRow {
                    title: "OFFLINE MAPS"
                    detail: root.tileCache ? (root.tileCache.offlineMode ? "Offline: cached map data only" : "Online with automatic tile caching") : "Offline map cache unavailable"
                    control: Switch {
                        id: offlineMapsSwitch
                        checked: root.tileCache && root.tileCache.offlineMode
                        enabled: root.tileCache !== null
                        onToggled: if (root.tileCache) root.tileCache.offlineMode = checked
                        indicator: Rectangle {
                            implicitWidth: 46; implicitHeight: 24; radius: 12
                            color: offlineMapsSwitch.checked ? (root.darkTheme ? "#06363f" : "#dff4f8") : (root.darkTheme ? "#141c29" : "#eef4fb")
                            border.color: offlineMapsSwitch.checked ? root.accentColor : root.borderColor
                            Rectangle { x: offlineMapsSwitch.checked ? parent.width - width - 3 : 3; anchors.verticalCenter: parent.verticalCenter; width: 18; height: 18; radius: 9; color: offlineMapsSwitch.checked ? root.accentColor : root.labelColor; Behavior on x { NumberAnimation { duration: 120 } } }
                        }
                        contentItem: Text { text: offlineMapsSwitch.checked ? "OFFLINE" : "ONLINE"; color: offlineMapsSwitch.checked ? root.accentColor : root.labelColor; font.pixelSize: 12; font.bold: true; verticalAlignment: Text.AlignVCenter; leftPadding: 56 }
                    }
                }

                MapSettingRow {
                    title: "TILE CACHE"
                    detail: root.tileCache ? root.mapCacheSizeText(root.tileCache.cacheSizeBytes) + " used of " + root.mapCacheSizeText(root.tileCache.maximumCacheSizeBytes) : "Unavailable"
                    control: RowLayout {
                        spacing: 8
                        SmallActionButton { text: "CLEAR"; enabled: root.tileCache !== null; onClicked: root.tileCache.clearCache() }
                        TextField {
                            id: cacheLimitField
                            Layout.preferredWidth: 62; Layout.preferredHeight: 30
                            text: root.tileCache ? Math.round(root.tileCache.maximumCacheSizeBytes / (1024 * 1024)).toString() : "1024"
                            inputMethodHints: Qt.ImhDigitsOnly
                            color: root.textColor; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter
                            onEditingFinished: if (root.tileCache) root.tileCache.setMaximumCacheSizeMB(Number(text))
                            background: Rectangle { color: root.inputColor; border.color: cacheLimitField.activeFocus ? root.strongBorderColor : root.borderColor; border.width: 1 }
                        }
                        Text { text: "MB"; color: root.labelColor; font.pixelSize: 11 }
                    }
                }

                MapSettingRow {
                    title: "MISSION AREA CACHE"
                    detail: root.tileCache ? root.tileCache.prefetchStatus : "Load a mission before caching its area"
                    control: SmallActionButton {
                        text: root.tileCache && root.tileCache.prefetchActive ? (root.tileCache.prefetchCompleted + "/" + root.tileCache.prefetchTotal) : "DOWNLOAD"
                        active: root.tileCache && root.tileCache.prefetchActive
                        enabled: root.tileCache !== null && root.mavlinkObject && root.mavlinkObject.missionWaypoints.length > 0 && !(root.tileCache && root.tileCache.offlineMode)
                        onClicked: root.tileCache.prefetchMissionArea(root.mavlinkObject.missionWaypoints, Number(minimumZoomField.text), Number(maximumZoomField.text), Number(bufferField.text), root.selectedMapStyleUrl())
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text { text: "MISSION ZOOM"; color: root.labelColor; font.pixelSize: 10; font.bold: true }
                    TextField { id: minimumZoomField; Layout.preferredWidth: 40; Layout.preferredHeight: 28; text: "13"; inputMethodHints: Qt.ImhDigitsOnly; color: root.textColor; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; background: Rectangle { color: root.inputColor; border.color: root.borderColor; border.width: 1 } }
                    Text { text: "TO"; color: root.labelColor; font.pixelSize: 10 }
                    TextField { id: maximumZoomField; Layout.preferredWidth: 40; Layout.preferredHeight: 28; text: "17"; inputMethodHints: Qt.ImhDigitsOnly; color: root.textColor; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; background: Rectangle { color: root.inputColor; border.color: root.borderColor; border.width: 1 } }
                    Text { text: "BUFFER"; color: root.labelColor; font.pixelSize: 10; font.bold: true }
                    TextField { id: bufferField; Layout.preferredWidth: 54; Layout.preferredHeight: 28; text: "500"; inputMethodHints: Qt.ImhDigitsOnly; color: root.textColor; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; background: Rectangle { color: root.inputColor; border.color: root.borderColor; border.width: 1 } }
                    Text { text: "m"; color: root.labelColor; font.pixelSize: 10 }
                    Item { Layout.fillWidth: true }
                }

                MapSettingRow {
                    title: "RASTER OVERLAYS"
                    detail: root.rasterManager && root.rasterManager.enabled ? "GeoTIFF overlays enabled" : "Enable to import map overlays"
                    control: SmallActionButton {
                        text: root.rasterManager && root.rasterManager.enabled ? "ON" : "OFF"
                        active: root.rasterManager && root.rasterManager.enabled
                        enabled: root.rasterManager !== null
                        onClicked: if (root.rasterManager) root.rasterManager.enabled = !root.rasterManager.enabled
                    }
                }

                AddDashedButton {
                    Layout.fillWidth: true
                    label: "IMPORT GEOTIFF"
                    enabled: root.rasterManager && root.rasterManager.enabled
                    onClicked: rasterImportDialog.open()
                }

                Text {
                    Layout.fillWidth: true
                    text: root.rasterManager ? root.rasterManager.statusMessage : "Raster manager unavailable"
                    color: root.mutedColor
                    font.pixelSize: 9
                    elide: Text.ElideRight
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    Rectangle {
        x: panel.x
        y: panel.y
        width: panel.width
        height: panel.height
        visible: root.commAddModalOpen || root.cameraAddModalOpen
        color: root.darkTheme ? "#aa000308" : "#66dbe7f2"
        z: 20

        MouseArea { anchors.fill: parent }

        Rectangle {
            width: parent.width - 48
            height: root.cameraAddModalOpen ? 230 : 330
            anchors.centerIn: parent
            color: root.panelColor
            border.color: root.strongBorderColor
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: root.editingLink ? (root.cameraAddModalOpen ? "EDIT CAMERA" : "EDIT COMM LINK") : (root.cameraAddModalOpen ? "ADD CAMERA" : "ADD COMM LINK")
                    color: root.accentColor
                    font.pixelSize: 14
                    font.bold: true
                }

                FormField { id: cameraNameField; visible: root.cameraAddModalOpen; label: "NAME"; placeholder: "EO / VIS" }
                FormField { id: cameraRtspField; visible: root.cameraAddModalOpen; label: "RTSP"; placeholder: "rtsp://host/stream" }

                FormField { id: commNameField; visible: root.commAddModalOpen; label: "NAME"; placeholder: "Primary GCS" }

                ColumnLayout {
                    visible: root.commAddModalOpen
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "PROTOCOL"; color: root.labelColor; font.pixelSize: 10; font.bold: true }
                    ComboBox {
                        id: commProtocolBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        model: ["UDP", "TCP", "Serial"]
                        currentIndex: 0
                        onCurrentTextChanged: if (currentText === "Serial") root.refreshSerialPorts()
                        contentItem: Text {
                            text: commProtocolBox.displayText
                            color: root.textColor
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }
                        indicator: Text {
                            x: commProtocolBox.width - width - 12
                            y: (commProtocolBox.height - height) / 2
                            text: "v"
                            color: root.labelColor
                            font.pixelSize: 12
                        }
                        background: Rectangle { color: root.inputColor; border.color: commProtocolBox.activeFocus ? root.strongBorderColor : root.borderColor; border.width: 1 }
                        popup: Popup {
                            y: commProtocolBox.height + 2
                            width: commProtocolBox.width
                            implicitHeight: contentItem.implicitHeight
                            padding: 0
                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: commProtocolBox.popup.visible ? commProtocolBox.delegateModel : null
                                currentIndex: commProtocolBox.highlightedIndex
                            }
                            background: Rectangle { color: root.panelColor; border.color: root.borderColor; border.width: 1 }
                        }
                        delegate: ItemDelegate {
                            highlighted: commProtocolBox.highlightedIndex === index
                            width: commProtocolBox.width
                            height: 32
                            contentItem: Text {
                                text: modelData
                                color: highlighted ? root.accentColor : root.textColor
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 12
                            }
                            background: Rectangle { color: highlighted ? root.hoverColor : root.panelColor }
                        }
                    }
                }

                FormField { id: commIpField; visible: root.commAddModalOpen && commProtocolBox.currentText !== "Serial"; label: "IP ADDRESS"; placeholder: "192.168.1.100" }
                FormField { id: commPortField; visible: root.commAddModalOpen && commProtocolBox.currentText !== "Serial"; label: "PORT"; placeholder: "14550" }

                ColumnLayout {
                    visible: root.commAddModalOpen && commProtocolBox.currentText === "Serial"
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "SERIAL PORT"; color: root.labelColor; font.pixelSize: 10; font.bold: true }
                    ComboBox {
                        id: commSerialPortBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        model: root.serialPortOptions
                        enabled: root.serialPortsAvailable
                        contentItem: Text {
                            text: commSerialPortBox.displayText
                            color: commSerialPortBox.enabled ? root.textColor : root.disabledColor
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }
                        indicator: Text {
                            x: commSerialPortBox.width - width - 12
                            y: (commSerialPortBox.height - height) / 2
                            text: "v"
                            color: commSerialPortBox.enabled ? root.labelColor : root.disabledColor
                            font.pixelSize: 12
                        }
                        background: Rectangle { color: root.inputColor; border.color: commSerialPortBox.activeFocus ? root.strongBorderColor : root.borderColor; border.width: 1 }
                    }
                }

                ColumnLayout {
                    visible: root.commAddModalOpen && commProtocolBox.currentText === "Serial"
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "BAUD RATE"; color: root.labelColor; font.pixelSize: 10; font.bold: true }
                    ComboBox {
                        id: commBaudRateBox
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        model: root.serialBaudRates
                        enabled: root.serialPortsAvailable
                        currentIndex: root.baudRateIndex(57600)
                        contentItem: Text {
                            text: commBaudRateBox.displayText
                            color: commBaudRateBox.enabled ? root.textColor : root.disabledColor
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 12
                        }
                        indicator: Text {
                            x: commBaudRateBox.width - width - 12
                            y: (commBaudRateBox.height - height) / 2
                            text: "v"
                            color: commBaudRateBox.enabled ? root.labelColor : root.disabledColor
                            font.pixelSize: 12
                        }
                        background: Rectangle { color: root.inputColor; border.color: commBaudRateBox.activeFocus ? root.strongBorderColor : root.borderColor; border.width: 1 }
                    }
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Item { Layout.fillWidth: true }
                    SmallActionButton { text: "CANCEL"; onClicked: root.closeLinkEditor() }
                    SmallActionButton { text: "SAVE"; active: true; onClicked: root.cameraAddModalOpen ? root.saveCameraLinkFromForm() : root.saveCommLinkFromForm() }
                }
            }
        }
    }

    Connections {
        target: root.mavlinkObject

        function onConnectionStatusChanged(connected) {
            if (!connected) {
                root.pendingCommAddress = ""
                root.setCommLinkConnectionState("", false)
                return
            }

            root.setCommLinkConnectionState(root.pendingCommAddress, true)
            root.pendingCommAddress = ""
        }
    }

    Connections {
        target: root.videoObject

        function onStreamingChanged() {
            if (root.videoObject && root.videoObject.streaming) {
                root.setCameraLinkConnectionState(root.videoObject.rtspLink, true)
                root.pendingCameraAddress = ""
            } else {
                root.pendingCameraAddress = ""
                root.setCameraLinkConnectionState("", false)
            }
        }
    }

    Rectangle {
        x: panel.x
        y: panel.y
        width: panel.width
        height: panel.height
        visible: root.oemPasswordModalOpen
        color: root.darkTheme ? "#aa000308" : "#66dbe7f2"
        z: 21

        MouseArea { anchors.fill: parent }

        Rectangle {
            width: Math.min(parent.width - 48, 420)
            height: 220
            anchors.centerIn: parent
            color: root.panelColor
            border.color: root.strongBorderColor
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: "OEM ACCESS"
                    color: root.accentColor
                    font.pixelSize: 14
                    font.bold: true
                }

                Text {
                    Layout.fillWidth: true
                    text: "Enter OEM password to continue"
                    color: root.mutedColor
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "PASSWORD"
                        color: root.labelColor
                        font.pixelSize: 10
                        font.bold: true
                    }

                    TextField {
                        id: oemPasswordField

                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        placeholderText: "Enter OEM password"
                        placeholderTextColor: root.mutedColor
                        color: root.textColor
                        font.pixelSize: 12
                        selectByMouse: true
                        echoMode: TextInput.Password
                        onAccepted: root.verifyOemPassword()
                        background: Rectangle {
                            color: root.inputColor
                            border.color: oemPasswordField.activeFocus ? root.strongBorderColor : root.borderColor
                            border.width: 1
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16
                    text: root.oemPasswordErrorText
                    color: "#ff6666"
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Item { Layout.fillWidth: true }
                    SmallActionButton { text: "CANCEL"; onClicked: root.closeOemPasswordPrompt() }
                    SmallActionButton { text: "OK"; active: true; onClicked: root.verifyOemPassword() }
                }
            }
        }
    }

    component LinkCard: Rectangle {
        id: card
        property int linkIndex: -1
        property string linkName: ""
        property string linkAddress: ""
        property string linkProtocol: "UDP"
        property int linkBaudRate: 57600
        property bool connected: false

        height: 70
        color: root.cardColor
        border.color: root.borderColor
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 8
            spacing: 7

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: card.linkName
                    color: root.textColor
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: card.linkProtocol === "Serial" ? card.linkProtocol + "  " + card.linkAddress + " @ " + card.linkBaudRate : card.linkProtocol.length > 0 ? card.linkProtocol + "  " + card.linkAddress : card.linkAddress
                    color: root.mutedColor
                    font.pixelSize: 12
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: 9
                    font.family: "Consolas"
                    elide: Text.ElideRight
                }
            }


            Item {
                readonly property bool connecting: !card.connected
                                               && ((root.activeCategory === "camera" && root.pendingCameraAddress === card.linkAddress)
                                                   || (root.activeCategory === "comm" && root.pendingCommAddress === card.linkAddress))
                Layout.preferredWidth: card.connected ? 76 : (connecting ? 120 : 64)
                Layout.preferredHeight: 28

                SmallActionButton {
                    anchors.fill: parent
                    visible: !parent.connecting
                    text: card.connected ? "DISCONNECT" : "CONNECT"
                    active: !card.connected
                    warning: card.connected
                    onClicked: root.connectLink(card.linkIndex)
                }

                RowLayout {
                    anchors.fill: parent
                    visible: parent.connecting
                    layoutDirection: Qt.LeftToRight
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 5

                    ConnectingSpinner {
                        visible: parent.visible
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        spinnerColor: root.accentColor
                    }

                    Text {
                        text: "CONNECTING"
                        color: root.accentColor
                        font.pixelSize: 10
                        font.bold: true
                    }

                    IconActionButton {
                        iconType: "cancel"
                        toolTipText: "Cancel connection"
                        onClicked: root.cancelPendingConnection()
                    }
                }
            }

            IconActionButton {
                iconType: "edit"
                toolTipText: "Edit link"
                onClicked: root.editLink(card.linkIndex)
            }

            IconActionButton {
                iconType: "trash"
                warning: true
                toolTipText: "Delete link"
                onClicked: root.removeLink(card.linkIndex)
            }
        }
    }

    component IconActionButton: Button {
        id: iconButton
        property string iconType: "edit"
        property string toolTipText: ""
        property bool warning: false
        Layout.preferredWidth: 36
        Layout.preferredHeight: 32
        padding: 0
        flat: true
        hoverEnabled: true
        ToolTip.visible: hovered && toolTipText.length > 0
        ToolTip.text: toolTipText

        contentItem: CanvasIcon {
            anchors.centerIn: parent
            width: 22
            height: 22
            iconType: iconButton.iconType
            color: !iconButton.enabled ? root.disabledColor
                   : iconButton.warning && iconButton.hovered ? "#ff6b5f"
                   : iconButton.hovered ? root.textColor
                   : "#aab5c7"
        }

        background: Rectangle {
            color: iconButton.down ? (root.darkTheme ? "#182131" : "#e1edf8") : iconButton.hovered ? root.hoverColor : "#00000000"
            border.width: 0
        }
    }

    component ConnectingSpinner: Item {
        id: spinner
        property color spinnerColor: root.accentColor
        implicitWidth: 18
        implicitHeight: 18

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
                ctx.arc(width / 2, height / 2, 6, Math.PI * 0.15, Math.PI * 1.55)
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

    component AddDashedButton: Button {
        id: addButton
        property string label: "ADD LINK"
        Layout.preferredHeight: 50
        height: 50
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: Item {
            Row {
                anchors.centerIn: parent
                spacing: 8
                Text {
                    text: "+"
                    color: addButton.enabled ? root.labelColor : root.disabledColor
                    font.pixelSize: 18
                    verticalAlignment: Text.AlignVCenter
                }
                Text {
                    id: addLabel
                    text: addButton.label
                    color: addButton.enabled ? root.labelColor : root.disabledColor
                    font.pixelSize: 12
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        background: Item {
            Rectangle {
                anchors.fill: parent
                color: addButton.hovered && addButton.enabled ? root.hoverColor : "#00000000"
            }
            Canvas {
                anchors.fill: parent
                property color dashColor: addButton.enabled ? root.borderColor : root.disabledColor
                onDashColorChanged: requestPaint()
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = dashColor
                    ctx.lineWidth = 1
                    ctx.setLineDash([3, 3])
                    ctx.strokeRect(0.5, 0.5, width - 1, height - 1)
                }
            }
        }
    }

    component MapSettingRow: Rectangle {
        id: rowRoot
        property string title: ""
        property string detail: ""
        property Component control
        Layout.fillWidth: true
        Layout.preferredHeight: 72
        color: root.cardColor
        border.color: root.borderColor
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { Layout.fillWidth: true; text: rowRoot.title; color: root.textColor; font.pixelSize: 13; font.bold: true }
                Text { Layout.fillWidth: true; text: rowRoot.detail; color: root.mutedColor; font.pixelSize: 11; elide: Text.ElideRight }
            }
            Loader { sourceComponent: rowRoot.control }
        }
    }

    component SmallActionButton: Button {
        id: actionButton
        property bool active: false
        property bool warning: false
        Layout.preferredWidth: Math.max(76, implicitContentWidth + 22)
        Layout.preferredHeight: 34
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: Text {
            text: actionButton.text
            color: !actionButton.enabled ? root.disabledColor : actionButton.warning ? "#ff9a16" : actionButton.active ? root.accentColor : root.labelColor
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: actionButton.down ? (root.darkTheme ? "#182131" : "#e1edf8") : actionButton.hovered ? root.hoverColor : root.inputColor
            border.color: !actionButton.enabled ? root.borderColor : actionButton.warning ? "#ff8a00" : actionButton.active ? root.strongBorderColor : root.borderColor
            border.width: 1
        }
    }

    component FormField: ColumnLayout {
        id: fieldRoot
        property string label: ""
        property string placeholder: ""
        property alias text: input.text
        Layout.fillWidth: true
        spacing: 4

        Text { text: fieldRoot.label; color: root.labelColor; font.pixelSize: 10; font.bold: true }
        TextField {
            id: input
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            placeholderText: fieldRoot.placeholder
            placeholderTextColor: root.mutedColor
            color: root.textColor
            font.pixelSize: 12
            selectByMouse: true
            background: Rectangle { color: root.inputColor; border.color: input.activeFocus ? root.strongBorderColor : root.borderColor; border.width: 1 }
        }
    }

    component SideButton: Button {
        id: sideButton
        property bool selected: false
        property string iconType: "comm"
        property string label: ""
        width: 72
        height: 76
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: ColumnLayout {
            anchors.centerIn: parent
            spacing: 5
            CanvasIcon {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                iconType: sideButton.iconType
                color: sideButton.selected ? root.accentColor : root.labelColor
            }
            Text {
                visible: sideButton.label.length > 0
                text: sideButton.label
                color: sideButton.selected ? root.accentColor : root.labelColor
                font.pixelSize: 10
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }
        }

        background: Rectangle {
            color: sideButton.selected ? root.selectedColor : sideButton.hovered ? root.hoverColor : "#00000000"
            border.color: sideButton.selected ? root.strongBorderColor : root.borderColor
            border.width: sideButton.selected ? 1 : 0
        }
    }

    component CanvasIcon: Canvas {
        id: iconCanvas
        property string iconType: "comm"
        property color color: "#ffffff"

        onIconTypeChanged: requestPaint()
        onColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = iconCanvas.color
            ctx.fillStyle = iconCanvas.color
            ctx.lineWidth = 1.8
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            var cx = width / 2
            var cy = height / 2

            if (iconType === "gear") {
                ctx.beginPath(); ctx.arc(cx, cy, 5, 0, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.arc(cx, cy, 1.8, 0, Math.PI * 2); ctx.stroke()
                for (var i = 0; i < 8; i++) {
                    var a = i * Math.PI / 4
                    ctx.beginPath(); ctx.moveTo(cx + Math.cos(a) * 7, cy + Math.sin(a) * 7); ctx.lineTo(cx + Math.cos(a) * 9, cy + Math.sin(a) * 9); ctx.stroke()
                }
                return
            }

            if (iconType === "general") {
                ctx.strokeRect(5, 5, width - 10, height - 10)
                ctx.beginPath(); ctx.moveTo(9, 10); ctx.lineTo(11, 12); ctx.lineTo(15, 8); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(17, 10); ctx.lineTo(width - 7, 10); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(9, 16); ctx.lineTo(11, 18); ctx.lineTo(15, 14); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(17, 16); ctx.lineTo(width - 7, 16); ctx.stroke()
                return
            }

            if (iconType === "comm") {
                ctx.beginPath(); ctx.arc(cx, cy + 6, 2, Math.PI, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.arc(cx, cy + 6, 7, Math.PI * 1.12, Math.PI * 1.88); ctx.stroke()
                ctx.beginPath(); ctx.arc(cx, cy + 6, 12, Math.PI * 1.15, Math.PI * 1.85); ctx.stroke()
                return
            }

            if (iconType === "camera") {
                ctx.strokeRect(4, 7, width - 8, height - 10)
                ctx.beginPath(); ctx.arc(cx, cy + 1, 4, 0, Math.PI * 2); ctx.stroke()
                ctx.strokeRect(8, 4, 5, 3)
                return
            }

            if (iconType === "log") {
                ctx.beginPath()
                ctx.moveTo(7, 4)
                ctx.lineTo(width - 8, 4)
                ctx.lineTo(width - 5, 7)
                ctx.lineTo(width - 5, height - 5)
                ctx.lineTo(7, height - 5)
                ctx.closePath()
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(width - 8, 4)
                ctx.lineTo(width - 8, 8)
                ctx.lineTo(width - 5, 8)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(10, 11)
                ctx.lineTo(width - 9, 11)
                ctx.moveTo(10, 15)
                ctx.lineTo(width - 9, 15)
                ctx.moveTo(10, 19)
                ctx.lineTo(width - 12, 19)
                ctx.stroke()
                return
            }

            if (iconType === "maps") {
                for (var j = 0; j < 3; j++) {
                    var y = 6 + j * 5
                    ctx.beginPath(); ctx.moveTo(4, y); ctx.lineTo(cx, y - 3); ctx.lineTo(width - 4, y); ctx.lineTo(cx, y + 3); ctx.closePath(); ctx.stroke()
                }
                return
            }

            if (iconType === "edit") {
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(5, height - 5)
                ctx.lineTo(6.8, height - 11)
                ctx.lineTo(width - 8, 6.2)
                ctx.lineTo(width - 5.2, 9)
                ctx.lineTo(11, height - 6.8)
                ctx.closePath()
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(width - 10.2, 8)
                ctx.lineTo(width - 7.2, 11)
                ctx.stroke()
                return
            }

            if (iconType === "trash") {
                ctx.lineWidth = 2
                ctx.strokeRect(7.5, 8.5, width - 15, height - 10)
                ctx.beginPath()
                ctx.moveTo(5.5, 8.5)
                ctx.lineTo(width - 5.5, 8.5)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(9, 5.5)
                ctx.lineTo(width - 9, 5.5)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(11.5, 11.5)
                ctx.lineTo(11.5, height - 5)
                ctx.moveTo(width - 11.5, 11.5)
                ctx.lineTo(width - 11.5, height - 5)
                ctx.stroke()
                return
            }

            if (iconType === "cancel") {
                ctx.beginPath()
                ctx.moveTo(8, 8)
                ctx.lineTo(width - 8, height - 8)
                ctx.moveTo(width - 8, 8)
                ctx.lineTo(8, height - 8)
                ctx.stroke()
                return
            }
        }
    }
}
