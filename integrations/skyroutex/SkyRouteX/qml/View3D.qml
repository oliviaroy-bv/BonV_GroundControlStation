// import QtQuick 6.0
// import QtQuick.Controls 6.0
// import QtQuick.Layouts 6.0
// import QtQuick3D 6.0
// import FlightPlanner 1.0

// // View3D.qml
// // 3D world view: real satellite tiles draped over SRTM terrain mesh,
// // with the drone flight path and waypoint markers in geographic position.
// //
// // Public API:
// //   setMission(waypoints, profile, bounds)  — called from MainView after generation
// //   clearScene()

// Item {
//     id: root

//     // ── State ─────────────────────────────────────────────────────────────────
//     property bool   hasData:       false
//     property bool   meshReady:     false
//     property bool   textureReady:  false
//     property bool   sceneReady:    false   // both mesh + texture done
//     property string statusMsg:     "Generate a mission to see the 3D view"
//     property int    tileSource:    0       // mirrors 2D map tile source

//     // Route data
//     property var    waypointList:  []      // sparse anchor [{lat,lon,droneAlt,terrainAlt}]
//     property double midLat:        20.5
//     property double midLon:        78.9
//     property double yMinWorld:     0       // terrain yMin from GeoTerrainGeometry

//     // Camera orbit state
//     property real   camDistance:   8000
//     property real   camAzimuth:    -35
//     property real   camElevation:  32

//     // SmoothedAnimation allows direct assignment (drag) to work immediately
//     // while preset button transitions animate smoothly
//     Behavior on camDistance  { SmoothedAnimation { velocity: -1; duration: 300 } }
//     Behavior on camAzimuth   { SmoothedAnimation { velocity: -1; duration: 200 } }
//     Behavior on camElevation { SmoothedAnimation { velocity: -1; duration: 200 } }

//     // ── Public API ────────────────────────────────────────────────────────────
//     function setMission(waypoints, profile, bounds) {
//         if (!waypoints || waypoints.length < 2) return

//         hasData      = true
//         textureReady = false
//         meshReady    = false
//         sceneReady   = false
//         waypointList = waypoints
//         statusMsg    = "Building terrain mesh..."

//         // bounds: {minLat, maxLat, minLon, maxLon} of route + 10km padding
//         // passed in from MainView after generation
//         var pad = 0.090   // ~10km in degrees
//         var minLat = bounds.minLat - pad
//         var maxLat = bounds.maxLat + pad
//         var minLon = bounds.minLon - pad
//         var maxLon = bounds.maxLon + pad

//         // Set terrain mesh bounds
//         terrainGeo.minLat    = minLat
//         terrainGeo.maxLat    = maxLat
//         terrainGeo.minLon    = minLon
//         terrainGeo.maxLon    = maxLon

//         // Trigger 3D tile fetch from backend (also prefetches SRTM for the area)
//         backend.fetch3DTiles(minLat, maxLat, minLon, maxLon, tileSource, 13)

//         // Rebuild terrain mesh — small delay lets SRTM prefetch start first
//         // For cached tiles this is instant; uncached areas may show flat terrain
//         // until tiles download (user can regenerate after tiles are cached)
//         Qt.callLater(function() { terrainGeo.rebuild() })
//     }

//     function clearScene() {
//         hasData      = false
//         meshReady    = false
//         textureReady = false
//         sceneReady   = false
//         waypointList = []
//         statusMsg    = "Generate a mission to see the 3D view"
//         terrainGeo.minLat = 0; terrainGeo.maxLat = 0
//         terrainGeo.minLon = 0; terrainGeo.maxLon = 0
//         routeGeo.waypoints = []
//     }

//     // ── Backend connections ───────────────────────────────────────────────────
//     Connections {
//         target: backend
//         ignoreUnknownSignals: true

//         // Tile texture stitched and saved to disk — set as file URL
//         function onTileTextureReady(imagePath, minLat, maxLat, minLon, maxLon) {
//             // Qt Quick 3D Texture needs a file:// URL
//             // Add a timestamp query to force reload if same path is reused
//             terrainTexture.source = "file:///" + imagePath.replace(/\\/g, "/")
//                                     + "?t=" + Date.now()

//             // Tell mesh the exact texture geographic extent for UV mapping
//             terrainGeo.texMinLat = minLat
//             terrainGeo.texMaxLat = maxLat
//             terrainGeo.texMinLon = minLon
//             terrainGeo.texMaxLon = maxLon

