import QtQuick
import QtQuick.Controls
import QtTest
import "../../ui/qml/settings"

TestCase {
    id: test
    name: "MavlinkConsole"
    width: 900
    height: 600
    when: windowShown
    QtObject {
        id: backend
        property string target: "System 42 / Component 1"
        property string status: "Ready"
        property bool available: true
        property bool running: false
        property bool commandMode: false
        property bool armingAllowed: false
        property string modeLabel: "PX4 NSH Shell"
        property var history: []
        property int starts: 0
        property int stops: 0
        property string sent: ""
        signal changed()
        function start() { starts++; running = true }
        function stop() { stops++; running = false }
        function send(text) { sent = text; return running }
        function interrupt() { return running }
        function clear() { history = [] }
    }
    MavlinkConsole { id: panel; controller: backend }
    SettingsWindow {
        id: settings
        activeCategory: "general"
        onConsoleRequested: panel.open()
    }
    function cleanup() { panel.close(); settings.close() }
    function test_generalOpensConsoleAndReopens() {
        settings.show()
        tryVerify(function() { return settings.visible })
        var openButton = findChild(settings.contentItem, "openMavlinkConsoleButton")
        verify(openButton !== null)
        openButton.clicked()
        tryCompare(panel, "opened", true)
        compare(settings.visible, false)
        compare(backend.starts, 0)
        var input = findChild(panel.contentItem, "consoleCommandInput")
        input.text = "help"
        backend.history = [{text:"stream", time:"12:00:00", kind:"output"}]
        compare(input.text, "help")
        panel.submit()
        compare(input.text, "help")
        backend.start()
        panel.submit()
        compare(backend.sent, "help")
        compare(input.text, "")
        findChild(panel.contentItem, "closeConsoleButton").clicked()
        tryCompare(panel, "opened", false)
        verify(!backend.running)
        panel.open()
        tryCompare(panel, "opened", true)
        compare(backend.starts, 1)
        compare(backend.history.length, 1)
        panel.minimized = true
        compare(panel.height, 64)
        panel.minimized = false
        panel.panelWidth = 430
        panel.panelHeight = 230
        panel.panelX = 40
        panel.panelY = 40
        wait(20)
        var move = findChild(panel.contentItem, "consoleMoveHandle")
        var oldX = panel.x, oldY = panel.y
        mousePress(move, 30, 20)
        mouseMove(move, 10, 10, 30)
        mouseRelease(move, 10, 10)
        verify(panel.x < oldX)
        verify(panel.y < oldY)
        var corner = findChild(panel.contentItem, "consoleResize10")
        verify(corner !== null)
        var oldWidth = panel.width, oldHeight = panel.height
        mousePress(corner, 8, 8)
        mouseMove(corner, 48, 38, 30)
        mouseRelease(corner, 48, 38)
        verify(panel.width > oldWidth)
        verify(panel.height > oldHeight)
        var savedWidth = panel.width
        var savedX = panel.x
        input.text = "pending command"
        backend.running = true
        var stopsBefore = backend.stops
        panel.popOut()
        tryCompare(panel, "opened", false)
        compare(panel.detached, true)
        compare(backend.stops, stopsBefore)
        compare(input.text, "pending command")
        panel.dock()
        tryCompare(panel, "opened", true)
        compare(panel.detached, false)
        compare(panel.width, savedWidth)
        compare(panel.x, savedX)
        compare(input.text, "pending command")
        compare(backend.stops, stopsBefore)
        panel.popOut()
        panel.closeConsole()
        compare(panel.detached, false)
        verify(!backend.running)
        backend.commandMode = true
        backend.modeLabel = "ArduPilot MAVLink Console"
        backend.running = true
        var previousStarts = backend.starts
        panel.open()
        tryCompare(panel, "opened", true)
        compare(panel.firmware, "ArduPilot MAVLink Console")
        input.text = "status"
        panel.submit()
        compare(backend.sent, "status")
        compare(backend.starts, previousStarts)
    }
}
