import QtQuick
import QtTest

TestCase {
    id: test
    name: "CommLinkQuality"
    property string source: ""
    property var subject: null
    QtObject { id: transport; property bool connected: false; property int connectionState: 0 }
    QtObject {
        id: vehicle
        property int commLinkQuality: -1
        property string commLinkCategory: ""
        property var commLinkDiagnostics: ({qualityEstimate:-1, unknownReason:"Insufficient fresh sequence evidence",
            confidence:"unavailable", availableMetrics:[], staleMetrics:[], latencyMs:-1, rawRssi:-1})
    }
    function propertySource(name, nextName) {
        var start = source.indexOf("    readonly property " + name)
        var end = source.indexOf("    readonly property " + nextName, start)
        verify(start >= 0 && end > start)
        return source.substring(start, end)
    }
    function initTestCase() {
        var request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../ui/qml/panels/TopPanel.qml"), false)
        request.send(); source = request.responseText
        verify(source.length > 0)
    }
    function init() {
        transport.connected = false; transport.connectionState = 0
        vehicle.commLinkQuality = -1; vehicle.commLinkCategory = ""
        vehicle.commLinkDiagnostics = {qualityEstimate:-1,unknownReason:"Insufficient fresh sequence evidence",
            confidence:"unavailable",availableMetrics:[],staleMetrics:[],latencyMs:-1,rawRssi:-1}
        subject = Qt.createQmlObject('import QtQml; QtObject { id: root; property var vehicle; property var mavlinkObject; '
            + 'readonly property bool vehicleConnected: !!mavlinkObject && mavlinkObject.connected; '
            + propertySource("int commLinkQuality:", "string commStatusText:")
            + propertySource("string commStatusText:", "string commQualityDetails:")
            + propertySource("string commQualityDetails:", "bool cameraConnected:") + ' }', test)
        subject.vehicle = vehicle; subject.mavlinkObject = transport
    }
    function cleanup() { subject.destroy(); subject = null }
    function test_presentation_data() {
        return [{tag:"Disconnected", connected:false, state:0, score:90, category:"GOOD", expected:"NO LINK"},
            {tag:"Connecting",connected:false,state:1,score:-1,category:"",expected:"CONNECTING"},
            {tag:"Unknown",connected:true,state:2,score:-1,category:"",expected:"CONNECTED"},
            {tag:"Assessing",connected:true,state:2,score:100,category:"",expected:"CONNECTED"},
            {tag:"Good",connected:true,state:2,score:90,category:"GOOD",expected:"GOOD"},
            {tag:"Fair",connected:true,state:2,score:65,category:"FAIR",expected:"FAIR"},
            {tag:"Poor",connected:true,state:2,score:20,category:"POOR",expected:"POOR"}]
    }
    function test_presentation(data) {
        transport.connected = data.connected; transport.connectionState = data.state
        vehicle.commLinkQuality = data.score; vehicle.commLinkCategory = data.category
        compare(subject.commStatusText, data.expected)
    }
    function test_numericBoundaryCannotFlickerCategory() {
        transport.connected = true; vehicle.commLinkCategory = "FAIR"
        for (var value of [84,86,84,86,55,54,56]) {
            vehicle.commLinkQuality = value
            compare(subject.commStatusText, "FAIR")
        }
        vehicle.commLinkQuality = -1
        compare(subject.commStatusText, "CONNECTED")
        verify(transport.connected)
        vehicle.commLinkQuality = 100; vehicle.commLinkCategory = "GOOD"
        compare(subject.commStatusText, "GOOD")
        transport.connected = false
        compare(subject.commStatusText, "NO LINK")
    }
    function test_diagnosticsExplainUnknownAndRawRssi() {
        transport.connected = true
        verify(subject.commQualityDetails.indexOf("Insufficient fresh sequence") >= 0)
        vehicle.commLinkDiagnostics = {qualityEstimate:80,unknownReason:"",confidence:"limited: sequence only",
            availableMetrics:["sequence continuity estimate"],staleMetrics:["PING"],latencyMs:-1,rawRssi:200}
        verify(subject.commQualityDetails.indexOf("80/100") >= 0)
        verify(subject.commQualityDetails.indexOf("200 (uncalibrated)") >= 0)
        verify(subject.commQualityDetails.indexOf("dBm") < 0)
        verify(subject.commQualityDetails.indexOf("Stale/unavailable: PING") >= 0)
    }
    function test_emptyDiagnosticsDuringTeardown() {
        transport.connected = true
        vehicle.commLinkDiagnostics = {}
        compare(subject.commQualityDetails, "Connected - quality unavailable")
    }

}
