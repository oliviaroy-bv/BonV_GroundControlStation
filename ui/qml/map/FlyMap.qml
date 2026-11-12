import QtQuick
import QtQuick.Controls
import Qt.labs.settings
import QtPositioning
import QtWebEngine
import QtQuick.Window
import "../accessories"
import "MapDispatch.js" as MapDispatch

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
        function onSelectedVehicleChanged() {
            root.resetVehicleCameraState()
            root.clearVehicleTrail()
        }
    }
    function syncVpsMarkers() {
        var status = mavlinkObject && mavlinkObject.vpsConnected ? mavlinkObject.vpsStatus : {}
        var points = []
        for (var name of ["vio", "vpr", "fused"]) {
            var lat = Number(status[name + "Lat"]), lon = Number(status[name + "Lon"])
            if (vpsMarkers[name] && isValidCoordinate(lat, lon))
                points.push({name: name, lat: lat, lon: lon})
        }
        root.mapDispatcher.setSnapshot("vps", points)
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
    readonly property real mapControlClusterTop: parameterStatusButton.y
    readonly property real mapControlClusterBottom: map3dButton.y + map3dButton.height

    property real systemLat: 0
    property real systemLon: 0
    property real vehicleLat: 0
    property real vehicleLon: 0
    property real vehicleHeading: 0

    // HTML success alone is not operational readiness.
    property string readinessState: "document-unloaded"
    property string readinessError: ""
    property int documentGeneration: 0
    property int styleGeneration: 0
    property string documentId: ""
    property bool documentLoaded: false
    property bool bridgeAvailable: false
    property bool readinessProbePending: false
    property string appliedMapViewMode: ""
    // Identifies the style replacement currently reported by the page. The
    // dispatcher owns command ordering; QML only mirrors its lifecycle.
    property string styleTransactionId: ""
    property string styleTransactionState: ""
    property int initializationTimeoutMs: 20000
    readonly property bool mapReady: documentLoaded && bridgeAvailable && readinessState === "style-ready"
    property bool systemPositionReady: false
    property bool vehiclePositionReady: false
    property bool compassFollowEnabled: false
    property bool mapInstanceActive: true
    // MP-5 only reports proposed manual editing gestures. MissionPlanModel is
    // never mutated by this bridge.
    property bool manualPlanningActive: false
    property string activeDraggedWaypointId: ""
    property var interactionSeenIds: ({})
    property var interactionSeenOrder: []
    readonly property bool cameraActive: mapInstanceActive && visible && enabled
                                        && mapReady && !map3dEnabled && width > 0 && height > 0
                                        && (!root.Window.window || (root.Window.window.visible
                                            && root.Window.window.visibility !== Window.Minimized))
    property real markerLat: NaN
    property real markerLon: NaN
    property bool vehicleRenderDirty: false
    property string lastVehicleMapPayload: ""
    readonly property string mapInstanceId: Date.now().toString(36) + "-" + Math.random().toString(36).slice(2)
    property var mapDispatcher: MapDispatch.create({
        instanceId: root.mapInstanceId,
        transport: function(script, callback) { webView.runJavaScript(script, callback) },
        result: function(result) {
            root.mapOperationCompleted(result)
            if (result.status !== "success")
                console.warn("Map operation", result.operationId, result.category, result.status, result.error)
        },
        beginStyle: function(mode, operation) { root.beginStyleTransaction(mode, operation) },
        timeout: function(reason) { root.failMapReadiness(reason) },
        log: function(reason) { console.warn("Map dispatch:", reason) }
    })
    signal mapOperationCompleted(var result)
    signal mapInteractionReceived(var event)
    signal waypointDragCancelled(string waypointId)
    Component.onDestruction: mapDispatcher.dispose()

    function updateDispatchContext() {
        mapDispatcher.setContext({documentGeneration:documentGeneration, documentId:documentId,
            styleGeneration:styleGeneration, ready:mapReady, documentLoaded:documentLoaded,
            bridgeAvailable:bridgeAvailable, failed:readinessState === "failed", mode:appliedMapViewMode,
            styleTransactionId:styleTransactionId})
    }

    function resetInteractionState() {
        if (activeDraggedWaypointId.length)
            root.waypointDragCancelled(activeDraggedWaypointId)
        activeDraggedWaypointId = ""
        interactionSeenIds = ({})
        interactionSeenOrder = []
    }

    // A MapLibre marker can move optimistically while it is dragged. Re-send
    // the canonical snapshot to put it back when QML rejects or cancels the
    // proposal; this has no camera operation and no model mutation.
    function restoreCanonicalMissionRendering() {
        root.mapDispatcher.refreshSnapshot("mission")
        root.mapDispatcher.pump()
    }

    function syncInteractionConfig() {
        root.mapDispatcher.setSnapshot("interaction", {
            instanceId: root.mapInstanceId,
            documentId: root.documentId,
            documentGeneration: root.documentGeneration,
            styleGeneration: root.styleGeneration,
            manualPlanningActive: root.manualPlanningActive,
            active: root.mapInstanceActive && root.visible && root.enabled && !root.map3dEnabled && root.mapReady
        })
    }

    function rememberInteractionId(eventId) {
        if (interactionSeenIds[eventId])
            return false
        interactionSeenIds[eventId] = true
        interactionSeenOrder.push(eventId)
        if (interactionSeenOrder.length > 256) {
            var expired = interactionSeenOrder.shift()
            delete interactionSeenIds[expired]
        }
        return true
    }

    function hasKnownWaypointId(waypointId) {
        var points = root.missionWaypoints || []
        for (var index = 0; index < points.length; ++index) {
            if (points[index] && String(points[index].id) === waypointId)
                return true
        }
        return false
    }

    function acceptInteractionNotification(message) {
        var prefix = "BONV_MAP_INTERACTION:"
        if (typeof message !== "string" || message.length > 4096 || message.indexOf(prefix) !== 0)
            return false
        var event
        try {
            event = JSON.parse(message.slice(prefix.length))
        } catch (error) {
            console.warn("Rejected malformed map interaction notification")
            return false
        }
        if (!event || typeof event !== "object" || Array.isArray(event)
                || typeof event.eventId !== "string" || event.eventId.length < 1 || event.eventId.length > 160
                || !/^[A-Za-z0-9_.:-]+$/.test(event.eventId)
                || ["MapClicked", "WaypointSelected", "WaypointDragStarted", "WaypointDragged",
                    "WaypointDragFinished", "MapContextRequested"].indexOf(event.type) < 0
                || event.instanceId !== root.mapInstanceId || event.documentId !== root.documentId
                || event.documentGeneration !== root.documentGeneration || event.styleGeneration !== root.styleGeneration
                || !root.mapReady || !root.mapInstanceActive || root.map3dEnabled) {
            return false
        }
        if (!rememberInteractionId(event.eventId))
            return false

        var needsCoordinate = event.type === "MapClicked" || event.type === "WaypointDragStarted"
                || event.type === "WaypointDragged" || event.type === "WaypointDragFinished"
                || event.type === "MapContextRequested"
        if (needsCoordinate && !root.isValidInteractionCoordinate(Number(event.latitude), Number(event.longitude)))
            return false
        if ((event.type === "MapClicked" || event.type === "MapContextRequested")
                && (!isFinite(Number(event.screenX)) || !isFinite(Number(event.screenY))
                    || Number(event.screenX) < 0 || Number(event.screenY) < 0
                    || Number(event.screenX) > root.width || Number(event.screenY) > root.height))
            return false

        var manualEvent = event.type !== "MapContextRequested"
        if (manualEvent && !root.manualPlanningActive)
            return false
        if (event.type.indexOf("Waypoint") === 0) {
            if (typeof event.waypointId !== "string" || event.waypointId.length < 1
                    || event.waypointId.length > 128 || !hasKnownWaypointId(event.waypointId))
                return false
            if (event.type === "WaypointDragStarted") {
                if (activeDraggedWaypointId.length && activeDraggedWaypointId !== event.waypointId)
                    return false
                activeDraggedWaypointId = event.waypointId
            } else if (event.type === "WaypointDragged" || event.type === "WaypointDragFinished") {
                if (activeDraggedWaypointId !== event.waypointId)
                    return false
                if (event.type === "WaypointDragFinished")
                    activeDraggedWaypointId = ""
            }
        }
        root.mapInteractionReceived(event)
        if (event.type === "MapContextRequested")
            root.mapContextMenuRequested(Number(event.screenX), Number(event.screenY), event)
        return true
    }

    Timer {
        id: readinessPoll
        interval: root.mapReady ? 1000 : 250
        repeat: true
        running: root.documentLoaded && root.readinessState !== "failed"
        onTriggered: root.probeMapReadiness()
    }
    Timer {
        id: initializationDeadline
        interval: root.initializationTimeoutMs
        onTriggered: root.failMapReadiness("Initialization timed out after " + interval + " ms")
    }

    function beginMapDocument() {
        var previousDocument = documentId
        documentGeneration++
        documentLoaded = false
        bridgeAvailable = false
        readinessState = "document-loading"
        readinessError = ""
        documentId = ""
        styleGeneration = 0
        styleTransactionId = ""
        styleTransactionState = ""
        appliedMapViewMode = ""
        readinessProbePending = false
        lastVehicleMapPayload = ""
        if (typeof resetInteractionState === "function")
            resetInteractionState()
        initializationDeadline.restart()
        updateDispatchContext()
        if (previousDocument.length) {
            mapDispatcher.control("invalidate", "if (typeof getMapReadiness === 'function' && getMapReadiness(0).documentId === "
                + JSON.stringify(previousDocument) + ") invalidateMapDocument();", null, documentGeneration-1)
        }
    }

    function failMapReadiness(reason) {
        if (readinessState === "failed") { updateDispatchContext(); return }
        readinessState = "failed"
        readinessError = String(reason)
        initializationDeadline.stop()
        updateDispatchContext()
        console.error("Map failed [document " + documentGeneration + ", style " + styleGeneration + "]:", readinessError)
        // Stop the JS camera/queue if the renderer is still available. Never
        // let an old failure affect a newly loaded document.
        if (documentId.length) {
            mapDispatcher.control("fail", "if (typeof getMapReadiness === 'function' && getMapReadiness(0).documentId === "
                + JSON.stringify(documentId) + " && getMapReadiness(0).styleGeneration <= " + styleGeneration
                + ") failMapLifecycle(" + JSON.stringify(readinessError) + ");")
        }
    }

    function probeMapReadiness() {
        if (!documentLoaded || readinessState === "failed" || readinessProbePending) return
        var generation = documentGeneration
        readinessProbePending = true
        mapDispatcher.control("probe", "typeof getMapReadiness === 'function' ? JSON.stringify(getMapReadiness("
                             + generation + ")) : ''", function(result) {
            if (generation !== root.documentGeneration || !root.documentLoaded || root.readinessState === "failed") return
            root.readinessProbePending = false
            if (typeof result !== "string" || !result.length) return
            try {
                root.acceptMapReadiness(JSON.parse(result), generation)
            } catch (error) {
                root.failMapReadiness("Invalid readiness response: " + error)
            }
        })
        mapDispatcher.pump()
    }

    function acceptMapReadiness(status, generation) {
        if (generation !== documentGeneration || status.documentGeneration !== generation
                || !documentLoaded || readinessState === "failed") return
        if (typeof status.documentId !== "string" || !status.documentId.length
                || !Number.isInteger(status.styleGeneration) || status.styleGeneration < 1) {
            failMapReadiness("Malformed map readiness snapshot")
            return
        }
        if (documentId.length && status.documentId !== documentId) {
            // A replacement document can be observed before WebEngine's load
            // signal; immediately revoke the old generation and handshake again.
            beginMapDocument()
            documentLoaded = true
            readinessState = "document-loaded"
            updateDispatchContext()
            return
        }
        if (status.styleGeneration < styleGeneration) return
        documentId = status.documentId
        if (status.styleGeneration > styleGeneration) {
            styleGeneration = status.styleGeneration
            initializationDeadline.restart()
        }
        appliedMapViewMode = status.mode
        styleTransactionId = typeof status.styleTransactionId === "string" ? status.styleTransactionId : ""
        styleTransactionState = typeof status.styleTransactionState === "string" ? status.styleTransactionState : ""
        if (status.state === "failed") {
            failMapReadiness(status.error || "JavaScript map initialization failed")
            return
        }
        if (!status.bridgeAvailable) return
        if (!bridgeAvailable) readinessState = "bridge-available"
        bridgeAvailable = true
        updateDispatchContext()
        // Apply the desired style before allowing data synchronization. This
        // also handles a style preference changed while initialization waited.
        syncMapViewMode()
        if (styleGeneration !== status.styleGeneration) return
        if (status.state === "style-ready") {
            readinessState = "style-ready"
            initializationDeadline.stop()
        } else if (status.state === "style-loading") {
            readinessState = "style-loading"
        } else {
            failMapReadiness("Unexpected JavaScript readiness state: " + status.state)
        }
        updateDispatchContext()
    }

    function dispatchCommand(category, payload, callback, ttl) {
        if ((category === "center" || category === "fitMission") && !cameraActive)
            return mapDispatcher.reject(category, payload, "map-unavailable", callback)
        var id = mapDispatcher.command(category, payload, callback, ttl)
        mapDispatcher.pump()
        return id
    }

    function retryMap() {
        // Recovery is operator-driven; no automatic reload loop.
        documentLoaded = false
        bridgeAvailable = false
        readinessState = "document-unloaded"
        updateDispatchContext()
        webView.reload()
    }

    onMapReadyChanged: {
        updateDispatchContext()
        if (typeof syncInteractionConfig === "function")
            syncInteractionConfig()
        if (!mapReady) return
        syncVpsMarkers()
        if (vehicleConnected) syncVehiclePosition()
        syncMissionWaypoints()
        syncRasterOverlays()
        initializeViewport()
        if (vehiclePositionReady) updatePointer(vehicleLat, vehicleLon, vehicleHeading)
        else if (systemPositionReady) updatePointer(systemLat, systemLon, 0)
        lastVehicleMapPayload = ""
        vehicleRenderDirty = true
        flushVehicleMapState(true)
    }

    Timer {
        interval: 100
        repeat: true
        // Keep the existing trail alive while hidden/covered; only camera movement
        // is gated by cameraActive.
        running: true
        onTriggered: {
            root.flushVehicleMapState(false)
            root.mapDispatcher.tick()
        }
    }

    onCameraActiveChanged: {
        if (mapReady && !cameraActive && readinessState !== "style-loading") {
            mapDispatcher.cancelCommands("cancelled", function(command) {
                return command.op.category === "center" || command.op.category === "fitMission"
            })
        }
        vehicleRenderDirty = true
        flushVehicleMapState(true)
    }

    onManualPlanningActiveChanged: {
        if (!manualPlanningActive) {
            if (activeDraggedWaypointId.length)
                root.waypointDragCancelled(activeDraggedWaypointId)
            activeDraggedWaypointId = ""
        }
        syncInteractionConfig()
    }
    onMapInstanceActiveChanged: {
        if (!mapInstanceActive) {
            if (activeDraggedWaypointId.length)
                root.waypointDragCancelled(activeDraggedWaypointId)
            activeDraggedWaypointId = ""
        }
        syncInteractionConfig()
    }
    onMap3dEnabledChanged: {
        if (map3dEnabled) {
            if (activeDraggedWaypointId.length)
                root.waypointDragCancelled(activeDraggedWaypointId)
            activeDraggedWaypointId = ""
        }
        syncInteractionConfig()
    }
    onStyleGenerationChanged: {
        if (activeDraggedWaypointId.length)
            root.waypointDragCancelled(activeDraggedWaypointId)
        activeDraggedWaypointId = ""
        syncInteractionConfig()
    }
    onDocumentGenerationChanged: {
        if (activeDraggedWaypointId.length)
            root.waypointDragCancelled(activeDraggedWaypointId)
        activeDraggedWaypointId = ""
    }

    function flushVehicleMapState(force) {
        if (!force && !vehicleRenderDirty) return
        var state = {
            lat: markerLat, lon: markerLon, heading: vehicleHeading,
            connected: vehicleConnected && vehicle !== null,
            active: cameraActive, follow: compassFollowEnabled,
            armed: !!(vehicle && vehicle.armed),
            flightStateFresh: !!(vehicle && vehicle.flightStateFresh),
            airborne: !!(vehicle && vehicle.airborne),
            positionFresh: !!(vehicle && vehicle.positionFresh),
            positionValid: vehiclePositionReady && vehicle !== null
                           && isValidCoordinate(Number(vehicle.latitude), Number(vehicle.longitude)),
            trail: false
        }
        var payload = JSON.stringify(state)
        if (payload !== lastVehicleMapPayload) {
            root.mapDispatcher.setSnapshot("vehicle", state)
            lastVehicleMapPayload = payload
        }
        vehicleRenderDirty = false
    }

    function resetVehicleCameraState() {
        vehiclePositionReady = false
        lastVehicleMapPayload = ""
        vehicleRenderDirty = true
        flushVehicleMapState(true)
    }

    Settings {
        id: persistedMapView
        category: "Map"
        property real latitude: 0
        property real longitude: 0
        property real zoom: 15
    }

    signal mapContextMenuRequested(real x, real y, var event)
    signal joystickEnabledRequested(bool enabled)

    onVehicleConnectedChanged: {
        if (vehicleConnected) {
            syncVehiclePosition()
        } else {
            vehiclePositionReady = false
            resetVehicleCameraState()
            clearVehicleTrail()

            if (systemPositionReady) {
                updatePointer(systemLat, systemLon, 0)
            }
        }
    }

    onVehicleChanged: {
        resetVehicleCameraState()
        clearVehicleTrail()
        syncVehiclePosition()
    }
    onVehicleHeadingChanged: compassIcon.requestPaint()
    onCompassFollowEnabledChanged: {
        compassIcon.requestPaint()
        vehicleRenderDirty = true
        flushVehicleMapState(true)
    }
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

    // Geographic interaction values are not telemetry sentinels: (0, 0) is
    // a valid operator-selected map coordinate.
    function isValidInteractionCoordinate(lat, lon)
    {
        return isFinite(lat) && isFinite(lon)
                && lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180
    }

    function centerMap(lat, lon, zoom, callback)
    {
        if (!root.isValidCoordinate(lat, lon))
            return mapDispatcher.reject("center", {lat:lat, lon:lon, zoom:zoom}, "invalid-payload", callback)

        return root.dispatchCommand("center", {lat:lat, lon:lon, zoom:zoom}, callback, 2000)
    }

    function fitMission(callback) { return dispatchCommand("fitMission", {}, callback, 2000) }
    function coordinateAt(x, y, callback) { return dispatchCommand("coordinate", {x:x, y:y}, callback, 1000) }

    function initializeViewport()
    {
        // Establish once per document. JavaScript restores its own per-page
        // session camera first and rejects initialization after operator input.
        var view = null
        if (root.isValidCoordinate(persistedMapView.latitude, persistedMapView.longitude)) {
            view = {latitude: persistedMapView.latitude, longitude: persistedMapView.longitude,
                    zoom: persistedMapView.zoom}
        } else if (root.vehiclePositionReady) {
            view = {latitude: root.vehicleLat, longitude: root.vehicleLon, zoom: 17}
        } else if (root.systemPositionReady) {
            view = {latitude: root.systemLat, longitude: root.systemLon, zoom: 15}
        }
        root.mapDispatcher.setSnapshot("initialViewport", view)
    }

    function persistCurrentMapView()
    {
        if (!root.mapReady || root.map3dEnabled || mapDispatcher.hasCommand("view"))
            return

        root.dispatchCommand("view", {}, function(result) {
            if (result.status !== "success") return
            try {
                var view = result.value
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
        }, 1000)
    }

    function updatePointer(lat, lon, heading)
    {
        if (!root.isValidCoordinate(lat, lon))
            return
        markerLat = lat
        markerLon = lon
        if (isFinite(heading)) vehicleHeading = ((heading % 360) + 360) % 360
        vehicleRenderDirty = true
    }

    function updateVehicleTrail(lat, lon)
    {
        if (!root.vehicleConnected || !root.isValidCoordinate(lat, lon))
            return

        root.mapDispatcher.appendTrail(lat, lon)
        vehicleRenderDirty = true
    }

    function clearVehicleTrail()
    {
        root.mapDispatcher.clearTrail()
    }

    function syncVehiclePosition()
    {
        if (!root.vehicle)
            return
        if (!root.vehiclePositionReady && !root.vehicle.positionFresh) return

        var nextLat = root.vehicle.latitude
        var nextLon = root.vehicle.longitude

        if (!root.isValidCoordinate(nextLat, nextLon)) {
            vehiclePositionReady = false
            vehicleRenderDirty = true
            flushVehicleMapState(true)
            return
        }

        root.vehicleLat = nextLat
        root.vehicleLon = nextLon
        root.vehiclePositionReady = true
        updatePointer(root.vehicleLat, root.vehicleLon, root.vehicleHeading)
        if (root.vehicle.positionFresh) updateVehicleTrail(root.vehicleLat, root.vehicleLon)
    }

    function syncVehicleHeading(preferYaw)
    {
        if (!root.vehicle)
            return

        if (!root.vehicle.headingValid || !root.vehicle.headingFresh) return
        var nextHeading = preferYaw ? Number(root.vehicle.yaw) * 180 / Math.PI : Number(root.vehicle.heading)
        if (!isFinite(nextHeading) || (!preferYaw && (nextHeading < 0 || nextHeading > 360))) {
            nextHeading = Number(root.vehicle.yaw) * 180 / Math.PI
        }
        if (isFinite(nextHeading)) {
            root.vehicleHeading = ((nextHeading % 360) + 360) % 360
            vehicleRenderDirty = true
        }
    }

    function centerOnVehicle(callback)
    {
        syncVehiclePosition()

        if (!root.vehiclePositionReady)
            return mapDispatcher.reject("center", {}, "map-unavailable", callback)

        var id = centerMap(root.vehicleLat, root.vehicleLon, 17, callback)
        flushVehicleMapState(true)
        return id
    }

    function syncMapViewMode()
    {
        if (!root.documentLoaded || !root.bridgeAvailable || root.readinessState === "failed") return
        var safeMode = root.mapViewMode === "street" ? "street" : "satellite"
        if (safeMode === mapDispatcher.lastStyleMode()) return
        root.dispatchCommand("style", {mode:safeMode}, null, 20000)
    }

    function beginStyleTransaction(mode, operation) {
        root.styleGeneration++
        root.appliedMapViewMode = mode
        root.styleTransactionId = operation && operation.payload ? operation.payload.transactionId : ""
        root.styleTransactionState = "starting"
        root.readinessState = "style-loading"
        initializationDeadline.restart()
        updateDispatchContext()
    }

    function syncMissionWaypoints()
    {
        var waypoints = root.missionWaypoints || []
        root.mapDispatcher.setSnapshot("mission", waypoints)
    }


    function syncRasterOverlays()
    {
        if (!root.rasterManager) {
            root.mapDispatcher.setSnapshot("rasters", {overlays:[], enabled:false})
            return
        }

        var overlays = root.rasterManager.enabled ? root.rasterManager.overlays : []
        root.mapDispatcher.setSnapshot("rasters", {overlays:overlays, enabled:root.rasterManager.enabled})
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
                updatePointer(root.systemLat, root.systemLon, 0)
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

            if (loadRequest.status === WebEngineView.LoadStartedStatus) {
                root.beginMapDocument()
            } else if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                if (root.readinessState === "failed") return
                root.documentLoaded = true
                root.readinessState = "document-loaded"
                root.updateDispatchContext()
                root.probeMapReadiness()
            } else if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                root.failMapReadiness("HTML load failed (" + loadRequest.errorCode + "): " + loadRequest.errorString)
            } else if (loadRequest.status === WebEngineView.LoadStoppedStatus && !root.documentLoaded) {
                root.failMapReadiness("HTML document loading stopped")
            }
        }

        onRenderProcessTerminated: function(terminationStatus, exitCode) {
            root.documentGeneration++
            root.documentLoaded = false
            root.bridgeAvailable = false
            root.documentId = ""
            root.failMapReadiness("WebEngine renderer terminated: status " + terminationStatus + ", exit code " + exitCode)
        }

        onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceID) {
            if (root.acceptInteractionNotification(message))
                return
            var diagnostic = "Map JS [document " + root.documentGeneration + ", style " + root.styleGeneration
                    + "] " + sourceID + ":" + lineNumber + ": " + message
            if (level === WebEngineView.ErrorMessageLevel) {
                console.error(diagnostic)
                // Console messages have no document token. Log them, but let
                // the generation-checked handshake (or timeout if syntax stops
                // bootstrap execution) decide readiness.
            } else if (level === WebEngineView.WarningMessageLevel) console.warn(diagnostic)
            else console.log(diagnostic)
        }

        onContextMenuRequested: function(request) {
            request.accepted = true
            // MapLibre emits the generation-stamped context event. This only
            // suppresses WebEngine's native menu.
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

    ParameterStatusButton {
        id: parameterStatusButton
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 12
        anchors.topMargin: 12
        z: 21
        vehicle: root.vehicle
        mavlinkObject: root.mavlinkObject
    }

    Button {
        id: centerVehicleButton

        width: 32
        height: 32
        anchors.right: parent.right
        anchors.top: parameterStatusButton.bottom
        anchors.rightMargin: 12
        anchors.topMargin: 8
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
            root.syncRasterOverlays()
        }
    }
    Connections {
        target: root.vehicle

        function onFlightStateChanged() {
            root.vehicleRenderDirty = true
            root.flushVehicleMapState(true)
        }
        function onTelemetryFreshnessChanged() {
            root.vehicleRenderDirty = true
            root.flushVehicleMapState(true)
        }
        function onArmedChanged() {
            root.vehicleRenderDirty = true
            root.flushVehicleMapState(true)
        }

        function onPositionChanged()
        {
            syncVehiclePosition()
        }

        function onVfrHudChanged()
        {
            if (!root.vehicle)
                return

            syncVehicleHeading(false)
        }

        function onYawChanged()
        {
            if (!root.vehicle)
                return

            syncVehicleHeading(true)
            compassIcon.requestPaint()
        }
    }
}