//             textureReady = true
//             statusMsg    = "Texture loaded"
//             checkSceneReady()
//         }

//         function onTileProgress(fetched, total) {
//             statusMsg = "Loading map tiles: " + fetched + " / " + total
//         }

//         function onTileStatus(msg) {
//             if (!sceneReady) statusMsg = msg
//         }
//     }

//     function checkSceneReady() {
//         if (meshReady && textureReady) {
//             sceneReady = true
//             statusMsg  = "3D view ready"
//             // Rebuild route geometry with correct origin from mesh
//             routeGeo.midLat    = terrainGeo.midLat
//             routeGeo.midLon    = terrainGeo.midLon
//             routeGeo.yMin      = terrainGeo.yMin
//             routeGeo.waypoints = waypointList
//             // Position camera
//             resetCamera()
//         }
//     }

//     function resetCamera() {
//         var xExtent = (terrainGeo.maxLon - terrainGeo.minLon) * 111320.0
//         var zExtent = (terrainGeo.maxLat - terrainGeo.minLat) * 111320.0
//         camDistance  = Math.max(xExtent, zExtent) * 1.2
//         camAzimuth   = -35
//         camElevation = 32
//     }

//     // ── Toolbar ───────────────────────────────────────────────────────────────
//     Rectangle {
//         id: toolbar
//         anchors { top: parent.top; left: parent.left; right: parent.right }
//         height: 38
//         color:  Theme.bg
//         Rectangle { anchors.bottom: parent.bottom; height: 0.5; width: parent.width; color: Theme.border }

//         RowLayout {
//             anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
//             spacing: 8

//             Text {
//                 text: "3D View"
//                 font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase
//                 font.weight: Font.Medium; color: Theme.textPrimary
//             }

//             Rectangle { width: 0.5; height: 18; color: Theme.border }

//             // Tile source selector (syncs with 2D map)
//             Text { text: "Map:"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
//             FpComboBox {
//                 implicitWidth: 130; implicitHeight: 26
//                 model: ["Bing Satellite", "Bing Maps", "Google Satellite", "Google Maps", "OpenStreetMap"]
//                 onCurrentIndexChanged: root.tileSource = currentIndex
//             }

//             Rectangle { width: 0.5; height: 18; color: Theme.border }

//             // Camera presets
//             Text { text: "Camera:"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

//             Repeater {
//                 model: [
//                     { label: "Orbit",  az: -35, el: 32 },
//                     { label: "Top",    az: -35, el: 88 },
//                     { label: "Front",  az: 0,   el: 15 },
//                     { label: "Side",   az: -90, el: 20 },
//                 ]
//                 FpButton {
//                     text: modelData.label; implicitWidth: 44; implicitHeight: 26
//                     onClicked: {
//                         root.camAzimuth   = modelData.az
//                         root.camElevation = modelData.el
//                     }
//                 }
//             }

//             Item { Layout.fillWidth: true }

//             FpButton {
//                 text: "Reset"; implicitWidth: 46; implicitHeight: 26
//                 onClicked: root.resetCamera()
//             }
//         }
//     }

//     // ── 3D Viewport ───────────────────────────────────────────────────────────
//     View3D {
//         id: viewport
//         anchors { top: toolbar.bottom; left: parent.left; right: parent.right; bottom: infoBar.top }

//         environment: SceneEnvironment {
//             backgroundMode:       SceneEnvironment.Color
//             clearColor:           "#D8EAF5"
//             antialiasingMode:     SceneEnvironment.MSAA
//             antialiasingQuality:  SceneEnvironment.High
//             Fog: Fog {
//                 enabled:   true
//                 color:     "#D8EAF5"
//                 density:   0.00002
//                 depthEnabled: true
//                 depthNear: 50000
//                 depthFar:  300000
//             }
//         }

//         // ── Camera ────────────────────────────────────────────────────────────
//         PerspectiveCamera {
//             id: cam3d
//             fieldOfView: 45
//             clipNear:    10
//             clipFar:     500000
//             position: Qt.vector3d(
//                 root.camDistance * Math.sin(-root.camAzimuth   * Math.PI/180) * Math.cos(root.camElevation * Math.PI/180),
//                 root.camDistance * Math.sin( root.camElevation * Math.PI/180),
//                 root.camDistance * Math.cos(-root.camAzimuth   * Math.PI/180) * Math.cos(root.camElevation * Math.PI/180)
//             )
//             eulerRotation: Qt.vector3d(-root.camElevation, -root.camAzimuth, 0)

