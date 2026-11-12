import QtQuick
import QtTest
import "../../ui/qml/map/MapDispatch.js" as MapDispatch

TestCase {
    name: "MapDispatch"
    function fixture(ready) {
        var f = {time:1000, pending:[], sent:[], results:[], timeouts:[], active:0, peak:0}
        f.context = {documentGeneration:1,documentId:"document-a",styleGeneration:1,styleTransactionId:"",ready:ready,
                     documentLoaded:true,bridgeAvailable:true,failed:false,mode:"satellite"}
        f.dispatch = MapDispatch.create({instanceId:"map-test",now:function(){return f.time},
            transport:function(script, callback) {
                var op = null
                if (script.indexOf("JSON.stringify(dispatchMapOperation(") === 0)
                    op = JSON.parse(script.substring("JSON.stringify(dispatchMapOperation(".length,script.lastIndexOf("));")))
                f.active++
                f.peak = Math.max(f.peak,f.active)
                var request = {op:op,script:script,callback:callback}
                f.pending.push(request)
                f.sent.push(request)
            },
            result:function(r){f.results.push(r)},
            beginStyle:function(mode,op){f.context.styleGeneration++;f.context.mode=mode;f.context.styleTransactionId=op.payload.transactionId;f.context.ready=false;f.dispatch.setContext(f.context)},
            timeout:function(reason){f.timeouts.push(reason);f.context.failed=true;f.context.ready=false;f.dispatch.setContext(f.context)}
        })
        f.dispatch.setContext(f.context)
        f.reply = function(status, value) {
            verify(f.pending.length > 0, "No pending transport")
            var request = f.pending.shift()
            f.active--
            var op = request.op
            var result = op ? {instanceId:op.instanceId,operationId:op.operationId,category:op.category,
                         documentGeneration:op.documentGeneration,styleGeneration:op.styleGeneration,
                         status:status || "success",value:value,error:status === "javascript-exception" ? "test exception" : ""} : value
            request.callback(op ? JSON.stringify(result) : result)
            return request
        }
        f.drain = function() {
            for (var i=0;i<2000;++i) {
                f.dispatch.pump()
                if (!f.pending.length) return
                f.reply()
            }
            fail("Dispatch did not quiesce")
        }
        f.operations = function(category) { return f.sent.filter(function(r){return r.op && r.op.category === category}).map(function(r){return r.op}) }
        return f
    }

    function test_loadingSnapshotsStayBounded() {
        var f=fixture(false)
        for (var i=0;i<30000;++i) {
            f.dispatch.setSnapshot("vehicle",{lat:10,lon:20+i/1000000,heading:i%360})
            f.dispatch.setSnapshot("mission",[{lat:10,lon:20,seq:i}])
            f.dispatch.setSnapshot("rasters",{overlays:[{id:i}],enabled:true})
            f.dispatch.setSnapshot("vps",[{name:"vio",lat:10,lon:i/1000000}])
            f.dispatch.tick()
        }
        compare(f.sent.length,0)
        compare(f.dispatch.stats().snapshots,4)
        compare(f.dispatch.stats().pendingSnapshots,4)
        compare(f.dispatch.stats().commands,0)
        f.context.ready=true;f.dispatch.setContext(f.context);f.drain()
        compare(f.operations("snapshot:vehicle").length,1)
        compare(f.operations("snapshot:vehicle")[0].payload.heading,29999%360)
        compare(f.operations("snapshot:mission")[0].payload[0].seq,29999)
        compare(f.operations("snapshot:rasters")[0].payload.overlays[0].id,29999)
        compare(f.peak,1)
    }

    function test_busyLatestWinsAndCopiesPayload() {
        var f=fixture(true)
        var payload={heading:1}
        f.dispatch.setSnapshot("vehicle",payload);f.dispatch.pump()
        payload.heading=999
        for(var i=2;i<=1000;++i) f.dispatch.setSnapshot("vehicle",{heading:i})
        compare(f.dispatch.stats().pendingSnapshots,1)
        f.drain()
        var ops=f.operations("snapshot:vehicle")
        compare(ops.length,2)
        compare(ops[0].payload.heading,1)
        compare(ops[1].payload.heading,1000)
        compare(f.peak,1)
    }

    function test_trailOrderCapacityAndDuplicates() {
        var f=fixture(false)
        for(var i=0;i<7000;++i) {
            f.dispatch.appendTrail(10+i/10000,20)
            f.dispatch.appendTrail(10+i/10000,20)
        }
        compare(f.dispatch.stats().trailSamples,5000)
        compare(f.dispatch.stats().trailSequence,7000)
        f.context.ready=true;f.dispatch.setContext(f.context);f.drain()
        var batches=f.operations("trail")
        var samples=[]
        for(var batch of batches) {
            verify(batch.payload.samples.length<=128)
            samples=samples.concat(batch.payload.samples)
        }
        compare(samples.length,5000)
        compare(samples[0].sequence,2001)
        compare(samples[4999].sequence,7000)
        for(var j=1;j<samples.length;++j) compare(samples[j].sequence,samples[j-1].sequence+1)
        verify(batches[0].payload.reset)
    }

    function test_clearDuringInflightTrail() {
        var f=fixture(true)
        f.dispatch.appendTrail(10,20);f.dispatch.pump()
        f.dispatch.clearTrail()
        f.dispatch.appendTrail(30,40)
        f.reply();f.drain()
        var batches=f.operations("trail")
        compare(batches.length,2)
        compare(batches[1].payload.epoch,batches[0].payload.epoch+1)
        verify(batches[1].payload.reset)
        compare(batches[1].payload.samples.length,1)
        compare(batches[1].payload.samples[0].lat,30)
        compare(f.dispatch.stats().trailSamples,1)
    }

    function test_disconnectPendingTrail() {
        var f=fixture(false)
        f.dispatch.appendTrail(10,20)
        f.dispatch.clearTrail()
        f.dispatch.setSnapshot("vehicle",{connected:false,positionValid:false})
        f.context.ready=true;f.dispatch.setContext(f.context);f.drain()
        compare(f.operations("trail")[0].payload.samples.length,0)
        verify(f.operations("trail")[0].payload.reset)
        verify(!f.operations("snapshot:vehicle")[0].payload.connected)
    }

    function test_commandLimitAndExactlyOnce() {
        var f=fixture(false)
        for(var i=0;i<40;++i) f.dispatch.command("view",{index:i})
        compare(f.dispatch.stats().commands,32)
        compare(f.results.length,8)
        for(var r of f.results) compare(r.status,"queue-full")
        f.context.ready=true;f.dispatch.setContext(f.context);f.drain()
        compare(f.results.length,40)
        var seen={}
        for(var answer of f.results) {verify(!seen[answer.operationId]);seen[answer.operationId]=true}
        var ops=f.operations("view")
        compare(ops.length,32)
        for(var j=0;j<32;++j) compare(ops[j].payload.index,j)
        compare(f.dispatch.stats().commands,0)
        compare(f.peak,1)
    }

    function test_inflightCountsTowardLimit() {
        var f=fixture(true)
        f.dispatch.command("view",{});f.dispatch.pump()
        for(var i=0;i<32;++i) f.dispatch.command("view",{})
        compare(f.dispatch.stats().commands,32)
        compare(f.results.length,1)
        compare(f.results[0].status,"queue-full")
        f.drain()
    }

    function test_commandExpiryAndLateResult() {
        var f=fixture(true)
        f.dispatch.command("center",{lat:10,lon:20},null,500);f.dispatch.pump()
        f.time+=501;f.dispatch.tick()
        compare(f.results.length,1)
        compare(f.results[0].status,"expired")
        f.reply();f.drain()
        compare(f.results.length,1)
        compare(f.dispatch.stats().commands,0)
    }

    function test_unsentExpiry() {
        var f=fixture(false)
        f.dispatch.command("coordinate",{x:1,y:2},null,1000)
        f.time+=1001;f.dispatch.tick()
        compare(f.results[0].status,"expired")
        compare(f.operations("coordinate").length,0)
    }

    function test_javascriptExceptionReleasesCommand() {
        var f=fixture(true)
        f.dispatch.command("view",{});f.dispatch.command("view",{});f.dispatch.pump()
        var old=f.reply("javascript-exception")
        old.callback("duplicate callback")
        f.reply("success",{zoom:12});f.drain()
        compare(f.results.length,2)
        compare(f.results[0].status,"javascript-exception")
        compare(f.results[1].status,"success")
    }

    function test_reloadCancelsAndReplaysCurrentState() {
        var f=fixture(true)
        f.dispatch.command("center",{lat:10,lon:20});f.dispatch.pump()
        f.dispatch.setSnapshot("vehicle",{heading:1})
        f.context.documentGeneration=2;f.context.documentLoaded=false;f.context.ready=false;f.dispatch.setContext(f.context)
        compare(f.results[0].status,"stale-document")
        f.dispatch.setSnapshot("vehicle",{heading:200})
        f.context.documentId="document-b";f.context.documentLoaded=true;f.context.ready=true;f.dispatch.setContext(f.context)
        f.reply() // Obsolete frame callback.
        f.drain()
        compare(f.results.length,1)
        compare(f.operations("snapshot:vehicle")[0].payload.heading,200)
        compare(f.operations("snapshot:vehicle")[0].documentGeneration,2)
    }

    function test_styleChangeCancelsOldCameraAndKeepsSnapshots() {
        var f=fixture(true)
        f.dispatch.command("center",{lat:10,lon:20})
        f.context.styleGeneration=2;f.context.ready=false;f.dispatch.setContext(f.context)
        compare(f.results[0].status,"stale-style")
        for(var i=0;i<1000;++i) {
            f.dispatch.setSnapshot("mission",[{seq:i}])
            f.dispatch.setSnapshot("rasters",{overlays:[{opacity:i/1000}],enabled:true})
        }
        f.context.ready=true;f.dispatch.setContext(f.context);f.drain()
        compare(f.operations("center").length,0)
        compare(f.operations("snapshot:mission")[0].payload[0].seq,999)
        compare(f.operations("snapshot:rasters")[0].styleGeneration,2)
    }

    function test_styleTransactionsSerialized() {
        var f=fixture(true)
        f.dispatch.command("style",{mode:"street"})
        f.dispatch.pump()
        f.dispatch.command("style",{mode:"satellite"})
        compare(f.operations("style").length,1)
        f.reply("success",{state:"style-loading"})
        f.dispatch.pump()
        compare(f.operations("style").length,1)
        f.context.ready=true;f.dispatch.setContext(f.context);f.dispatch.pump()
        compare(f.operations("style").length,2)
        f.reply("success",{state:"style-loading"})
        f.context.ready=true;f.dispatch.setContext(f.context);f.drain()
        compare(f.results.length,2)
        compare(f.results[0].status,"success")
        compare(f.results[1].status,"success")
    }

    function test_controlLaneSharesSingleFlight() {
        var f=fixture(true)
        f.dispatch.command("view",{});f.dispatch.pump()
        f.dispatch.control("probe","probe-one",function(){})
        f.dispatch.control("probe","probe-latest",function(){})
        f.dispatch.control("fail","failure",function(){})
        f.dispatch.pump()
        compare(f.pending.length,1)
        compare(f.dispatch.stats().controls,2)
        f.reply();f.reply("success","");f.reply("success","");f.drain()
        compare(f.sent[1].script,"failure")
        compare(f.sent[2].script,"probe-latest")
        compare(f.peak,1)
    }

    function test_hungTransportTerminatesCommands() {
        var f=fixture(true)
        f.dispatch.command("view",{});f.dispatch.pump()
        f.dispatch.command("view",{})
        f.time+=5001;f.dispatch.tick();f.dispatch.tick()
        compare(f.dispatch.stats().commands,0)
        compare(f.results.length,2)
        compare(f.timeouts.length,1)
        compare(f.dispatch.stats().inFlight,1) // Bounded physical slot; no further sends into a hung renderer.
        compare(f.sent.length,1)
    }

    function test_repeatedUnavailableRecovery() {
        var f=fixture(false)
        for(var cycle=0;cycle<50;++cycle) {
            for(var i=0;i<200;++i) {
                f.dispatch.setSnapshot("vehicle",{heading:i})
                f.dispatch.appendTrail(10+(cycle*200+i)/10000,20)
            }
            f.context.documentGeneration++;f.context.documentId="document-"+cycle
            f.context.documentLoaded=true;f.context.ready=true;f.dispatch.setContext(f.context);f.drain()
            f.context.ready=false;f.dispatch.setContext(f.context)
            verify(f.dispatch.stats().snapshots<=5 && f.dispatch.stats().trailSamples<=5000)
            compare(f.dispatch.stats().commands,0)
        }
        compare(f.dispatch.stats().trailSamples,5000)
        compare(f.peak,1)
    }

    function test_trailNotStarvedBySustainedVehicleSnapshots() {
        var f=fixture(true)
        f.dispatch.appendTrail(10,20)
        f.dispatch.setSnapshot("vehicle",{heading:0});f.dispatch.pump()
        for(var i=1;i<20;++i) {
            f.dispatch.setSnapshot("vehicle",{heading:i})
            f.reply()
        }
        f.drain()
        verify(f.operations("trail").length>0)
    }

    function test_destroyCancelsExactlyOnce() {
        var f=fixture(true)
        f.dispatch.command("view",{});f.dispatch.command("view",{});f.dispatch.pump()
        f.dispatch.dispose()
        compare(f.results.length,2)
        for(var r of f.results) compare(r.status,"cancelled")
        f.reply()
        compare(f.results.length,2)
        compare(f.dispatch.stats().commands,0)
        compare(f.dispatch.stats().snapshots,0)
        f.dispatch.tick()
        compare(f.sent.length,1)
    }

    function test_malformedResultSettlesCommand() {
        var f=fixture(true)
        f.dispatch.command("view",{});f.dispatch.pump()
        var request=f.pending.shift();f.active--
        request.callback('{"operationId":"wrong"}')
        compare(f.results.length,1)
        compare(f.results[0].status,"javascript-exception")
        f.drain()
        compare(f.dispatch.stats().commands,0)
    }

    function test_styleFailureCancelsRemainingCommands() {
        var f=fixture(true)
        f.dispatch.command("style",{mode:"street"})
        f.dispatch.pump()
        f.dispatch.command("style",{mode:"satellite"})
        f.reply("javascript-exception")
        compare(f.results.length,2)
        compare(f.results[0].status,"javascript-exception")
        compare(f.results[1].status,"map-unavailable")
        compare(f.operations("style").length,1)
    }

    function test_invalidStyleDoesNotStartTransaction() {
        var f=fixture(true)
        f.dispatch.command("style",{mode:"unsupported"});f.dispatch.pump()
        compare(f.results.length,1)
        compare(f.results[0].status,"invalid-payload")
        compare(f.context.styleGeneration,1)
        verify(f.context.ready)
        compare(f.operations("style").length,0)
        f.drain()
    }

    function test_latestStyleRequestWinsWhileTransactionIsActive() {
        var f=fixture(true)
        f.dispatch.command("style",{mode:"street"})
        f.dispatch.pump()
        var first=f.operations("style")[0]
        f.dispatch.command("style",{mode:"satellite"})
        f.dispatch.command("style",{mode:"satellite"})
        // The intermediate request is completed exactly once before it can run.
        compare(f.results.length,1)
        compare(f.results[0].status,"cancelled")
        verify(f.results[0].transactionId.length>0)
        f.reply("success",{state:"style-loading",transactionId:first.payload.transactionId})
        f.context.ready=true; f.dispatch.setContext(f.context); f.dispatch.pump()
        compare(f.operations("style").length,2)
        compare(f.operations("style")[1].payload.mode,"satellite")
    }

    function test_explicitCameraCommandRebindsAfterStyleReplacement() {
        var f=fixture(true)
        f.dispatch.command("style",{mode:"street"}); f.dispatch.pump()
        f.dispatch.command("center",{lat:10,lon:20,zoom:15})
        f.reply("success",{state:"style-loading"})
        f.context.ready=true; f.dispatch.setContext(f.context); f.dispatch.pump()
        var center=f.operations("center")[0]
        compare(center.styleGeneration,2)
        f.reply("success",{})
        verify(f.results.some(function(r) { return r.category==="center" && r.status==="success" }))
    }
}
