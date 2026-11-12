import QtQuick
import QtTest

TestCase {
    name: "ManualMissionPresentation"

    property string flyViewSource: ""
    property string mapSource: ""

    function read(path) {
        var request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl(path), false)
        request.send()
        return request.responseText.replace(/\r\n/g, "\n")
    }

    function initTestCase() {
        flyViewSource = read("../../ui/qml/core/FlyView.qml")
        mapSource = read("../../integrations/map_provider/map/map.html")
    }

    function test_missionMarkersUsePinsAndOperatorNumbering() {
        verify(mapSource.indexOf("mission-marker-pin") >= 0)
        verify(mapSource.indexOf("createElementNS('http://www.w3.org/2000/svg', 'svg')") >= 0)
        verify(mapSource.indexOf("M16 1C7.9 1") >= 0)
        verify(mapSource.indexOf("anchor: 'bottom'") >= 0)
        verify(mapSource.indexOf("offset: [0, 0]") >= 0)
        verify(mapSource.indexOf("point.displaySequence") >= 0)
    }

    function test_manualModeHidesCommandPanelAndUsesBottomList() {
        verify(flyViewSource.indexOf("property bool manualTelemetryPanelExplicitlyShown") >= 0)
        verify(flyViewSource.indexOf("property bool manualCommandPanelExplicitlyShown") >= 0)
        verify(flyViewSource.indexOf("anchors.bottomMargin: root.commandPanelDisplayed ? 0 : -height") >= 0)
        verify(flyViewSource.indexOf("id: manualMissionListPanel") >= 0)
        verify(flyViewSource.indexOf("bottomBar.height + 10") >= 0)
        verify(flyViewSource.indexOf("anchors.bottom: parent.bottom") >= 0)
        verify(flyViewSource.indexOf("id: manualWaypointList") >= 0)
        verify(flyViewSource.indexOf("model: missionPlanModel") >= 0)
    }

    function test_listKeepsRequiredFieldsAndIconActions() {
        verify(flyViewSource.indexOf("MISSION WAYPOINTS (") >= 0)
        verify(flyViewSource.indexOf("id: commandCombo") >= 0)
        verify(flyViewSource.indexOf("id: frameCombo") >= 0)
        verify(flyViewSource.indexOf("missionCommandOptions") >= 0)
        verify(flyViewSource.indexOf("missionFrameOptions") >= 0)
        verify(flyViewSource.indexOf("ToolTip.text: \"Move waypoint up\"") >= 0)
        verify(flyViewSource.indexOf("ToolTip.text: \"Move waypoint down\"") >= 0)
        verify(flyViewSource.indexOf("ToolTip.text: \"Delete waypoint\"") >= 0)
    }

    function test_transferContextMenuIsNotGatedByManualMode() {
        var menuStart = flyViewSource.indexOf("id: mapMissionMenu")
        verify(menuStart >= 0)
        var menuEnd = flyViewSource.indexOf("component MissionMenuItem", menuStart)
        var menu = flyViewSource.substring(menuStart, menuEnd)
        verify(menu.indexOf("root.planningMode") < 0)
        verify(menu.indexOf("text: \"Read\"") >= 0)
        verify(menu.indexOf("text: \"Write\"") >= 0)
        verify(menu.indexOf("text: \"Clear\"") >= 0)
    }

    function test_homeIsFilteredAndClearWaitsForVehicleAcknowledgement() {
        verify(flyViewSource.indexOf("function isMissionHomeItem") >= 0)
        verify(flyViewSource.indexOf("function clearMissionAfterVehicleClear") >= 0)
        verify(flyViewSource.indexOf("transferId === root.pendingVehicleMissionClearId") >= 0)
    }
}
