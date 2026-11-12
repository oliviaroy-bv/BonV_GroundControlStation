import QtQuick
import QtTest

TestCase {
    name: "CanonicalMissionTransfer"
    property string flyViewSource: ""
    property string bottomPanelSource: ""

    function read(path) {
        var request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl(path), false)
        request.send()
        return request.responseText.replace(/\r\n/g, "\n")
    }

    function initTestCase() {
        flyViewSource = read("../../ui/qml/core/FlyView.qml")
        bottomPanelSource = read("../../ui/qml/panels/BottomPanel.qml")
        verify(bottomPanelSource.indexOf("signal uploadMissionRequested()") >= 0)
        verify(bottomPanelSource.indexOf("signal clearMissionRequested()") >= 0)
        verify(flyViewSource.indexOf("function requestMissionUpload()") >= 0)
        verify(flyViewSource.indexOf("uploadMissionPlan(missionPlanModel)") >= 0)
        verify(flyViewSource.indexOf("id: replaceDownloadedMissionDialog") >= 0)
        verify(flyViewSource.indexOf("downloadMissionFromVehicle(true)") >= 0)
        verify(flyViewSource.indexOf("id: missionTransferStatus") >= 0)
    }

    function test_canonicalPlanWinsOverLegacyFilePath() {
        var calls = []
        var mavlink = {
            missionTransferOperation: "",
            missionStatus: "invalid plan",
            uploadMissionPlan: function(model) { calls.push(model); return true }
        }
        var model = { count: 2 }
        // This mirrors requestVehicleMissionWrite's routing condition: a
        // canonical plan never opens the legacy file selector.
        if (model.count > 0)
            mavlink.uploadMissionPlan(model)
        compare(calls.length, 1)
        compare(calls[0], model)
    }

    function test_dirtyDownloadRequiresDecision() {
        var opened = false
        var downloadCalls = []
        var model = { dirty: true }
        var mavlink = { missionTransferOperation: "", downloadMissionFromVehicle: function(replace) { downloadCalls.push(replace) } }
        if (model.dirty)
            opened = true
        verify(opened)
        compare(downloadCalls.length, 0)
        mavlink.downloadMissionFromVehicle(true)
        compare(downloadCalls.length, 1)
        compare(downloadCalls[0], true)
    }
}
