import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DroneTerrainSim

ApplicationWindow {
    id: window
    width: 1440
    height: 900
    minimumWidth: 980
    minimumHeight: 640
    visible: true
    title: "Drone Terrain Simulator"
    color: "#101619"

    palette.window: "#151d21"
    palette.windowText: "#e5ecee"
    palette.base: "#202a2f"
    palette.text: "#e5ecee"
    palette.button: "#28353a"
    palette.buttonText: "#e5ecee"
    palette.highlight: "#34a889"
    palette.highlightedText: "#ffffff"

    function loadSelectedRegion() {
        errorBanner.text = ""
        const centerLat = Math.max(-84.5, Math.min(84.5, Number(centerLatField.text)))
        const centerLon = Math.max(-180.0, Math.min(180.0, Number(centerLonField.text)))
        const radiusKm = Math.max(0.1, Math.min(1000.0, Number(radiusField.value)))
        terrainView.terrainManager.loadRegion(centerLat, centerLon, radiusKm,
                                              Number(zoomField.value), tileUrlField.text)
    }

    Component.onCompleted: loadSelectedRegion()

    ImageryTileProvider {
        id: imageryTileProvider
    }

    MavlinkReceiver {
        id: vehicle
        listenPort: Number(portField.text)
        tcpHost: tcpHostField.text
        tcpPort: Number(tcpPortField.text)
        tcpEnabled: tcpSwitch.checked
        demoMode: demoSwitch.checked
        originLatitude: terrainView.terrainManager.originLatitude
        originLongitude: terrainView.terrainManager.originLongitude
        originHeight: terrainView.terrainManager.originHeight
        onErrorOccurred: message => errorBanner.text = message
    }

    header: ToolBar {
        height: 52
        background: Rectangle { color: "#162024" }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 8

            Label {
                text: "DRONE TERRAIN"
                font.pixelSize: 15
                font.bold: true
                color: "#f0f5f6"
                Layout.rightMargin: 14
            }

            Button {
                text: "Orbit"
                checkable: true
                checked: terrainView.cameraMode === "ORBIT"
                onClicked: terrainView.setOrbitPreset()
            }
            Button {
                text: "Top"
                checkable: true
                checked: terrainView.cameraMode === "TOP"
                onClicked: terrainView.setTopPreset()
            }
            Button {
                text: "Side"
                checkable: true
                checked: terrainView.cameraMode === "SIDE"
                onClicked: terrainView.setSidePreset()
            }
            Button {
                text: "Chase"
                checkable: true
                checked: terrainView.cameraMode === "CHASE"
                onClicked: terrainView.setChasePreset()
            }
            Button {
                text: "Explore"
                checkable: true
                checked: terrainView.cameraMode === "EXPLORE"
                onClicked: terrainView.setExplorePreset()
                ToolTip.visible: hovered
                ToolTip.text: "Left-drag to look around, right-drag to pan across the loaded region, scroll to zoom"
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: vehicle.connected ? "#34d399" : "#f2b84b"
            }
            Label {
                text: vehicle.connected ? "SITL " + vehicle.systemId : "DEMO"
                font.pixelSize: 12
                font.bold: true
            }
            Label {
                text: vehicle.flightMode
                color: "#aebdc1"
                Layout.leftMargin: 6
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 326
            Layout.fillHeight: true
            color: "#172126"

            ScrollView {
                anchors.fill: parent
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    Label {
                        text: "REGION"
                        font.pixelSize: 12
                        font.bold: true
                        color: "#8fa3a8"
                        Layout.topMargin: 18
                        Layout.leftMargin: 16
                    }

                    GridLayout {
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 8
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16

                        Label { text: "Center lat"; color: "#aebdc1" }
                        TextField { id: centerLatField; text: "28.2707850"; Layout.fillWidth: true }
                        Label { text: "Center lon"; color: "#aebdc1" }
                        TextField { id: centerLonField; text: "85.4542458"; Layout.fillWidth: true }
                        Label { text: "Radius km"; color: "#aebdc1" }
                        SpinBox {
                            id: radiusField
                            from: 1
                            to: 1000
                            value: 25
                            editable: true
                            Layout.fillWidth: true
                        }
                        Label { text: "Zoom"; color: "#aebdc1" }
                        SpinBox {
                            id: zoomField
                            from: 1
                            to: 22
                            value: 11
                            editable: true
                            hoverEnabled: true
                            Layout.fillWidth: true
                            ToolTip.visible: hovered
                            ToolTip.text: "Minimum mesh zoom — the app auto-refines finer than this whenever the tile budget allows, so it's a floor, not a ceiling."
                        }
                    }

                    Label {
                        text: "IMAGERY URL"
                        font.pixelSize: 12
                        font.bold: true
                        color: "#8fa3a8"
                        Layout.topMargin: 8
                        Layout.leftMargin: 16
                    }

                    TextField {
                        id: tileUrlField
                        placeholderText: "https://server/{z}/{x}/{y}.jpg"
                        text: "https://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"
                        selectByMouse: true
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16

                        Button {
                            text: terrainView.terrainManager.busy ? "Loading" : "Load region"
                            enabled: !terrainView.terrainManager.busy
                            Layout.fillWidth: true
                            onClicked: window.loadSelectedRegion()
                        }
                        Button {
                            text: "Cancel"
                            enabled: terrainView.terrainManager.busy
                            onClicked: terrainView.terrainManager.cancel()
                        }
                    }

                    ProgressBar {
                        value: terrainView.terrainManager.progress
                        visible: terrainView.terrainManager.busy
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                    }

                    Label {
                        text: terrainView.terrainManager.status
                        color: "#9fb0b4"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                    }

                    Label {
                        id: errorBanner
                        color: "#ff9b8f"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        visible: text.length > 0
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                    }

                    Rectangle {
                        color: "#2a363b"
                        height: 1
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                    }

                    Label {
                        text: "TERRAIN"
                        font.pixelSize: 12
                        font.bold: true
                        color: "#8fa3a8"
                        Layout.leftMargin: 16
                    }

                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16

                        Label { text: "Mesh"; color: "#aebdc1" }
                        ComboBox {
                            id: meshSize
                            model: ["17", "33", "65"]
                            currentIndex: 1
                            Layout.fillWidth: true
                            onActivated: terrainView.terrainManager.gridSize = Number(currentText)
                        }
                        Label { text: "Height"; color: "#aebdc1" }
                        SpinBox {
                            id: verticalScale
                            from: 1
                            to: 20
                            value: 4
                            Layout.fillWidth: true
                            onValueModified: terrainView.terrainManager.verticalScale = value
                        }
                        Label { text: "Detail"; color: "#aebdc1" }
                        SpinBox {
                            id: imageryDetail
                            from: 0
                            to: 4
                            value: 3
                            hoverEnabled: true
                            Layout.fillWidth: true
                            onValueModified: terrainView.terrainManager.imageryDetailLevels = value
                            ToolTip.visible: hovered
                            ToolTip.text: "Base imagery sharpness applied when a region first loads, for tiles the camera isn't near yet. Once loaded, each tile sharpens further on its own as the camera gets closer to it (Cesium-style), independent of this setting."
                        }
                        Label { text: "DEM"; color: "#aebdc1" }
                        Label {
                            text: terrainView.terrainManager.hasDemCoverage
                                  ? "SRTM " + terrainView.terrainManager.loadedDemTileCount
                                    + "/" + terrainView.terrainManager.demTileCount
                                  : "Missing " + terrainView.terrainManager.missingDemTiles
                            color: terrainView.terrainManager.hasDemCoverage ? "#34d399" : "#f2b84b"
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                        Label { text: "Elev"; color: "#aebdc1" }
                        Label {
                            text: terrainView.terrainManager.elevationMinimum.toFixed(1)
                                  + " .. "
                                  + terrainView.terrainManager.elevationMaximum.toFixed(1)
                                  + " m"
                            color: "#9fb0b4"
                        }
                        Label { text: "DEM folder"; color: "#aebdc1" }
                        Label {
                            text: imageryTileProvider.demDirectoryUrl.toString().replace("file:///", "")
                            color: "#9fb0b4"
                            font.pixelSize: 10
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                    }

                    Rectangle {
                        color: "#2a363b"
                        height: 1
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                    }

                    Label {
                        text: "SITL"
                        font.pixelSize: 12
                        font.bold: true
                        color: "#8fa3a8"
                        Layout.leftMargin: 16
                    }

                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16

                        Label { text: "UDP port"; color: "#aebdc1" }
                        TextField { id: portField; text: "14550"; inputMethodHints: Qt.ImhDigitsOnly; Layout.fillWidth: true }
                        Label { text: "TCP client"; color: "#aebdc1" }
                        Switch { id: tcpSwitch; checked: false }
                        Label { text: "TCP host"; color: "#aebdc1" }
                        TextField { id: tcpHostField; text: "127.0.0.1"; Layout.fillWidth: true }
                        Label { text: "TCP port"; color: "#aebdc1" }
                        TextField { id: tcpPortField; text: "5760"; inputMethodHints: Qt.ImhDigitsOnly; Layout.fillWidth: true }
                        Label { text: "TCP state"; color: "#aebdc1" }
                        Label {
                            text: vehicle.tcpConnected ? "Connected" : (vehicle.tcpEnabled ? "Connecting" : "Off")
                            color: vehicle.tcpConnected ? "#34d399" : (vehicle.tcpEnabled ? "#f2b84b" : "#8fa3a8")
                        }
                        Label { text: "Demo flight"; color: "#aebdc1" }
                        Switch { id: demoSwitch; checked: true }
                        Label { text: "Groundspeed"; color: "#aebdc1" }
                        Label { text: vehicle.groundspeed.toFixed(1) + " m/s" }
                        Label { text: "Relative alt"; color: "#aebdc1" }
                        Label { text: vehicle.relativeAltitude.toFixed(1) + " m" }
                        Label { text: "Position"; color: "#aebdc1" }
                        Label {
                            text: vehicle.latitude.toFixed(5) + ", " + vehicle.longitude.toFixed(5)
                            font.pixelSize: 11
                        }
                    }

                    Item { Layout.fillHeight: true; Layout.minimumHeight: 18 }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TerrainView {
                id: terrainView
                anchors.fill: parent
                imageryTileProvider: imageryTileProvider
                vehicle: vehicle
                onErrorOccurred: message => errorBanner.text = message
            }

            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 14
                width: cursorLabel.implicitWidth + 24
                height: 34
                radius: 4
                color: "#d9131b1e"
                border.color: "#46565c"

                Label {
                    id: cursorLabel
                    anchors.centerIn: parent
                    text: terrainView.cursorReadout
                    font.family: "Consolas"
                    font.pixelSize: 12
                    color: "#edf3f4"
                }
            }

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 14
                width: telemetryLabel.implicitWidth + 24
                height: 34
                radius: 4
                color: "#d9131b1e"
                border.color: vehicle.connected ? "#34a889" : "#8f7137"

                Label {
                    id: telemetryLabel
                    anchors.centerIn: parent
                    text: vehicle.flightMode + "  " + vehicle.relativeAltitude.toFixed(1) + " m  "
                          + vehicle.groundspeed.toFixed(1) + " m/s"
                    font.family: "Consolas"
                    font.pixelSize: 12
                }
            }

            Rectangle {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 14
                width: imageryCredit.implicitWidth + 20
                height: 28
                radius: 4
                color: "#d9131b1e"
                border.color: "#46565c"
                visible: tileUrlField.text.indexOf("arcgisonline.com") >= 0

                Label {
                    id: imageryCredit
                    anchors.centerIn: parent
                    text: "Imagery: Esri, Maxar, Earthstar Geographics, GIS User Community"
                    font.pixelSize: 10
                    color: "#d7e0e2"
                }
            }
        }
    }
}
