import QtQuick
import QtTest

TestCase {
    name: "ManualWaypointEditing"
    property string source: ""

    function initTestCase() {
        var request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../ui/qml/core/FlyView.qml"), false)
        request.send()
        source = request.responseText.replace(/\r\n/g, "\n")
        verify(source.indexOf("function finishManualWaypointDrag") >= 0)
        verify(source.indexOf("id: deleteWaypointButton") >= 0)
        verify(source.indexOf("id: moveWaypointUpButton") >= 0)
        verify(source.indexOf("id: moveWaypointDownButton") >= 0)
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
            planningModeManual:"manual", planningMode:"manual", waypointEditorError:"",
            manualWaypointDrag:null, statuses:[], restored:0,
            waypointEditorPanel:{loadSelectedItem:function() { h.editorLoads++ }}, editorLoads:0
        }
        h.rows = [
            {id:"a", sequence:0, command:16, frame:3, latitude:10, longitude:20, altitude:40, param1:1, param2:2, param3:3, param4:4, hasCoordinate:true},
            {id:"b", sequence:1, command:16, frame:3, latitude:11, longitude:21, altitude:50, param1:5, param2:6, param3:7, param4:8, hasCoordinate:true},
            {id:"command", sequence:2, command:178, frame:2, altitude:0, param1:9, param2:10, param3:11, param4:12, hasCoordinate:false}
        ]
        h.revision = 0
        h.missionPlanModel = {
            sourceState:1, selectedItemId:"a", get count() { return h.rows.length }, lastError:"",
            indexOfId:function(id) { for (var i=0;i<h.rows.length;i++) if (h.rows[i].id === id) return i; return -1 },
            itemById:function(id) { var i=this.indexOfId(id); return i < 0 ? ({}) : h.rows[i] },
            itemAt:function(i) { return h.rows[i] || ({}) },
            setSelectedItemId:function(id) { this.selectedItemId=id },
            updateItem:function(id, changes) { var i=this.indexOfId(id); if (i<0) {this.lastError="missing"; return false}; Object.assign(h.rows[i], changes); h.revision++; return true },
            removeItem:function(id) { var i=this.indexOfId(id); if (i<0) {this.lastError="missing"; return false}; h.rows.splice(i,1); for(var j=0;j<h.rows.length;j++)h.rows[j].sequence=j; h.revision++; return true },
            moveItem:function(from,to) { if(from<0||to<0||from>=h.rows.length||to>=h.rows.length||from===to) return false; var item=h.rows.splice(from,1)[0]; h.rows.splice(to,0,item); for(var j=0;j<h.rows.length;j++)h.rows[j].sequence=j; h.revision++; return true },
            lastOperationError:function() { return this.lastError }
        }
        h.root=h
        h.showActionStatus=function(message) { h.statuses.push(message) }
        h.isValidManualWaypointCoordinate=function(lat,lon) { return isFinite(Number(lat)) && isFinite(Number(lon)) && Number(lat)>=-90 && Number(lat)<=90 && Number(lon)>=-180 && Number(lon)<=180 }
        h.beginManualMissionEdit=function() { return h.planningMode === "manual" }
        h.selectManualWaypoint=function(id) { if (h.missionPlanModel.indexOfId(id) < 0) return false; h.missionPlanModel.setSelectedItemId(id); return true }
        var names=["dragMatchesMap","beginManualWaypointDrag","updateManualWaypointDrag","restoreManualWaypointDrag","cancelManualWaypointDrag","finishManualWaypointDrag","deleteSelectedManualWaypoint","moveSelectedManualWaypoint"]
        var declarations="", exports=[]
        for (var i=0;i<names.length;i++) {
            var name=names[i], start=source.indexOf("function "+name+"(")
            var brace=source.indexOf("{",start)
            declarations += source.substring(start,brace)+block("function "+name+"(")+"\n"
            exports.push(name+":"+name)
        }
        Object.assign(h,new Function("scope","with(scope){"+declarations+"return {"+exports.join(",")+"};}")(h))
        return h
    }

    function map(instance, documentGeneration, styleGeneration) {
        return {mapInstanceId:instance || "main", documentId:"doc", documentGeneration:documentGeneration || 1,
                styleGeneration:styleGeneration || 2, restoreCanonicalMissionRendering:function() { this.restored=true }}
    }
    function drag(type, id, latitude, longitude) { return {type:type, waypointId:id || "a", latitude:latitude, longitude:longitude} }

    function test_dragCommitsOnlyFinalCoordinatesAndPreservesFields() {
        var h=harness(), m=map()
        verify(h.beginManualWaypointDrag(m,drag("WaypointDragStarted","a",10,20)))
        verify(h.updateManualWaypointDrag(m,drag("WaypointDragged","a",10.1,20.1)))
        compare(h.revision,0)
        verify(h.finishManualWaypointDrag(m,drag("WaypointDragFinished","a",12,22)))
        compare(h.revision,1); compare(h.rows[0].latitude,12); compare(h.rows[0].longitude,22)
        compare(h.rows[0].altitude,40); compare(h.rows[0].param4,4); compare(h.missionPlanModel.selectedItemId,"a")
    }

    function test_dragRejectsInvalidStaleWrongMapAndUnknownIds() {
        var h=harness(), m=map()
        verify(!h.beginManualWaypointDrag(m,drag("WaypointDragStarted","command",10,20)))
        verify(h.beginManualWaypointDrag(m,drag("WaypointDragStarted","a",10,20)))
        verify(!h.updateManualWaypointDrag(map("other"),drag("WaypointDragged","a",11,21)))
        verify(!h.finishManualWaypointDrag(map("main",1,3),drag("WaypointDragFinished","a",11,21)))
        compare(h.revision,0); compare(h.rows[0].latitude,10)
    }

    function test_dragCancellationRestoresCanonicalAndManualExitIsSafe() {
        var h=harness(), m=map()
        verify(h.beginManualWaypointDrag(m,drag("WaypointDragStarted","a",10,20)))
        verify(h.cancelManualWaypointDrag(m,"a"))
        compare(h.manualWaypointDrag,null); verify(m.restored); compare(h.revision,0)
        verify(h.beginManualWaypointDrag(m,drag("WaypointDragStarted","a",10,20)))
        h.planningMode="inactive"
        verify(!h.finishManualWaypointDrag(m,drag("WaypointDragFinished","a",11,21)))
        compare(h.rows[0].latitude,10); compare(h.revision,0)
    }

    function test_deleteChoosesNeighborAndPreservesOtherItems() {
        var h=harness(); h.missionPlanModel.selectedItemId="b"
        verify(h.deleteSelectedManualWaypoint())
        compare(h.rows.length,2); compare(h.rows[0].id,"a"); compare(h.rows[1].id,"command")
        compare(h.rows[1].sequence,1); compare(h.rows[1].param4,12); compare(h.missionPlanModel.selectedItemId,"command")
        compare(h.revision,1)
        h.missionPlanModel.selectedItemId="missing"
        verify(!h.deleteSelectedManualWaypoint()); compare(h.revision,1)
    }

    function test_reorderRetainsSelectedIdFieldsAndBoundaries() {
        var h=harness(); h.missionPlanModel.selectedItemId="b"
        verify(h.moveSelectedManualWaypoint(-1))
        compare(h.rows[0].id,"b"); compare(h.rows[0].altitude,50); compare(h.rows[1].id,"a")
        compare(h.rows[0].sequence,0); compare(h.missionPlanModel.selectedItemId,"b"); compare(h.revision,1)
        verify(!h.moveSelectedManualWaypoint(-1)); compare(h.revision,1)
        verify(h.moveSelectedManualWaypoint(1)); compare(h.rows[1].id,"b"); compare(h.revision,2)
    }

    function test_noCameraOrDirectMapModelMutation() {
        var finish=block("function finishManualWaypointDrag")
        verify(finish.indexOf("centerMap") < 0)
        verify(block("function updateManualWaypointDrag").indexOf("updateItem") < 0)
        verify(source.indexOf("restoreCanonicalMissionRendering") >= 0)
    }
}
