pragma ComponentBehavior: Bound
import QtQuick
import QtTest
import "../../ui/qml/accessories" as Accessories
import "../../ui/qml/panels" as Panels

TestCase {
    id: test
    name: "ParameterReadiness"
    width: 900; height: 700
    when: windowShown
    visible: true
    property var subject: null
    QtObject {
        id: manager
        property bool parametersReady: false
        property bool hasCompleteSnapshot: false
        property int syncState: 0
        property int progress: 0
        property int receivedParameterCount: 0
        property int expectedParameterCount: 0
        property string syncError: ""
    }
    QtObject {
        id: transport
        property bool connected: false
        property int connectionState: 0
        property int refreshes: 0
        property int starts: 0
        property int stops: 0
        property var motorTestPrerequisites: ({available: false, reason: "Parameters are loading.", motorCount: 0})
        signal motorTestCommandRejected(string mode, int firstMotor, int motorCount, string reason)
        signal connectionStatusChanged(bool connected)
        function requestParameters() { refreshes++ }
        function testMotor(motor, throttle, duration) { starts++ }
        function runAllMotorTest(throttle, duration, count) { starts++ }
        function stopAllMotorTests(count) { stops++ }
        function parameterValue(name) { return {value: 1} }
    }
    QtObject {
        id: replacement
        property bool connected: true
        property var motorTestPrerequisites: ({available: true, reason: "", motorCount: 4})
        signal motorTestCommandRejected(string mode, int firstMotor, int motorCount, string reason)
        function parameterValue(name) { return {value: 1} }
    }
    QtObject {
        id: vehicle
        property var mavlink: transport
        property var parameterManager: manager
        property int motorCount: 4
        property string vehicleType: "Quadrotor"
    }
    Component { id: statusComponent; Accessories.ParameterSyncStatus { width: 400; vehicle: test.testVehicle } }
    // Context property keeps the production component's vehicle binding intact.
    property var testVehicle: vehicle
    Component { id: motorComponent; Panels.MotorTestPanel { width: 900; height: 700; vehicle: test.testVehicle; mavlinkObject: transport; opened: true; propellersRemoved: true } }
    Component {
        id: mapStatusComponent
        Accessories.ParameterStatusButton { x: 500; y: 12; vehicle: test.testVehicle; mavlinkObject: transport }
    }
    function init() {
        transport.connected = false; transport.connectionState = 0
        transport.refreshes = 0; transport.starts = 0; transport.stops = 0
        transport.motorTestPrerequisites = {available: false, reason: "Parameters are loading.", motorCount: 0}
        manager.parametersReady = false; manager.hasCompleteSnapshot = false
        manager.syncState = 0; manager.progress = 0; manager.receivedParameterCount = 0; manager.expectedParameterCount = 0; manager.syncError = ""
    }
    function cleanup() { if (subject) subject.destroy(); subject = null }
    function test_status_data() {
        return [
            {tag: "disconnected", connected: false, connection: 0, state: 0, ready: false, text: "Parameters unavailable"},
            {tag: "connecting", connected: false, connection: 1, state: 0, ready: false, text: "Parameters unavailable"},
            {tag: "loading", connected: true, connection: 2, state: 1, ready: false, text: "Loading parameters - 42%"},
            {tag: "recovering", connected: true, connection: 2, state: 2, ready: false, text: "Recovering parameters - 42%"},
            {tag: "ready", connected: true, connection: 2, state: 3, ready: true, text: "Parameters ready"},
            {tag: "failed", connected: true, connection: 2, state: 4, ready: false, text: "Parameter synchronization failed"}
        ]
    }
    function test_status(data) {
        transport.connected = data.connected; transport.connectionState = data.connection
        manager.syncState = data.state; manager.parametersReady = data.ready; manager.progress = 42
        manager.receivedParameterCount = 42; manager.expectedParameterCount = 100; manager.syncError = "Missing index retry exhausted"
        subject = createTemporaryObject(statusComponent, test)
        compare(subject.statusText, data.text)
        if (data.connection === 1) verify(subject.connectionText.indexOf("awaiting heartbeat") >= 0)
        if (data.state === 2) verify(subject.countText.indexOf("58 missing") >= 0)
        if (data.state === 4) verify(subject.detailText.indexOf(manager.syncError) >= 0)
        wait(0)
        grabImage(subject).save(decodeURIComponent(Qt.resolvedUrl("../../build/readiness-status-" + data.tag + ".png").toString().replace(/^file:\/\/\//, "")))
    }
    function test_progressAndRetainedSnapshot() {
        transport.connected = true; manager.syncState = 1; manager.progress = 100
        subject = createTemporaryObject(statusComponent, test)
        compare(subject.progressValue, 99)
        manager.parametersReady = true; manager.syncState = 3; manager.hasCompleteSnapshot = true
        compare(subject.progressValue, 100); verify(subject.ready)
        transport.connected = false
        verify(!subject.ready); verify(subject.statusText.indexOf("stale") >= 0)
        manager.parametersReady = false; manager.syncState = 1; transport.connected = true
        verify(!subject.ready); verify(subject.retained); verify(subject.detailText.indexOf("stale") >= 0)
    }
    function test_retry() {
        transport.connected = true; manager.syncState = 4
        subject = createTemporaryObject(statusComponent, test)
        var retry = findChild(subject, "parameterRetry")
        verify(retry.visible); retry.clicked(); compare(transport.refreshes, 1)
    }
    function test_motor_data() { return [{tag:"Loading", state:1}, {tag:"Recovering", state:2}, {tag:"Failed", state:4}] }
    function test_motor(data) {
        transport.connected = true; manager.syncState = data.state
        subject = createTemporaryObject(motorComponent, test)
        verify(subject !== null)
        var start = findChild(subject, "motorStartAll"), stop = findChild(subject, "motorStopAll")
        verify(!start.enabled); verify(stop.enabled)
        if (data.state === 1) {
            wait(0)
            grabImage(subject).save(decodeURIComponent(Qt.resolvedUrl("../../build/readiness-motor-loading.png").toString().replace(/^file:\/\/\//, "")))
        }
        subject.runAll(); compare(transport.starts, 0)
        stop.clicked(); compare(transport.stops, 1)
        transport.motorTestPrerequisites = {available: true, reason: "", motorCount: 6}
        verify(start.enabled); compare(subject.motorCount, 6)
        subject.runAll(); compare(transport.starts, 1)
        transport.motorTestCommandRejected("all", 1, 6, "Unsupported configuration")
        verify(subject.motorTestStatus.indexOf("Unsupported configuration") >= 0)
        transport.connected = false; transport.connectionStatusChanged(false)
        verify(!stop.enabled)
    }
    function test_oldVehicleSignalsAndRecreation() {
        subject = createTemporaryObject(motorComponent, test)
        subject.mavlinkObject = replacement
        var before = subject.motorTestStatus
        transport.motorTestCommandRejected("single", 1, 1, "Old vehicle")
        compare(subject.motorTestStatus, before)
        replacement.motorTestCommandRejected("single", 1, 1, "New vehicle")
        verify(subject.motorTestStatus.indexOf("New vehicle") >= 0)
        subject.destroy(); subject = createTemporaryObject(motorComponent, test)
        transport.motorTestCommandRejected("single", 1, 1, "Current vehicle")
        verify(subject.motorTestStatus.indexOf("Current vehicle") >= 0)
    }
    function test_backendAvailabilityIsAuthoritative() {
        transport.connected = true; manager.parametersReady = true; manager.syncState = 3
        transport.motorTestPrerequisites = {available: false, reason: "Unsupported FRAME_CLASS value.", motorCount: 0}
        subject = createTemporaryObject(motorComponent, test)
        verify(!subject.controlsEnabled())
        compare(subject.startUnavailableReason, "Unsupported FRAME_CLASS value.")
        verify(findChild(subject, "motorStopAll").enabled)
    }
    function test_oldStatusSourceCannotRestoreReady() {
        transport.connected = true; manager.parametersReady = true; manager.syncState = 3
        subject = createTemporaryObject(statusComponent, test)
        verify(subject.ready)
        subject.vehicle = null
        verify(!subject.ready)
        manager.parametersReady = false; manager.parametersReady = true
        verify(!subject.ready)
    }

    function test_recreateWhileLoadingAndRapidStateChanges() {
        transport.connected = true; manager.syncState = 1; manager.progress = 25
        manager.receivedParameterCount = 1; manager.expectedParameterCount = 4
        subject = createTemporaryObject(statusComponent, test)
        compare(subject.progressValue, 25); verify(!subject.ready)
        subject.destroy(); subject = createTemporaryObject(statusComponent, test)
        compare(subject.progressValue, 25); verify(!subject.ready)
        manager.syncState = 2; manager.progress = 50
        verify(subject.statusText.indexOf("Recovering") >= 0)
        manager.syncState = 4; manager.syncError = "Retry exhausted"
        verify(subject.detailText.indexOf("Retry exhausted") >= 0)
        manager.hasCompleteSnapshot = true; transport.connected = false
        verify(subject.statusText.indexOf("stale") >= 0)
        manager.syncState = 1; manager.progress = 0; transport.connected = true
        verify(!subject.ready); compare(subject.progressValue, 0)
    }

    function test_mapStatusButton_data() {
        return [{tag:"Loading", state:1}, {tag:"Recovering", state:2}, {tag:"Ready", state:3}, {tag:"Failed", state:4}]
    }
    function test_mapStatusButton(data) {
        transport.connected = true; manager.syncState = data.state
        manager.parametersReady = data.state === 3; manager.progress = data.state === 3 ? 100 : 60
        manager.receivedParameterCount = data.state === 3 ? 10 : 6; manager.expectedParameterCount = 10
        manager.syncError = "Missing parameter retry exhausted"
        subject = createTemporaryObject(mapStatusComponent, test)
        compare(subject.width, 32); compare(subject.height, 32)
        verify(subject.statusText.indexOf(data.state === 3 ? "10 / 10" : "6 / 10") >= 0)
        compare(subject.parametersReady, data.state === 3)
        if (data.state === 3) compare(String(subject.statusColor), "#70b997")
        subject.clicked()
        var popup = findChild(subject, "parameterStatusPopup")
        tryCompare(popup, "opened", true)
        if (data.state === 4) {
            verify(subject.statusText.indexOf(manager.syncError) >= 0)
            findChild(popup, "parameterRetry").clicked()
            compare(transport.refreshes, 1)
        }
        popup.close()
        transport.connected = false; manager.parametersReady = false; manager.hasCompleteSnapshot = true
        verify(!subject.parametersReady)
        verify(subject.statusText.indexOf("stale") >= 0)
    }

}