//             // No Behavior here — position driven by camDistance/Azimuth/Elevation
//             // which have their own SmoothedAnimation above
//         }

//         // ── Lighting ──────────────────────────────────────────────────────────
//         DirectionalLight {
//             eulerRotation: Qt.vector3d(-50, -30, 0)
//             color:         "#FFFFFF"
//             brightness:    1.1
//             castsShadow:   false
//         }
//         DirectionalLight {
//             eulerRotation: Qt.vector3d(20, 150, 0)
//             color:         "#B0C8E0"
//             brightness:    0.4
//         }

//         // ── Terrain mesh with satellite texture ───────────────────────────────
//         Model {
//             id: terrainModel
//             visible: root.sceneReady

//             geometry: GeoTerrainGeometry {
//                 id: terrainGeo
//                 stepDeg: 0.0009   // ~100m steps

//                 Component.onCompleted: {
//                     // Wire the SrtmReader from the C++ backend into the mesh geometry
//                     setSrtmReaderObject(backend.srtmReader())
//                 }

//                 onRebuildDone: {
//                     root.meshReady  = true
//                     root.midLat     = midLat
//                     root.midLon     = midLon
//                     root.yMinWorld  = yMin
//                     root.statusMsg  = "Terrain mesh ready, loading tiles..."
//                     root.checkSceneReady()
//                 }
//             }

//             materials: PrincipledMaterial {
//                 baseColorMap: Texture {
//                     id: terrainTexture
//                     // sourceImage is set from onTileTextureReady
//                     generateMipmaps: true
//                     mipFilter:       Texture.Linear
//                     minFilter:       Texture.Linear
//                     magFilter:       Texture.Linear
//                 }
//                 roughness:     0.95
//                 metalness:     0.0
//                 // Slight colour tint while texture loads
//                 baseColor:     root.textureReady ? "#FFFFFF" : "#C8B898"
//             }
//         }

//         // ── Flight path ribbon ────────────────────────────────────────────────
//         Model {
//             id: routeModel
//             visible: root.sceneReady

//             geometry: GeoRouteGeometry {
//                 id: routeGeo
//                 tubeRadius: 60
//             }

//             materials: PrincipledMaterial {
//                 baseColor:      "#0A30D0"
//                 roughness:      0.08
//                 metalness:      0.25
//                 opacity:        1.0
//                 emissiveFactor: Qt.vector3d(0.1, 0.18, 0.6)
//             }
//         }

//         // ── Waypoint markers ──────────────────────────────────────────────────
//         Repeater3D {
//             model: root.sceneReady ? root.waypointList : []

//             delegate: Node {
//                 required property var modelData
//                 required property int index

//                 property bool isTakeoff: index === 0
//                 property bool isLanding: index === root.waypointList.length - 1

//                 property real mPerDegLon: 111320.0 * Math.cos(root.midLat * Math.PI / 180)
//                 property real mPerDegLat: 111320.0

//                 property real wx: (modelData.lon - root.midLon) * mPerDegLon
//                 property real wy: modelData.droneAlt - root.yMinWorld
//                 property real wz: (modelData.lat - root.midLat) * mPerDegLat
//                 property real gy: modelData.terrainAlt - root.yMinWorld

//                 position: Qt.vector3d(wx, wy, wz)

//                 // Sphere at drone altitude
//                 // Takeoff = green, Landing = yellow, Intermediate = red
//                 Model {
//                     source: "#Sphere"
//                     scale:  Qt.vector3d(0.40, 0.40, 0.40)  // 40m radius — larger
//                     materials: PrincipledMaterial {
//                         baseColor: parent.parent.isTakeoff ? "#00CC44"
//                                  : parent.parent.isLanding ? "#FFD700"
//                                  : "#E82020"
//                         roughness: 0.1
//                         metalness: 0.35
//                         emissiveFactor: parent.parent.isTakeoff
//                                             ? Qt.vector3d(0.0, 0.35, 0.1)
//                                             : parent.parent.isLanding
//                                                 ? Qt.vector3d(0.4, 0.3, 0.0)
//                                                 : Qt.vector3d(0.4, 0.0, 0.0)
//                     }
//                 }

