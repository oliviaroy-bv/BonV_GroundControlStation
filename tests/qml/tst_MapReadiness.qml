import QtQuick
import QtTest
import "../../ui/qml/map/MapDispatch.js" as MapDispatch

TestCase {
    name: "MapReadiness"
    property string source: ""

    function initTestCase() {
        var request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../ui/qml/map/FlyMap.qml"), false)
        request.send()
        source = request.responseText.replace(/\r\n/g, "\n")
        verify(source.indexOf("function probeMapReadiness()") >= 0)
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
            documentGeneration: 0, styleGeneration: 0, documentId: "",
            documentLoaded: false, bridgeAvailable: false, readinessState: "document-unloaded",
            readinessError: "", readinessProbePending: false, appliedMapViewMode: "",
            styleTransactionId: "", styleTransactionState: "",
            mapViewMode: "satellite", lastVehicleMapPayload: "old", initializationTimeoutMs: 20,
            callbacks: [], results:[], deadlineStarts: 0, deadlineStops: 0, reloads: 0, diagnostics: [],
            WebEngineView: {LoadStartedStatus:1, LoadSucceededStatus:2, LoadFailedStatus:3, LoadStoppedStatus:4,
                           ErrorMessageLevel:2, WarningMessageLevel:1}
        }
        h.root = h
        h.console = {error:function(message) { h.diagnostics.push(message) }, log:function() {}, warn:function() {}}
        h.webView = {
            runJavaScript:function(script, callback) { h.callbacks.push({script:script, callback:callback}) },
            reload:function() { h.reloads++ }
        }
        h.initializationDeadline = {restart:function() { h.deadlineStarts++ }, stop:function() { h.deadlineStops++ }}
        Object.defineProperty(h, "mapReady", {get:function() {
            return h.documentLoaded && h.bridgeAvailable && h.readinessState === "style-ready"
        }})
        var names = ["beginMapDocument", "failMapReadiness", "probeMapReadiness", "acceptMapReadiness",
                     "dispatchCommand", "syncMapViewMode", "beginStyleTransaction", "updateDispatchContext", "retryMap", "clearVehicleTrail"]
        var declarations = ""
        var exports = []
        for (var name of names) {
            var start = source.indexOf("function " + name + "(")
            var brace = source.indexOf("{", start)
            declarations += source.substring(start, brace) + block("function " + name + "(") + "\n"
            exports.push(name + ":" + name)
        }
        // Compile the actual source functions into a controlled WebEngine fixture.
        Object.assign(h, new Function("scope", "with(scope){" + declarations
                      + "return {" + exports.join(",") + "};}")(h))
        h.mapDispatcher = MapDispatch.create({instanceId:"readiness-test", transport:h.webView.runJavaScript,
            result:function(r) { h.results.push(r) }, beginStyle:function(mode, op) { h.beginStyleTransaction(mode, op) },
            timeout:function(reason) { h.failMapReadiness(reason) }})
        h.reply = function(index, status, value) {
            var request = h.callbacks[index]
            var start = request.script.indexOf("dispatchMapOperation(") + "dispatchMapOperation(".length
            var op = JSON.parse(request.script.substring(start, request.script.lastIndexOf("));")))
            request.callback(JSON.stringify({instanceId:op.instanceId,operationId:op.operationId,category:op.category,
                documentGeneration:op.documentGeneration,styleGeneration:op.styleGeneration,status:status,value:value}))
        }
        return h
    }

    function snapshot(h, extra) {
        return Object.assign({documentGeneration:h.documentGeneration, documentId:"page-a",
                             styleGeneration:1, bridgeAvailable:true, mode:"satellite", state:"style-ready", error:""}, extra || {})
    }
    function loaded(h) {
        h.beginMapDocument()
        h.documentLoaded = true
        h.readinessState = "document-loaded"
        h.updateDispatchContext()
    }
    function handler(h, prefix, parameters, args) {
        return new Function("scope", "args", "with(scope){return (function(" + parameters + ")"
                            + block(prefix) + ").apply(null,args);}")(h, args)
    }

    function test_normalAndDelayedInitialization() {
        var h = harness()
        verify(!h.mapReady)
        handler(h, "onLoadingChanged: function(loadRequest)", "loadRequest", [{status:1}])
        compare(h.readinessState, "document-loading")
        handler(h, "onLoadingChanged: function(loadRequest)", "loadRequest", [{status:2}])
        compare(h.readinessState, "document-loaded")
        verify(!h.mapReady)
        compare(h.callbacks.length, 1)
        h.callbacks[0].callback(JSON.stringify(snapshot(h, {bridgeAvailable:false,state:"style-loading"})))
        verify(!h.mapReady && !h.bridgeAvailable)
        h.acceptMapReadiness(snapshot(h, {state:"style-loading"}), h.documentGeneration)
        compare(h.readinessState, "style-loading")
        verify(h.bridgeAvailable && !h.mapReady)
        h.acceptMapReadiness(snapshot(h), h.documentGeneration)
        verify(h.mapReady)
        compare(h.deadlineStops, 1)
    }

    function test_htmlSuccessStyleFailure() {
        var h = harness()
        loaded(h)
        h.acceptMapReadiness(snapshot(h, {state:"failed",error:"offline style JSON"}), h.documentGeneration)
        compare(h.readinessState, "failed")
        verify(!h.mapReady && h.readinessError.indexOf("offline style JSON") >= 0)
        h.acceptMapReadiness(snapshot(h), h.documentGeneration)
        verify(!h.mapReady) // A late completion cannot heal a failed transaction.
        compare(h.reloads, 0)
    }

    function test_htmlFailure() {
        var h = harness()
        h.beginMapDocument()
        handler(h, "onLoadingChanged: function(loadRequest)", "loadRequest",
                [{status:3,errorCode:-105,errorString:"host unavailable"}])
        compare(h.readinessState, "failed")
        verify(h.readinessError.indexOf("host unavailable") >= 0)
        verify(!h.mapReady)
    }

    function test_probeSingleFlightAndStaleDocument() {
        var h = harness()
        loaded(h)
        h.probeMapReadiness()
        h.probeMapReadiness()
        compare(h.callbacks.length, 1)
        var old = h.callbacks[0].callback
        var status = snapshot(h)
        loaded(h)
        h.probeMapReadiness()
        old(JSON.stringify(status))
        verify(h.readinessProbePending) // Old callback must not release the new probe.
        verify(!h.mapReady && h.documentId === "")
        h.callbacks[1].callback(JSON.stringify(snapshot(h)))
        verify(h.mapReady && !h.readinessProbePending)
    }

    function test_styleGenerationGuards() {
        var h = harness()
        loaded(h)
        h.acceptMapReadiness(snapshot(h), h.documentGeneration)
        verify(h.mapReady)
        h.mapViewMode = "street"
        h.syncMapViewMode()
        compare(h.styleGeneration, 2)
        compare(h.readinessState, "style-loading")
        verify(!h.mapReady)
        h.acceptMapReadiness(snapshot(h), h.documentGeneration)
        verify(!h.mapReady)
        h.acceptMapReadiness(snapshot(h, {state:"failed",error:"old style failure"}), h.documentGeneration)
        compare(h.readinessState, "style-loading")
        h.reply(0, "success", {state:"style-loading"})
        h.acceptMapReadiness(snapshot(h, {styleGeneration:2,mode:"street"}), h.documentGeneration)
        verify(h.mapReady)
    }

    function test_preferredStyleBeforeOperational() {
        var h = harness()
        loaded(h)
        h.mapViewMode = "street"
        h.acceptMapReadiness(snapshot(h), h.documentGeneration)
        compare(h.styleGeneration, 2)
        verify(!h.mapReady)
        h.reply(0, "success", {state:"style-loading"})
        h.acceptMapReadiness(snapshot(h, {styleGeneration:2,mode:"street"}), h.documentGeneration)
        verify(h.mapReady)
    }

    function test_unexpectedDocumentReplacement() {
        var h = harness()
        loaded(h)
        h.acceptMapReadiness(snapshot(h), h.documentGeneration)
        var generation = h.documentGeneration
        h.acceptMapReadiness(snapshot(h, {documentId:"page-b"}), generation)
        compare(h.documentGeneration, generation + 1)
        verify(!h.mapReady && !h.bridgeAvailable)
        h.acceptMapReadiness(snapshot(h, {documentId:"page-b"}), h.documentGeneration)
        verify(h.mapReady)
    }

    function test_rendererTerminationAndRetry() {
        var h = harness()
        loaded(h)
        h.acceptMapReadiness(snapshot(h), h.documentGeneration)
        var old = snapshot(h)
        handler(h, "onRenderProcessTerminated: function(terminationStatus, exitCode)", "terminationStatus,exitCode", [2,9])
        compare(h.readinessState, "failed")
        verify(!h.mapReady && !h.documentLoaded)
        verify(h.readinessError.indexOf("exit code 9") >= 0)
        h.acceptMapReadiness(old, old.documentGeneration)
        verify(!h.mapReady)
        h.retryMap()
        compare(h.reloads, 1)
        compare(h.readinessState, "document-unloaded")
        loaded(h)
        h.acceptMapReadiness(snapshot(h, {documentId:"page-b"}), h.documentGeneration)
        verify(h.mapReady && h.readinessError === "")
    }

    function test_guardedOperationsAndCallbacks() {
        var h = harness()
        h.dispatchCommand("view", {})
        compare(h.callbacks.length, 0)
        compare(h.results[0].status, "map-unavailable")
        loaded(h)
        h.acceptMapReadiness(snapshot(h), h.documentGeneration)
        var returned = []
        h.dispatchCommand("view", {}, function(value) { returned.push(value.status) })
        h.styleGeneration++
        h.updateDispatchContext()
        h.reply(0, "success", {})
        compare(returned.join(","), "stale-style")
        h.dispatchCommand("view", {}, function(value) { returned.push(value.status) })
        loaded(h)
        h.reply(1, "success", {})
        compare(returned.join(","), "stale-style,stale-document")
    }

    function test_malformedResponse() {
        var h = harness()
        loaded(h)
        h.probeMapReadiness()
        h.callbacks[0].callback("not-json")
        compare(h.readinessState, "failed")
        verify(h.readinessError.indexOf("Invalid readiness response") >= 0)
    }

    function test_hiddenMapCancelsQueuedCameraCommands() {
        var h=harness()
        loaded(h)
        h.acceptMapReadiness(snapshot(h),h.documentGeneration)
        h.cameraActive=true
        h.dispatchCommand("view",{})
        h.dispatchCommand("center",{lat:10,lon:20,zoom:17})
        h.cameraActive=false
        h.vehicleRenderDirty=false
        h.flushVehicleMapState=function() {}
        handler(h,"onCameraActiveChanged:","",[])
        compare(h.results.length,1)
        compare(h.results[0].status,"cancelled")
        compare(h.results[0].category,"center")
        h.reply(0,"success",{})
        compare(h.mapDispatcher.stats().commands,0)
    }

    function test_vehicleSwitchClearSurvivesUnreadiness() {
        var h = harness()
        h.clearVehicleTrail()
        compare(h.mapDispatcher.stats().trailEpoch, 1)
        compare(h.callbacks.length, 0)
        loaded(h)
        h.acceptMapReadiness(snapshot(h), h.documentGeneration)
        h.mapDispatcher.pump()
        h.styleGeneration++
        h.updateDispatchContext()
        h.reply(0, "success", {})
        compare(h.mapDispatcher.stats().trailEpoch, 1)
        h.reply(1, "success", {})
        compare(h.mapDispatcher.stats().trailSamples, 0)
    }

    function test_oldTrailClearCannotAcknowledgeNewSelection() {
        var h = harness()
        loaded(h)
        h.acceptMapReadiness(snapshot(h), h.documentGeneration)
        h.clearVehicleTrail()
        h.mapDispatcher.pump()
        h.clearVehicleTrail()
        h.reply(0, "success", {})
        h.reply(1, "success", {})
        compare(h.mapDispatcher.stats().trailEpoch, 2)
        compare(h.mapDispatcher.stats().trailSamples, 0)
    }

    function test_missingBridgeTimesOut() {
        var h = harness()
        var deadlineBlock = block("Timer {\n        id: initializationDeadline")
        // Run the actual QML deadline handler and interval binding, using Qt's timer.
        var holder = Qt.createQmlObject('import QtQuick; Item { property var root: ({initializationTimeoutMs:20}); '
                     + 'property alias deadline: initializationDeadline; Timer ' + deadlineBlock + '}', this)
        holder.root = h
        h.initializationDeadline = holder.deadline
        loaded(h)
        h.probeMapReadiness()
        h.callbacks[0].callback("")
        verify(!h.mapReady)
        tryCompare(h, "readinessState", "failed", 1000)
        verify(h.readinessError.indexOf("timed out") >= 0)
        compare(h.reloads, 0)
        holder.destroy()
    }
}
