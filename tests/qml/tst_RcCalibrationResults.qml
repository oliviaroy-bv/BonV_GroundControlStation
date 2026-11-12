import QtQuick
import QtTest

TestCase {
    id: test
    name: "RcCalibrationResults"
    property string source: ""
    property var subject: null
    QtObject {
        id: transport
        property bool connected: true
        property bool accept: true
        property int requests: 0
        property int connectionGeneration: 1
        function requestRcCalibrationWrite(rows) {
            requests++
            return {id: requests, accepted: accept, reason: accept ? "" : "Snapshot is not current"}
        }
    }
    function extractFunction(name) {
        var start = source.indexOf("function " + name + "(")
        verify(start >= 0, "Missing production function: " + name)
        var body = source.indexOf("{", start)
        var depth = 1
        var end = body + 1
        while (end < source.length && depth > 0) {
            if (source[end] === "{") depth++
            if (source[end] === "}") depth--
            end++
        }
        compare(depth, 0)
        return source.substring(start, end)
    }
    function initTestCase() {
        var request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../ui/qml/settings/OemSettingsPage.qml"), false)
        request.send()
        source = request.responseText
        verify(source.length > 0)
    }
    function init() {
        transport.requests = 0
        transport.accept = true
        transport.connected = true
        // Execute the actual production handlers without constructing the unrelated
        // settings layout and its joystick/backend dependencies.
        subject = Qt.createQmlObject('import QtQml; QtObject { id: root; property var transport; '
            + 'property var rcCalibrationWriteId: 0; property string rcCalibrationStatus: "Ready"; '
            + 'property string rcCalibrationWriteState: "Not submitted"; property var rcCalibrationWriteResults: []; property var rcCalibrationWriteGeneration: 0; '
            + 'property bool rcCalibrationActive: false; property int rcCalibrationStep: 1; '
            + 'function setRcCalibrationRows(rows) {} function makeRcCalibrationRows(current) { return [] } '
            + 'function mavlinkObject() { return transport } function rcCalibrationRows() { return [] } '
            + extractFunction("beginRcCalibration") + '\n'
            + extractFunction("saveRcCalibration") + '\n'
            + extractFunction("onConnectionLifecycleChanged") + '\n'
            + extractFunction("resetRcCalibrationForVehicleChange") + '\n'
            + extractFunction("finishRcCalibrationWrite") + '\n'
            + extractFunction("onRcCalibrationWriteFinished") + ' }', test)
        subject.transport = transport
    }
    function cleanup() { subject.destroy(); subject = null }
    function test_acceptanceIsNotConfirmation() {
        subject.saveRcCalibration()
        compare(transport.requests, 1)
        verify(subject.rcCalibrationStatus.indexOf("awaiting vehicle confirmations") >= 0)
        subject.saveRcCalibration()
        compare(transport.requests, 1)
        subject.onRcCalibrationWriteFinished({id: 99, success: true, confirmedCount: 4, totalCount: 4})
        compare(subject.rcCalibrationWriteId, 1)
        subject.onRcCalibrationWriteFinished({id: 1, success: true, confirmedCount: 4, totalCount: 4})
        compare(subject.rcCalibrationWriteId, 0)
        verify(subject.rcCalibrationStatus.indexOf("confirmed by vehicle (4/4)") >= 0)
    }
    function test_partialFailureNamesParameter() {
        subject.saveRcCalibration()
        subject.onRcCalibrationWriteFinished({id: 1, success: false, status: "PartialFailure", reason: "RC1_MAX: Rejected"})
        verify(subject.rcCalibrationStatus.indexOf("RC1_MAX: Rejected") >= 0)
        compare(subject.rcCalibrationWriteId, 0)
    }
    function test_cancellationIsExplicit() {
        subject.saveRcCalibration()
        subject.onRcCalibrationWriteFinished({id: 1, success: false, status: "Cancelled", reason: "Vehicle disconnected"})
        verify(subject.rcCalibrationStatus.indexOf("Cancelled") >= 0)
        compare(subject.rcCalibrationWriteId, 0)
    }
    function test_rejectedRequestIsNotSuccess() {
        transport.accept = false
        subject.saveRcCalibration()
        verify(subject.rcCalibrationStatus.indexOf("request rejected") >= 0)
        subject.onRcCalibrationWriteFinished({id: 1, success: false, status: "InvalidRequest", reason: "Snapshot is not current"})
        compare(subject.rcCalibrationWriteId, 0)
    }
    function test_timeoutAndIndividualResults() {
        subject.saveRcCalibration()
        compare(subject.rcCalibrationWriteState, "Pending")
        subject.onRcCalibrationWriteFinished({id: 1, success: false, status: "PartialFailure", confirmedCount: 0, totalCount: 1,
            reason: "RC1_MIN: TimedOut", results: [{parameter: "RC1_MIN", outcomeName: "TimedOut", reason: "No response"}]})
        compare(subject.rcCalibrationWriteState, "Timed out")
        compare(subject.rcCalibrationWriteResults[0].parameter, "RC1_MIN")
    }
    function test_partialConfirmationRetainedAndDuplicateIgnored() {
        subject.saveRcCalibration()
        subject.onRcCalibrationWriteFinished({id: 1, success: false, status: "PartialFailure", confirmedCount: 1, totalCount: 2,
            reason: "RC1_MAX: Rejected", results: [{parameter:"RC1_MIN", outcomeName:"Confirmed"}, {parameter:"RC1_MAX", outcomeName:"Rejected"}]})
        compare(subject.rcCalibrationWriteState, "Partial failure")
        compare(subject.rcCalibrationWriteResults.length, 2)
        compare(subject.rcCalibrationWriteResults[0].outcomeName, "Confirmed")
        var before = subject.rcCalibrationStatus
        subject.onRcCalibrationWriteFinished({id: 1, success: true, confirmedCount: 2, totalCount: 2})
        compare(subject.rcCalibrationStatus, before)
    }
    function test_rejectedOutcomeAndCompleteSuccess() {
        subject.saveRcCalibration()
        subject.onRcCalibrationWriteFinished({id: 1, success: false, status: "PartialFailure", confirmedCount: 0, results: [{parameter:"RC1_MIN", outcomeName:"Rejected"}], reason:"RC1_MIN: Rejected"})
        compare(subject.rcCalibrationWriteState, "Rejected")
        subject.saveRcCalibration()
        compare(subject.rcCalibrationWriteState, "Pending")
        subject.onRcCalibrationWriteFinished({id: 2, success: true, status:"Confirmed", confirmedCount: 4, totalCount: 4})
        compare(subject.rcCalibrationWriteState, "Confirmed")
        verify(subject.rcCalibrationStatus.indexOf("Complete success") >= 0)
    }

    function test_disconnectCancelsPendingAndIgnoresOldResult() {
        subject.saveRcCalibration()
        transport.connected = false; transport.connectionGeneration++
        subject.onConnectionLifecycleChanged()
        compare(subject.rcCalibrationWriteState, "Cancelled")
        compare(subject.rcCalibrationWriteId, 0)
        subject.onRcCalibrationWriteFinished({id: 1, success: true, confirmedCount: 4, totalCount: 4})
        compare(subject.rcCalibrationWriteState, "Cancelled")
    }
    function test_vehicleChangeCancelsAndClearsOldRows() {
        subject.saveRcCalibration()
        subject.resetRcCalibrationForVehicleChange()
        compare(subject.rcCalibrationWriteId, 0)
        compare(subject.rcCalibrationWriteState, "Cancelled")
        compare(subject.rcCalibrationWriteResults.length, 0)
        subject.onRcCalibrationWriteFinished({id: 1, success: true, confirmedCount: 4, totalCount: 4})
        compare(subject.rcCalibrationWriteState, "Cancelled")
    }

    function test_partialTimeoutRemainsPartialFailure() {
        subject.saveRcCalibration()
        subject.onRcCalibrationWriteFinished({id: 1, success: false, status: "PartialFailure", confirmedCount: 1, totalCount: 2,
            reason: "RC1_MAX: TimedOut", results: [{parameter:"RC1_MIN", outcomeName:"Confirmed"}, {parameter:"RC1_MAX", outcomeName:"TimedOut"}]})
        compare(subject.rcCalibrationWriteState, "Partial failure")
        compare(subject.rcCalibrationWriteResults[0].outcomeName, "Confirmed")
    }

    function test_newCalibrationIsNotAlreadyConfirmed() {
        subject.saveRcCalibration()
        subject.onRcCalibrationWriteFinished({id: 1, success:true, status:"Confirmed", confirmedCount:4, totalCount:4})
        compare(subject.rcCalibrationWriteState, "Confirmed")
        subject.beginRcCalibration()
        compare(subject.rcCalibrationWriteState, "Not submitted")
        compare(subject.rcCalibrationWriteResults.length, 0)
    }

}