//                 // Vertical drop line — colour matches the marker
//                 Model {
//                     source: "#Cylinder"
//                     property real dropH: Math.max(1, parent.wy - parent.gy)
//                     position:  Qt.vector3d(0, -(dropH / 2), 0)
//                     scale:     Qt.vector3d(0.03, dropH / 100.0, 0.03)
//                     materials: PrincipledMaterial {
//                         baseColor: parent.parent.isTakeoff ? "#00CC44"
//                                  : parent.parent.isLanding ? "#FFD700"
//                                  : "#E82020"
//                         roughness: 0.8
//                         opacity:   0.7
//                     }
//                 }
//             }
//         }

//         // ── Horizon plane (sea-level reference) ───────────────────────────────
//         Model {
//             visible: root.sceneReady
//             source:  "#Rectangle"
//             scale:   Qt.vector3d(50, 1, 50)
//             position: Qt.vector3d(0, -root.yMinWorld * 0.3, 0)
//             eulerRotation: Qt.vector3d(-90, 0, 0)
//             materials: PrincipledMaterial {
//                 baseColor: "#7AAAC8"
//                 roughness: 1.0
//                 opacity:   0.25
//             }
//         }
//     }

//     // ── Loading / empty overlay ───────────────────────────────────────────────
//     Rectangle {
//         anchors.fill: viewport
//         color:        "transparent"
//         visible:      !root.sceneReady

//         Rectangle {
//             anchors.centerIn: parent
//             width:   320; height: col.implicitHeight + 40
//             radius:  Theme.radiusLg
//             color:   Theme.bg
//             border.color: Theme.border; border.width: 0.5

//             Column {
//                 id: col
//                 anchors { left: parent.left; right: parent.right; top: parent.top; margins: 24 }
//                 spacing: 14

//                 // 3D cube icon
//                 Canvas {
//                     width: 40; height: 40
//                     anchors.horizontalCenter: parent.horizontalCenter
//                     onPaint: {
//                         var c = getContext("2d")
//                         c.clearRect(0, 0, 40, 40)
//                         c.strokeStyle = root.hasData ? Theme.blueMid : Theme.textMuted
//                         c.lineWidth = 2; c.lineCap = "round"; c.lineJoin = "round"
//                         c.strokeRect(4, 14, 22, 22)
//                         c.beginPath(); c.moveTo(4,14); c.lineTo(12,6); c.lineTo(34,6); c.lineTo(26,14); c.closePath(); c.stroke()
//                         c.beginPath(); c.moveTo(26,14); c.lineTo(34,6); c.lineTo(34,28); c.lineTo(26,36); c.closePath(); c.stroke()
//                     }
//                 }

//                 Text {
//                     anchors.horizontalCenter: parent.horizontalCenter
//                     text:           root.hasData ? "Building 3D Scene" : "3D View"
//                     font.family:    Theme.fontFamily
//                     font.pixelSize: Theme.fontMd
//                     font.weight:    Font.Medium
//                     color:          Theme.textPrimary
//                 }

//                 Text {
//                     anchors.horizontalCenter: parent.horizontalCenter
//                     width:          270
//                     text:           root.statusMsg
//                     font.family:    Theme.fontFamily
//                     font.pixelSize: Theme.fontSm
//                     color:          Theme.textMuted
//                     wrapMode:       Text.WordWrap
//                     horizontalAlignment: Text.AlignHCenter
//                 }

//                 // Progress bar during loading
//                 Rectangle {
//                     visible:        root.hasData && !root.sceneReady
//                     anchors.horizontalCenter: parent.horizontalCenter
//                     width: 240; height: 4; radius: 2
//                     color: Theme.blueLight

//                     Rectangle {
//                         id: progressFill
//                         width:  parent.width * (root.meshReady ? 0.5 : 0.1) + (root.textureReady ? parent.width * 0.5 : 0)
//                         height: parent.height; radius: 2
//                         color:  Theme.blueMid
//                         Behavior on width { NumberAnimation { duration: 300 } }
//                     }
//                 }

//                 Item { height: 4 }
//             }
//         }
//     }

//     // ── Mouse orbit controller ────────────────────────────────────────────────
//     MouseArea {
//         anchors.fill:    viewport
//         acceptedButtons: Qt.LeftButton | Qt.MiddleButton
//         property real lastX: 0
//         property real lastY: 0

//         onPressed: function(mouse) { lastX = mouse.x; lastY = mouse.y }

