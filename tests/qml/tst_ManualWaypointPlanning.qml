import QtQuick
import QtTest

TestCase {
    name: "ManualWaypointPlanning"
    property string source: ""

    function initTestCase() {
        var request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../ui/qml/core/FlyView.qml"), false)
        request.send()
        source = request.responseText.replace(/\r\n/g, "\n")
        verify(source.indexOf("id: waypointEditorPanel") >= 0)
        verify(source.indexOf("id: waypointSequenceField") >= 0)
        verify(source.indexOf("id: waypointParam4Field") >= 0)
        verify(source.indexOf("defaultWaypointAltitude: 40.0") >= 0)
    }

    function block(prefix) {
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

    function harness() {
        var h = {
            planningModeInactive: "inactive", planningModeManual: "manual", planningModeAutomatic: "automatic",
            planningMode: "manual", waypointEditorError: "", handledManualInteractionIds: ({}),
            handledManualInteractionOrder: [], statuses: [], delivered: [],
            manualMissionSettings: {defaultWaypointAltitude: 40},
            waypointCommandField: {text:"16"}, waypointFrameField: {text:"3"},
            waypointLatitudeField: {text:"10"}, waypointLongitudeField: {text:"20"},
            waypointAltitudeField: {text:"40"}, waypointParam1Field: {text:"0"},
            waypointParam2Field: {text:"0"}, waypointParam3Field: {text:"0"}, waypointParam4Field: {text:"0"}
        }
        h.rows = []
        h.missionPlanModel = {
            sourceState: 0, selectedItemId: "", count: 0, lastError: "",
            indexOfId: function(id) { for (var i = 0; i < h.rows.length; ++i) if (h.rows[i].id === id) return i; return -1 },
            insertItem: function(item) {
                var id = "waypoint-" + (h.rows.length + 1)
                h.rows.push({id:id, sequence:h.rows.length, command:item.command, frame:item.frame,
                             latitude:item.latitude, longitude:item.longitude, altitude:item.altitude,
                             param1:0, param2:0, param3:0, param4:0, hasCoordinate:item.hasCoordinate})
                this.count = h.rows.length; this.sourceState = 1; return true
            },
            itemAt: function(row) { return h.rows[row] || ({}) },
            itemById: function(id) { var index = this.indexOfId(id); return index >= 0 ? h.rows[index] : ({}) },
            setSelectedItemId: function(id) { this.selectedItemId = id },
            updateItem: function(id, values) {
                var index = this.indexOfId(id); if (index < 0) { this.lastError = "missing"; return false }
                Object.assign(h.rows[index], values); return true
            },
            lastOperationError: function() { return this.lastError }
        }
        h.root = h
        h.showActionStatus = function(message) { h.statuses.push(message) }
        h.manualMapInteractionReceived = function(event) { h.delivered.push(event) }
        var names = ["rememberManualInteractionId", "isValidManualWaypointCoordinate", "defaultManualWaypointAltitude",
                     "beginManualMissionEdit", "createManualWaypoint", "selectManualWaypoint", "closeWaypointEditor",
                     "applyWaypointEditorChanges", "handleMapInteraction"]
        var declarations = ""
        var exports = []
        for (var name of names) {
            var start = source.indexOf("function " + name + "(")
            var brace = source.indexOf("{", start)
            declarations += source.substring(start, brace) + block("function " + name + "(") + "\n"
            exports.push(name + ":" + name)
        }
        Object.assign(h, new Function("scope", "with(scope){" + declarations + "return {" + exports.join(",") + "};}")(h))
        return h
    }

    function event(id, type, extra) {
        var value = {eventId:id, type:type, instanceId:"main", documentId:"document", documentGeneration:1, styleGeneration:2}
        if (type === "MapClicked") { value.latitude = 10.0; value.longitude = 20.0 }
        if (type === "WaypointSelected") value.waypointId = "waypoint-1"
        if (extra) Object.assign(value, extra)
        return value
    }

    function map() { return {mapInstanceActive:true, mapReady:true, map3dEnabled:false, mapInstanceId:"main", documentId:"document", documentGeneration:1, styleGeneration:2} }

    function test_validClickCreatesOneStableCanonicalWaypoint() {
        var h = harness()
        verify(h.handleMapInteraction(map(), event("click-1", "MapClicked")))
        compare(h.rows.length, 1)
        compare(h.rows[0].id, "waypoint-1")
        compare(h.rows[0].sequence, 0)
        compare(h.rows[0].command, 16)
        compare(h.rows[0].frame, 3)
        compare(h.rows[0].altitude, 40)
        compare(h.missionPlanModel.selectedItemId, "waypoint-1")
    }

    function test_invalidDuplicateStaleAndNonManualClicksAreRejected() {
        var h = harness()
        verify(!h.handleMapInteraction(map(), event("bad", "MapClicked", {latitude:91})))
        verify(!h.handleMapInteraction(map(), event("stale", "MapClicked", {styleGeneration:1})))
        h.planningMode = h.planningModeInactive
        verify(!h.handleMapInteraction(map(), event("inactive", "MapClicked")))
        h.planningMode = h.planningModeManual
        verify(h.handleMapInteraction(map(), event("one", "MapClicked")))
        verify(!h.handleMapInteraction(map(), event("one", "MapClicked")))
        compare(h.rows.length, 1)
    }

    function test_insertPreservesExistingFieldsAndSelectionDoesNotChangeContent() {
        var h = harness()
        verify(h.handleMapInteraction(map(), event("first", "MapClicked", {latitude:1, longitude:2})))
        h.rows[0].param2 = 77
        verify(h.handleMapInteraction(map(), event("second", "MapClicked", {latitude:3, longitude:4})))
        compare(h.rows[0].id, "waypoint-1")
        compare(h.rows[0].param2, 77)
        compare(h.rows[1].sequence, 1)
        verify(h.handleMapInteraction(map(), event("select", "WaypointSelected")))
        compare(h.missionPlanModel.selectedItemId, "waypoint-1")
    }

    function test_editorAppliesSupportedFieldsAndRejectsInvalidValues() {
        var h = harness()
        verify(h.handleMapInteraction(map(), event("first", "MapClicked")))
        h.waypointCommandField.text = "22"; h.waypointFrameField.text = "3"
        h.waypointLatitudeField.text = "11"; h.waypointLongitudeField.text = "21"
        h.waypointAltitudeField.text = "75"; h.waypointParam1Field.text = "1"
        h.waypointParam2Field.text = "2"; h.waypointParam3Field.text = "3"; h.waypointParam4Field.text = "4"
        verify(h.applyWaypointEditorChanges())
        compare(h.rows[0].command, 22); compare(h.rows[0].latitude, 11); compare(h.rows[0].param4, 4)
        h.waypointLatitudeField.text = "91"
        verify(!h.applyWaypointEditorChanges())
        verify(h.waypointEditorError.length > 0)
        h.waypointLatitudeField.text = "11"; h.waypointAltitudeField.text = "not-a-number"
        verify(!h.applyWaypointEditorChanges())
        verify(h.waypointEditorError.length > 0)
    }

    function test_nonCoordinateSelectionDoesNotRequireOrOverwriteCoordinates() {
        var h = harness()
        h.rows.push({id:"command-1", sequence:0, command:178, frame:2,
                     latitude:undefined, longitude:undefined, altitude:0,
                     param1:1, param2:2, param3:3, param4:4, hasCoordinate:false})
        h.missionPlanModel.count = 1
        h.missionPlanModel.sourceState = 1
        h.missionPlanModel.setSelectedItemId("command-1")
        h.waypointLatitudeField.text = ""
        h.waypointLongitudeField.text = ""
        h.waypointParam1Field.text = "9"
        verify(h.applyWaypointEditorChanges())
        compare(h.rows[0].param1, 9)
        compare(h.rows[0].latitude, undefined)
        compare(h.rows[0].longitude, undefined)
    }

    function test_manualMutationExplicitlyTransitionsAutomaticPlanAndExitDeselects() {
        var h = harness()
        h.missionPlanModel.sourceState = 4
        verify(h.handleMapInteraction(map(), event("automatic-edit", "MapClicked")))
        compare(h.missionPlanModel.sourceState, 1)
        verify(h.statuses[0].indexOf("manual draft") >= 0)
        h.closeWaypointEditor()
        compare(h.missionPlanModel.selectedItemId, "")
    }
}
