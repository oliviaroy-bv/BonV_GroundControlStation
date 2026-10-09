import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0

Item {
    id: root

    // Drone list driven by DroneManager — refreshed when fleet changes
    property var  drones:          []
    property var  currentDrone:    null
    property bool isReturn:        false
    // Selected mission type (from combo current text)
    property string selectedMission: missionCombo.currentText || "Delivery"
    property bool   isDropMission:   selectedMission === "Drop" || selectedMission === "Paradrop"
    property bool   isDelivery:      selectedMission === "Delivery"

    // Drop/Paradrop always return — they never land at destination
    property bool forcedReturn: isDropMission
    property real distResult:      -1
    property bool inRange:         false
    property bool profileExpanded: true
    readonly property bool isAndroid: Qt.platform.os === "android"
    readonly property bool androidPortrait: isAndroid && height > width
    readonly property bool showLeftPanel: !isAndroid || sideNav.currentIndex === 0
    readonly property int adaptiveLeftPanelW: isAndroid
                                             ? Math.max(300, Math.min(androidPortrait ? 340 : 420,
                                                                      Math.round(width * (androidPortrait ? 0.42 : 0.34))))
                                             : Theme.leftPanelW
    readonly property int adaptiveMapH: isAndroid
                                        ? Math.max(220, Math.min(Math.round(height * 0.42), 430))
                                        : 400
    readonly property int adaptiveProfileH: isAndroid ? 140 : 220
    readonly property int adaptiveProfileBarH: isAndroid ? 24 : 28

    // Last generated mission — so 3D view loads immediately when tab opened
    property var  lastWaypoints:   []
    property var  lastProfile:     []
    property var  lastBounds:      null

    // ── Initialise drone list once context properties are available ──────────
    Component.onCompleted: {
        outputDirField.text = backend.defaultOutputDir()
        srtmDirField.text = backend.srtmCacheDir()
        root.drones = droneManager.activeDrones()
        if (root.drones.length > 0) {
            root.currentDrone = root.drones[0]
            Qt.callLater(function() {
                root.applyDroneSelection(0, true)
            })
        }
    }

    onIsDropMissionChanged: {
        if (root.isDropMission) {
            root.isReturn = true
            tripToggle.forceReturn()
        }
    }

    // ── Backend connections ───────────────────────────────────────────────────
    Connections {
        target: backend
        ignoreUnknownSignals: true

        function onLogLine(level, message)  { log.appendLine(level, message) }
        function onProgressChanged(percent) {
            progressBar.value   = percent
            progressBar.visible = (percent > 0 && percent < 100)
        }
        function onStatusChanged(text, busy) {
            statusBar.statusText = text
            statusBar.busy       = busy
        }
        function onSrtmInfoChanged(text)    { statusBar.srtmInfo = text }
        function onOutputDirChanged(dir)    { outputDirField.text = dir }
        function onRouteReady(payload) {
            if (payload.length < 1) return
            var data      = payload[0]
            var profile   = data["profile"]   || []
            var waypoints = data["waypoints"] || []

            // 2D map
            mapView.setRoute(waypoints)

            // Terrain profile chart
            terrainProfile.points = profile
            profilePanel.visible  = profile.length >= 2

            // Compute bounding box of waypoints
            if (waypoints.length >= 2) {
                var minLat = waypoints[0].lat, maxLat = waypoints[0].lat
                var minLon = waypoints[0].lon, maxLon = waypoints[0].lon
                for (var i = 1; i < waypoints.length; i++) {
                    if (waypoints[i].lat < minLat) minLat = waypoints[i].lat
                    if (waypoints[i].lat > maxLat) maxLat = waypoints[i].lat
                    if (waypoints[i].lon < minLon) minLon = waypoints[i].lon
                    if (waypoints[i].lon > maxLon) maxLon = waypoints[i].lon
                }
                var bounds = { minLat: minLat, maxLat: maxLat, minLon: minLon, maxLon: maxLon }

                // Store for when 3D tab is opened later
                root.lastWaypoints = waypoints
                root.lastProfile   = profile
                root.lastBounds    = bounds

                // Only call setMission immediately if 3D tab is already open
                if (sideNav.currentIndex === 2)
                    view3d.setMission(waypoints, profile, bounds)
            }

            if (waypoints.length >= 2) {
                var f = waypoints[0]
                var l = waypoints[waypoints.length - 1]
                mapView.panTo((f.lat + l.lat) / 2, (f.lon + l.lon) / 2)
            }
        }
    }

    // ── Root layout ───────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TitleBar { Layout.fillWidth: true }

        RowLayout {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            spacing: 0

            // ── Sidebar ───────────────────────────────────────────────────────
            SideNav {
                id: sideNav
                Layout.fillHeight: true
                currentIndex: 0
                onIndexChanged: function(i) {
                    // Map sidebar index to left panel page:
                    // 0=Mission→0, 1=Drones→1, 2=3D→stays on 0 (form visible), 3=Settings→2
                    if (i === 1) {
                        leftStack.currentIndex = 0   // Fleet tab: left stays on mission form
                    } else if (i === 2) {
                        leftStack.currentIndex = 1   // Drone Catalogue: left page 1
                    } else if (i === 3) {
                        leftStack.currentIndex = 0   // 3D: left stays on mission form
                        if (root.lastBounds !== null && root.lastWaypoints.length >= 2)
                            view3d.setMission(root.lastWaypoints, root.lastProfile, root.lastBounds)
                    } else if (i === 4) {
                        leftStack.currentIndex = 0   // Settings: left stays on mission form
                    } else {
                        leftStack.currentIndex = i   // 0 = Mission form
                    }
                }
            }

            // ── Left panel — StackLayout switches views ────────────────────────
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: root.showLeftPanel ? root.adaptiveLeftPanelW : 0
                Layout.minimumWidth:   root.showLeftPanel ? (root.isAndroid ? 300 : Theme.leftPanelW) : 0
                Layout.maximumWidth:   root.showLeftPanel ? (root.isAndroid ? 420 : Theme.leftPanelW) : 0
                implicitWidth:         root.showLeftPanel ? root.adaptiveLeftPanelW : 0
                visible:               root.showLeftPanel
                color:             Theme.bg

                Rectangle {
                    anchors.right:  parent.right
                    width: 0.5; height: parent.height
                    color: Theme.border
                }

                StackLayout {
                    id: leftStack
                    anchors.fill: parent
                    currentIndex: 0

                    // ── Page 0: Mission Form ──────────────────────────────────
                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            Flickable {
                                id: flick
                                Layout.fillWidth:  true
                                Layout.fillHeight: true
                                contentHeight:     formCol.implicitHeight
                                clip:              true
                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                    width:  5
                                }

                                ColumnLayout {
                                    id: formCol
                                    width:   flick.width
                                    spacing: 0

                                    // Mission Setup
                                    SectionBlock {
                                        title: "Mission Setup"
                                        Layout.fillWidth: true

                                        ColumnLayout {
                                            spacing: 8
                                            width: parent.width

                                            FormRow {
                                                label: "Drone"
                                                FpComboBox {
                                                    id: droneCombo
                                                    Layout.fillWidth: true
                                                    model: root.drones.map(function(d) { return d.name })
                                                   
                                                    onCurrentIndexChanged: {

                                                        root.applyDroneSelection(currentIndex, true)

                                                    }
                                                }
                                            }

                                            FormRow {
                                                label: "Mission Type"
                                                FpComboBox {
                                                    id: missionCombo
                                                    Layout.fillWidth: true
                                                    // Populated dynamically based on selected drone's capabilities
                                                    model: root.currentDrone
                                                               ? (root.currentDrone.capabilities || ["Delivery"])
                                                               : ["Delivery"]
                                                }
                                            }

                                            FormRow {
                                                label: "Trip Mode"
                                                TripToggle {
                                                    id: tripToggle
                                                    Layout.fillWidth: true
                                                    enabled: !root.isDropMission
                                                    opacity: root.isDropMission ? 0.5 : 1.0
                                                    onModeChanged: function(ret) {
                                                        if (!root.isDropMission)
                                                            root.isReturn = ret
                                                    }
                                                }
                                            }

                                            // Drop/Paradrop locked-return notice
                                            Text {
                                                visible: root.isDropMission
                                                text: "⚠  Drop / Paradrop missions always return to takeoff point"
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSm
                                                color: Theme.amber
                                                wrapMode: Text.WordWrap
                                                Layout.fillWidth: true
                                                leftPadding: 4
                                            }
                                        }
                                    }

                                    // Coordinates
                                    SectionBlock {
                                        title: "Coordinates"
                                        Layout.fillWidth: true

                                        ColumnLayout {
                                            spacing: 8
                                            width: parent.width

                                            CoordGroup {
                                                id: takeoffGroup
                                                label: "Takeoff"
                                                Layout.fillWidth: true
                                                onLatChanged: root.syncTakeoffToMap()
                                                onLonChanged: root.syncTakeoffToMap()
                                            }

                                            CoordGroup {
                                                id: landGroup
                                                label: "Landing"
                                                Layout.fillWidth: true
                                                onLatChanged: root.syncLandToMap()
                                                onLonChanged: root.syncLandToMap()
                                            }

                                            RowLayout {
                                                spacing: 8
                                                Layout.fillWidth: true

                                                FpButton {
                                                    text: "Check Distance"
                                                    onClicked: root.checkDistance()
                                                }

                                                DistanceBadge {
                                                    id: distBadge
                                                    Layout.fillWidth: true
                                                    visible: false
                                                }
                                            }
                                        }
                                    }

                                    // Flight Parameters
                                    SectionBlock {
                                        title: "Flight Parameters"
                                        Layout.fillWidth: true

                                        ColumnLayout {
                                            spacing: 6
                                            width: parent.width

                                            // ── Always shown ──────────────────────────────────
                                            FpSpinBox {
                                                id: takeoffAltSpin
                                                label: "Takeoff Alt"
                                                value: 30; from: 5; to: 500; stepSize: 5
                                                unit: "m AGL"
                                                Layout.fillWidth: true
                                            }
                                            FpSpinBox {
                                                id: nearHomeAltSpin
                                                label: "Near-Home Alt"
                                                value: 60; from: 5; to: 500; stepSize: 5
                                                unit: "m AGL"
                                                Layout.fillWidth: true
                                            }
                                            FpSpinBox {
                                                id: cruiseAltSpin
                                                label: "Terrain Clearance"
                                                value: 120; from: 80; to: 800; stepSize: 10
                                                unit: "m"
                                                Layout.fillWidth: true
                                            }

                                            // ── Drop/Paradrop only: Dropping Alt ─────────────
                                            FpSpinBox {
                                                id: dropAltSpin
                                                visible: root.isDropMission
                                                label: "Dropping Alt"
                                                value: 30; from: 5; to: 300; stepSize: 5
                                                unit: "m AGL"
                                                Layout.fillWidth: true
                                            }

                                            // ── Land Alt — label changes for Drop/Paradrop ────
                                            FpSpinBox {
                                                id: landAltSpin
                                                label: root.isDropMission ? "Land Alt (Return side)" : "Land Alt"
                                                value: 80; from: 80; to: 150; stepSize: 5
                                                unit: "m AGL"
                                                Layout.fillWidth: true
                                            }

                                            FpSpinBox {
                                                id: speedSpin
                                                label: "Cruise Speed"
                                                value: 5; from: 1; to: 30; stepSize: 1
                                                unit: "m/s"
                                                decimals: 0
                                                Layout.fillWidth: true
                                            }

                                            // ── Delivery: Delay Time (seconds) ───────────────
                                            FpSpinBox {
                                                id: waitSpin
                                                visible: true
                                                label: root.isDropMission ? "Hover Time" : "Delay Time"
                                                value: 5; from: 0; to: 3600; stepSize: 5
                                                unit: "sec"
                                                enabled: root.isReturn || root.isDropMission
                                                Layout.fillWidth: true
                                            }
                                        }
                                    }

                                    // Output Folder
                                    SectionBlock {
                                        title: "Output Folder"
                                        Layout.fillWidth: true

                                        RowLayout {
                                            width:   parent.width
                                            spacing: 8

                                            FpInput {
                                                id: outputDirField
                                                Layout.fillWidth: true
                                                tinted: true
                                                placeholderText: "Select output folder..."
                                                font.pixelSize:  Theme.fontSm
                                            }

                                            FpButton {
                                                text: "Browse..."
                                                implicitWidth: 68
                                                onClicked: backend.browseOutputDir()
                                            }
                                        }
                                    }

                                    Item { implicitHeight: 8 }
                                }
                            }

                            // Generate + progress
                            ColumnLayout {
                                spacing: 4
                                Layout.fillWidth: true
                                Layout.margins:  12

                                ProgressBar {
                                    id: progressBar
                                    Layout.fillWidth: true
                                    from: 0; to: 100; value: 0
                                    visible: false

                                    background: Rectangle {
                                        color:          Theme.blueLight
                                        radius:         99
                                        implicitHeight: 3
                                    }
                                    contentItem: Item {
                                        Rectangle {
                                            width:  progressBar.visualPosition * parent.width
                                            height: parent.height
                                            radius: 99
                                            color:  Theme.blueMid
                                            Behavior on width {
                                                NumberAnimation { duration: 200 }
                                            }
                                        }
                                    }
                                }

                                FpButton {
                                    text: "Generate Waypoints"
                                    primary: true
                                    Layout.fillWidth: true
                                    onClicked: {
                                        mapView.clearRoute()
                                        backend.generateMission(
                                            takeoffGroup.lat,  takeoffGroup.lon,
                                            takeoffGroup.name,
                                            landGroup.lat,     landGroup.lon,
                                            landGroup.name,
                                            takeoffAltSpin.value,
                                            nearHomeAltSpin.value,
                                            cruiseAltSpin.value,
                                            root.isDropMission ? dropAltSpin.value : 0,
                                            landAltSpin.value,
                                            speedSpin.value * 1.0,
                                            (root.isReturn || root.isDropMission) ? waitSpin.value : 0,
                                            root.missionTypeIndex(),
                                            root.isReturn ? 1 : 0,
                                            root.currentDrone ? root.currentDrone.id : "",
                                            outputDirField.text
                                        )
                                    }
                                }
                            }
                        }
                    }

                    // ── Page 1: Drone Info (PIN protected) ───────────────────
                    Item {
                        // Protected content
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12
                            visible: droneGate.unlocked

                            Text {
                                text: "DRONE CATALOGUE"
                                font.family:        Theme.fontFamily
                                font.pixelSize:     Theme.fontXs
                                font.weight:        Font.Medium
                                font.letterSpacing: 0.8
                                color:              Theme.textMuted
                            }

                            Repeater {
                                model: root.drones
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    height: droneCol.implicitHeight + 20
                                    radius: Theme.radiusLg
                                    color:  Theme.bgSection
                                    border.color: Theme.border
                                    border.width: 0.5

                                    ColumnLayout {
                                        id: droneCol
                                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                                        spacing: 4

                                        Text {
                                            text:           modelData.name
                                            font.family:    Theme.fontFamily
                                            font.pixelSize: Theme.fontMd
                                            font.weight:    Font.Medium
                                            color:          Theme.textPrimary
                                        }

                                        RowLayout {
                                            spacing: 16
                                            Text {
                                                text:           "Max range: " + (modelData.maxOneWayKm * 2).toFixed(0) + " km"
                                                font.family:    Theme.fontFamily
                                                font.pixelSize: Theme.fontSm
                                                color:          Theme.textSecond
                                            }
                                            Text {
                                                text:           "Speed: " + modelData.speed + " m/s"
                                                font.family:    Theme.fontFamily
                                                font.pixelSize: Theme.fontSm
                                                color:          Theme.textSecond
                                            }
                                        }

                                        RowLayout {
                                            spacing: 16
                                            Text {
                                                text:           "Cruise alt: " + modelData.cruiseAlt + " m"
                                                font.family:    Theme.fontFamily
                                                font.pixelSize: Theme.fontSm
                                                color:          Theme.textMuted
                                            }
                                            Text {
                                                text:           "RC switch: CH" + modelData.rcSwitch
                                                font.family:    Theme.fontFamily
                                                font.pixelSize: Theme.fontSm
                                                color:          Theme.textMuted
                                            }
                                        }
                                    }
                                }
                            }

                            Item { Layout.fillHeight: true }
                        }

                        PinGate {
                            id: droneGate
                            sectionName: "Drone Catalogue"
                            autoLockSecs: 300   // auto-lock after 5 min
                        }
                    }

                }
            }

            // ── Right panel — switches between normal view and full 3D ──────
            // Page 0 (index 0): map + terrain profile + log  (all tabs except 3D)
            // Page 1 (index 1): full-screen 3D view          (3D tab only)
            StackLayout {
                id: rightStack
                Layout.fillWidth:  true
                Layout.fillHeight: true
                // sideNav index 2 = 3D tab → show page 1
                // 0=map/log  1=FleetPanel  2=3D  3=Settings
                currentIndex: sideNav.currentIndex === 1 ? 1
                             : sideNav.currentIndex === 3 ? 2
                             : sideNav.currentIndex === 4 ? 3
                             : 0

                // ── Page 0: normal map / log panel ───────────────────────────
                ColumnLayout {
                    spacing: 0

                    MapView {
                        id: mapView
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.adaptiveMapH
                        Layout.minimumHeight:   root.isAndroid ? 180 : 150
                        cacheDir: backend.mapTilesCacheDir()

                        onTakeoffPlaced: function(lat, lon) {
                            takeoffGroup.lat = lat.toFixed(7)
                            takeoffGroup.lon = lon.toFixed(7)
                            mapView.setClickModeInternal(1)
                        }
                        onLandPlaced: function(lat, lon) {
                            landGroup.lat = lat.toFixed(7)
                            landGroup.lon = lon.toFixed(7)
                        }
                    }

                    // Splitter
                    Rectangle {
                        Layout.fillWidth: true
                        height: 5
                        color:  Theme.bgSection

                        Row {
                            anchors.centerIn: parent
                            spacing: 3
                            Repeater {
                                model: 3
                                Rectangle {
                                    width: 3; height: 3; radius: 1.5
                                    color: Theme.border
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape:  Qt.SizeVerCursor
                            property real pressY: 0
                            property real pressH: 0
                            onPressed:  { pressY = mouseY; pressH = mapView.height }
                            onPositionChanged: {
                                if (pressed) {
                                    var newH = Math.max(root.isAndroid ? 180 : 150,
                                               Math.min(root.height - (root.isAndroid ? 250 : 250),
                                                        pressH + (mouseY - pressY)))
                                    mapView.Layout.preferredHeight = newH
                                }
                            }
                        }
                    }

                    // Terrain Profile — collapsible, shown after generation
                    Rectangle {
                        id: profilePanel
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.profileExpanded ? root.adaptiveProfileH : root.adaptiveProfileBarH
                        Layout.minimumHeight:   root.adaptiveProfileBarH
                        visible: false
                        color:   Theme.bg

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            // Collapse/expand bar
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.adaptiveProfileBarH
                                color:  Theme.bgSection
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    height: 0.5; width: parent.width
                                    color: Theme.border
                                }

                                Row {
                                    anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                                    spacing: 6
                                    Text {
                                        text: profileExpanded ? "▾" : "▸"
                                        font.pixelSize: 11; color: Theme.textMuted
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Text {
                                        text: "Terrain Profile"
                                        font.family:    Theme.fontFamily
                                        font.pixelSize: Theme.fontSm
                                        font.weight:    Font.Medium
                                        color:          Theme.textSecond
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape:  Qt.PointingHandCursor
                                    onClicked:    profileExpanded = !profileExpanded
                                }
                            }

                            TerrainProfile {
                                id: terrainProfile
                                Layout.fillWidth:  true
                                Layout.fillHeight: true
                                visible: root.profileExpanded
                            }
                        }
                    }

                    // Activity Log (PIN protected)
                    Item {
                        Layout.fillWidth:     true
                        Layout.fillHeight:    true
                        Layout.minimumHeight: root.isAndroid ? 72 : 100

                        ActivityLog {
                            id: log
                            anchors.fill: parent
                            visible: logGate.unlocked
                        }

                        PinGate {
                            id: logGate
                            sectionName:  "Activity Log"
                            autoLockSecs: 0
                        }
                    }

                    StatusBar {
                        id: statusBar
                        Layout.fillWidth: true
                    }
                }

                // ── Page 1: Drone Fleet (full width, PIN protected) ──────────
                Item {
                    ScrollView {
                        anchors.fill: parent
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ScrollBar.vertical.policy:   ScrollBar.AsNeeded
                        contentWidth: availableWidth

                        ColumnLayout {
                            width: parent.width
                            spacing: 0

                            Item { implicitHeight: 16 }

                            Text {
                                text: "DRONE FLEET"
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                                font.weight: Font.Medium; font.letterSpacing: 0.8
                                color: Theme.textMuted
                                leftPadding: 20; bottomPadding: 8
                                visible: fleetGate.unlocked
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.leftMargin: root.isAndroid ? 12 : 20
                                Layout.rightMargin: root.isAndroid ? 12 : 20
                                implicitHeight: fleetPanel.implicitHeight
                                visible: fleetGate.unlocked

                                DroneFleetPanel {
                                    id: fleetPanel
                                    width: parent.width
                                    onFleetChanged: root.refreshDroneCombo()
                                }
                            }

                            Item { implicitHeight: 20 }
                        }
                    }

                    PinGate {
                        id: fleetGate
                        sectionName:  "Drone Fleet"
                        autoLockSecs: 300
                    }
                }

                // ── Page 2: full 3D view ──────────────────────────────────────
                View3D {
                    id: view3d
                }

                // ── Page 3: full-width Settings ───────────────────────────────
                Item {
                    ScrollView {
                        id: settingsScroll
                        anchors.fill: parent
                        visible: settingsGate.unlocked
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ScrollBar.vertical.policy:   ScrollBar.AsNeeded
                        contentWidth: availableWidth

                        ColumnLayout {
                            id: settingsCol
                            width: settingsScroll.availableWidth
                            spacing: 0

                            Item { implicitHeight: 16 }

                            Text {
                                text: "SETTINGS"
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                                font.weight: Font.Medium; font.letterSpacing: 0.8
                                color: Theme.textMuted
                                leftPadding: 20; bottomPadding: 8
                            }

                            SectionBlock {
                                title: "SRTM Terrain Cache"
                                Layout.fillWidth: true

                                ColumnLayout {
                                    spacing: 8; width: parent.width

                                    Text {
                                        text: "Cache directory (pre-load .hgt files here)"
                                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                        color: Theme.textMuted; wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    RowLayout {
                                        spacing: 8; Layout.fillWidth: true
                                        FpInput {
                                            id: srtmDirField
                                            Layout.fillWidth: true; tinted: true
                                            placeholderText: "Path to SRTM1 folder..."
                                            font.pixelSize: Theme.fontSm
                                        }
                                        FpButton { text: "Browse..."; implicitWidth: 68; onClicked: backend.browseSrtmDir() }
                                    }

                                    RowLayout {
                                        spacing: 8
                                        Rectangle {
                                            width: 16; height: 16; radius: 3
                                            color: allowDownloadCheck ? Theme.blueMid : Theme.bgSection
                                            border.color: allowDownloadCheck ? Theme.blueMid : Theme.border
                                            border.width: 0.5
                                            property bool allowDownloadCheck: false
                                            Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 10; visible: parent.allowDownloadCheck }
                                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: parent.allowDownloadCheck = !parent.allowDownloadCheck }
                                        }
                                        Text { text: "Allow online tile download"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
                                    }

                                    Text {
                                        text: "Disable on ops networks. Pre-load tiles from terrain.ardupilot.org on a separate machine."
                                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                        color: Theme.textMuted; wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            SectionBlock {
                                title: "PIN Management"
                                Layout.fillWidth: true

                                ColumnLayout {
                                    spacing: 8; width: parent.width

                                    Text {
                                        text: "Change PIN for a protected section"
                                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                        color: Theme.textMuted; Layout.fillWidth: true
                                    }

                                    FormRow {
                                        label: "Section"
                                        FpComboBox { id: pinSectionCombo; Layout.fillWidth: true; model: ["Drone Fleet", "Drone Catalogue", "Settings", "Activity Log"] }
                                    }

                                    FormRow {
                                        label: "New PIN"
                                        FpInput {
                                            id: newPinField; Layout.fillWidth: true
                                            echoMode: TextInput.Password; placeholderText: "4-digit PIN"
                                            maximumLength: 4
                                            validator: RegularExpressionValidator { regularExpression: /[0-9]*/ }
                                        }
                                    }

                                    FormRow {
                                        label: "Confirm"
                                        FpInput {
                                            id: confirmPinField; Layout.fillWidth: true
                                            echoMode: TextInput.Password; placeholderText: "Repeat PIN"
                                            maximumLength: 4
                                            validator: RegularExpressionValidator { regularExpression: /[0-9]*/ }
                                        }
                                    }

                                    RowLayout {
                                        spacing: 8; Layout.fillWidth: true

                                        FpButton {
                                            text: "Update PIN"; primary: true; implicitWidth: 100
                                            onClicked: {
                                                if (newPinField.text.length < 4) {
                                                    pinStatusText.color = Theme.red
                                                    pinStatusText.text  = "PIN must be 4 digits"
                                                } else if (newPinField.text !== confirmPinField.text) {
                                                    pinStatusText.color = Theme.red
                                                    pinStatusText.text  = "PINs do not match"
                                                } else {
                                                    backend.setPin(pinSectionCombo.currentText, newPinField.text)
                                                    pinStatusText.color = Theme.green
                                                    pinStatusText.text  = "PIN updated for: " + pinSectionCombo.currentText
                                                    newPinField.text = ""; confirmPinField.text = ""
                                                }
                                            }
                                        }

                                        Text {
                                            id: pinStatusText; text: "Default PIN: 1234"
                                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                            color: Theme.textMuted
                                        }
                                    }
                                }
                            }

                            Item { implicitHeight: 24 }
                        }
                    }

                    PinGate {
                        id: settingsGate
                        sectionName: "Settings"
                        autoLockSecs: 300
                    }
                }
            }
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    function safeInt(v, def) {
        return (v === undefined || v === null || isNaN(v)) ? def : Math.round(v)
    }

    function positiveInt(v, def) {
        var n = safeInt(v, def)
        return n > 0 ? n : def
    }

    function clampInt(v, lo, hi) {
        return Math.max(lo, Math.min(hi, v))
    }

    function setSpinRange(spin, minValue, maxValue, desiredValue, fallbackValue) {
        var lo = safeInt(minValue, spin.from)
        var hi = safeInt(maxValue, spin.to)
        if (lo > hi) {
            var tmp = lo
            lo = hi
            hi = tmp
        }

        spin.from = lo
        spin.to = hi
        spin.value = clampInt(safeInt(desiredValue, fallbackValue), lo, hi)
    }

    function applyDroneFlightLimits(drone, resetToDefaults) {
        if (!drone)
            return

        root.setSpinRange(takeoffAltSpin,
                          positiveInt(drone.minTakeoffAlt, 5),
                          positiveInt(drone.maxTakeoffAlt, 500),
                          resetToDefaults ? drone.takeoffAlt : takeoffAltSpin.value,
                          30)
        root.setSpinRange(nearHomeAltSpin,
                          positiveInt(drone.minNearHomeAlt, 5),
                          positiveInt(drone.maxNearHomeAlt, 500),
                          resetToDefaults ? drone.nearHomeAlt : nearHomeAltSpin.value,
                          60)
        root.setSpinRange(cruiseAltSpin,
                          positiveInt(drone.minCruiseAlt, 80),
                          positiveInt(drone.maxCruiseAlt, 800),
                          resetToDefaults ? drone.cruiseAlt : cruiseAltSpin.value,
                          120)
        root.setSpinRange(landAltSpin,
                          positiveInt(drone.minLandAlt, 80),
                          positiveInt(drone.maxLandAlt, 150),
                          resetToDefaults ? drone.landAlt : landAltSpin.value,
                          80)
        root.setSpinRange(dropAltSpin,
                          positiveInt(drone.minTakeoffAlt, 5),
                          positiveInt(drone.maxTakeoffAlt, 500),
                          resetToDefaults ? drone.takeoffAlt : dropAltSpin.value,
                          30)

        var speedDefault = safeInt(drone.maxSpeedMs, 5)
        root.setSpinRange(speedSpin,
                          safeInt(drone.minSpeedMs, 1),
                          safeInt(drone.maxSpeedMs, 30),
                          resetToDefaults ? speedDefault : speedSpin.value,
                          speedDefault)
    }

    function applyDroneSelection(index, resetToDefaults) {
        if (index < 0 || index >= root.drones.length)
            return

        root.currentDrone = root.drones[index]
        root.applyDroneFlightLimits(root.currentDrone, resetToDefaults)

        var caps = root.currentDrone.capabilities || ["Delivery"]
        missionCombo.model = caps
        missionCombo.currentIndex = 0

        log.appendLine(
            "INFO",
            "Drone: " + root.currentDrone.name
            + " | Max range: "
            + (root.currentDrone.maxOneWayKm * 2).toFixed(1)
            + " km two-way | Caps: "
            + caps.join(", ")
        )

        statusBar.droneInfo =
            root.currentDrone.name + " - "
            + root.currentDrone.maxOneWayKm + " km one-way"
    }

    // Called when drone fleet changes in Settings — refreshes combo list
    function missionTypeIndex() {
        if (root.selectedMission === "Drop")
            return 1
        if (root.selectedMission === "Paradrop")
            return 2
        return 0
    }

    function refreshDroneCombo() {
        var selectedId = root.currentDrone ? root.currentDrone.id : ""
        root.drones = droneManager.activeDrones()
        if (root.drones.length <= 0) {
            root.currentDrone = null
            return
        }

        var nextIndex = 0
        for (var i = 0; i < root.drones.length; ++i) {
            if (root.drones[i].id === selectedId) {
                nextIndex = i
                break
            }
        }

        droneCombo.currentIndex = nextIndex
        root.applyDroneSelection(nextIndex, true)
    }

    function syncTakeoffToMap() {
        var lat = parseFloat(takeoffGroup.lat)
        var lon = parseFloat(takeoffGroup.lon)
        if (!isNaN(lat) && !isNaN(lon) && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
            mapView.setTakeoffMarker(lat, lon)
            mapView.panTo(lat, lon)
        }
    }

    function syncLandToMap() {
        var lat = parseFloat(landGroup.lat)
        var lon = parseFloat(landGroup.lon)
        if (!isNaN(lat) && !isNaN(lon) && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
            mapView.setLandMarker(lat, lon)
        }
    }

    function checkDistance() {
        var tlat = parseFloat(takeoffGroup.lat)
        var tlon = parseFloat(takeoffGroup.lon)
        var llat = parseFloat(landGroup.lat)
        var llon = parseFloat(landGroup.lon)

        if (isNaN(tlat)||isNaN(tlon)||isNaN(llat)||isNaN(llon)) {
            distBadge.visible = false
            return
        }

        var R    = 6371000
        var dLat = (llat - tlat) * Math.PI / 180
        var dLon = (llon - tlon) * Math.PI / 180
        var a    = Math.sin(dLat/2) * Math.sin(dLat/2)
                 + Math.cos(tlat * Math.PI / 180) * Math.cos(llat * Math.PI / 180)
                 * Math.sin(dLon/2) * Math.sin(dLon/2)
        var distM = 2 * R * Math.asin(Math.sqrt(a))

        root.distResult   = distM / 1000
        var tripM         = root.isReturn ? distM * 2 : distM
        root.inRange      = tripM <= root.currentDrone.maxOneWayKm * 2000

        distBadge.oneWayKm = root.distResult
        distBadge.tripKm   = root.isReturn ? root.distResult * 2 : root.distResult
        distBadge.maxKm    = root.currentDrone.maxOneWayKm * 2
        distBadge.visible  = true
    }
}