//         onPositionChanged: function(mouse) {
//             if (!(mouse.buttons & Qt.LeftButton)) return
//             var dx = mouse.x - lastX
//             var dy = mouse.y - lastY
//             lastX  = mouse.x; lastY = mouse.y
//             root.camAzimuth   -= dx * 0.25
//             root.camElevation  = Math.max(2, Math.min(87, root.camElevation - dy * 0.18))
//         }

//         onWheel: function(wheel) {
//             var f = wheel.angleDelta.y > 0 ? 0.88 : 1.14
//             root.camDistance = Math.max(300, Math.min(200000, root.camDistance * f))
//         }
//     }

//     // ── Info bar ──────────────────────────────────────────────────────────────
//     Rectangle {
//         id: infoBar
//         anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
//         height: 22
//         color:  Theme.bg
//         Rectangle { anchors.top: parent.top; height: 0.5; width: parent.width; color: Theme.border }

//         Row {
//             anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
//             spacing: 16

//             Text {
//                 text:    root.statusMsg
//                 font.family: Theme.fontFamily; font.pixelSize: 10; color: Theme.textMuted
//             }
//         }

//         Text {
//             anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
//             text:    "Drag to orbit  ·  Scroll to zoom"
//             font.family: Theme.fontFamily; font.pixelSize: 10; color: Theme.textHint
//             visible: root.sceneReady
//         }
//     }
// }



import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0
import QtQuick3D 6.0
import FlightPlanner 1.0

// View3D.qml
// 3D world view: real satellite tiles draped over SRTM terrain mesh,
// with the drone flight path and waypoint markers in geographic position.
//
// Public API:
//   setMission(waypoints, profile, bounds)  — called from MainView after generation
//   clearScene()

