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
        compare(popup.width, 310)
        compare(firstRow.value("received", ""), "N/A")
        compare(toolbar.width, 1600); compare(toolbar.height, 72)
        var tile = findChild(toolbar, "commLinkStatusTile")
        verify(tile !== null); compare(tile.width, 112)
        popup.close()
        tryCompare(popup, "opened", false)
        compare(toolbar.commLinks[0].connected, false)
    }    function test_disconnectedArrowIsDisabled() {
        verify(!findChild(tcp, "expandDiagnostics").enabled)
        tcp.toggleDiagnostics()
        verify(!tcp.expanded)
        udp.connectionActive = false
        verify(!findChild(udp, "expandDiagnostics").enabled)
        udp.toggleDiagnostics()
        verify(!udp.expanded)
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
