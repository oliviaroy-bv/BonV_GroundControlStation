import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick3D
import DroneTerrainSim
import "../offline3d" as Offline3D

Item {
    id: root

    property bool vehicleConnected: false
    property var vehicle: null
    property var missionWaypoints: []
    property bool systemPositionReady: false
    property real systemLatitude: 0
    property real systemLongitude: 0
    property bool compactControls: false
    property string imageryUrlTemplate: "https://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"

    readonly property color panelColor: "#dd071019"
    readonly property color borderColor: "#33465c"
    readonly property color textColor: "#f3f8ff"
    readonly property color mutedColor: "#8ea3b8"

    function isValidCoordinate(lat, lon) {
        return isFinite(lat)
                && isFinite(lon)
                && lat >= -90
                && lat <= 90
                && lon >= -180
                && lon <= 180
                && !(lat === 0 && lon === 0)
    }

    function safeMissionPoints() {
        const points = []
        const source = root.missionWaypoints || []
        for (let i = 0; i < source.length; ++i) {
            const point = source[i]
            const lat = Number(point.lat)
            const lon = Number(point.lon)
            if (root.isValidCoordinate(lat, lon))
                points.push(point)
        }
        return points
    }

    function distanceMeters(lat1, lon1, lat2, lon2) {
        const r = 6371000.0
        const toRad = value => value * Math.PI / 180.0
        const p1 = toRad(lat1)
        const p2 = toRad(lat2)
        const dLat = toRad(lat2 - lat1)
        const dLon = toRad(lon2 - lon1)
        const a = Math.sin(dLat / 2) * Math.sin(dLat / 2)
                + Math.cos(p1) * Math.cos(p2) * Math.sin(dLon / 2) * Math.sin(dLon / 2)
        return r * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a))
    }

    function regionFromMission(points) {
        let minLat = Number(points[0].lat)
        let maxLat = minLat
        let minLon = Number(points[0].lon)
        let maxLon = minLon

        for (let i = 1; i < points.length; ++i) {
            const lat = Number(points[i].lat)
            const lon = Number(points[i].lon)
            minLat = Math.min(minLat, lat)
            maxLat = Math.max(maxLat, lat)
            minLon = Math.min(minLon, lon)
            maxLon = Math.max(maxLon, lon)
        }

        const centerLat = (minLat + maxLat) / 2
        const centerLon = (minLon + maxLon) / 2
        let radiusMeters = 1000
        for (let j = 0; j < points.length; ++j) {
            radiusMeters = Math.max(radiusMeters,
                                    root.distanceMeters(centerLat, centerLon,
                                                        Number(points[j].lat),
                                                        Number(points[j].lon)))
        }

        return {
            lat: centerLat,
            lon: centerLon,
            radiusKm: Math.max(1.0, Math.min(1000.0, radiusMeters / 1000.0 + 0.8)),
            zoom: radiusMeters > 30000 ? 10 : radiusMeters > 12000 ? 11 : 12
        }
    }

    function currentRegion() {
        const points = root.safeMissionPoints()
        if (points.length > 0)
            return root.regionFromMission(points)

        if (root.vehicleConnected && root.vehicle) {
            const lat = Number(root.vehicle.latitude)
            const lon = Number(root.vehicle.longitude)
            if (root.isValidCoordinate(lat, lon))
                return { lat: lat, lon: lon, radiusKm: 5.0, zoom: 13 }
        }

        if (root.systemPositionReady
                && root.isValidCoordinate(root.systemLatitude, root.systemLongitude)) {
            return {
                lat: root.systemLatitude,
                lon: root.systemLongitude,
                radiusKm: 5.0,
                zoom: 13
            }
        }

        return { lat: 28.2707850, lon: 85.4542458, radiusKm: 25.0, zoom: 11 }
    }

    function loadCurrentRegion() {
        const region = root.currentRegion()
        terrainView.terrainManager.loadRegion(region.lat, region.lon, region.radiusKm,
                                              region.zoom, root.imageryUrlTemplate)
        terrainView.setOrbitPreset()
    }

    function vehicleScenePosition() {
        if (!root.vehicle)
            return Qt.vector3d(0, 80, 0)

        const lat = Number(root.vehicle.latitude)
        const lon = Number(root.vehicle.longitude)
        if (!root.isValidCoordinate(lat, lon))
            return Qt.vector3d(0, 80, 0)

        const originLat = terrainView.terrainManager.originLatitude
        const originLon = terrainView.terrainManager.originLongitude
        const originHeight = terrainView.terrainManager.originHeight
        const metersPerLon = 111320.0 * Math.max(0.01, Math.cos(originLat * Math.PI / 180.0))
        const x = (lon - originLon) * metersPerLon
        const z = -(lat - originLat) * 111320.0
        const terrain = terrainView.terrainManager.elevationAt(lat, lon)
        const heightAboveGround = Math.max(0, Number(root.vehicle.altitude || 0))
        const y = (terrain - originHeight + heightAboveGround) * terrainView.terrainManager.verticalScale + 28
        return Qt.vector3d(x, y, z)
    }

    function vehicleSceneRotation() {
        const yaw = root.vehicle ? Number(root.vehicle.yaw || 0) : 0
        return Qt.quaternion(Math.cos(yaw / 2), 0, Math.sin(yaw / 2), 0)
    }

    function scheduleRegionLoad() {
        if (root.visible)
            regionReloadTimer.restart()
    }

    Component.onCompleted: scheduleRegionLoad()
    onVisibleChanged: scheduleRegionLoad()
    onMissionWaypointsChanged: scheduleRegionLoad()
    onVehicleChanged: scheduleRegionLoad()
    onVehicleConnectedChanged: scheduleRegionLoad()
    onSystemPositionReadyChanged: scheduleRegionLoad()
    onSystemLatitudeChanged: scheduleRegionLoad()
    onSystemLongitudeChanged: scheduleRegionLoad()

    Timer {
        id: regionReloadTimer
        interval: 120
        repeat: false
        onTriggered: root.loadCurrentRegion()
    }

    ImageryTileProvider {
        id: imageryTileProvider
    }

    QtObject {
        id: terrainVehicle

        property bool connected: root.vehicleConnected
        property bool armed: root.vehicle ? root.vehicle.armed : false
        property string flightMode: root.vehicle ? root.vehicle.flightMode : "OFFLINE"
        property real latitude: root.vehicle ? root.vehicle.latitude : 0
        property real longitude: root.vehicle ? root.vehicle.longitude : 0
        property real relativeAltitude: root.vehicle ? root.vehicle.altitude : 0
        property real groundspeed: root.vehicle ? root.vehicle.groundspeed : 0
        property vector3d scenePosition: root.vehicleScenePosition()
        property quaternion sceneRotation: root.vehicleSceneRotation()
    }

    Offline3D.TerrainView {
        id: terrainView

        anchors.fill: parent
        imageryTileProvider: imageryTileProvider
        vehicle: terrainVehicle
        missionWaypoints: root.missionWaypoints
        onErrorOccurred: message => statusText.text = message
    }

    RowLayout {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 52
        spacing: 6
        visible: !root.compactControls
        z: 80

        TerrainButton { text: "Orbit"; checked: terrainView.cameraMode === "ORBIT"; onClicked: terrainView.setOrbitPreset() }
        TerrainButton { text: "Top"; checked: terrainView.cameraMode === "TOP"; onClicked: terrainView.setTopPreset() }
        TerrainButton { text: "Side"; checked: terrainView.cameraMode === "SIDE"; onClicked: terrainView.setSidePreset() }
        TerrainButton { text: "Explore"; checked: terrainView.cameraMode === "EXPLORE"; onClicked: terrainView.setExplorePreset() }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 12
        anchors.bottomMargin: 12
        width: Math.min(parent.width - 24, Math.max(230, statusText.implicitWidth + 24))
        height: 30
        color: root.panelColor
        border.color: root.borderColor
        radius: 4
        visible: !root.compactControls
        z: 80

        Text {
            id: statusText
            anchors.centerIn: parent
            text: terrainView.terrainManager.status
            color: root.mutedColor
            font.pixelSize: 11
            elide: Text.ElideRight
            width: parent.width - 18
            horizontalAlignment: Text.AlignHCenter
        }
    }

    component TerrainButton: Button {
        id: button

        Layout.preferredWidth: 62
        Layout.preferredHeight: 30
        padding: 0
        flat: true
        checkable: true

        contentItem: Text {
            text: button.text
            color: button.checked ? "#061018" : root.textColor
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 4
            color: button.checked ? "#32f4ff"
                  : button.down ? "#27384a"
                  : button.hovered ? "#162536"
                  : root.panelColor
            border.color: button.checked ? "#ffffff" : root.borderColor
            border.width: 1
        }
    }
}
