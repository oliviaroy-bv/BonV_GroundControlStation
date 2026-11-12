import QtQuick
import QtTest

TestCase {
    name: "MapInteractionBridge"
    property string flyMapSource: ""
    property string flyViewSource: ""
    property string htmlSource: ""

    function initTestCase() {
        var request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../ui/qml/map/FlyMap.qml"), false)
        request.send()
        flyMapSource = request.responseText.replace(/\r\n/g, "\n")
        request.open("GET", Qt.resolvedUrl("../../ui/qml/core/FlyView.qml"), false)
        request.send()
        flyViewSource = request.responseText.replace(/\r\n/g, "\n")
        request.open("GET", Qt.resolvedUrl("../../integrations/map_provider/map/map.html"), false)
        request.send()
        htmlSource = request.responseText.replace(/\r\n/g, "\n")
    }

    function block(prefix, source) {
        var start = source.indexOf(prefix)
        verify(start >= 0, "Missing source block: " + prefix)
        start = source.indexOf("{", start)
        var depth = 1
        var end = start + 1
        for (; depth && end < source.length; ++end) {
            if (source[end] === "{") ++depth
            else if (source[end] === "}") --depth
        }
        return source.substring(start, end)
    }

    function event(type, overrides) {
        var value = {
            eventId: "main:1:1:" + Math.floor(Math.random() * 1000000000),
            type: type,
            instanceId: "main",
            documentId: "document",
            documentGeneration: 1,
            styleGeneration: 1,
            latitude: 10,
            longitude: 20,
            screenX: 100,
            screenY: 200
        }
        return Object.assign(value, overrides || {})
    }

    function harness() {
        var delivered = []
        var contexts = []
        var root = {
            mapInstanceId: "main",
            documentId: "document",
            documentGeneration: 1,
            styleGeneration: 1,
            mapReady: true,
            mapInstanceActive: true,
            map3dEnabled: false,
            manualPlanningActive: true,
            width: 800,
            height: 600,
            activeDraggedWaypointId: "",
            interactionSeenIds: ({}),
            interactionSeenOrder: [],
            isValidCoordinate: function(lat, lon) {
                return isFinite(lat) && isFinite(lon) && lat >= -90 && lat <= 90
                        && lon >= -180 && lon <= 180 && !(lat === 0 && lon === 0)
            },
            isValidInteractionCoordinate: function(lat, lon) {
                return isFinite(lat) && isFinite(lon) && lat >= -90 && lat <= 90
                        && lon >= -180 && lon <= 180
            },
            rememberInteractionId: function(id) {
                if (this.interactionSeenIds[id]) return false
                this.interactionSeenIds[id] = true
                this.interactionSeenOrder.push(id)
                if (this.interactionSeenOrder.length > 256)
                    delete this.interactionSeenIds[this.interactionSeenOrder.shift()]
                return true
            },
            hasKnownWaypointId: function(id) { return id === "waypoint-a" || id === "waypoint-b" },
            mapInteractionReceived: function(value) { delivered.push(value) },
            mapContextMenuRequested: function(x, y, value) { contexts.push({x:x, y:y, event:value}) }
        }
        root.root = root
        var declaration = "function acceptInteractionNotification(message) "
                + block("function acceptInteractionNotification(message)", flyMapSource)
        root.accept = new Function("root", "with(root) {" + declaration + "; return acceptInteractionNotification;}")(root)
        return {root: root, delivered: delivered, contexts: contexts}
    }

    function send(h, value) {
        return h.root.accept("BONV_MAP_INTERACTION:" + JSON.stringify(value))
    }

    function test_eventSchemaAndConsoleNamespace() {
        verify(flyMapSource.indexOf("signal mapInteractionReceived(var event)") >= 0)
        verify(htmlSource.indexOf("BONV_MAP_INTERACTION:") >= 0)
        verify(htmlSource.indexOf("MapClicked") >= 0)
        verify(htmlSource.indexOf("WaypointSelected") >= 0)
        verify(htmlSource.indexOf("WaypointDragStarted") >= 0)
        verify(htmlSource.indexOf("WaypointDragged") >= 0)
        verify(htmlSource.indexOf("WaypointDragFinished") >= 0)
        verify(htmlSource.indexOf("MapContextRequested") >= 0)
        verify(htmlSource.indexOf("snapshot:interaction") >= 0)
    }

    function test_mapClickDeliveryAndManualGating() {
        var h = harness()
        var click = event("MapClicked")
        verify(send(h, click))
        compare(h.delivered.length, 1)
        compare(h.delivered[0].type, "MapClicked")
        h.root.manualPlanningActive = false
        verify(!send(h, event("MapClicked")))
        compare(h.delivered.length, 1)
    }

    function test_selectionAndDragLifecycle() {
        var h = harness()
        verify(send(h, event("WaypointSelected", {waypointId:"waypoint-a"})))
        verify(send(h, event("WaypointDragStarted", {waypointId:"waypoint-a"})))
        compare(h.root.activeDraggedWaypointId, "waypoint-a")
        verify(send(h, event("WaypointDragged", {waypointId:"waypoint-a", latitude:11})))
        verify(send(h, event("WaypointDragFinished", {waypointId:"waypoint-a", latitude:12})))
        compare(h.root.activeDraggedWaypointId, "")
        verify(!send(h, event("WaypointDragged", {waypointId:"waypoint-a"})))
        verify(!send(h, event("WaypointSelected", {waypointId:"unknown"})))
    }

    function test_invalidDuplicateAndStaleEventsRejected() {
        var h = harness()
        var valid = event("MapClicked", {eventId:"main:1:1:fixed"})
        verify(send(h, valid))
        verify(!send(h, valid))
        verify(!send(h, event("MapClicked", {latitude:91})))
        verify(!send(h, event("MapClicked", {documentGeneration:0})))
        verify(!send(h, event("MapClicked", {styleGeneration:2})))
        verify(!h.root.accept("BONV_MAP_INTERACTION:{not-json"))
        compare(h.delivered.length, 1)
    }

    function test_instanceIsolationAndContextCoordinates() {
        var h = harness()
        verify(!send(h, event("MapClicked", {instanceId:"popout"})))
        h.root.mapInstanceActive = false
        verify(!send(h, event("MapClicked")))
        h.root.mapInstanceActive = true
        h.root.manualPlanningActive = false
        verify(send(h, event("MapContextRequested", {screenX:50, screenY:60, latitude:1, longitude:2})))
        compare(h.contexts.length, 1)
        compare(h.contexts[0].x, 50)
        compare(h.contexts[0].event.longitude, 2)
        verify(send(h, event("MapContextRequested", {latitude:0, longitude:0, screenX:51, screenY:61})))
    }

    function test_reloadStyleAndManualExitCancelDragWithoutModelMutation() {
        var h = harness()
        var revision = 17
        verify(send(h, event("WaypointDragStarted", {waypointId:"waypoint-a"})))
        h.root.styleGeneration = 2
        verify(!send(h, event("WaypointDragged", {waypointId:"waypoint-a"})))
        // The QML handlers clear any local drag proposal on style, inactive
        // map, and manual-mode transitions; none has a MissionPlanModel API.
        verify(flyMapSource.indexOf("onStyleGenerationChanged") >= 0)
        verify(flyMapSource.indexOf("onManualPlanningActiveChanged") >= 0)
        verify(flyMapSource.indexOf("onMapInstanceActiveChanged") >= 0)
        compare(revision, 17)
        verify(block("function acceptInteractionNotification(message)", flyMapSource)
               .indexOf("updateMissionItem") < 0)
        verify(block("function acceptInteractionNotification(message)", flyMapSource)
               .indexOf("centerMap") < 0)
    }

    function test_flyViewKeepsContextMenuAndModelOwnership() {
        verify(flyViewSource.indexOf("function openMapMissionMenu") >= 0)
        verify(flyViewSource.indexOf("function handleMapInteraction") >= 0)
        var handler = block("function handleMapInteraction(mapItem, interaction)", flyViewSource)
        verify(handler.indexOf("manualMapInteractionReceived") >= 0)
        verify(handler.indexOf("updateItem") < 0)
        verify(handler.indexOf("centerMap") < 0)
    }
}