Item {
    id: root

    // ── State ─────────────────────────────────────────────────────────────────
    property bool   hasData:       false
    property bool   meshReady:     false
    property bool   textureReady:  false
    property bool   sceneReady:    false   // both mesh + texture done
    property string statusMsg:     "Generate a mission to see the 3D view"
    property int    tileSource:    0       // mirrors 2D map tile source

    // Route data
    property var    waypointList:  []      // sparse anchor [{lat,lon,droneAlt,terrainAlt}]
    property double midLat:        20.5
    property double midLon:        78.9
    property double yMinWorld:     0       // terrain yMin from GeoTerrainGeometry

    // Camera orbit state
    property real   camDistance:   8000
    property real   camAzimuth:    -35
    property real   camElevation:  32

    // SmoothedAnimation allows direct assignment (drag) to work immediately
    // while preset button transitions animate smoothly
    Behavior on camDistance  { SmoothedAnimation { velocity: -1; duration: 300 } }
    Behavior on camAzimuth   { SmoothedAnimation { velocity: -1; duration: 200 } }
    Behavior on camElevation { SmoothedAnimation { velocity: -1; duration: 200 } }

    // ── Public API ────────────────────────────────────────────────────────────
    function setMission(waypoints, profile, bounds) {
        if (!waypoints || waypoints.length < 2) return

        hasData      = true
        textureReady = false
        meshReady    = false
        sceneReady   = false
        waypointList = waypoints
        statusMsg    = "Building terrain mesh..."

        // bounds: {minLat, maxLat, minLon, maxLon} of route + 10km padding
        // passed in from MainView after generation
        var pad = 0.090   // ~10km in degrees
        var minLat = bounds.minLat - pad
        var maxLat = bounds.maxLat + pad
        var minLon = bounds.minLon - pad
        var maxLon = bounds.maxLon + pad

        // Set terrain mesh bounds
        terrainGeo.minLat    = minLat
        terrainGeo.maxLat    = maxLat
        terrainGeo.minLon    = minLon
        terrainGeo.maxLon    = maxLon

        // Trigger 3D tile fetch from backend (also prefetches SRTM for the area)
        backend.fetch3DTiles(minLat, maxLat, minLon, maxLon, tileSource, 13)

        // Rebuild terrain mesh — small delay lets SRTM prefetch start first
        // For cached tiles this is instant; uncached areas may show flat terrain
        // until tiles download (user can regenerate after tiles are cached)
        Qt.callLater(function() { terrainGeo.rebuild() })
    }

    function clearScene() {
        hasData      = false
        meshReady    = false
        textureReady = false
        sceneReady   = false
        waypointList = []
        statusMsg    = "Generate a mission to see the 3D view"
        terrainGeo.minLat = 0; terrainGeo.maxLat = 0
        terrainGeo.minLon = 0; terrainGeo.maxLon = 0
        routeGeo.waypoints = []
    }

    // ── Backend connections ───────────────────────────────────────────────────
    Connections {
        target: backend
        ignoreUnknownSignals: true

        // Tile texture stitched and saved to disk — set as file URL
        function onTileTextureReady(imagePath, minLat, maxLat, minLon, maxLon) {
            // Qt Quick 3D Texture needs a file:// URL
            // Add a timestamp query to force reload if same path is reused
            terrainTexture.source = "file:///" + imagePath.replace(/\\/g, "/")
                                    + "?t=" + Date.now()

            // Tell mesh the exact texture geographic extent for UV mapping
            terrainGeo.texMinLat = minLat
            terrainGeo.texMaxLat = maxLat
            terrainGeo.texMinLon = minLon
            terrainGeo.texMaxLon = maxLon

            textureReady = true
            statusMsg    = "Texture loaded"
            checkSceneReady()
        }

        function onTileProgress(fetched, total) {
            statusMsg = "Loading map tiles: " + fetched + " / " + total
        }

        function onTileStatus(msg) {
            if (!sceneReady) statusMsg = msg
        }
    }

    function checkSceneReady() {
        if (meshReady && textureReady) {
            sceneReady = true
            statusMsg  = "3D view ready"
            // Rebuild route geometry with correct origin from mesh
            routeGeo.midLat    = terrainGeo.midLat
            routeGeo.midLon    = terrainGeo.midLon
            routeGeo.yMin      = terrainGeo.yMin
            routeGeo.waypoints = waypointList
            // Position camera
            resetCamera()
        }
    }

    function resetCamera() {
        var xExtent = (terrainGeo.maxLon - terrainGeo.minLon) * 111320.0
        var zExtent = (terrainGeo.maxLat - terrainGeo.minLat) * 111320.0
        camDistance  = Math.max(xExtent, zExtent) * 1.2
        camAzimuth   = -35
        camElevation = 32
    }

    // ── Toolbar ───────────────────────────────────────────────────────────────
    Rectangle {
        id: toolbar
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 38
        color:  Theme.bg
        Rectangle { anchors.bottom: parent.bottom; height: 0.5; width: parent.width; color: Theme.border }

        RowLayout {
            anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
            spacing: 8

            Text {
                text: "3D View"
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase
                font.weight: Font.Medium; color: Theme.textPrimary
            }

            Rectangle { width: 0.5; height: 18; color: Theme.border }

            // Tile source selector (syncs with 2D map)
            Text { text: "Map:"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
            FpComboBox {
                implicitWidth: 130; implicitHeight: 26
                model: ["Bing Satellite", "Bing Maps", "Google Satellite", "Google Maps", "OpenStreetMap"]
                onCurrentIndexChanged: root.tileSource = currentIndex
            }

            Rectangle { width: 0.5; height: 18; color: Theme.border }

            // Camera presets
            Text { text: "Camera:"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

            Repeater {
                model: [
                    { label: "Orbit",  az: -35, el: 32 },
                    { label: "Top",    az: -35, el: 88 },
                    { label: "Front",  az: 0,   el: 15 },
                    { label: "Side",   az: -90, el: 20 },
                ]
                FpButton {
                    text: modelData.label; implicitWidth: 44; implicitHeight: 26
                    onClicked: {
                        root.camAzimuth   = modelData.az
                        root.camElevation = modelData.el
                    }
                }
            }

            Item { Layout.fillWidth: true }

            FpButton {
                text: "Reset"; implicitWidth: 46; implicitHeight: 26
                onClicked: root.resetCamera()
            }
        }
    }

    // ── 3D Viewport ───────────────────────────────────────────────────────────
    View3D {
        id: viewport
        anchors { top: toolbar.bottom; left: parent.left; right: parent.right; bottom: infoBar.top }

        environment: SceneEnvironment {
            backgroundMode:       SceneEnvironment.Color
            clearColor:           "#D8EAF5"
            antialiasingMode:     SceneEnvironment.MSAA
            antialiasingQuality:  SceneEnvironment.High

        }

        // ── Camera ────────────────────────────────────────────────────────────
        PerspectiveCamera {
            id: cam3d
            fieldOfView: 45
            clipNear:    10
            clipFar:     500000
            position: Qt.vector3d(
                root.camDistance * Math.sin(-root.camAzimuth   * Math.PI/180) * Math.cos(root.camElevation * Math.PI/180),
                root.camDistance * Math.sin( root.camElevation * Math.PI/180),
                root.camDistance * Math.cos(-root.camAzimuth   * Math.PI/180) * Math.cos(root.camElevation * Math.PI/180)
            )
            eulerRotation: Qt.vector3d(-root.camElevation, -root.camAzimuth, 0)

            // No Behavior here — position driven by camDistance/Azimuth/Elevation
            // which have their own SmoothedAnimation above
        }

        // ── Lighting ──────────────────────────────────────────────────────────
        DirectionalLight {
            eulerRotation: Qt.vector3d(-50, -30, 0)
            color:         "#FFFFFF"
            brightness:    1.1
            castsShadow:   false
        }
        DirectionalLight {
            eulerRotation: Qt.vector3d(20, 150, 0)
            color:         "#B0C8E0"
            brightness:    0.4
        }

        // ── Terrain mesh with satellite texture ───────────────────────────────
        Model {
            id: terrainModel
            visible: root.sceneReady

            geometry: GeoTerrainGeometry {
                id: terrainGeo
                stepDeg: 0.0009   // ~100m steps

                Component.onCompleted: {
                    // Wire the SrtmReader from the C++ backend into the mesh geometry
                    setSrtmReaderObject(backend.srtmReader())
                }

                onRebuildDone: {
                    root.meshReady  = true
                    root.midLat     = midLat
                    root.midLon     = midLon
                    root.yMinWorld  = yMin
                    root.statusMsg  = "Terrain mesh ready, loading tiles..."
                    root.checkSceneReady()
                }
            }

            materials: PrincipledMaterial {
                baseColorMap: Texture {
                    id: terrainTexture
                    // sourceImage is set from onTileTextureReady
                    generateMipmaps: true
                    mipFilter:       Texture.Linear
                    minFilter:       Texture.Linear
                    magFilter:       Texture.Linear
                }
                roughness:     0.95
                metalness:     0.0
                // Slight colour tint while texture loads
                baseColor:     root.textureReady ? "#FFFFFF" : "#C8B898"
            }
        }

        // ── Flight path ribbon ────────────────────────────────────────────────
        Model {
            id: routeModel
            visible: root.sceneReady

            geometry: GeoRouteGeometry {
                id: routeGeo
                tubeRadius: 60
            }

            materials: PrincipledMaterial {
                baseColor:      "#0A30D0"
                roughness:      0.08
                metalness:      0.25
                opacity:        1.0
                emissiveFactor: Qt.vector3d(0.1, 0.18, 0.6)
            }
        }

        // ── Waypoint markers ──────────────────────────────────────────────────
        Repeater3D {
            model: root.sceneReady ? root.waypointList : []

            delegate: Node {
                required property var modelData
                required property int index

                property bool isTakeoff: index === 0
                property bool isLanding: index === root.waypointList.length - 1

                property real mPerDegLon: 111320.0 * Math.cos(root.midLat * Math.PI / 180)
                property real mPerDegLat: 111320.0

                property real wx: (modelData.lon - root.midLon) * mPerDegLon
                property real wy: modelData.droneAlt - root.yMinWorld
                property real wz: (modelData.lat - root.midLat) * mPerDegLat
                property real gy: modelData.terrainAlt - root.yMinWorld

                position: Qt.vector3d(wx, wy, wz)

                // Sphere at drone altitude
                // Takeoff = green, Landing = yellow, Intermediate = red
                Model {
                    source: "#Sphere"
                    scale:  Qt.vector3d(0.40, 0.40, 0.40)  // 40m radius — larger
                    materials: PrincipledMaterial {
                        baseColor: parent.parent.isTakeoff ? "#00CC44"
                                 : parent.parent.isLanding ? "#FFD700"
                                 : "#E82020"
                        roughness: 0.1
                        metalness: 0.35
                        emissiveFactor: parent.parent.isTakeoff
                                            ? Qt.vector3d(0.0, 0.35, 0.1)
                                            : parent.parent.isLanding
                                                ? Qt.vector3d(0.4, 0.3, 0.0)
                                                : Qt.vector3d(0.4, 0.0, 0.0)
                    }
                }

                // Vertical drop line — colour matches the marker
                Model {
                    source: "#Cylinder"
                    property real dropH: Math.max(1, parent.wy - parent.gy)
                    position:  Qt.vector3d(0, -(dropH / 2), 0)
                    scale:     Qt.vector3d(0.03, dropH / 100.0, 0.03)
                    materials: PrincipledMaterial {
                        baseColor: parent.parent.isTakeoff ? "#00CC44"
                                 : parent.parent.isLanding ? "#FFD700"
                                 : "#E82020"
                        roughness: 0.8
                        opacity:   0.7
                    }
                }
            }
        }

        // ── Horizon plane (sea-level reference) ───────────────────────────────
        Model {
            visible: root.sceneReady
            source:  "#Rectangle"
            scale:   Qt.vector3d(50, 1, 50)
            position: Qt.vector3d(0, -root.yMinWorld * 0.3, 0)
            eulerRotation: Qt.vector3d(-90, 0, 0)
            materials: PrincipledMaterial {
                baseColor: "#7AAAC8"
                roughness: 1.0
                opacity:   0.25
            }
        }
    }

    // ── Loading / empty overlay ───────────────────────────────────────────────
    Rectangle {
        anchors.fill: viewport
        color:        "transparent"
        visible:      !root.sceneReady

        Rectangle {
            anchors.centerIn: parent
            width:   320; height: col.implicitHeight + 40
            radius:  Theme.radiusLg
            color:   Theme.bg
            border.color: Theme.border; border.width: 0.5

            Column {
                id: col
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 24 }
                spacing: 14

                // 3D cube icon
                Canvas {
                    width: 40; height: 40
                    anchors.horizontalCenter: parent.horizontalCenter
                    onPaint: {
                        var c = getContext("2d")
                        c.clearRect(0, 0, 40, 40)
                        c.strokeStyle = root.hasData ? Theme.blueMid : Theme.textMuted
                        c.lineWidth = 2; c.lineCap = "round"; c.lineJoin = "round"
                        c.strokeRect(4, 14, 22, 22)
                        c.beginPath(); c.moveTo(4,14); c.lineTo(12,6); c.lineTo(34,6); c.lineTo(26,14); c.closePath(); c.stroke()
                        c.beginPath(); c.moveTo(26,14); c.lineTo(34,6); c.lineTo(34,28); c.lineTo(26,36); c.closePath(); c.stroke()
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text:           root.hasData ? "Building 3D Scene" : "3D View"
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontMd
                    font.weight:    Font.Medium
                    color:          Theme.textPrimary
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width:          270
                    text:           root.statusMsg
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSm
                    color:          Theme.textMuted
                    wrapMode:       Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                // Progress bar during loading
                Rectangle {
                    visible:        root.hasData && !root.sceneReady
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 240; height: 4; radius: 2
                    color: Theme.blueLight

                    Rectangle {
                        id: progressFill
                        width:  parent.width * (root.meshReady ? 0.5 : 0.1) + (root.textureReady ? parent.width * 0.5 : 0)
                        height: parent.height; radius: 2
                        color:  Theme.blueMid
                        Behavior on width { NumberAnimation { duration: 300 } }
                    }
                }

                Item { height: 4 }
            }
        }
    }

    // ── Mouse orbit controller ────────────────────────────────────────────────
    MouseArea {
        anchors.fill:    viewport
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        property real lastX: 0
        property real lastY: 0

        onPressed: function(mouse) { lastX = mouse.x; lastY = mouse.y }

        onPositionChanged: function(mouse) {
            if (!(mouse.buttons & Qt.LeftButton)) return
            var dx = mouse.x - lastX
            var dy = mouse.y - lastY
            lastX  = mouse.x; lastY = mouse.y
            root.camAzimuth   -= dx * 0.25
            root.camElevation  = Math.max(2, Math.min(87, root.camElevation - dy * 0.18))
        }

        onWheel: function(wheel) {
            var f = wheel.angleDelta.y > 0 ? 0.88 : 1.14
            root.camDistance = Math.max(300, Math.min(200000, root.camDistance * f))
        }
    }

    // ── Info bar ──────────────────────────────────────────────────────────────
    Rectangle {
        id: infoBar
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 22
        color:  Theme.bg
        Rectangle { anchors.top: parent.top; height: 0.5; width: parent.width; color: Theme.border }

        Row {
            anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
            spacing: 16

            Text {
                text:    root.statusMsg
                font.family: Theme.fontFamily; font.pixelSize: 10; color: Theme.textMuted
            }
        }

        Text {
            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            text:    "Drag to orbit  ·  Scroll to zoom"
            font.family: Theme.fontFamily; font.pixelSize: 10; color: Theme.textHint
            visible: root.sceneReady
        }
    }
}
