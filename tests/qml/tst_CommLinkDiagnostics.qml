import QtQuick
import QtTest
import "../../ui/qml/accessories"
import "../../ui/qml/panels" as Panels

TestCase {
    id: test
    name: "CommLinkDiagnostics"
    width: 900; height: 700
    visible: true
    when: windowShown
    Component { id: toolbarComponent; Panels.TopPanel { width: 1600; height: 72 } }
    Component { id: rowComponent; CommLinkDiagnostics { width: 400 } }
    property var udp: null
    property var tcp: null
    property var serial: null
    SignalSpy { id: selections; signalName: "connectionRequested" }
    SignalSpy { id: cancellations; signalName: "cancelRequested" }
    QtObject {
        id: transport
        property bool connected: true
        property int connectionState: 2
        property var serialPorts: []
        property var linkQualityDiagnostics: ({qualityEstimate:80,confidence:"limited: sequence only",availableMetrics:["sequence continuity estimate"]})
        signal connectionStatusChanged(bool connected)
        signal connectionLifecycleChanged()
        signal commandPrerequisiteRejected(string operation, string reason, var context)
        function transportDiagnosticSnapshot() { return {endpoint:"TCP:127.0.0.1:5762",quality:80,rssi:-1,latencyMs:-1} }
        function setTelemetryLoggingEnabled(enabled) {}
        function setFlightControllerLoggingEnabled(enabled) {}
        function setTelemetryLogDirectory(path) {}
        function setFlightControllerLogDirectory(path) {}
    }
    function init() {
        udp = createTemporaryObject(rowComponent, test, {linkRecord:{name:"UDP",protocol:"UDP",address:"127.0.0.1:14550"},endpoint:"UDP:127.0.0.1:14550",connectionActive:true})
        tcp = createTemporaryObject(rowComponent, test, {x:450,linkRecord:{name:"TCP",protocol:"TCP",address:"127.0.0.1:5760"},endpoint:"UDP:127.0.0.1:14550"})
        serial = createTemporaryObject(rowComponent, test, {x:450,y:300,linkRecord:{name:"Serial",protocol:"Serial",address:"COM5",baudRate:57600},endpoint:"UDP:127.0.0.1:14550"})
        waitForPolish(udp)
        selections.target = udp; selections.clear()
        cancellations.target = udp; cancellations.clear()
    }
    function test_independentExpansionDoesNotSelect() {
        mouseClick(findChild(udp, "expandDiagnostics"))
        verify(udp.expanded); verify(!tcp.expanded); verify(!serial.expanded)
        compare(selections.count, 0); compare(cancellations.count, 0)
        mouseClick(findChild(udp, "expandDiagnostics"))
        verify(!udp.expanded); compare(selections.count, 0)
        verify(udp.connectionActive)
    }
    function test_existingSelectionAction() {
        mouseClick(findChild(udp, "linkSelection"))
        compare(selections.count, 1); verify(!udp.expanded)
    }
    function test_unavailable() {
        compare(udp.value("rssi", ""), "N/A")
        compare(tcp.value("received", ""), "N/A")
        compare(serial.value("latencyMs", " ms"), "N/A")
    }
    function test_liveUpdatesAndAttribution() {
        udp.diagnostics = {received:12,rssi:200,latencyMs:28,radioState:"Available"}
        tcp.diagnostics = udp.diagnostics; serial.diagnostics = udp.diagnostics
        compare(udp.value("received", ""), "12")
        compare(udp.value("latencyMs", " ms"), "28 ms")
        compare(tcp.value("received", ""), "N/A")
        compare(serial.value("rssi", ""), "N/A")
        udp.diagnostics = {received:13,rssi:-1,radioState:"Stale"}
        compare(udp.value("received", ""), "13")
        compare(udp.value("rssi", ""), "N/A")
        compare(udp.linkData.radioState, "Stale")
    }
    function test_transportReplacement() {
        udp.diagnostics = {received:15}; udp.expanded = true
        udp.endpoint = "TCP:127.0.0.1:5760"
        compare(udp.value("received", ""), "N/A")
        tcp.endpoint = "TCP:127.0.0.1:5760"; tcp.diagnostics = {received:2}
        compare(tcp.value("received", ""), "2")
        verify(!tcp.expanded)
        udp.endpoint = ""; udp.diagnostics = ({})
        compare(udp.value("received", ""), "N/A")
    }
    function test_serialIdentityIncludesBaud() {
        serial.endpoint = "SERIAL:COM5:57600"; serial.diagnostics = {received:4}
        verify(serial.attributed); compare(serial.value("received", ""), "4")
        serial.endpoint = "SERIAL:COM5:115200"
        verify(!serial.attributed)
    }
    function test_replacedRecordResetsExpansion() {
        udp.expanded = true
        udp.linkRecord = {name:"New",protocol:"TCP",address:"localhost:5760"}
        verify(!udp.expanded); verify(!udp.attributed)
    }
    function test_destructionAndRecreation() {
        var row = rowComponent.createObject(test, {expanded:true,linkRecord:{protocol:"UDP",address:"a:1"}})
        row.destroy()
        var replacement = createTemporaryObject(rowComponent, test, {linkRecord:{protocol:"UDP",address:"a:1"}})
        verify(!replacement.expanded); compare(replacement.value("received", ""), "N/A")
    }
    function test_realToolbarDropdown() {
        var toolbar = createTemporaryObject(toolbarComponent, test)
        verify(toolbar !== null)
        toolbar.commLinks = [{name:"UDP",protocol:"UDP",address:"127.0.0.1:14550",connected:false},
            {name:"Serial",protocol:"Serial",address:"COM5",baudRate:57600,connected:false}]
        var popup = findChild(toolbar, "commLinksPopover")
        verify(popup !== null)
        popup.open()
        tryCompare(popup, "opened", true)
        var firstRow = findChild(popup.contentItem, "commLinkDiagnosticsRow")
        verify(firstRow !== null)
        verify(!findChild(firstRow, "expandDiagnostics").enabled)
        firstRow.toggleDiagnostics()
        verify(!firstRow.expanded)
        compare(popup.width, 374)
        compare(firstRow.value("received", ""), "N/A")
        compare(toolbar.width, 1600); compare(toolbar.height, 72)
        var tile = findChild(toolbar, "commLinkStatusTile")
        verify(tile !== null); compare(tile.width, 112)
        popup.close()
        tryCompare(popup, "opened", false)
        compare(toolbar.commLinks[0].connected, false)
    }
    function test_referenceLayoutAndMetrics() {
        udp.width = 354
        udp.linkRecord = {name:"Simulation",protocol:"TCP",address:"127.0.0.1:5762"}
        udp.endpoint = "TCP:127.0.0.1:5762"
        udp.diagnostics = {quality:80, rssi:-1, latencyMs:-1}
        udp.qualityDiagnostics = {qualityEstimate:80,confidence:"limited: sequence only",availableMetrics:["sequence continuity estimate"]}
        udp.toggleDiagnostics()
        compare(udp.height,180)
        compare(udp.sequenceText,"Limited"); compare(udp.continuityText,"Fresh")
        compare(udp.qualityScore,80)
        compare(findChild(udp,"linkConnectionIndicator").color,udp.goodColor)
        compare(findChild(udp,"linkSelection").text,"DISCONNECT")
        compare(findChild(tcp,"linkSelection").text,"CONNECT")
        udp.connectionActive = false
        compare(udp.height,54); verify(!udp.expanded)
        verify(!findChild(udp,"expandDiagnostics").enabled)
    }
    function test_connectingCanCancelButCannotExpand() {
        udp.connectionActive = false
        udp.connecting = true
        verify(!findChild(udp,"linkSelection").enabled)
        verify(!findChild(udp,"expandDiagnostics").enabled)
        var cancel = findChild(udp,"cancelLink")
        verify(cancel.visible)
        mouseClick(cancel)
        compare(cancellations.count,1)
        compare(selections.count,0)
    }
    function test_qualityCannotLeakAcrossGenerations() {
        udp.diagnostics = {connectionGeneration:2,quality:70}
        udp.qualityDiagnostics = {connectionGeneration:1,qualityEstimate:95,confidence:"limited: sequence only"}
        compare(udp.qualityScore,70)
        compare(udp.sequenceText,"N/A")
        udp.qualityDiagnostics = {connectionGeneration:2,qualityEstimate:72,confidence:"limited: sequence only"}
        compare(udp.qualityScore,72)
        compare(udp.sequenceText,"Limited")
    }
    function test_allTopDropdownsShareFrameAndTheme() {
        var toolbar = createTemporaryObject(toolbarComponent,test)
        toolbar.cameraLinks = [{name:"Main Camera",protocol:"RTSP",address:"rtsp://127.0.0.1/live",connected:false}]
        var ids = ["commLinksPopover","cameraLinksPopover","gpsDetailsPopup","batteryDetailsPopup"]
        var titles = ["Communication Links","Camera Links","GPS Details","Battery"]
        for (var i = 0; i < ids.length; ++i) {
            var popup = findChild(toolbar,ids[i])
            verify(popup !== null)
            popup.open(); tryCompare(popup,"opened",true)
            compare(popup.width,374); compare(popup.background.radius,10)
            compare(popup.background.border.color,toolbar.accentColor)
            compare(findChild(popup.contentItem,"topDropdownHeader").title,titles[i])
            verify(popup.height > 60)
            toolbar.darkTheme = false; wait(10)
            compare(popup.background.color,"#f9fcff")
            compare(popup.background.border.color,toolbar.accentColor)
            toolbar.darkTheme = true
            popup.close(); tryCompare(popup,"opened",false)
        }
        compare(toolbar.cameraLinks.length,1)
        compare(toolbar.cameraLinks[0].connected,false)
    }
    function test_disconnectedArrowIsDisabled() {
        verify(!findChild(tcp, "expandDiagnostics").enabled)
        tcp.toggleDiagnostics()
        verify(!tcp.expanded)
        udp.connectionActive = false
        verify(!findChild(udp, "expandDiagnostics").enabled)
        udp.toggleDiagnostics()
        verify(!udp.expanded)
    }
    function test_connectedDropdownVisualAndTheme() {
        var toolbar = createTemporaryObject(toolbarComponent,test,{mavlinkObject:transport})
        toolbar.commLinks = [{name:"Simulation",protocol:"TCP",address:"127.0.0.1:5762",connected:true},
            {name:"FC",protocol:"Serial",address:"COM5",baudRate:115200,connected:false}]
        var popup = findChild(toolbar,"commLinksPopover")
        popup.open(); tryCompare(popup,"opened",true)
        var row = findChild(popup.contentItem,"commLinkDiagnosticsRow")
        verify(row.canExpand)
        mouseClick(findChild(row,"expandDiagnostics"))
        verify(row.expanded); compare(row.qualityScore,80)
        wait(30)
        grabImage(test).save(decodeURIComponent(Qt.resolvedUrl("../../build/links-dropdown-dark.png").toString().replace(/^file:\/\/\//,"")))
        toolbar.darkTheme = false; wait(30)
        verify(row.expanded)
        grabImage(test).save(decodeURIComponent(Qt.resolvedUrl("../../build/links-dropdown-light.png").toString().replace(/^file:\/\/\//,"")))
        popup.close()
    }
    function test_disconnectCollapsesRssi() {
        udp.toggleDiagnostics()
        verify(udp.expanded)
        udp.connectionActive = false
        verify(!udp.expanded)
        verify(!findChild(udp, "rssiDetails").visible)
        udp.connectionActive = true
        verify(!udp.expanded)
    }}
