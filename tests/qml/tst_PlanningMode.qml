import QtQuick
import QtTest

TestCase {
    name: "PlanningMode"
    property string source: ""

    function initTestCase() {
        var request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../ui/qml/core/FlyView.qml"), false)
        request.send()
        source = request.responseText.replace(/\r\n/g, "\n")
        verify(source.indexOf("id: planningSelector") >= 0)
        verify(source.indexOf("id: manualPlanningIndicator") >= 0)
        verify(source.indexOf("onClosed: root.automaticPlanningClosed()") >= 0)
        verify(source.indexOf("id: replaceGeneratedMissionDialog") >= 0)
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
            planningModeInactive: "inactive",
            planningModeManual: "manual",
            planningModeAutomatic: "automatic",
            planningMode: "inactive",
            mavlinkObject: {missionUploaded:false, missionWaypoints:[]},
            pendingGeneratedMissionPath: "",
            missionPlanModel: { dirty: false, setSelectedItemId:function(id) {} },
            missionPlanConverter: { imports:0, importWplFile:function(path, model, source) {
                this.imports++
                return {success:true, error:""}
            } },
            statuses: [],
            planningSelector: {
                opened:false,
                opens:0,
                closes:0,
                open:function() { this.opened=true; this.opens++ },
                close:function() { this.opened=false; this.closes++ }
            },
            missionBuilderPanel: {
                opens:0,
                closes:0,
                opened:false,
                open:function(hasExistingPlan) { this.opens++; this.opened=true; this.lastExistingPlan=hasExistingPlan },
                close:function() { this.closes++; this.opened=false }
            },
            replaceGeneratedMissionDialog: {
                opened:false,
                open:function() { this.opened=true },
                close:function() { this.opened=false; h.pendingGeneratedMissionPath="" }
            }
        }
        h.root = h
        h.showActionStatus = function(status) { h.statuses.push(status) }
        h.localFileUrl = function(path) { return path }
        h.syncCanonicalMissionProjection = function() {}
        var names = ["openPlanningSelector", "activateManualPlanning", "activateAutomaticPlanning",
                     "exitManualPlanning", "automaticPlanningClosed", "closeWaypointEditor"]
        names.push("loadGeneratedMission", "importGeneratedMission")
        var declarations = ""
        var exports = []
        for (var name of names) {
            var start = source.indexOf("function " + name + "(")
            var brace = source.indexOf("{", start)
            declarations += source.substring(start, brace) + block("function " + name + "(") + "\n"
            exports.push(name + ":" + name)
        }
        Object.assign(h, new Function("scope", "with(scope){" + declarations
                                      + "return {" + exports.join(",") + "};}")(h))
        return h
    }

    function test_inactiveToManualAndExit() {
        var h = harness()
        h.openPlanningSelector()
        verify(h.planningSelector.opened)
        h.activateManualPlanning()
        compare(h.planningMode, h.planningModeManual)
        verify(!h.planningSelector.opened)
        compare(h.missionBuilderPanel.opens, 0)
        h.exitManualPlanning()
        compare(h.planningMode, h.planningModeInactive)
    }

    function test_inactiveToAutomaticUsesExistingBuilderOpenPath() {
        var h = harness()
        h.openPlanningSelector()
        h.activateAutomaticPlanning()
        compare(h.planningMode, h.planningModeAutomatic)
        compare(h.missionBuilderPanel.opens, 1)
        compare(h.missionBuilderPanel.lastExistingPlan, false)
        verify(!h.planningSelector.opened)
        h.automaticPlanningClosed()
        compare(h.planningMode, h.planningModeInactive)
    }

    function test_modeSwitchesCannotRemainSimultaneouslyActive() {
        var h = harness()
        h.activateManualPlanning()
        h.activateAutomaticPlanning()
        compare(h.planningMode, h.planningModeAutomatic)
        compare(h.missionBuilderPanel.opens, 1)
        h.activateManualPlanning()
        compare(h.planningMode, h.planningModeManual)
        compare(h.missionBuilderPanel.closes, 1)
        // The delayed automatic-panel close cannot clear the new manual mode.
        h.automaticPlanningClosed()
        compare(h.planningMode, h.planningModeManual)
    }

    function test_repeatedPlanAndDismissalDoNotChangeMode() {
        var h = harness()
        h.openPlanningSelector()
        h.openPlanningSelector()
        compare(h.planningSelector.opens, 2)
        h.planningSelector.close() // dismissal without selection
        compare(h.planningMode, h.planningModeInactive)
        h.activateManualPlanning()
        h.openPlanningSelector()
        h.planningSelector.close()
        compare(h.planningMode, h.planningModeManual)
    }

    function test_autoAfterManualExitAndManualAfterAutoClose() {
        var h = harness()
        h.activateManualPlanning()
        h.exitManualPlanning()
        h.activateAutomaticPlanning()
        compare(h.planningMode, h.planningModeAutomatic)
        h.automaticPlanningClosed()
        h.activateManualPlanning()
        compare(h.planningMode, h.planningModeManual)
    }

    function test_generatedAutoPlanDoesNotReplaceDirtyDraftWithoutDecision() {
        var h = harness()
        h.missionPlanModel.dirty = true
        h.loadGeneratedMission("generated.waypoints")
        verify(h.replaceGeneratedMissionDialog.opened)
        compare(h.pendingGeneratedMissionPath, "generated.waypoints")
        compare(h.missionPlanConverter.imports, 0)
        h.replaceGeneratedMissionDialog.close()
        compare(h.pendingGeneratedMissionPath, "")
        compare(h.missionPlanConverter.imports, 0)
        h.missionPlanModel.dirty = false
        h.loadGeneratedMission("generated.waypoints")
        compare(h.missionPlanConverter.imports, 1)
    }
}
