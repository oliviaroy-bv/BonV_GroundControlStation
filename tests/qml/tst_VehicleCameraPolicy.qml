import QtQuick
import QtTest
import "../../ui/qml/map/MapDispatch.js" as MapDispatch

TestCase {
    name: "VehicleCameraPolicy"
    property string policyScript: ""
    property string qmlSource: ""
    function initTestCase() {
        var request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../integrations/map_provider/map/map.html"), false)
        request.send()
        var html = request.responseText
        var start = html.indexOf("<script>") + 8
        var end = html.lastIndexOf("</script>")
        verify(start >= 8 && end > start, "Cannot read the actual map script")
        policyScript = html.substring(start, end)
        request.open("GET", Qt.resolvedUrl("../../ui/qml/map/FlyMap.qml"), false)
        request.send()
        qmlSource = request.responseText
        verify(qmlSource.indexOf("Item {") >= 0, "Cannot read the actual QML integration")
    }
    function qmlBlock(prefix) {
        var start = qmlSource.indexOf(prefix)
        verify(start >= 0, "Missing QML handler: " + prefix)
        start = qmlSource.indexOf("{", start)
        var depth = 1
        var end = start + 1
        for (; end < qmlSource.length && depth; ++end) {
            if (qmlSource[end] === "{") ++depth
            else if (qmlSource[end] === "}") --depth
        }
        return qmlSource.substring(start + 1, end - 1)
    }
    function test_qmlSynchronization_data() {
        return [{tag: "late-system-gps", handler: "onPositionChanged:", connected: false},
                {tag: "vehicle-disconnect", handler: "onVehicleConnectedChanged:", connected: false},
                {tag: "vehicle-connect", handler: "onVehicleConnectedChanged:", connected: true},
                {tag: "mission-sync", handler: "function syncMissionWaypoints()", connected: true},
                {tag: "overlay-sync", handler: "function syncRasterOverlays()", connected: true},
                {tag: "readiness-sync", handler: "onMapReadyChanged:", connected: true}]
    }
    function test_qmlSynchronization(data) {
        var calls = []
        var rootMock = {
            mapReady: true, cameraActive: true, map3dEnabled: false,
            vehicleConnected: data.connected, vehiclePositionReady: false,
            systemPositionReady: true, systemLat: 10, systemLon: 20,
            missionWaypoints: [{lat:10,lon:20}], rasterManager: {enabled:true,overlays:[]},
            isValidCoordinate: function() { return true },
            updatePointer: function() { calls.push("marker") },
            syncVehiclePosition: function() { calls.push("position") },
            resetVehicleCameraState: function() {}, clearVehicleTrail: function() {},
            updateDispatchContext: function() {},
            syncMapViewMode: function() {}, syncVpsMarkers: function() {},
            syncMissionWaypoints: function() {}, syncRasterOverlays: function() {},
            initializeViewport: function() { calls.push("initialize") },
            flushVehicleMapState: function() {},
            centerMap: function() { fail("Synchronization acquired the camera") }
        }
        var webMock = {runJavaScript: function(script) {
            verify(!/centerMap|fitMission|fitBounds|easeTo|jumpTo/.test(script), script)
            calls.push("render")
        }}
        rootMock.mapDispatcher = {setSnapshot:function() { calls.push("render") }}
        // Execute the current source handler, rather than a copied implementation.
        var run = new Function("root", "position", "webView", "loadRequest", "WebEngineView",
                               "with(root) {" + qmlBlock(data.handler) + "}")
        run(rootMock, {coordinate:{latitude:30,longitude:40}}, webMock,
            {status:2}, {LoadStartedStatus:1,LoadSucceededStatus:2,LoadFailedStatus:3})
        if (data.tag === "late-system-gps") {
            compare(rootMock.systemLat, 30)
            compare(calls.join(","), "marker")
        } else if (data.tag === "vehicle-disconnect") compare(calls.join(","), "marker")
        else if (data.tag === "vehicle-connect") compare(calls.join(","), "position")
        else if (data.tag === "readiness-sync") compare(calls.join(","), "position,initialize,marker")
        else compare(calls.join(","), "render")
    }
    function test_policy_data() {
        return [
            {tag: "hidden-trail-rendering", script: "apply({active:false,trail:true}); apply({active:false,lat:2,trail:true}); tick(); check(map.moves===0); check(vehicleTrailCoordinates.length===2);"},
            {tag: "disarmed-rendering", script: "apply({armed:false,airborne:false,trail:true}); tick(); check(map.moves===0); check(droneMarker!==null); check(vehicleTrailCoordinates.length===1);"},
            {tag: "armed-on-ground", script: "apply({airborne:false}); tick(); check(map.moves===0);"},
            {tag: "takeoff-not-confirmed", script: "apply({airborne:false,flightStateFresh:true}); tick(); check(map.moves===0);"},
            {tag: "airborne-inside", script: "apply({lon:1}); tick(); tick(); check(map.moves===0);"},
            {tag: "right-boundary", script: "apply({lon:40}); tick(); check(map.moves===1); check(map.project([40,1]).x===850);"},
            {tag: "left-boundary", script: "apply({lon:-40}); tick(); check(map.moves===1); check(map.project([-40,1]).x===150);"},
            {tag: "top-boundary", script: "apply({lat:35,lon:1}); tick(); check(map.moves===1); check(map.project([1,35]).y===120);"},
            {tag: "bottom-boundary", script: "apply({lat:-35,lon:1}); tick(); check(map.moves===1); check(map.project([1,-35]).y===680);"},
            {tag: "camera-invariants", script: "apply({}); tick(); check(!('zoom' in map.lastMove)); check(!('bearing' in map.lastMove)); check(!('pitch' in map.lastMove)); check(map.zoom===15 && map.bearing===27 && map.pitch===12);"},
            {tag: "limited-single-animation", script: "apply({lon:100}); tick(); check(map.center.lng===50); apply({lon:-100}); tick(); check(map.moves===1); map.finish(); tick(); check(map.moves===2); check(map.center.lng===0);"},
            {tag: "hysteresis", script: "apply({}); tick(); map.finish(); tick(); tick(); check(map.moves===1);"},
            {tag: "landing-confirmed", script: "apply({airborne:true}); tick(); check(map.moves===1);"},
            {tag: "landing-unconfirmed", script: "apply({airborne:false}); tick(); check(map.moves===0);"},
            {tag: "disarm-cancels-immediately", script: "apply({}); tick(); apply({armed:false,airborne:false}); check(map.stops===1); tick(); check(map.moves===1);"},
            {tag: "stale-position", script: "apply({}); tick(); apply({positionFresh:false}); check(map.stops===1); tick(); check(map.moves===1);"},
            {tag: "stale-landed-state", script: "apply({}); tick(); apply({flightStateFresh:false}); check(map.stops===1); tick(); check(map.moves===1);"},
            {tag: "unknown-flight", script: "apply({flightStateFresh:false,airborne:false}); tick(); check(map.moves===0);"},
            {tag: "invalid-position", script: "apply({positionValid:false}); tick(); check(map.moves===0);"},
            {tag: "manual-drag", script: "apply({}); tick(); dom(container,'pointerdown',{pointerId:1}); check(map.stops===1); tick(6000); check(map.moves===1); dom(window,'pointerup',{pointerId:1}); tick(4900); check(map.moves===1); apply({lon:70}); tick(200); check(map.moves===2);"},
            {tag: "continuous-wheel", script: "apply({}); for(let i=0;i<8;i++){ dom(container,'wheel',{}); tick(1000); } check(map.moves===0); tick(5100); check(map.moves===1);"},
            {tag: "touch-pinch", script: "apply({}); dom(container,'pointerdown',{pointerId:1,pointerType:'touch'}); dom(container,'pointerdown',{pointerId:2,pointerType:'touch'}); tick(6000); dom(window,'pointerup',{pointerId:1}); tick(6000); check(map.moves===0); dom(window,'pointercancel',{pointerId:2}); tick(5100); check(map.moves===1);"},
            {tag: "keyboard", script: "apply({}); dom(container,'keydown',{key:'ArrowUp'}); tick(6000); check(map.moves===0); dom(window,'keyup',{key:'ArrowUp'}); tick(5100); check(map.moves===1);"},
            {tag: "navigation-control", script: "apply({}); dom(container,'click',{}); tick(4900); check(map.moves===0); tick(200); check(map.moves===1);"},
            {tag: "follow-enabled", script: "apply({follow:true}); tick(); check(map.center.lng===40 && map.center.lat===1); check(vehicleCameraOwner==='follow');"},
            {tag: "follow-is-explicit-on-ground", script: "apply({follow:true,armed:false,airborne:false,flightStateFresh:false}); tick(); check(map.moves===1);"},
            {tag: "follow-disabled", script: "apply({follow:false}); tick(); check(vehicleCameraOwner==='visibility'); check(map.center.lng!==40);"},
            {tag: "follow-mode-change-cancels", script: "apply({follow:true}); tick(); apply({follow:false}); check(map.stops===1); check(vehicleCameraOwner===null);"},
            {tag: "follow-manual-cooldown", script: "apply({follow:true}); dom(container,'wheel',{}); tick(4900); check(map.moves===0); tick(200); check(map.moves===1); check(vehicleCameraState.follow);"},
            {tag: "explicit-center", script: "apply({}); tick(); centerMap(2,3,17); check(map.stops===1); check(map.center.lat===2 && map.center.lng===3); apply({lon:80}); tick(4900); check(map.moves===1); tick(200); check(map.moves===2);"},
            {tag: "explicit-yields-to-drag", script: "apply({}); dom(container,'pointerdown',{pointerId:1}); centerMap(2,3,17); check(map.jumps===0);"},
            {tag: "cooldown-without-new-position", script: "apply({}); dom(container,'wheel',{}); tick(5100); check(map.moves===1);"},
            {tag: "hidden-shown", script: "apply({}); tick(); apply({active:false}); check(map.stops===1); apply({active:false,lon:-40}); tick(6000); check(map.moves===1); apply({lon:-40}); tick(); check(map.moves===2);"},
            {tag: "document-hidden", script: "apply({}); document.visibilityState='hidden'; tick(); check(map.moves===0); document.visibilityState='visible'; tick(); check(map.moves===1);"},
            {tag: "style-not-ready", script: "apply({}); mapStyleReady=false; tick(); check(map.moves===0); mapStyleReady=true; tick(); check(map.moves===1);"},
            {tag: "vehicle-switch", script: "apply({}); tick(); resetVehicleCamera(); check(map.stops===1); tick(6000); check(map.moves===1); apply({connected:false,positionFresh:false,lon:-40}); tick(); check(map.moves===1); apply({lon:-40}); tick(); check(map.moves===2);"},
            {tag: "one-marker-and-no-duplicate-trail", script: "apply({trail:true}); apply({trail:true,heading:90}); tick(); check(markerCount===2); check(vehicleTrailCoordinates.length===1); check(droneArrow.style.transform==='rotate(90deg)');"},
            {tag: "render-only", script: "updateDrone(1,40,0); check(map.moves===0); check(droneMarker!==null);"},
            {tag: "not-more-than-10Hz", script: "apply({lon:100}); tick(); map.finish(); tick(50); check(map.moves===1); tick(50); check(map.moves===2);"},
            {tag: "initial-viewport-once", script: "initializeViewport({latitude:10,longitude:20,zoom:12}); initializeMap(30,40); check(map.jumps===1); check(map.center.lat===10 && map.center.lng===20 && map.zoom===12);"},
            {tag: "initialization-after-window-blur", script: "dom(window,'blur',{}); initializeMap(3,4); check(map.jumps===1 && map.center.lat===3);"},
            {tag: "default-startup-rejects-late-gps", script: "initializeViewport(null); initializeMap(30,40); check(map.jumps===0); check(viewportEstablished);"},
            {tag: "initialization-yields-to-manual", script: "mapStyleReady=false; initializeMap(30,40); dom(container,'wheel',{}); map.center={lat:3,lng:4}; map.fire('style.load'); initializeMap(30,40); check(map.jumps===0 && map.center.lat===3);"},
            {tag: "initialization-yields-to-explicit", script: "centerMap(3,4,12); initializeMap(30,40); check(map.jumps===1 && map.center.lat===3);"},
            {tag: "initialization-yields-to-visibility", script: "apply({}); tick(); initializeMap(30,40); check(map.jumps===0 && map.moves===1);"},
            {tag: "session-reload", script: "centerMap(3,4,12); map.bearing=55; map.pitch=32; map.center={lat:5,lng:6}; dom(window,'pagehide',{});", reloadScript: "initializeViewport({latitude:30,longitude:40,zoom:17}); check(map.center.lat===5 && map.center.lng===6 && map.zoom===12 && map.bearing===55 && map.pitch===32);"},
            {tag: "session-zero-center", script: "centerMap(3,4,12); map.center={lat:0,lng:0}; dom(window,'pagehide',{});", reloadScript: "initializeMap(30,40); check(map.center.lat===0 && map.center.lng===0 && map.zoom===12);"},
            {tag: "unavailable-session-storage", script: "window.sessionStorage.getItem=()=>{throw Error('denied');}; window.sessionStorage.setItem=()=>{throw Error('denied');}; initializeMap(3,4); check(map.center.lat===3 && map.center.lng===4);"},
            {tag: "invalid-session-fallback", script: "window.sessionStorage.data[sessionViewportKey]=JSON.stringify({latitude:999,longitude:2,zoom:12}); initializeMap(3,4); check(map.center.lat===3);"},
            {tag: "mission-render-neutral", script: "const before=camera(); setMissionWaypoints(mission); check(missionMarkers.length===2 && map.getLayer('mission-route-line')); sameCamera(before); setMissionWaypoints([mission[0]]); check(missionMarkers.length===1); sameCamera(before); setMissionWaypoints(mission); setMissionWaypoints(mission); sameCamera(before); clearMissionWaypoints(); check(missionMarkers.length===0 && !map.getSource('mission-route')); sameCamera(before);"},
            {tag: "overlay-render-neutral", script: "const before=camera(); setRasterOverlays([overlay],true); check(map.getLayer(rasterOverlayLayerPrefix+'test')); sameCamera(before); setRasterOverlayOpacity('test',0.3); check(map.getLayer(rasterOverlayLayerPrefix+'test').paint['raster-opacity']===0.3); sameCamera(before); setRasterOverlays([overlay],true); sameCamera(before); setRasterOverlays([Object.assign({},overlay,{visible:false})],true); check(!map.getLayer(rasterOverlayLayerPrefix+'test')); sameCamera(before); setRasterOverlays([overlay],true); setRasterOverlays([],true); sameCamera(before); setRasterOverlays([overlay],false); sameCamera(before);"},
            {tag: "manual-then-render", script: "dom(container,'wheel',{}); map.center={lat:12,lng:13}; map.zoom=9; const before=camera(); setMissionWaypoints(mission); setRasterOverlays([overlay],true); sameCamera(before); check(vehicleCameraOwner===null);"},
            {tag: "visibility-during-render", script: "apply({}); tick(); const before=camera(); const owner=vehicleCameraOwner; setMissionWaypoints(mission); setRasterOverlays([overlay],true); sameCamera(before); check(vehicleCameraOwner===owner && map.stops===0 && map.moving); map.finish(); apply({lon:80}); tick(); check(map.moves===2);"},
            {tag: "style-load-does-not-replay-stale-caches", script: "setMissionWaypoints(mission); setRasterOverlays([overlay],true); map.layers={}; map.sources={}; const before=camera(); map.fire('style.load'); sameCamera(before); check(!map.getLayer('mission-route-line') && !map.getLayer(rasterOverlayLayerPrefix+'test'));"},
            {tag: "unavailable-render-neutral", script: "mapStyleReady=false; const before=camera(); setMissionWaypoints(mission); setRasterOverlays([overlay],true); sameCamera(before); check(typeof pendingOperations==='undefined');"},
            {tag: "explicit-mission-fit", script: "setMissionWaypoints(mission); apply({}); tick(); fitMission(); check(map.stops===1 && map.fits===1 && viewportEstablished); tick(4900); check(map.moves===1);"},
            {tag: "explicit-single-mission-fit", script: "setMissionWaypoints([mission[0]]); check(map.moves===0); fitMission(); check(map.moves===1 && map.lastMove.center[0]===mission[0].lon);"},
            {tag: "mission-fit-yields-to-manual", script: "setMissionWaypoints(mission); dom(container,'pointerdown',{pointerId:1}); fitMission(); check(map.fits===0);"},
            {tag: "empty-mission-fit", script: "fitMission(); check(map.moves===0 && map.fits===0 && !viewportEstablished);"},
            {tag: "lifecycle-delayed-style", script: "mapStyleReady=false; const s=window.getMapReadiness(4); check(s.documentGeneration===4 && s.styleGeneration===1 && s.bridgeAvailable && s.state==='style-loading'); map.fire('load'); check(!mapStyleReady); map.fire('style.load'); check(mapStyleReady && window.getMapReadiness(4).state==='style-ready');"},
            {tag: "lifecycle-style-generation", script: "map.fire('style.load'); const initial=map.style; const first=setMapViewMode('street','transaction-one'); const second=map.style; check(first.transactionId==='transaction-one' && mapLifecycle.styleGeneration===2 && !mapStyleReady && map.styleOptions.diff===false); map.fire('style.load',{style:initial}); check(!mapStyleReady); map.fire('style.load'); check(mapStyleReady && mapLifecycle.state==='style-ready' && mapLifecycle.styleTransactionState==='succeeded'); const next=setMapViewMode('satellite','transaction-two'); check(next.transactionId==='transaction-two' && mapLifecycle.styleGeneration===3); map.fire('style.load'); check(mapStyleReady && mapLifecycle.styleTransactionId==='transaction-two');"},
            {tag: "lifecycle-style-viewport-transaction", script: "map.fire('style.load'); map.center={lat:12,lng:13}; map.zoom=9; map.bearing=37; map.pitch=24; const start=setMapViewMode('street','viewport-transaction'); check(start.transactionId==='viewport-transaction' && mapLifecycle.styleTransactionState==='loading'); map.center={lat:1,lng:2}; map.zoom=3; map.bearing=0; map.pitch=0; map.fire('style.load'); check(map.center.lat===12 && map.center.lng===13 && map.zoom===9 && map.bearing===37 && map.pitch===24 && mapLifecycle.styleTransactionState==='succeeded');"},
            {tag: "lifecycle-style-error", script: "mapStyleReady=false; map.styleLoaded=false; map.fire('error',{style:map.style,error:Error('offline root style')}); check(mapLifecycle.state==='failed' && !mapStyleReady && mapLifecycle.error.indexOf('offline root style')>=0); map.fire('style.load'); check(!mapStyleReady);"},
            {tag: "lifecycle-stale-style-error", script: "const initial=map.style; setMapViewMode('street'); map.fire('error',{style:initial,error:Error('old transaction')}); check(mapLifecycle.state==='style-loading'); map.fire('style.load'); check(mapStyleReady);"},
            {tag: "lifecycle-style-change-exception", script: "map.setStyle=()=>{throw Error('invalid style');}; setMapViewMode('street'); check(mapLifecycle.state==='failed' && !mapStyleReady && mapLifecycle.error.indexOf('invalid style')>=0);"},
            {tag: "lifecycle-style-awaits-current-vehicle", script: "apply({}); tick(); setMapViewMode('street'); check(map.stops===1); map.fire('style.load'); tick(); check(map.moves===1); apply({lon:-40}); tick(); check(map.moves===2);"},
            {tag: "lifecycle-document-invalidation", script: "apply({}); tick(); window.invalidateMapDocument(); check(!mapStyleReady && map.stops===1 && mapLifecycle.state==='document-unloaded'); map.fire('style.load'); tick(); check(!mapStyleReady && map.moves===1); setMapViewMode('street'); setMissionWaypoints(mission); check(mapLifecycle.styleGeneration===1 && typeof pendingOperations==='undefined');"},
            {tag: "lifecycle-tile-error", script: "map.fire('style.load'); map.fire('error',{style:map.style,sourceId:'tiles',tile:{},error:Error('missing tile')}); check(mapStyleReady && mapLifecycle.state==='style-ready');"},
            {tag: "lifecycle-tile-before-style", script: "mapStyleReady=false; map.styleLoaded=false; map.fire('error',{style:map.style,sourceId:'tiles',tile:{},error:Error('missing tile')}); check(mapLifecycle.state==='style-loading'); map.fire('style.load'); check(mapStyleReady);"},
            {tag: "lifecycle-initialization-exception", script: "dom(window,'error',{message:'Uncaught initialization error',error:Error('initialization error')}); check(mapLifecycle.state==='failed' && !mapStyleReady);"},
            {tag: "lifecycle-unhandled-rejection", script: "dom(window,'unhandledrejection',{reason:Error('initialization rejected')}); check(mapLifecycle.state==='failed');"},
            {tag: "lifecycle-resource-error-nonfatal", script: "dom(window,'error',{target:{tagName:'IMG'}}); check(mapLifecycle.state!=='failed');"},
            {tag: "lifecycle-timeout", script: "mapStyleReady=false; now+=19999; window.timer(); check(mapLifecycle.state==='style-loading'); now++; window.timer(); check(mapLifecycle.state==='failed' && !mapStyleReady);"},
            {tag: "lifecycle-timeout-has-no-queue", script: "mapStyleReady=false; setMissionWaypoints(mission); now+=20000; window.timer(); setRasterOverlays([overlay],true); check(typeof pendingOperations==='undefined');"},
            {tag: "lifecycle-failure-stops-camera", script: "apply({}); tick(); failMapLifecycle('failed'); check(map.stops===1 && !mapStyleReady); tick(); check(map.moves===1);"},
            {tag: "lifecycle-no-closure-replay", script: "mapStyleReady=false; let ran=0; check(runWhenMapReady(()=>ran++)===false); const initial=map.style; setMapViewMode('street'); map.fire('style.load',{style:initial}); map.fire('style.load'); check(ran===0);"},
            {tag: "lifecycle-explicit-retry", script: "failMapLifecycle('offline');", reloadScript: "check(mapLifecycle.state==='style-loading'); map.fire('style.load'); check(mapLifecycle.state==='style-ready' && mapLifecycle.bridgeAvailable);"},
            {tag: "lifecycle-construction-failure", constructionFailure: true, script: ""},
            {tag: "receiver-stale-document", script: "map.fire('style.load'); check(receive('view',{}, {documentId:'old'}).status==='stale-document'); check(receive('view',{}).status==='success'); check(receive('view',{}, {documentGeneration:2}).status==='stale-document');"},
            {tag: "receiver-stale-style", script: "map.fire('style.load'); check(receive('view',{}, {styleGeneration:0}).status==='stale-style');"},
            {tag: "receiver-instance-identity", script: "map.fire('style.load'); check(receive('view',{}).status==='success'); check(receive('view',{}, {instanceId:'other-map'}).status==='stale-instance');"},
            {tag: "receiver-expired-camera", script: "map.fire('style.load'); check(receive('center',{lat:10,lon:20,zoom:15},{deadline:0}).status==='expired'); check(map.jumps===0);"},
            {tag: "receiver-javascript-exception", script: "map.fire('style.load'); map.addSource=()=>{throw Error('simulated render exception');}; const r=receive('snapshot:mission',mission); check(r.status==='javascript-exception' && r.error.indexOf('simulated')>=0); check(receive('view',{}).status==='success');"},
            {tag: "receiver-unavailable-style", script: "mapStyleReady=false; check(receive('view',{}).status==='map-unavailable');"},
            {tag: "receiver-coordinate-result", script: "map.fire('style.load'); const r=receive('coordinate',{x:600,y:300}); check(r.status==='success' && r.value.longitude===10 && r.value.latitude===10 && r.operationId);"},
            {tag: "receiver-camera-cancelled", script: "map.fire('style.load'); dom(container,'pointerdown',{pointerId:1}); check(receive('center',{lat:10,lon:20,zoom:17}).status==='cancelled'); check(map.jumps===0);"},
            {tag: "receiver-invalid-camera", script: "map.fire('style.load'); check(receive('center',{lat:999,lon:20,zoom:17}).status==='invalid-payload'); check(map.jumps===0);"},
            {tag: "receiver-invalid-style-configuration", script: "map.fire('style.load'); mapStyles.street=''; const r=receive('style',{mode:'street',transactionId:'invalid-style'}); check(r.status==='invalid-payload' && mapLifecycle.styleGeneration===1);"},
            {tag: "receiver-opacity-snapshot-retains-source", script: "map.fire('style.load'); receive('snapshot:rasters',{overlays:[overlay],enabled:true}); const source=map.getSource(rasterOverlaySourcePrefix+'test'); const changed=Object.assign({},overlay,{opacity:0.2}); check(receive('snapshot:rasters',{overlays:[changed],enabled:true}).status==='success'); check(map.getSource(rasterOverlaySourcePrefix+'test')===source && map.getLayer(rasterOverlayLayerPrefix+'test').paint['raster-opacity']===0.2 && lastRasterOverlays[0].opacity===0.2);"},
            {tag: "receiver-ordered-trail", script: "map.fire('style.load'); const p={epoch:1,reset:true,samples:[{sequence:1,lat:10,lon:20},{sequence:2,lat:11,lon:21}]}; check(receive('trail',p).status==='success'); p.reset=false; receive('trail',p); check(vehicleTrailCoordinates.length===2); receive('trail',{epoch:2,reset:true,samples:[]}); check(vehicleTrailCoordinates.length===0); check(receive('trail',p).status==='cancelled');"},
            {tag: "receiver-invalid-trail-order", script: "map.fire('style.load'); const r=receive('trail',{epoch:1,reset:true,samples:[{sequence:2,lat:10,lon:20},{sequence:1,lat:11,lon:21}]}); check(r.status==='javascript-exception' && vehicleTrailCoordinates.length===0);"},
            {tag: "dispatch-receiver-trail-integration", script: "map.fire('style.load'); const d=actualDispatcher(); for(let i=0;i<7000;i++)d.appendTrail(10+i/10000,20); for(let i=0;i<100;i++)d.tick(); check(vehicleTrailCoordinates.length===5000 && vehicleTrailCoordinates[0][1]===10.2 && d.stats().pendingTrailSamples===0); d.clearTrail(); d.appendTrail(30,40); for(let i=0;i<5;i++)d.tick(); check(vehicleTrailCoordinates.length===1 && vehicleTrailCoordinates[0][1]===30);"},
            {tag: "dispatch-receiver-latest-state-integration", script: "map.fire('style.load'); const d=actualDispatcher(); for(let i=0;i<10000;i++)d.setSnapshot('vehicle',{lat:10,lon:20,heading:i%360,active:false,connected:true,trail:true}); for(let i=0;i<10;i++)d.tick(); check(lastDroneHeading===9999%360 && vehicleTrailCoordinates.length===0); d.setSnapshot('mission',mission); d.setSnapshot('rasters',{overlays:[overlay],enabled:true}); for(let i=0;i<10;i++)d.tick(); check(missionMarkers.length===2 && map.getLayer(rasterOverlayLayerPrefix+'test'));"},
            {tag: "dispatch-receiver-style-integration", script: "map.fire('style.load'); const d=actualDispatcher(); const answers=[]; d.setSnapshot('vehicle',{lat:10,lon:20,heading:90,connected:true,active:false}); d.setSnapshot('mission',mission); d.setSnapshot('rasters',{overlays:[overlay],enabled:true}); d.setSnapshot('vps',[{name:'fused',lat:12,lon:22}]); d.appendTrail(10,20); d.appendTrail(11,21); for(let i=0;i<20;i++)d.tick(); d.command('style',{mode:'street'},r=>answers.push(r)); d.tick(); check(!mapStyleReady && answers.length===0); for(let i=0;i<100;i++)d.setSnapshot('mission',[{lat:12,lon:22,seq:i}]); map.fire('style.load'); d.testContext.ready=true; d.setContext(d.testContext); for(let i=0;i<30;i++)d.tick(); check(answers.length===1 && answers[0].status==='success' && answers[0].value.styleGeneration===2 && answers[0].transactionId); check(missionMarkers.length===1 && lastMissionWaypoints[0].seq===99 && vehicleTrailCoordinates.length===2 && droneMarker && vpsMarkers.fused && map.getLayer(rasterOverlayLayerPrefix+'test'));"}
        ]
    }
    function test_policy(data) {
        var mock = `

        var now=10000, markerCount=0;
        var performance={now:()=>now};
        var console={log:()=>{},error:()=>{},warn:()=>{}};
        function eventTarget(){ return {events:{},addEventListener:function(n,f){(this.events[n]||(this.events[n]=[])).push(f);}}; }
        var window=eventTarget(); window.setInterval=function(f){this.timer=f;};
        window.sessionStorage={data:sessionData,getItem:function(k){return this.data[k]||null;},setItem:function(k,v){this.data[k]=v;}};
        var document=eventTarget(); document.visibilityState='visible';
        // The production marker renderer uses SVG elements. Model the DOM
        // surface it requires; policy assertions below remain unchanged.
        document.createElement=()=>({style:{},appendChild:()=>{},setAttribute:()=>{}});
        document.createElementNS=(namespace,name)=>document.createElement(name);
        var container=eventTarget(); container.clientWidth=1000; container.clientHeight=800;
        function btoa(s){return s;}
        function dom(target,name,event){(target.events[name]||[]).forEach(f=>f(event));}
        var maplibregl={
          Map:function(){
            this.events={}; this.center={lng:0,lat:0}; this.moves=0; this.stops=0; this.jumps=0; this.fits=0;
            this.zoom=15; this.bearing=27; this.pitch=12; this.moving=false; this.sources={}; this.layers={};
            this.style={}; this.styleLoaded=true;
            this.on=function(n,f){(this.events[n]||(this.events[n]=[])).push(f);return this;};
            this.fire=function(n,e){e=e||{};if(n==='style.load'){if(!e.style)e.style=this.style;if(e.style===this.style)this.styleLoaded=true;}(this.events[n]||[]).forEach(f=>f(e));};
            this.getContainer=()=>container; this.getCanvas=()=>container;
            this.addControl=()=>{}; this.getCenter=()=>this.center; this.getZoom=()=>this.zoom;
            this.getBearing=()=>this.bearing; this.getPitch=()=>this.pitch;
            this.project=c=>({x:((Array.isArray(c)?c[0]:c.lng)-this.center.lng)*10+500,
                              y:400-((Array.isArray(c)?c[1]:c.lat)-this.center.lat)*10});
            this.unproject=p=>({lng:(p[0]-500)/10+this.center.lng,lat:(400-p[1])/10+this.center.lat});
            this.isMoving=()=>this.moving;
            this.easeTo=o=>{this.moves++;this.lastMove=o;this.moving=true;this.center=Array.isArray(o.center)?{lng:o.center[0],lat:o.center[1]}:o.center;};
            this.finish=()=>{this.moving=false;this.fire('moveend');};
            this.stop=()=>{this.stops++;this.finish();};
            this.jumpTo=o=>{this.jumps++;this.center={lng:o.center[0],lat:o.center[1]};this.zoom=o.zoom;if(o.bearing!==undefined)this.bearing=o.bearing;if(o.pitch!==undefined)this.pitch=o.pitch;this.finish();};
            this.fitBounds=(bounds,options)=>{this.fits++;this.lastFit={bounds:bounds,options:options};};
            this.getSource=id=>this.sources[id]; this.getLayer=id=>this.layers[id];
            this.addSource=(id,o)=>{this.sources[id]={setData:d=>{}};};
            this.addLayer=o=>{this.layers[o.id]=o;};
            this.getStyle=()=>this.styleLoaded?{layers:Object.values(this.layers),sources:this.sources}:undefined;
            this.setStyle=(url,options)=>{this.style={url:url};this.styleLoaded=false;this.styleOptions=options;this.layers={};this.sources={};};
            this.removeLayer=id=>{delete this.layers[id];}; this.removeSource=id=>{delete this.sources[id];};
            this.setPaintProperty=(id,key,value)=>{this.layers[id].paint[key]=value;};
          },
          Marker:function(){markerCount++;this.setLngLat=function(){return this;};this.addTo=function(){return this;};this.remove=function(){};},
          LngLatBounds:function(a,b){this.coordinates=[a,b];this.extend=function(c){this.coordinates.push(c);return this;};},
          NavigationControl:function(){}
        };
    
        `
        var helpers = `

        mapStyleReady=true;
        function check(condition){if(!condition)throw new Error('camera policy assertion failed');}
        function apply(extra){setVehicleMapState(Object.assign({lat:1,lon:40,heading:0,active:true,follow:false,
            connected:true,armed:true,flightStateFresh:true,airborne:true,positionFresh:true,positionValid:true,trail:false},extra));}
        function tick(ms=100){now+=ms;evaluateVehicleCamera();}
        const mission=[{lat:10,lon:20,seq:1},{lat:11,lon:21,seq:2}];
        const overlay={id:'test',visible:true,image:'bonv://raster/test',opacity:0.7,coordinates:[[20,11],[21,11],[21,10],[20,10]]};
        function camera(){return JSON.stringify({view:getMapView(),moves:map.moves,jumps:map.jumps,fits:map.fits,stops:map.stops,owner:vehicleCameraOwner,blockedUntil:vehicleCameraBlockedUntil,established:viewportEstablished});}
        function sameCamera(before){check(camera()===before);}
        let receiverOperationId=0;
        function receive(category,payload,extra){return window.dispatchMapOperation(Object.assign({instanceId:'receiver-test',operationId:'op-'+(++receiverOperationId),documentGeneration:1,documentId:mapLifecycle.documentId,styleGeneration:mapLifecycle.styleGeneration,category:category,payload:payload,createdAt:Date.now(),deadline:Date.now()+5000},extra));}
        const dispatchMapOperation=window.dispatchMapOperation;
        function actualDispatcher(){
            const context={documentGeneration:1,documentId:mapLifecycle.documentId,styleGeneration:mapLifecycle.styleGeneration,styleTransactionId:'',ready:true,documentLoaded:true,bridgeAvailable:true,failed:false,mode:'satellite'};
            const d=Dispatch.create({instanceId:'integration-test',transport:(script,callback)=>callback(eval(script)),beginStyle:(mode,op)=>{context.styleGeneration++;context.mode=mode;context.styleTransactionId=op.payload.transactionId;context.ready=false;d.setContext(context);}});
            d.setContext(context);
            d.testContext=context;
            return d;
        }
    
        `
        if (data.constructionFailure) {
            var broken = policyScript.replace("map = new maplibregl.Map({", "throw Error('simulated construction failure'); map = new maplibregl.Map({")
            var construct = new Function("sessionData", mock + '\ntry {\n' + broken
                + '\n} catch (error) {}\nconst status=window.getMapReadiness(1);'
                + "if(status.state!=='failed' || status.bridgeAvailable || status.error.indexOf('construction failed')<0) throw Error('Construction failure not reported');")
            construct({})
            return
        }
        var run = new Function("sessionData", "Dispatch", mock + '\n' + policyScript + '\n' + helpers + '\n' + data.script + '; return window.sessionStorage.data;')
        var storage = run({}, MapDispatch)
        if (data.reloadScript) {
            var reload = new Function("sessionData", "Dispatch", mock + '\n' + policyScript + '\n' + helpers + '\n' + data.reloadScript)
            reload(storage, MapDispatch)
        }
    }
}

