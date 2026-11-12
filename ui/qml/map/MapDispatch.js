.pragma library

// One owner per FlyMap. Desired state survives renderer documents; commands do not.
function create(options) {
    var context = {documentGeneration:0, styleGeneration:0, documentId:"", ready:false,
                   documentLoaded:false, bridgeAvailable:false, failed:false}
    var snapshots = {}, commands = [], controls = {}, wire = null, disposed = false
    var nextId = 0, pumping = false, stateTurn = 0, stateBurst = 0, commandBurst = 0
    var categories = ["vehicle", "mission", "rasters", "vps", "initialViewport", "interaction"]
    var trail = [], trailEpoch = 0, trailSequence = 0, trailSent = 0, trailReset = true
    var trailDirty = false, waitingStyle = null
    var limit = 32, capacity = 5000, batchSize = 128, transportTimeout = 5000
    function now() { return options.now ? options.now() : Date.now() }
    function identity(category, payload, ttl) {
        return {instanceId:options.instanceId, operationId:options.instanceId + ":" + (++nextId),
                documentGeneration:context.documentGeneration, documentId:context.documentId,
                styleGeneration:context.styleGeneration, category:category, payload:payload,
                createdAt:now(), deadline:now() + ttl}
    }
    function result(op, status, value, error) {
        return {instanceId:op.instanceId, operationId:op.operationId, category:op.category,
                documentGeneration:op.documentGeneration, styleGeneration:op.styleGeneration,
                transactionId:op.payload && op.payload.transactionId ? op.payload.transactionId : "",
                status:status, value:value, error:error || ""}
    }
    function finish(command, status, value, error) {
        if (command.done) return
        command.done = true
        var index = commands.indexOf(command)
        if (index >= 0) commands.splice(index, 1)
        if (waitingStyle === command) waitingStyle = null
        var answer = result(command.op, status, value, error)
        var callback = command.callback
        command.callback = null
        try { if (options.result) options.result(answer) } catch (e) { if (options.log) options.log(String(e)) }
        try { if (callback) callback(answer) } catch (e) { if (options.log) options.log(String(e)) }
    }
    function cancelCommands(status, predicate) {
        commands.slice().forEach(function(c) { if (!predicate || predicate(c)) finish(c, status) })
    }
    function dirtyAll() {
        Object.keys(snapshots).forEach(function(key) { snapshots[key].dirty = true })
        trailSent = 0
        trailReset = true
        trailDirty = true
    }
    function setContext(next) {
        if (disposed) return
        var documentChanged = next.documentGeneration !== context.documentGeneration
        var styleChanged = next.styleGeneration !== context.styleGeneration
        var newDocumentLoaded = next.documentLoaded && (!context.documentLoaded || documentChanged)
        context = Object.assign({}, next)
        if (documentChanged) {
            cancelCommands("stale-document")
            waitingStyle = null
            controls = {}
            dirtyAll()
        } else if (styleChanged) {
            // A requested operator camera action remains meaningful after a
            // successful style replacement. Rebind an unsent action to the
            // replacement style; every other ordinary command is style-bound.
            commands.slice().forEach(function(c) {
                if (c.op.category === "style") return
                if (next.styleTransactionId && (c.op.category === "center" || c.op.category === "fitMission") && !c.started
                    && now() < c.op.deadline) {
                    c.op.styleGeneration = next.styleGeneration
                    c.op.documentGeneration = next.documentGeneration
                    c.op.documentId = next.documentId
                } else finish(c, "stale-style")
            })
            dirtyAll()
        }
        // Load success means the previous JS execution context has been replaced.
        // Its late callback cannot release this document's transport slot.
        if (newDocumentLoaded && wire && wire.op.documentGeneration !== context.documentGeneration) wire = null
        if (context.failed) cancelCommands("map-unavailable")
        if (waitingStyle && waitingStyle.started && waitingStyle.acknowledged && context.ready
                && context.styleGeneration === waitingStyle.expectedStyle
                && context.styleTransactionId === waitingStyle.expectedTransactionId) {
            finish(waitingStyle, "success", {mode:context.mode, styleGeneration:context.styleGeneration,
                                               transactionId:waitingStyle.expectedTransactionId})
        }
    }
    function setSnapshot(category, payload) {
        if (disposed) return
        if (categories.indexOf(category) < 0) throw Error("Unknown snapshot category: " + category)
        var encoded = JSON.stringify(payload)
        if (!snapshots[category] || snapshots[category].encoded !== encoded) {
            snapshots[category] = {payload:JSON.parse(encoded), encoded:encoded, dirty:true}
        }
    }
    function refreshSnapshot(category) {
        if (disposed || !snapshots[category]) return
        snapshots[category].dirty = true
    }
    function distance(a, b) {
        var r = Math.PI / 180, dlat = (b.lat-a.lat)*r, dlon = (b.lon-a.lon)*r
        var h = Math.sin(dlat/2)*Math.sin(dlat/2) + Math.cos(a.lat*r)*Math.cos(b.lat*r)*Math.sin(dlon/2)*Math.sin(dlon/2)
        return 12742000 * Math.atan2(Math.sqrt(h), Math.sqrt(Math.max(0, 1-h)))
    }
    function appendTrail(lat, lon) {
        if (disposed) return
        if (!isFinite(lat) || !isFinite(lon) || Math.abs(lat)>90 || Math.abs(lon)>180 || (!lat && !lon)) return
        var sample = {lat:lat, lon:lon, sequence:trailSequence+1}
        if (trail.length && distance(trail[trail.length-1], sample) < 0.75) return
        trailSequence++
        trail.push(sample)
        if (trail.length > capacity) trail.splice(0, trail.length-capacity)
        trailDirty = true
    }
    function clearTrail() {
        if (disposed) return
        trailEpoch++
        trail = []
        trailSequence = 0
        trailSent = 0
        trailReset = true
        trailDirty = true
    }
    function enqueue(category, payload, callback, ttl) {
        var op = identity(category, null, ttl || (category === "style" ? 20000 : 2000))
        var c = {op:op, callback:callback, done:false, started:false}
        try { op.payload = JSON.parse(JSON.stringify(payload || {})) }
        catch (error) { finish(c, "invalid-payload", null, String(error)); return op.operationId }
        if (category === "style" && ["satellite", "street"].indexOf(op.payload.mode) < 0) {
            finish(c, "invalid-payload", null, "Unknown map style")
            return op.operationId
        }
        if (category === "style") {
            // The currently executing transaction is allowed to finish, but
            // only the newest requested successor is retained.
            commands.slice().forEach(function(old) {
                if (old.op.category === "style" && !old.started) finish(old, "cancelled", null,
                                                                           "Superseded by a newer style request")
            })
            op.payload.transactionId = op.operationId
        }
        if (disposed) finish(c, "cancelled")
        else if (commands.length >= limit) finish(c, "queue-full")
        else if (context.failed || !context.documentLoaded || !context.bridgeAvailable) finish(c, "map-unavailable")
        else commands.push(c)
        return op.operationId
    }
    function reject(category, payload, status, callback) {
        var c = {op:identity(category, payload, 1), callback:callback, done:false}
        finish(c, status)
        return c.op.operationId
    }
    function hasCommand(category) { return commands.some(function(c) { return c.op.category === category }) }
    function lastStyleMode() {
        for (var i=commands.length-1;i>=0;--i) if (commands[i].op.category === "style") return commands[i].op.payload.mode
        return context.mode
    }
    function control(key, script, callback, targetGeneration) {
        if (disposed) return
        // Only lifecycle controls and a single replaceable readiness probe use this lane.
        if (["probe", "invalidate", "fail"].indexOf(key) < 0) throw Error("Unknown control")
        controls[key] = {op:identity("control:"+key, {}, transportTimeout), script:script, callback:callback}
        if (targetGeneration !== undefined) controls[key].op.documentGeneration = targetGeneration
    }
    function send(entry, script, callback) {
        wire = entry
        entry.sentAt = now()
        try {
            options.transport(script, function(raw) {
                if (wire !== entry) return
                wire = null
                if (entry.op.documentGeneration !== context.documentGeneration) { pump(); return }
                try { callback(raw) } catch (error) { if (options.log) options.log(String(error)) }
                pump()
            })
        } catch (error) {
            wire = null
            callback(JSON.stringify(result(entry.op, "javascript-exception", null, String(error))))
        }
    }
    function decode(op, raw) {
        var answer
        try { answer = typeof raw === "string" ? JSON.parse(raw) : raw } catch (_) {}
        if (!answer || answer.operationId !== op.operationId || answer.instanceId !== op.instanceId
                || answer.category !== op.category || answer.documentGeneration !== op.documentGeneration || answer.styleGeneration !== op.styleGeneration
                || ["success","stale-document","stale-style","stale-instance","expired","queue-full","javascript-exception",
                    "map-unavailable","cancelled","invalid-operation","invalid-payload"].indexOf(answer.status) < 0) {
            return result(op, "javascript-exception", null, "Missing or mismatched operation result")
        }
        return answer
    }
    function sendOperation(op, callback) {
        send({op:op}, "JSON.stringify(dispatchMapOperation(" + JSON.stringify(op) + "));", function(raw) {
            callback(decode(op, raw))
        })
    }
    function pump() {
        if (disposed || pumping || wire) return
        pumping = true
        try {
            expireCommands()
            var keys = ["invalidate", "fail", "probe"]
            for (var k of keys) {
                if (controls[k]) {
                    var controlEntry = controls[k]
                    delete controls[k]
                    send(controlEntry, controlEntry.script, function(raw) { if (controlEntry.callback) controlEntry.callback(raw) })
                    return
                }
            }
            if (context.failed || !context.bridgeAvailable || !context.documentLoaded || waitingStyle) return
            var c = commands[0]
            if (c && c.op.category === "style") {
                if (c.op.payload.mode === context.mode && !waitingStyle) {
                    finish(c, "success", {mode:context.mode,styleGeneration:context.styleGeneration,
                                             transactionId:context.styleTransactionId || ""}); return
                }
                c.started = true
                c.op.styleGeneration = context.styleGeneration
                waitingStyle = c
                c.expectedStyle = context.styleGeneration+1
                c.expectedTransactionId = c.op.payload.transactionId
                try { if (options.beginStyle) options.beginStyle(c.op.payload.mode, c.op) }
                catch (error) { finish(c, "javascript-exception", null, String(error)); return }
                sendOperation(c.op, function(answer) {
                    if (c.done) return
                    if (answer.status !== "success") {
                        finish(c, answer.status, answer.value, answer.error)
                        if (options.timeout) options.timeout("Style transaction failed: " + (answer.error || answer.status))
                        return
                    }
                    c.acknowledged = true
                    if (answer.value && answer.value.state === "failed") {
                        finish(c, "map-unavailable", null, answer.value.error)
                        if (options.timeout) options.timeout("Style transaction failed: " + answer.value.error)
                        return
                    }
                    if (context.ready && context.styleGeneration === c.expectedStyle) finish(c, "success", answer.value)
                })
                return
            }
            if (!context.ready) return
            // Fresh vehicle eligibility precedes camera commands; other state and
            // trail work rotate so sustained telemetry cannot starve either lane.
            var category = null
            if (snapshots.vehicle && snapshots.vehicle.dirty && stateBurst < 2) category = "vehicle"
            if (c && c.op.category === "fitMission" && snapshots.mission && snapshots.mission.dirty) category = "mission"
            if (!category && c && (!trailDirty || commandBurst < 2)) {
                stateBurst = 0
                commandBurst++
                c.started = true
                sendOperation(c.op, function(answer) {
                    if (c.op.styleGeneration !== context.styleGeneration) finish(c, "stale-style")
                    else finish(c, answer.status, answer.value, answer.error)
                })
                return
            }
            if (!category && !(trailDirty && (stateBurst >= 2 || commandBurst >= 2))) {
                for (var j=0;j<categories.length;++j) {
                    var candidate = categories[(stateTurn+j)%categories.length]
                    if (snapshots[candidate] && snapshots[candidate].dirty) {
                        category = candidate
                        stateTurn = (stateTurn+j+1)%categories.length
                        break
                    }
                }
            }
            if (category) {
                stateBurst++
                var snapshot = snapshots[category]
                snapshot.dirty = false
                var snapshotOp = identity("snapshot:"+category, snapshot.payload, transportTimeout)
                sendOperation(snapshotOp, function(answer) {
                    if (snapshotOp.styleGeneration !== context.styleGeneration) return
                    if (answer.status !== "success" && options.result) options.result(answer)
                })
                return
            }
            if (trailDirty) {
                stateBurst = 0
                commandBurst = 0
                var epoch = trailEpoch
                var reset = trailReset
                var samples = trail.filter(function(s) { return s.sequence > trailSent }).slice(0, batchSize)
                var last = samples.length ? samples[samples.length-1].sequence : trailSent
                var trailOp = identity("trail", {epoch:epoch, reset:reset, samples:samples}, transportTimeout)
                trailDirty = false
                sendOperation(trailOp, function(answer) {
                    if (epoch !== trailEpoch || trailOp.styleGeneration !== context.styleGeneration) return
                    if (answer.status === "success") {
                        trailSent = last
                        trailReset = false
                        trailDirty = trailSequence > trailSent
                    } else if (options.result) options.result(answer)
                })
            }
        } finally { pumping = false }
    }
    function expireCommands() {
        commands.slice().forEach(function(c) {
            if (now() >= c.op.deadline) {
                var wasStyle = c === waitingStyle
                finish(c, "expired")
                if (wasStyle && options.timeout) options.timeout("Style transaction expired")
            }
        })
    }
    function tick() {
        if (disposed) return
        expireCommands()
        if (wire && !wire.timedOut && now()-wire.sentAt >= transportTimeout) {
            wire.timedOut = true
            cancelCommands("map-unavailable")
            if (options.timeout) options.timeout("JavaScript dispatch timed out: " + wire.op.operationId)
        }
        pump()
    }
    function stats() {
        return {snapshots:Object.keys(snapshots).length, pendingSnapshots:Object.keys(snapshots).filter(function(k){return snapshots[k].dirty}).length,
                commands:commands.length, controls:Object.keys(controls).length, inFlight:wire?1:0,
                trailSamples:trail.length, trailEpoch:trailEpoch, trailSequence:trailSequence,
                pendingTrailSamples:trail.filter(function(s){return s.sequence>trailSent}).length, trailResetPending:trailReset}
    }
    function dispose() {
        disposed = true
        cancelCommands("cancelled")
        snapshots = {}
        controls = {}
        trail = []
        wire = null
    }
    return {setContext:setContext, setSnapshot:setSnapshot, refreshSnapshot:refreshSnapshot, appendTrail:appendTrail, clearTrail:clearTrail,
            command:enqueue, control:control, pump:pump, tick:tick, stats:stats,
            hasCommand:hasCommand, lastStyleMode:lastStyleMode, cancelCommands:cancelCommands, reject:reject, dispose:dispose}
}
