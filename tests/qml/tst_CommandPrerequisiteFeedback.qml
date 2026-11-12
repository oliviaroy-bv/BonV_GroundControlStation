import QtQuick
import QtTest

TestCase {
    id: test
    name: "CommandPrerequisiteFeedback"
    property string source: ""
    property var subject: null
    QtObject {
        id: first
        signal commandPrerequisiteRejected(string operation, string reason, var context)
        signal connectionLifecycleChanged()
    }
    QtObject {
        id: second
        signal commandPrerequisiteRejected(string operation, string reason, var context)
        signal connectionLifecycleChanged()
    }
    SignalSpy { id: messages; target: test.subject; signalName: "statusMessageRequested" }
    function extractFunction(name) {
        var start = source.indexOf("function " + name + "(")
        verify(start >= 0)
        var body = source.indexOf("{", start), depth = 1, end = body + 1
        while (end < source.length && depth) {
            if (source[end] === "{") depth++
            if (source[end] === "}") depth--
            end++
        }
        return source.substring(start, end)
    }
    function initTestCase() {
        var request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../ui/qml/panels/TopPanel.qml"), false)
        request.send(); source = request.responseText; verify(source.length > 0)
    }
    function createSubject() {
        return Qt.createQmlObject('import QtQuick; Item { id: root; property var mavlinkObject: null; '
            + 'property string lastCommandRejection: ""; signal statusMessageRequested(string message); '
            + 'onMavlinkObjectChanged: lastCommandRejection = ""; Connections { target: root.mavlinkObject; ignoreUnknownSignals: true; '
            + extractFunction("onConnectionLifecycleChanged") + '\n'
            + extractFunction("onCommandPrerequisiteRejected") + ' } }', test)
    }
    function init() { subject = createSubject(); subject.mavlinkObject = first; messages.clear() }
    function cleanup() { subject.destroy(); subject = null }
    function test_reasonIsVisibleAndIdenticalMessagesAreDeduplicated() {
        first.commandPrerequisiteRejected("motorTestStart", "Parameters are loading.", {accepted: false})
        compare(messages.count, 1); compare(messages.signalArguments[0][0], "Parameters are loading.")
        first.commandPrerequisiteRejected("motorTestStart", "Parameters are loading.", {accepted: false})
        compare(messages.count, 1)
        first.commandPrerequisiteRejected("motorTestStart", "Required parameter FRAME_CLASS is missing.", {accepted: false})
        compare(messages.count, 2)
    }
    function test_replacementDisconnectsOldSignals() {
        subject.mavlinkObject = second
        first.commandPrerequisiteRejected("motorTestStart", "Old vehicle", {})
        compare(messages.count, 0)
        second.commandPrerequisiteRejected("motorTestStart", "New vehicle", {})
        compare(messages.count, 1)
    }
    function test_lifecycleResetsDeduplication() {
        first.commandPrerequisiteRejected("parameterRead", "Vehicle is not connected.", {})
        first.connectionLifecycleChanged()
        first.commandPrerequisiteRejected("parameterRead", "Vehicle is not connected.", {})
        compare(messages.count, 2)
    }
    function test_recreationDoesNotDuplicateNotifications() {
        subject.destroy(); subject = createSubject(); subject.mavlinkObject = first; messages.clear()
        first.commandPrerequisiteRejected("parameterRefresh", "Vehicle is not connected.", {})
        compare(messages.count, 1)
    }
}
