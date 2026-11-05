import QtQuick
import QtQuick.Controls
import QtQuick3D
import DroneTerrainSim

Item {
    id: root

    required property ImageryTileProvider imageryTileProvider
    // Accept both the standalone receiver and the GCS telemetry adapter.
    required property var vehicle
    property alias terrainManager: terrainManager
    property string cursorReadout: "Lat --  Lon --  Alt --"
    property string cameraMode: "ORBIT"
    property real cameraDistance: 2850
    property real cameraYaw: -34
    property real cameraPitch: -48
    property vector3d cameraTarget: Qt.vector3d(0, 0, 0)

    signal errorOccurred(string message)

    function setOrbitPreset() {
        cameraMode = "ORBIT"
        cameraTarget = Qt.vector3d(0, 0, 0)
        cameraYaw = -34
        cameraPitch = -48
        cameraDistance = 2850
    }

    function setTopPreset() {
        cameraMode = "TOP"
        cameraTarget = Qt.vector3d(0, 0, 0)
        cameraYaw = 0
        cameraPitch = -89
        cameraDistance = 3300
    }

    function setSidePreset() {
        cameraMode = "SIDE"
        cameraTarget = Qt.vector3d(0, 0, 0)
        cameraYaw = -90
        cameraPitch = -18
        cameraDistance = 2500
    }

    function setChasePreset() {
        cameraMode = "CHASE"
        cameraYaw = 0
        cameraPitch = -18
        cameraDistance = 230
    }

    // Free-roam: keep the current view and let the mouse look around (left-drag)
    // and pan across the whole loaded region (right-drag), instead of orbiting a
    // fixed target.
    function setExplorePreset() {
        cameraMode = "EXPLORE"
    }

    function clampToRegion(x, z) {
        const radius = terrainManager.regionRadiusMeters
        if (radius <= 0)
            return Qt.vector2d(x, z)
        const distance = Math.sqrt(x * x + z * z)
        if (distance <= radius)
            return Qt.vector2d(x, z)
        const scale = radius / distance
        return Qt.vector2d(x * scale, z * scale)
    }

    // directionX/directionZ are in camera-relative (right/forward) units, not world
    // units, so the same call moves you "forward" regardless of current yaw.
    function panDirection(directionX, directionZ) {
        cameraMode = "EXPLORE"
        const yawRad = cameraYaw * Math.PI / 180
        const rightX = Math.cos(yawRad)
        const rightZ = -Math.sin(yawRad)
        const forwardX = -Math.sin(yawRad)
        const forwardZ = -Math.cos(yawRad)
        // Clamp the step so panning stays usable both when zoomed in close and
        // when zoomed far out over a large loaded region.
        const step = Math.max(cameraDistance, 200) * 0.9
        const movedX = cameraTarget.x + (rightX * directionX + forwardX * directionZ) * step
        const movedZ = cameraTarget.z + (rightZ * directionX + forwardZ * directionZ) * step
        const clamped = clampToRegion(movedX, movedZ)
        cameraTarget = Qt.vector3d(clamped.x, cameraTarget.y, clamped.y)
    }

    // Cesium-style clarity: each terrain tile independently sharpens its own imagery
    // based on the camera's actual distance to that tile (handled per-tile in
    // TerrainManager::updateCameraPosition), instead of one blanket setting bumped
    // for the whole region. Polling the camera's scene position on a light timer
    // (rather than hooking every yaw/pitch/distance/pan signal) keeps this simple
    // and correctly covers every way the view can move, including chase mode.
    Timer {
        id: lodTimer
        interval: 200
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            if (camera)
                terrainManager.updateCameraPosition(camera.scenePosition.x,
                                                     camera.scenePosition.y,
                                                     camera.scenePosition.z)
        }
    }

    function panByScreenDelta(dx, dy) {
        const yawRad = cameraYaw * Math.PI / 180
        const rightX = Math.cos(yawRad)
        const rightZ = -Math.sin(yawRad)
        const forwardX = -Math.sin(yawRad)
        const forwardZ = -Math.cos(yawRad)
        const scale = Math.max(cameraDistance, 200) * 0.0025
        const movedX = cameraTarget.x - (rightX * dx - forwardX * dy) * scale
        const movedZ = cameraTarget.z - (rightZ * dx - forwardZ * dy) * scale
        const clamped = clampToRegion(movedX, movedZ)
        cameraTarget = Qt.vector3d(clamped.x, cameraTarget.y, clamped.y)
    }

    View3D {
        id: viewport
        anchors.fill: parent

        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: "#9db2b7"
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
            tonemapMode: SceneEnvironment.TonemapModeAces
        }

        Node {
            id: sceneRoot

            DirectionalLight {
                eulerRotation.x: -48
                eulerRotation.y: -32
                brightness: 1.15
                castsShadow: true
                shadowFactor: 72
                shadowMapQuality: Light.ShadowMapQualityHigh
            }

            DirectionalLight {
                eulerRotation.x: 35
                eulerRotation.y: 140
                brightness: 0.28
                color: "#b9d7df"
            }

            TerrainManager {
                id: terrainManager
                demDirectory: root.imageryTileProvider.demDirectoryUrl
                imageryProvider: root.imageryTileProvider
                onErrorOccurred: message => root.errorOccurred(message)
            }

            Repeater3D {
                model: terrainManager

                delegate: Model {
                    pickable: true
                    property bool isTerrainTile: true

                    geometry: TerrainTileGeometry {
                        manager: terrainManager
                        zoom: model.zoom
                        tileX: model.tileX
                        tileY: model.tileY
                        gridSize: terrainManager.gridSize
                        verticalScale: terrainManager.verticalScale
                        skirtDepth: terrainManager.skirtDepth
                        originLatitude: terrainManager.originLatitude
                        originLongitude: terrainManager.originLongitude
                        originHeight: terrainManager.originHeight
                    }

                    materials: PrincipledMaterial {
                        // Skirt winding isn't guaranteed consistent with the top face;
                        // NoCulling avoids the curtain vanishing from some angles.
                        cullMode: Material.NoCulling
                        baseColorMap: Texture {
                            source: model.imageryUrl
                            flipV: true
                            generateMipmaps: true
                            mipFilter: Texture.Linear
                            minFilter: Texture.Linear
                            magFilter: Texture.Linear
                        }
                        emissiveMap: Texture {
                            source: model.imageryUrl
                            flipV: true
                            generateMipmaps: true
                            mipFilter: Texture.Linear
                            minFilter: Texture.Linear
                            magFilter: Texture.Linear
                        }
                        emissiveFactor: Qt.vector3d(0.32, 0.32, 0.32)
                        roughness: 0.96
                        metalness: 0.0
                        baseColor: "#ffffff"
                    }
                }
            }

            DroneEntity {
                id: droneEntity
                vehicle: root.vehicle
                visualScale: 4.5
            }

            Node {
                id: cameraTargetNode
                position: root.cameraMode === "CHASE" ? root.vehicle.scenePosition : root.cameraTarget

                Node {
                    id: cameraYawNode
                    eulerRotation.y: root.cameraMode === "CHASE"
                                     ? root.vehicle.sceneRotation.toEulerAngles().y
                                     : root.cameraYaw

                    Node {
                        id: cameraPitchNode
                        eulerRotation.x: root.cameraPitch

                        PerspectiveCamera {
                            id: camera
                            z: root.cameraDistance
                            clipNear: 0.5
                            clipFar: 50000
                            fieldOfView: 52
                        }
                    }
                }
            }
        }

        camera: camera

        MouseArea {
            id: interaction
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            property real lastX
            property real lastY

            onPressed: mouse => {
                lastX = mouse.x
                lastY = mouse.y
            }

            onPositionChanged: mouse => {
                if (pressed && (mouse.buttons & Qt.RightButton)) {
                    const dx = mouse.x - lastX
                    const dy = mouse.y - lastY
                    root.cameraMode = "EXPLORE"
                    root.panByScreenDelta(dx, dy)
                    lastX = mouse.x
                    lastY = mouse.y
                } else if (pressed) {
                    const dx = mouse.x - lastX
                    const dy = mouse.y - lastY
                    if (root.cameraMode !== "EXPLORE")
                        root.cameraMode = "ORBIT"
                    root.cameraYaw -= dx * 0.28
                    root.cameraPitch = Math.max(-89, Math.min(-4, root.cameraPitch - dy * 0.22))
                    lastX = mouse.x
                    lastY = mouse.y
                } else {
                    const result = viewport.pick(mouse.x, mouse.y)
                    if (result.objectHit && result.objectHit.isTerrainTile) {
                        const coordinate = result.objectHit.geometry.coordinatesForUv(result.uvPosition)
                        root.cursorReadout = "Lat " + Number(coordinate.latitude).toFixed(7)
                                + "  Lon " + Number(coordinate.longitude).toFixed(7)
                                + "  Alt " + Number(coordinate.altitude).toFixed(1) + " m"
                    }
                }
            }

            onWheel: wheel => {
                root.cameraDistance = Math.max(35, Math.min(18000,
                    root.cameraDistance * Math.pow(1.0015, -wheel.angleDelta.y)))
                wheel.accepted = true
            }
        }

        // On-screen D-pad for Explore mode: click-and-hold any arrow to travel
        // across the whole loaded region without needing the mouse drag gestures.
        Grid {
            id: explorePad
            visible: root.cameraMode === "EXPLORE"
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 18
            columns: 3
            rows: 3
            spacing: 4

            Item { width: 44; height: 44 }
            Button {
                width: 44; height: 44
                text: "▲"
                autoRepeat: true
                autoRepeatInterval: 45
                autoRepeatDelay: 200
                onClicked: root.panDirection(0, 1)
            }
            Item { width: 44; height: 44 }

            Button {
                width: 44; height: 44
                text: "◄"
                autoRepeat: true
                autoRepeatInterval: 45
                autoRepeatDelay: 200
                onClicked: root.panDirection(-1, 0)
            }
            Item { width: 44; height: 44 }
            Button {
                width: 44; height: 44
                text: "►"
                autoRepeat: true
                autoRepeatInterval: 45
                autoRepeatDelay: 200
                onClicked: root.panDirection(1, 0)
            }

            Item { width: 44; height: 44 }
            Button {
                width: 44; height: 44
                text: "▼"
                autoRepeat: true
                autoRepeatInterval: 45
                autoRepeatDelay: 200
                onClicked: root.panDirection(0, -1)
            }
            Item { width: 44; height: 44 }
        }
    }
}
