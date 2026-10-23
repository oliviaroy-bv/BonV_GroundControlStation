import QtQuick
import QtQuick.Controls
import Qt.labs.settings
import QtPositioning
import QtWebEngine

Item {
    id: root

    property bool vehicleConnected: false
    property var vehicle: null
    property var mavlinkObject: null
    property var vpsMarkers: ({})
    onVpsMarkersChanged: syncVpsMarkers()
    onMavlinkObjectChanged: syncVpsMarkers()
    Connections {
        target: root.mavlinkObject
        function onVpsStatusChanged() { root.syncVpsMarkers() }
    }
    function syncVpsMarkers() {
        if (!mapReady) return
        var status = mavlinkObject && mavlinkObject.vpsConnected ? mavlinkObject.vpsStatus : {}
        var fusedLat = Number(status.fusedLat), fusedLon = Number(status.fusedLon)
        if (isValidCoordinate(fusedLat, fusedLon))
            webView.runJavaScript("updateVpsPosition(" + fusedLat + "," + fusedLon + ");")
        else if (!mavlinkObject || !mavlinkObject.vpsConnected)
            webView.runJavaScript("clearVpsTrack();")
        if (isValidCoordinate(fusedLat, fusedLon)
                && (String(status.aiding || "").toLowerCase() === "on"
                    || String(status.autoAid || "").toLowerCase() === "on"))
            updatePointer(fusedLat, fusedLon, root.vehicleHeading, root.compassFollowEnabled, true)
        var points = []
        for (var name of ["vio", "vpr", "fused"]) {
            var lat = Number(status[name + "Lat"]), lon = Number(status[name + "Lon"])
            if (vpsMarkers[name] && isValidCoordinate(lat, lon))
                points.push({name: name, lat: lat, lon: lon})
        }
        webView.runJavaScript("setVpsMarkers(" + JSON.stringify(points) + ");")
    }
    property var missionWaypoints: []
    property var mission3dWaypoints: missionWaypoints
    property string mapViewMode: "satellite"
    property bool compactControls: false
    property bool map3dEnabled: false
    property bool joystickAvailable: false
    property bool joystickEnabled: false
    readonly property var rasterManager: typeof rasterOverlayManager !== "undefined" ? rasterOverlayManager : null
    readonly property real mapControlClusterLeft: centerVehicleButton.x
    readonly property real mapControlClusterRight: centerVehicleButton.x + centerVehicleButton.width
    readonly property real mapControlClusterTop: centerVehicleButton.y
    readonly property real mapControlClusterBottom: map3dButton.y + map3dButton.height

    property real systemLat: 0
    property real systemLon: 0
    property real vehicleLat: 0
    property real vehicleLon: 0
    property real vehicleHeading: 0

    property bool mapReady: false
    property bool mapCentered: false
    property bool systemPositionReady: false
    property bool vehiclePositionReady: false
    property bool vehicleCentered: false
    property bool compassFollowEnabled: false
    property double lastMapUpdateMs: 0
    property double pendingMapLat: NaN
    property double pendingMapLon: NaN
    property double pendingMapHeading: 0
    property double pendingTrailLat: NaN
    property double pendingTrailLon: NaN

    Timer {
        id: mapUpdateTimer
        interval: 75
        repeat: false
        onTriggered: {
            if (!root.isValidCoordinate(root.pendingMapLat, root.pendingMapLon))
                return
            root.updatePointer(root.pendingMapLat, root.pendingMapLon, root.pendingMapHeading,
                               root.compassFollowEnabled, true)
        }
    }

    Timer {
        id: trailUpdateTimer
        interval: 150
        repeat: false
        onTriggered: {
            if (root.isValidCoordinate(root.pendingTrailLat, root.pendingTrailLon))
                root.updateVehicleTrail(root.pendingTrailLat, root.pendingTrailLon)
        }
    }

    Settings {
        id: persistedMapView
        category: "Map"
        property real latitude: 0
        property real longitude: 0
        property real zoom: 15
    }

    signal mapContextMenuRequested(real x, real y, real latitude, real longitude)
    signal joystickEnabledRequested(bool enabled)

    onVehicleConnectedChanged: {
        if (vehicleConnected) {
            syncVehiclePosition()
        } else {
            vehiclePositionReady = false
            vehicleCentered = false
            compassFollowEnabled = false
            mapUpdateTimer.stop()
            trailUpdateTimer.stop()
            clearVehicleTrail()

            if (systemPositionReady) {
                centerMap(systemLat, systemLon, 15)
                updatePointer(systemLat, systemLon, 0, false)
            }
        }
    }

    onVehicleChanged: {
        clearVehicleTrail()
        syncVehiclePosition()
    }
    onVehicleHeadingChanged: compassIcon.requestPaint()
    onCompassFollowEnabledChanged: compassIcon.requestPaint()
    onMapViewModeChanged: syncMapViewMode()
    onMissionWaypointsChanged: syncMissionWaypoints()
    
    function isValidCoordinate(lat, lon)
    {
        return isFinite(lat)
                && isFinite(lon)
                && lat >= -90
                && lat <= 90
                && lon >= -180
                && lon <= 180
                && !(lat === 0 && lon === 0)
    }

    function centerMap(lat, lon, zoom)
    {
        if (!root.mapReady || !root.isValidCoordinate(lat, lon))
            return

        root.mapCentered = true
        webView.runJavaScript(`centerMap(${lat}, ${lon}, ${zoom});`)
    }

    function restorePersistedMapView()
    {
        if (!root.isValidCoordinate(persistedMapView.latitude, persistedMapView.longitude))
            return

        centerMap(persistedMapView.latitude, persistedMapView.longitude, persistedMapView.zoom)
    }

    function persistCurrentMapView()
    {
        if (!root.mapReady || root.map3dEnabled)
            return

        webView.runJavaScript("JSON.stringify(getMapView());", function(result) {
            if (typeof result !== "string" || result.length === 0)
                return

            try {
                var view = JSON.parse(result)
                var latitude = Number(view.latitude)
                var longitude = Number(view.longitude)
                var zoom = Number(view.zoom)
                if (root.isValidCoordinate(latitude, longitude) && isFinite(zoom)) {
                    persistedMapView.latitude = latitude
                    persistedMapView.longitude = longitude
                    persistedMapView.zoom = zoom
                }
            } catch (error) {
                console.warn("Unable to persist map view:", error)
            }
        })
    }

    function updatePointer(lat, lon, heading, follow, keepVisible)
    {
        if (!root.mapReady || !root.isValidCoordinate(lat, lon))
            return

        var shouldKeepVisible = keepVisible === undefined ? follow : keepVisible
        webView.runJavaScript(`updateDrone(${lat}, ${lon}, ${heading}, ${follow}, ${shouldKeepVisible});`)
    }

    function updateVehicleTrail(lat, lon)
    {
        if (!root.mapReady || !root.vehicleConnected || !root.isValidCoordinate(lat, lon))
            return

        webView.runJavaScript(`updateVehicleTrail(${lat}, ${lon});`)
    }

    function clearVehicleTrail()
    {
        if (!root.mapReady)
            return

        webView.runJavaScript("clearVehicleTrail();")
    }

    function syncVehiclePosition()
    {
        if (!root.vehicle)
            return

        syncVehicleHeading()

        var nextLat = root.vehicle.latitude
        var nextLon = root.vehicle.longitude

        if (!root.isValidCoordinate(nextLat, nextLon))
            return

        root.vehicleLat = nextLat
        root.vehicleLon = nextLon
        root.vehicleHeading = root.vehicle.heading
        root.vehiclePositionReady = true

        if (!root.vehicleCentered) {
            centerMap(root.vehicleLat, root.vehicleLon, 17)
            root.vehicleCentered = true
        }

        var status = mavlinkObject && mavlinkObject.vpsConnected ? mavlinkObject.vpsStatus : {}
        var fusedLat = Number(status.fusedLat), fusedLon = Number(status.fusedLon)
        var aidingOn = String(status.aiding || status.autoAid || "").toLowerCase() === "on"
        if (aidingOn && isValidCoordinate(fusedLat, fusedLon)) {
            updatePointer(fusedLat, fusedLon, root.vehicleHeading, root.compassFollowEnabled, true)
        } else {
            var now = Date.now()
            root.pendingMapLat = root.vehicleLat
            root.pendingMapLon = root.vehicleLon
            root.pendingMapHeading = root.vehicleHeading
            if (now - root.lastMapUpdateMs >= 75) {
                root.lastMapUpdateMs = now
                updatePointer(root.vehicleLat, root.vehicleLon, root.vehicleHeading, root.compassFollowEnabled, true)
            } else if (!mapUpdateTimer.running) {
                mapUpdateTimer.start()
            }
        }
        root.pendingTrailLat = root.vehicleLat
        root.pendingTrailLon = root.vehicleLon
        if (!trailUpdateTimer.running)
            trailUpdateTimer.start()
    }

    function syncVehicleHeading()
    {
        if (!root.vehicle)
            return

        var nextHeading = Number(root.vehicle.heading)
        if (!isFinite(nextHeading) || Math.abs(nextHeading) < 0.001) {
            nextHeading = Number(root.vehicle.yaw) * 180 / Math.PI
        }

        root.vehicleHeading = ((nextHeading % 360) + 360) % 360
    }

    function centerOnVehicle()
    {
        syncVehiclePosition()

        if (!root.vehiclePositionReady)
            return

        centerMap(root.vehicleLat, root.vehicleLon, 17)
        updatePointer(root.vehicleLat, root.vehicleLon, root.vehicleHeading, root.compassFollowEnabled, true)
    }

    function syncMapViewMode()
    {
        if (!root.mapReady)
            return

        var safeMode = root.mapViewMode === "street" ? "street" : "satellite"
        webView.runJavaScript("setMapViewMode(" + JSON.stringify(safeMode) + ");")
    }

    function syncMissionWaypoints()
    {
        if (!root.mapReady)
            return

        var waypoints = root.missionWaypoints || []
        if (waypoints.length === 0) {
            webView.runJavaScript("clearMissionWaypoints();")
            return
        }

        webView.runJavaScript("setMissionWaypoints(" + JSON.stringify(waypoints) + ");")
    }


    function syncRasterOverlays()
    {
        if (!root.mapReady)
            return

        if (!root.rasterManager) {
            webView.runJavaScript("clearRasterOverlays();")
            return
        }

        var overlays = root.rasterManager.enabled ? root.rasterManager.overlays : []
        webView.runJavaScript("setRasterOverlays(" + JSON.stringify(overlays) + ", " + root.rasterManager.enabled + ");")
    }
    PositionSource {
        id: gps

        active: true
        updateInterval: 1000

        onPositionChanged: {
            var nextLat = position.coordinate.latitude
            var nextLon = position.coordinate.longitude

            if (!root.isValidCoordinate(nextLat, nextLon))
                return

            root.systemLat = nextLat
            root.systemLon = nextLon
            root.systemPositionReady = true

            if (!root.vehiclePositionReady) {
                if (!root.mapCentered) {
                    centerMap(root.systemLat, root.systemLon, 15)
                }

                updatePointer(root.systemLat, root.systemLon, 0, false)
            }
        }
    }

    WebEngineView {
        id: webView

        anchors.fill: parent
        visible: !root.map3dEnabled

        // The page shares the tile-cache origin so MapLibre can read style JSON
        // and other proxied MapTiler resources without a CORS violation.
        url: "bonv://tile/map.html"

        settings.localContentCanAccessRemoteUrls: true
        settings.localContentCanAccessFileUrls: true
        settings.javascriptEnabled: true

        onLoadingChanged: function(loadRequest) {
            console.log("Map load status:", loadRequest.status)

            if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                root.mapReady = true

                syncMapViewMode()
                syncVpsMarkers()

                if (root.vehicleConnected) {
                    syncVehiclePosition()
                }

                syncMissionWaypoints()
                syncRasterOverlays()

                if (root.vehiclePositionReady) {
                    if (!root.vehicleCentered) {
                        centerMap(root.vehicleLat, root.vehicleLon, 17)
                        root.vehicleCentered = true
                    }

                    updatePointer(root.vehicleLat, root.vehicleLon, root.vehicleHeading, root.compassFollowEnabled, true)
                } else if (root.systemPositionReady) {
                    centerMap(root.systemLat, root.systemLon, 15)
                    updatePointer(root.systemLat, root.systemLon, 0, false)
                } else {
                    restorePersistedMapView()
                }
            } else if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                root.mapReady = false
                console.log("Map load failed:", loadRequest.errorString)
            }
        }

        onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceID) {
            console.log("Map JavaScript:", message, "at", sourceID, lineNumber)
        }

        onContextMenuRequested: function(request) {
            request.accepted = true
            root.openMapContextMenu(request.position.x, request.position.y)
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: root.mapReady && !root.map3dEnabled
        onTriggered: root.persistCurrentMapView()
    }

    OfflineTerrainMap {
        id: offlineTerrainMap

        anchors.fill: parent
        visible: root.map3dEnabled
        vehicleConnected: root.vehicleConnected
        vehicle: root.vehicle
        missionWaypoints: root.mission3dWaypoints
        systemPositionReady: root.systemPositionReady
        systemLatitude: root.systemLat
        systemLongitude: root.systemLon
        compactControls: root.compactControls
    }

    MouseArea {
        anchors.fill: webView
        z: 10
        acceptedButtons: Qt.RightButton
        hoverEnabled: false
        enabled: !root.map3dEnabled

        onPressed: function(mouse) {
            if (mouse.button !== Qt.RightButton) {
                mouse.accepted = false
                return
            }

            mouse.accepted = true
            root.openMapContextMenu(mouse.x, mouse.y)
        }
    }

    function openMapContextMenu(x, y) {
        webView.runJavaScript("getCoordinateAt(" + x + "," + y + ")", function(result) {
            if (result && isValidCoordinate(Number(result.latitude), Number(result.longitude)))
                root.mapContextMenuRequested(x, y, Number(result.latitude), Number(result.longitude))
            else
                root.mapContextMenuRequested(x, y, NaN, NaN)
        })
    }

    Button {
        id: compassButton

        width: 148
        height: 148
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 12
        anchors.topMargin: 12
        z: 20
        padding: 0
        flat: true
        hoverEnabled: true
        enabled: root.vehicleConnected
        visible: !root.compactControls
        opacity: 1.0
        ToolTip.visible: hovered
        ToolTip.text: enabled ? (root.compassFollowEnabled ? "Vehicle follow on" : "Vehicle follow off") : "Waiting for vehicle"

        contentItem: Item {
            Canvas {
                id: compassIcon

                anchors.centerIn: parent
                width: 148
                height: 148

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()

                    var cx = width / 2
                    var cy = height / 2
                    var radius = 70
                    var heading = ((root.vehicleHeading % 360) + 360) % 360
                    var headingRad = heading * Math.PI / 180
                    var active = root.vehicleConnected
                    var accent = "#f28a49"

                    ctx.fillStyle = "#df14171c"
                    ctx.beginPath()
                    ctx.arc(cx, cy, radius, 0, Math.PI * 2)
                    ctx.fill()

                    var gradient = ctx.createRadialGradient(cx, cy, 8, cx, cy, radius)
                    gradient.addColorStop(0, "rgba(255,255,255,0.06)")
                    gradient.addColorStop(0.72, "rgba(255,255,255,0.00)")
                    gradient.addColorStop(1, "rgba(0,0,0,0.34)")
                    ctx.fillStyle = gradient
                    ctx.beginPath()
                    ctx.arc(cx, cy, radius, 0, Math.PI * 2)
                    ctx.fill()

                    ctx.strokeStyle = "rgba(255,255,255,0.04)"
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    ctx.arc(cx, cy, radius - 1, 0, Math.PI * 2)
                    ctx.stroke()

                    ctx.save()
                    ctx.translate(cx, cy)
                    ctx.rotate(-headingRad)
                    ctx.fillStyle = "#d7dbe1"
                    ctx.textAlign = "center"
                    ctx.textBaseline = "middle"

                    var labels = [
                        { text: "N", angle: 0, size: 20, radius: 48 },
                        { text: "E", angle: 90, size: 10, radius: 48 },
                        { text: "S", angle: 180, size: 10, radius: 48 },
                        { text: "W", angle: 270, size: 10, radius: 48 },
                        { text: "30", angle: 30, size: 10, radius: 52 },
                        { text: "60", angle: 60, size: 10, radius: 52 },
                        { text: "120", angle: 120, size: 10, radius: 52 },
                        { text: "150", angle: 150, size: 10, radius: 52 },
                        { text: "210", angle: 210, size: 10, radius: 52 },
                        { text: "240", angle: 240, size: 10, radius: 52 },
                        { text: "300", angle: 300, size: 10, radius: 52 },
                        { text: "330", angle: 330, size: 10, radius: 52 }
                    ]

                    for (var i = 0; i < labels.length; i++) {
                        var angle = (labels[i].angle - 90) * Math.PI / 180
                        ctx.save()
                        ctx.translate(Math.cos(angle) * labels[i].radius, Math.sin(angle) * labels[i].radius)
                        ctx.rotate(headingRad)
                        ctx.font = "bold " + labels[i].size + "px sans-serif"
                        ctx.fillText(labels[i].text, 0, 0)
                        ctx.restore()
                    }

                    ctx.fillStyle = "#d7dbe1"
                    for (var mark = 0; mark < 360; mark += 30) {
                        var markAngle = (mark - 90) * Math.PI / 180
                        ctx.beginPath()
                        ctx.arc(Math.cos(markAngle) * 34, Math.sin(markAngle) * 34, mark % 90 === 0 ? 2.5 : 1.9, 0, Math.PI * 2)
                        ctx.fill()
                    }
                    ctx.restore()

                    if (active) {
                        ctx.fillStyle = accent
                        ctx.beginPath()
                        ctx.moveTo(cx, cy - 48)
                        ctx.quadraticCurveTo(cx - 5, cy - 43, cx - 7, cy - 37)
                        ctx.lineTo(cx + 7, cy - 37)
                        ctx.quadraticCurveTo(cx + 5, cy - 43, cx, cy - 48)
                        ctx.closePath()
                        ctx.fill()

                        ctx.strokeStyle = accent
                        ctx.lineWidth = 7
                        ctx.beginPath()
                        ctx.arc(cx, cy, 30, 0, Math.PI * 2)
                        ctx.stroke()
                    }

                    ctx.fillStyle = active ? "#edf5ff" : "#edf5ff"
                    ctx.font = "bold " + (active ? 22 : 22) + "px sans-serif"
                    ctx.textAlign = "center"
                    ctx.textBaseline = "middle"
                    ctx.fillText(active ? Math.round(heading) + "\u00b0" : "OFF", cx, cy + 2)
                }
            }
        }

        background: Rectangle {
            radius: width / 2
            color: "#00000000"
            border.width: 0
        }

        onEnabledChanged: compassIcon.requestPaint()
        onClicked: {
            root.compassFollowEnabled = !root.compassFollowEnabled
            root.centerOnVehicle()
        }
    }

    Button {
        id: centerVehicleButton

        width: 32
        height: 32
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 12
        anchors.topMargin: 12
        z: 20
        padding: 0
        flat: true
        hoverEnabled: true
        enabled: root.vehiclePositionReady
        opacity: enabled ? 1.0 : 0.55
        ToolTip.visible: hovered
        ToolTip.text: enabled ? "Center on vehicle" : "Waiting for vehicle position"

        contentItem: Item {
            Canvas {
                id: centerVehicleIcon

                anchors.centerIn: parent
                width: 22
                height: 22

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = centerVehicleButton.enabled ? "#ffffff" : "#9aacc0"
                    ctx.fillStyle = centerVehicleButton.enabled ? "#32f4ff" : "#718397"
                    ctx.lineWidth = 2.2
                    ctx.lineCap = "round"

                    ctx.beginPath()
                    ctx.arc(11, 11, 6.5, 0, Math.PI * 2)
                    ctx.stroke()

                    ctx.beginPath()
                    ctx.moveTo(11, 2)
                    ctx.lineTo(11, 6)
                    ctx.moveTo(11, 16)
                    ctx.lineTo(11, 20)
                    ctx.moveTo(2, 11)
                    ctx.lineTo(6, 11)
                    ctx.moveTo(16, 11)
                    ctx.lineTo(20, 11)
                    ctx.stroke()

                    ctx.beginPath()
                    ctx.arc(11, 11, 2.2, 0, Math.PI * 2)
                    ctx.fill()
                }
            }
        }

        background: Rectangle {
            radius: 5
            color: centerVehicleButton.down ? "#ee008fb8"
                  : centerVehicleButton.hovered ? "#ee152231"
                  : "#dd0b151e"
            border.color: centerVehicleButton.enabled ? "#88dff6ff" : "#66728090"
            border.width: 1
        }

        onEnabledChanged: centerVehicleIcon.requestPaint()
        onClicked: root.centerOnVehicle()
    }

    Button {
        id: map3dButton

        width: 32
        height: 32
        anchors.right: parent.right
        anchors.top: centerVehicleButton.bottom
        anchors.rightMargin: 12
        anchors.topMargin: 8
        z: 20
        padding: 0
        flat: true
        hoverEnabled: true
        enabled: true
        opacity: enabled ? 1.0 : 0.55
        ToolTip.visible: hovered
        ToolTip.text: root.map3dEnabled ? "Switch to 2D map" : "Switch to offline 3D map"

        contentItem: Text {
            text: root.map3dEnabled ? "2D" : "3D"
            color: root.map3dEnabled ? "#071018" : "#ffffff"
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 5
            color: !map3dButton.enabled ? "#dd0b151e"
                  : map3dButton.down ? "#ee008fb8"
                  : root.map3dEnabled ? "#ee32f4ff"
                  : map3dButton.hovered ? "#ee152231"
                  : "#dd0b151e"
            border.color: root.map3dEnabled ? "#ffffff" : "#88dff6ff"
            border.width: 1
        }

        onClicked: root.map3dEnabled = !root.map3dEnabled
    }

    Button {
        id: joystickToggleButton

        width: 32
        height: 32
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.compactControls ? 12 : 18
        z: 20
        padding: 0
        flat: true
        hoverEnabled: true
        visible: root.joystickAvailable
        enabled: root.joystickAvailable
        opacity: enabled ? 1.0 : 0.55
        ToolTip.visible: hovered
        ToolTip.text: root.joystickEnabled ? "Disable joystick" : "Enable joystick"

        contentItem: Item {
            Image {
                anchors.centerIn: parent
                width: 21
                height: 21
                source: "qrc:/icons/joystick.png"
                fillMode: Image.PreserveAspectFit
                opacity: root.joystickEnabled ? 1.0 : 0.72
            }
        }

        background: Rectangle {
            radius: 5
            color: !joystickToggleButton.enabled ? "#dd0b151e"
                  : joystickToggleButton.down ? "#ee008fb8"
                  : root.joystickEnabled ? "#ee32f4ff"
                  : joystickToggleButton.hovered ? "#ee152231"
                  : "#dd0b151e"
            border.color: root.joystickEnabled ? "#ffffff" : "#88dff6ff"
            border.width: 1
        }

        onClicked: root.joystickEnabledRequested(!root.joystickEnabled)
    }

    Connections {
        target: root.rasterManager

        function onEnabledChanged()
        {
            syncRasterOverlays()
        }

        function onOverlaysChanged()
        {
            syncRasterOverlays()
        }
        function onOverlayOpacityChanged(id, opacity)
        {
            if (!root.mapReady)
                return

            webView.runJavaScript("setRasterOverlayOpacity(" + JSON.stringify(id) + ", " + opacity + ");")
        }
    }
    Connections {
        target: root.vehicle

        function onPositionChanged()
        {
            syncVehiclePosition()
        }

        function onVfrHudChanged()
        {
            if (!root.vehicle)
                return

            syncVehicleHeading()

            if (root.vehiclePositionReady) {
                updatePointer(root.vehicleLat, root.vehicleLon, root.vehicleHeading, root.compassFollowEnabled, true)
            }
        }

        function onYawChanged()
        {
            if (!root.vehicle)
                return

            syncVehicleHeading()
            compassIcon.requestPaint()

            if (root.vehiclePositionReady) {
                updatePointer(root.vehicleLat, root.vehicleLon, root.vehicleHeading, root.compassFollowEnabled, true)
            }
        }
    }
}
