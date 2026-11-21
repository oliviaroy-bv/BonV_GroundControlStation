pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtTest
import QtCore as Core
import "../../ui/qml/settings" as SettingsPages

TestCase {
    id: test
    name: "FullParameterList"
    width: 1400; height: 850
    visible: true; when: windowShown
    property var subject: null
    property var testVehicle: vehicle
    property url settingsLocation: Qt.resolvedUrl("../../build/phase5-favorites-tests.ini")
    Core.Settings { id: testPreferences; location: test.settingsLocation; category: "FullParameterList/Favorites/v1" }
    component TestManager: QtObject {
        property bool parametersReady: false
        property bool hasCompleteSnapshot: false
        property int syncState: 0
        property int progress: 0
        property int receivedParameterCount: 0
        property int expectedParameterCount: 0
        property string syncError: ""
        property int snapshotRevision: 0
        property var snapshotProvenance: ({})
        signal snapshotChanged()
        signal parameterUpdated(string name, var parameter)
        signal snapshotPublished(int revision)
        onSnapshotRevisionChanged: snapshotChanged()
    }
    component TestTransport: QtObject {
        property var parameterConfiguration: null
        property var firmwareIdentity: QtObject {
            property bool commandMode: false
            property string modeLabel: "MAVLink Console"
            signal changed()
            onCommandModeChanged: changed()
            onModeLabelChanged: changed()
        }
        property bool connected: false
        property int connectionState: 0
        property int refreshes: 0
        property int connectionGeneration: 1
        property bool acceptWrites: true
        property string refusalReason: "Write queue is full."
        property int nextWriteId: 0
        property var submittedWrites: []
        property var records: ({})
        property var reads: ({maps: 0, values: {}})
        function parameters() { reads.maps++; return records }
        function parameterValue(name) { reads.values[name] = (reads.values[name] || 0) + 1; return records[name] || ({}) }
        function requestParameters() { refreshes++ }
        function requestParameterWrite(name, value, type) {
            var id = ++nextWriteId
            submittedWrites = submittedWrites.concat([{id:id, name:name, value:value, type:type}])
            return {id:id, accepted:acceptWrites, reason:acceptWrites ? "" : refusalReason}
        }
        signal parameterWriteFinished(var result)
        signal connectionStatusChanged()
        signal rcChannelsChanged()
        signal rcCalibrationWriteFinished(var result)
        signal connectionLifecycleChanged()
        signal parameterUpdated(string name, var parameter)
    }
    TestManager { id: manager }
    TestTransport { id: transport }
    QtObject { id: vehicle; property var mavlink: transport; property var parameterManager: manager }
    Component {
        id: sourceComponent
        QtObject {
            property var parameterManager: TestManager {}
            property var mavlink: TestTransport {}
        }
    }
    // OEM uses the application backend for joystick settings even on its category grid.
    QtObject {
        id: backend
        property bool joystickEnabled: false
        property bool joystickAvailable: false
        property string joystickSelectedDevice: ""
        property var joystickDevices: []
        property var joystickStatus: ({})
        property var joystickLiveInput: ({})
        property var joystickAxisRanges: ({})
        property var joystickAssignments: ({})
        property var joystickButtonAssignments: ({})
        function setJoystickMavlink(value) {}
        function refreshJoystickDevices() {}
    }
    Component { id: page; SettingsPages.FullParameterListPage { id: testPage; width: test.width; height: test.height; vehicle: test.testVehicle; favoritesSettingsLocation: test.settingsLocation; firmwareIdentity: testPage.vehicle ? testPage.vehicle.mavlink.firmwareIdentity : null } }
    Component { id: oem; SettingsPages.OemSettingsPage { width: test.width; height: test.height; vehicle: test.testVehicle; open: true } }
    function init() {
        testVehicle = vehicle
        testPreferences.setValue("selections", "{}"); testPreferences.sync()
        transport.firmwareIdentity.commandMode = false; transport.firmwareIdentity.modeLabel = "MAVLink Console"
        manager.parametersReady = false; manager.hasCompleteSnapshot = false
        manager.syncState = 0; manager.progress = 0; manager.syncError = ""
        manager.receivedParameterCount = 0; manager.expectedParameterCount = 0
        transport.connected = false; transport.records = ({}); transport.refreshes = 0
        transport.reads = {maps: 0, values: {}}
        transport.connectionGeneration = 1; transport.acceptWrites = true
        transport.parameterConfiguration = null
        transport.nextWriteId = 0; transport.submittedWrites = []
        manager.snapshotRevision = 0; manager.snapshotProvenance = ({})
    }
    function cleanup() { if (subject) subject.destroy(); subject = null }
    function populate(count) {
        var records = {}
        for (var i = count - 1; i >= 0; --i) {
            var name = "TEST_" + String(i).padStart(4, "0")
            records[name] = {name: name, value: i, type: 6, index: i}
        }
        transport.records = records; manager.hasCompleteSnapshot = true
        manager.snapshotRevision++; manager.snapshotPublished(manager.snapshotRevision)
    }
    function publish(source, records) {
        source.mavlink.records = records
        source.parameterManager.hasCompleteSnapshot = true
        source.parameterManager.snapshotRevision++
        source.parameterManager.snapshotPublished(source.parameterManager.snapshotRevision)
        // Real publication follows the full-snapshot signal with one update per row.
        for (var name in records) source.parameterManager.parameterUpdated(name, records[name])
    }
    function updateValue(source, name, value) {
        var previous = source.mavlink.records[name]
        var record = {name: name, value: value, type: previous.type, index: previous.index}
        // Mutate a JS member without a property notification, like the opaque C++ facade.
        source.mavlink.records[name] = record
        source.parameterManager.snapshotRevision++
        source.parameterManager.parameterUpdated(name, record)
    }
    function makeReady(source) {
        source.parameterManager.snapshotProvenance = {systemId:42, componentId:1, connectionGeneration:1,
            transferGeneration:1, encoding:2, sourceConnectionGeneration:source.mavlink.connectionGeneration, current:true}
        source.mavlink.connected = true
        source.parameterManager.parametersReady = true
        source.parameterManager.syncState = 3
    }
    function writeResult(name, outcome, value) {
        var ticket = subject.pendingWrites[name]
        return {id:ticket.id, parameter:name, outcomeName:outcome, reason:outcome === "Confirmed" ? "" : "Backend " + outcome,
            requestedValue:Number(ticket.text), confirmedValue:value, hasConfirmedValue:value !== undefined,
            generation:ticket.identity.transferGeneration, connectionGeneration:ticket.identity.connectionGeneration,
            systemId:ticket.identity.systemId, componentId:ticket.identity.componentId, type:ticket.type}
    }
    function editingSetup(type, value) {
        subject = createTemporaryObject(page, test)
        publish(vehicle, {VALUE:{name:"VALUE", value:value === undefined ? 0 : value, type:type, index:0},
            OTHER:{name:"OTHER", value:5, type:6, index:1}})
        makeReady(vehicle)
        subject.selectParameter("VALUE")
    }
    Component {
        id: configurationComponent
        QtObject {
            property bool busy: false
            property bool applying: false
            property bool comparisonCurrent: true
            property bool canCompare: true
            property int selectedCount: 2
            property string message: "Comparison ready"
            property string compatibility: "Unverified firmware identity"
            property string firmware: "unknown"
            property string filter: "All"
            property var rows: null
            property var importSummary: ({total:2, valid:2, invalid:0, duplicates:0, unsupported:0, format:"ArduPilot pairs", firmware:"unknown"})
            property var progress: ({total:0})
            property int applyCalls: 0
            property int stops: 0
            property int compares: 0
            property var lastArguments: ({})
            function compare() { compares++ }
            function selectAll(selected) { selectedCount = selected ? 2 : 0 }
            function selectRow(row, selected) {}
            function applySelected(names, unknown, conflicts) { applyCalls++; lastArguments = {names:names, unknown:unknown, conflicts:conflicts}; applying = true }
            function stop() { stops++ }
            function importFile(url) {}
            function exportFile(url, stale, unknown) {}
        }
    }
    function test_configurationConfirmationAndPendingIsolation() {
        editingSetup(6, 10); subject.editDraft("15")
        var configuration = createTemporaryObject(configurationComponent, test)
        transport.parameterConfiguration = configuration
        findChild(subject, "parameterConfigurationTools").clicked()
        verify(subject.configurationVisible)
        findChild(subject, "configurationUnknownAcknowledgement").checked = true
        findChild(subject, "configurationDraftAcknowledgement").checked = true
        findChild(subject, "applyParameterConfiguration").clicked()
        var dialog = findChild(subject, "configurationApplyConfirmation")
        tryCompare(dialog, "opened", true)
        compare(configuration.applyCalls, 0) // no implicit write on import/compare/selection
        dialog.standardButton(Dialog.Apply).clicked()
        compare(configuration.applyCalls, 1)
        compare(configuration.lastArguments.names.join(","), "VALUE")
        verify(configuration.lastArguments.unknown); verify(configuration.lastArguments.conflicts)
        verify(!subject.writableReady)
        findChild(subject, "configurationBack").clicked()
        verify(!subject.configurationVisible)
        compare(configuration.stops, 0) // closing does not pretend backend writes stopped
        compare(subject.proposedValue, "15")
        subject.active = false; subject.active = true
        compare(configuration.stops, 0)
        subject.vehicle = null
        verify(configuration.stops > 0) // changing active vehicle prevents further submissions
    }
    function test_configurationStaleExportAndResizing() {
        editingSetup(6, 10)
        var configuration = createTemporaryObject(configurationComponent, test)
        transport.parameterConfiguration = configuration
        manager.parametersReady = false
        subject.configurationVisible = true
        findChild(subject, "exportParameterConfiguration").clicked()
        var dialog = findChild(subject, "configurationExportConfirmation")
        tryCompare(dialog, "opened", true)
        verify(findChild(subject, "configurationExportWarning").text.indexOf("stale retained snapshot") >= 0)
        dialog.reject()
        subject.width = 480; subject.height = 800; subject.darkTheme = false; wait(20)
        verify(findChild(subject, "parameterConfigurationRows").height > 100)
        compare(transport.submittedWrites.length, 0)
        compare(transport.refreshes, 0)
    }
    function test_modifiedViewTracksDraftsAndPending() {
        editingSetup(6, 10)
        subject.selectView("modified")
        compare(subject.filteredNames.length, 0)
        compare(subject.emptyText, "No modified parameters")
        subject.editDraft("15")
        compare(subject.filteredNames.join(","), "VALUE")
        compare(subject.unsubmittedCount, 1); compare(subject.pendingCount, 0)
        var rows = subject.filteredNames
        subject.editDraft("16")
        verify(subject.filteredNames === rows) // typing does not rebuild the list
        subject.selectParameter("OTHER"); subject.editDraft("20")
        compare(subject.filteredNames.join(","), "OTHER,VALUE")
        subject.applySelected()
        compare(subject.unsubmittedCount, 1); compare(subject.pendingCount, 1)
        subject.searchText = "value"
        compare(subject.filteredNames.join(","), "VALUE")
        subject.selectParameter("VALUE"); compare(subject.proposedValue, "16")
        updateValue(vehicle, "VALUE", 12)
        verify(subject.selectedDraft.review)
        compare(subject.filteredNames.join(","), "VALUE")
        subject.discardDraft()
        compare(subject.filteredNames.length, 0)
        subject.resetFilters()
        compare(subject.filteredNames.join(","), "OTHER")
        compare(transport.refreshes, 0)
    }
    function test_modifiedViewTerminalResults_data() {
        return [{tag:"Confirmed"}, {tag:"Rejected"}, {tag:"TimedOut"}, {tag:"Cancelled"}, {tag:"TransportError"}]
    }
    function test_modifiedViewTerminalResults(data) {
        editingSetup(6, 10)
        subject.editDraft("15"); subject.selectView("modified"); subject.applySelected()
        var result = writeResult("VALUE", data.tag, data.tag === "Confirmed" ? 15 : undefined)
        compare(subject.filteredNames.join(","), "VALUE")
        if (data.tag === "Confirmed") updateValue(vehicle, "VALUE", 15)
        if (data.tag === "Rejected") updateValue(vehicle, "VALUE", 12)
        transport.parameterWriteFinished(result)
        compare(subject.pendingCount, 0)
        compare(subject.filteredNames.length, data.tag === "Confirmed" ? 0 : 1)
        compare(subject.modifiedCount, data.tag === "Confirmed" ? 0 : 1)
        compare(subject.selectedName, "VALUE")
        compare(subject.parameterState("VALUE"), data.tag === "TimedOut" ? "Timed Out" : data.tag === "TransportError" ? "Transport Error" : data.tag)
    }
    function test_favoriteInteractionsAndPersistence() {
        editingSetup(6, 10)
        subject.selectView("favorites")
        compare(subject.filteredNames.length, 0); compare(subject.emptyText, "No favorites yet")
        findChild(subject, "favoriteSelectedParameter").clicked()
        verify(subject.isFavorite("VALUE"))
        subject.setFavorite("VALUE", true)
        compare(subject.favoriteNames.length, 1)
        compare(subject.filteredNames.join(","), "VALUE")
        subject.editDraft("15")
        testPreferences.sync()
        var saved = String(testPreferences.value("selections", "{}"))
        verify(saved.indexOf("VALUE") >= 0)
        verify(saved.indexOf("15") < 0); verify(saved.indexOf("10") < 0)
        subject.editDraft("16")
        compare(String(testPreferences.value("selections", "{}")), saved)
        subject.destroy(); subject = createTemporaryObject(page, test)
        verify(subject.isFavorite("VALUE")); compare(subject.modifiedCount, 0)
        subject.selectView("favorites")
        compare(subject.filteredNames.join(","), "VALUE")
        subject.toggleFavorite("VALUE")
        compare(subject.favoriteNames.length, 0); compare(subject.filteredNames.length, 0)
        compare(transport.submittedWrites.length, 0); compare(transport.refreshes, 0)
    }
    function test_favoriteRowDoesNotSelectOrWrite() {
        editingSetup(6, 10)
        var before = subject.selectedName
        var star = findChild(subject, "favoriteRow_OTHER")
        tryVerify(function() { return findChild(subject, "favoriteRow_OTHER") !== null })
        star = findChild(subject, "favoriteRow_OTHER")
        mouseClick(star)
        compare(subject.selectedName, before)
        verify(subject.isFavorite("OTHER"))
        compare(transport.submittedWrites.length, 0)
    }
    function test_firmwareFavoriteNamespacesAndReconnect() {
        editingSetup(6, 10)
        transport.firmwareIdentity.commandMode = true
        compare(subject.snapshotFavoriteNamespace, "ardupilot/classic-v1")
        subject.setFavorite("VALUE", true)
        manager.parametersReady = false; transport.connected = false
        transport.firmwareIdentity.commandMode = false
        verify(subject.isFavorite("VALUE")) // retained snapshot keeps its namespace
        transport.firmwareIdentity.modeLabel = "PX4 NSH Shell"
        makeReady(vehicle)
        compare(subject.snapshotFavoriteNamespace, "px4/classic-v1")
        verify(!subject.isFavorite("VALUE"))
        subject.setFavorite("OTHER", true)
        transport.firmwareIdentity.commandMode = true
        verify(subject.isFavorite("VALUE")); verify(!subject.isFavorite("OTHER"))
        subject.destroy(); subject = createTemporaryObject(page, test)
        verify(subject.isFavorite("VALUE"))
        compare(subject.snapshotFavoriteNamespace, "ardupilot/classic-v1")
    }
    function test_favoriteRemovedAndRestoredInSnapshot() {
        editingSetup(6, 10)
        subject.setFavorite("VALUE", true)
        subject.selectView("favorites")
        publish(vehicle, {OTHER:{name:"OTHER", value:5, type:6, index:0}})
        compare(subject.availableFavoriteCount, 0); compare(subject.filteredNames.length, 0)
        verify(subject.isFavorite("VALUE")) // retain the bookmark for future firmware snapshots
        publish(vehicle, {VALUE:{name:"VALUE", value:8, type:9, index:0}})
        compare(subject.availableFavoriteCount, 1); compare(subject.filteredNames.join(","), "VALUE")
    }
    function test_typeFilters_data() {
        return [{tag:"all", type:-1}, {tag:"uint8", type:1}, {tag:"int8", type:2}, {tag:"uint16", type:3},
            {tag:"int16", type:4}, {tag:"uint32", type:5}, {tag:"int32", type:6}, {tag:"real32", type:9}]
    }
    function test_typeFilters(data) {
        subject = createTemporaryObject(page, test)
        var records = {}, types = [1,2,3,4,5,6,9]
        for (var i = 0; i < types.length; ++i) records["VALUE_" + types[i]] = {name:"VALUE_" + types[i], value:1, type:types[i], index:i}
        publish(vehicle, records)
        subject.selectedType = data.type
        compare(subject.filteredNames.length, data.type === -1 ? 7 : 1)
        if (data.type !== -1) compare(subject.filteredNames[0], "VALUE_" + data.type)
        compare(transport.refreshes, 0)
    }
    function test_combinedSearchMetadataAndFilters() {
        subject = createTemporaryObject(page, test)
        publish(vehicle, {AXIS_GAIN:{name:"AXIS_GAIN", value:1, type:9, index:0, description:"Pitch response", category:"Actuation"},
            AXIS_MODE:{name:"AXIS_MODE", value:1, type:6, index:1}, UNGROUPED:{name:"UNGROUPED", value:1, type:6, index:2}})
        makeReady(vehicle)
        compare(subject.groups.join(","), "AXIS")
        compare(subject.groupCounts.AXIS, 2)
        subject.searchText = "pitch"; compare(subject.filteredNames.join(","), "AXIS_GAIN")
        subject.searchText = "ACTUat"; compare(subject.filteredNames.join(","), "AXIS_GAIN")
        subject.searchText = "axis"; subject.selectedType = 6
        compare(subject.filteredNames.join(","), "AXIS_MODE")
        subject.setFavorite("AXIS_MODE", true); subject.selectView("favorites")
        compare(subject.filteredNames.join(","), "AXIS_MODE")
        subject.searchText = "absent"; compare(subject.filteredNames.length, 0)
        compare(subject.emptyText, "No matching search results")
        findChild(subject, "clearParameterSearch").clicked()
        compare(subject.filteredNames.join(","), "AXIS_MODE")
        subject.selectParameter("AXIS_MODE"); subject.editDraft("2"); subject.selectView("modified")
        subject.searchText = "axis_m"; compare(subject.filteredNames.join(","), "AXIS_MODE")
        subject.selectedType = 9; compare(subject.filteredNames.length, 0)
        findChild(subject, "resetParameterFilters").clicked()
        compare(subject.navigationView, "modified"); compare(subject.filteredNames.join(","), "AXIS_MODE")
        subject.selectView("all"); compare(subject.filteredNames.length, 3)
        subject.selectedGroup = "AXIS"; compare(subject.filteredNames.length, 2)
        subject.resetFilters(); verify(subject.filteredNames.indexOf("UNGROUPED") >= 0)
        compare(transport.refreshes, 0)
    }
    function test_filterReplacementAndIncrementalMetadata() {
        editingSetup(6, 10)
        subject.selectedType = 6
        subject.selectParameter("VALUE"); subject.editDraft("15")
        publish(vehicle, {VALUE:{name:"VALUE", value:1.5, type:9, index:0}, NEW_GROUP:{name:"NEW_GROUP", value:2, type:6, index:1}})
        compare(subject.filteredNames.join(","), "NEW_GROUP")
        compare(subject.selectedName, "VALUE"); compare(subject.modifiedCount, 0)
        compare(subject.groups.join(","), "NEW"); compare(subject.groupCounts.NEW, 1)
        subject.selectedType = 9; subject.searchText = "updated"
        var record = {name:"VALUE", value:2.5, type:9, index:0, description:"Updated response"}
        transport.records.VALUE = record; manager.snapshotRevision++; manager.parameterUpdated("VALUE", record)
        compare(subject.filteredNames.join(","), "VALUE")
        compare(subject.selectedRecord.value, 2.5)
        var replacement = createTemporaryObject(sourceComponent, test)
        publish(replacement, {PX_NAME:{name:"PX_NAME", value:1, type:6, index:0}})
        replacement.mavlink.firmwareIdentity.modeLabel = "PX4 NSH Shell"
        makeReady(replacement)
        subject.vehicle = replacement; wait(20)
        compare(subject.selectedName, ""); compare(subject.navigationView, "all"); compare(subject.selectedType, -1)
        compare(subject.filteredNames.join(","), "PX_NAME")
        compare(subject.snapshotFavoriteNamespace, "px4/classic-v1")
    }
    function test_largeListFilterPerformanceAndPendingIntegrity() {
        subject = createTemporaryObject(page, test); populate(1500); makeReady(vehicle)
        subject.selectParameter("TEST_1499"); subject.editDraft("2000")
        subject.selectView("modified")
        var names = subject.parameterNames, filtered = subject.filteredNames, fields = subject.filterFields, maps = transport.reads.maps
        for (var i = 0; i < 20; ++i) subject.editDraft(String(2000 + i))
        verify(subject.parameterNames === names); verify(subject.filteredNames === filtered); verify(subject.filterFields === fields)
        subject.applySelected(); var result = writeResult("TEST_1499", "Confirmed", 2019)
        subject.setFavorite("TEST_1499", true); subject.selectView("favorites")
        subject.searchText = "test_1"; subject.selectedType = 6
        compare(subject.filteredNames.join(","), "TEST_1499")
        subject.toggleFavorite("TEST_1499"); compare(subject.filteredNames.length, 0)
        verify(subject.pendingWrites.TEST_1499 !== undefined)
        updateValue(vehicle, "TEST_1499", 2019); transport.parameterWriteFinished(result)
        compare(subject.pendingCount, 0); compare(subject.modifiedCount, 0)
        verify(subject.filterFields === fields)
        subject.selectView("all"); subject.resetFilters(); wait(20)
        var list = findChild(subject, "parameterList")
        list.positionViewAtIndex(1499, ListView.End); verify(list.contentY > 0)
        compare(transport.reads.maps, maps)
        compare(transport.refreshes, 0)
    }
    function test_organizationThemesAndNarrowNavigation() {
        editingSetup(6, 10)
        subject.editDraft("15"); subject.setFavorite("VALUE", true)
        subject.selectParameter("OTHER"); subject.editDraft("20")
        subject.selectView("modified")
        for (var i = 0; i < 2; ++i) {
            subject.darkTheme = i === 0; subject.width = i === 0 ? 1300 : 900
            wait(20)
            compare(subject.filteredNames.length, 2)
            grabImage(subject).save(decodeURIComponent(Qt.resolvedUrl("../../build/phase5-organization-" + (i === 0 ? "dark" : "light") + ".png").toString().replace(/^file:\/\/\//, "")))
        }
        subject.width = 480; subject.height = 800; wait(20)
        var navigation = findChild(subject, "parameterViewFilter")
        verify(navigation.visible)
        navigation.currentIndex = 2; navigation.activated(2)
        compare(subject.navigationView, "favorites")
        compare(subject.filteredNames.join(","), "VALUE")
        verify(findChild(subject, "parameterList").height > 100)
        compare(subject.modifiedCount, 2)
    }
    function test_corruptFavoritePreferencesAreSafe() {
        testPreferences.setValue("selections", "not json"); testPreferences.sync()
        editingSetup(6, 10)
        compare(subject.favoriteNames.length, 0)
        subject.setFavorite("VALUE", true)
        testPreferences.sync()
        var saved = JSON.parse(String(testPreferences.value("selections", "{}")))
        compare(saved[subject.snapshotFavoriteNamespace].join(","), "VALUE")
    }
    function test_editSupportedTypes_data() {
        var rows = []
        var types = [{type:1, min:0, max:255}, {type:2, min:-128, max:127}, {type:3, min:0, max:65535},
            {type:4, min:-32768, max:32767}, {type:5, min:0, max:4294967295}, {type:6, min:-2147483648, max:2147483647}]
        for (var i = 0; i < types.length; ++i) {
            rows.push({tag:"type" + types[i].type + "-min", type:types[i].type, text:String(types[i].min)})
            rows.push({tag:"type" + types[i].type + "-max", type:types[i].type, text:String(types[i].max)})
        }
        rows.push({tag:"real32-precision", type:9, text:"1.2345678912345"})
        rows.push({tag:"real32-exponent", type:9, text:"-1.25e-20"})
        return rows
    }
    function test_editSupportedTypes(data) {
        editingSetup(data.type, 1)
        subject.editDraft(data.text)
        compare(subject.inputError, "")
        verify(subject.canApply)
        compare(subject.selectedRecord.value, 1)
        compare(subject.modifiedCount, 1)
        subject.applySelected()
        compare(subject.editorState, "Pending")
        compare(transport.submittedWrites.length, 1)
        compare(transport.submittedWrites[0].type, data.type)
        compare(transport.submittedWrites[0].value, Number(data.text))
        compare(subject.selectedRecord.value, 1)
        compare(transport.records.VALUE.value, 1)
    }
    function test_invalidEdits_data() {
        var rows = [
            {tag:"empty", type:6, text:""}, {tag:"whitespace", type:9, text:" "}, {tag:"letters", type:6, text:"abc"},
            {tag:"fraction", type:6, text:"1.5"}, {tag:"integer-exponent", type:6, text:"1e2"},
            {tag:"hex", type:6, text:"0x10"}, {tag:"NaN", type:9, text:"NaN"}, {tag:"infinity", type:9, text:"Infinity"},
            {tag:"real-overflow", type:9, text:"3.5e38"}, {tag:"real-underflow", type:9, text:"1e-50"},
            {tag:"double-underflow", type:9, text:"1e-999"}, {tag:"double-overflow", type:9, text:"1e999"},
            {tag:"numeric-cast-precision", type:6, text:"16777217", numeric:true}, {tag:"unsupported-64bit", type:7, text:"2"}
        ]
        var ranges = [{type:1, min:0, max:255}, {type:2, min:-128, max:127}, {type:3, min:0, max:65535},
            {type:4, min:-32768, max:32767}, {type:5, min:0, max:4294967295}, {type:6, min:-2147483648, max:2147483647}]
        for (var i = 0; i < ranges.length; ++i) {
            rows.push({tag:"type" + ranges[i].type + "-underflow", type:ranges[i].type, text:String(ranges[i].min - 1)})
            rows.push({tag:"type" + ranges[i].type + "-overflow", type:ranges[i].type, text:String(ranges[i].max + 1)})
        }
        return rows
    }
    function test_invalidEdits(data) {
        editingSetup(data.type, 1)
        if (data.numeric) manager.snapshotProvenance = Object.assign({}, manager.snapshotProvenance, {encoding:1})
        subject.editDraft(data.text)
        verify(subject.inputError.length > 0)
        verify(!subject.canApply)
        subject.applySelected()
        compare(transport.submittedWrites.length, 0)
        compare(subject.selectedRecord.value, 1)
    }
    function test_editorInteractionAndMultipleDrafts() {
        editingSetup(6, 10)
        var editor = findChild(subject, "parameterEditor")
        editor.forceActiveFocus(); editor.selectAll()
        keyClick(Qt.Key_1); keyClick(Qt.Key_5)
        compare(subject.proposedValue, "15")
        compare(subject.editorState, "Modified")
        compare(subject.selectedRecord.value, 10)
        subject.selectParameter("OTHER"); subject.editDraft("22")
        compare(subject.modifiedCount, 2)
        subject.selectParameter("VALUE"); compare(subject.proposedValue, "15")
        subject.discardDraft()
        compare(subject.proposedValue, "10")
        compare(subject.modifiedCount, 1)
        subject.selectParameter("OTHER"); compare(subject.proposedValue, "22")
    }
    function test_confirmedAfterAuthoritativeEchoOnly() {
        editingSetup(9, 10)
        subject.editDraft("0.1"); subject.applySelected()
        verify(!subject.canApply)
        verify(!findChild(subject, "parameterEditor").enabled)
        subject.applySelected(); compare(transport.submittedWrites.length, 1)
        var result = writeResult("VALUE", "Confirmed", Math.fround(0.1))
        compare(subject.selectedRecord.value, 10)
        updateValue(vehicle, "VALUE", Math.fround(0.1))
        compare(subject.editorState, "Pending") // a value notification alone is not transaction success
        compare(subject.modifiedCount, 1)
        transport.parameterWriteFinished(result)
        compare(subject.editorState, "Confirmed")
        compare(subject.selectedRecord.value, Math.fround(0.1))
        compare(subject.modifiedCount, 0)
        subject.editDraft("0.2")
        transport.parameterWriteFinished(result) // duplicate must not affect the new proposal
        compare(subject.editorState, "Modified")
        compare(subject.proposedValue, "0.2")
    }
    function test_terminalOutcomes_data() {
        return [{tag:"Rejected"}, {tag:"TimedOut"}, {tag:"Cancelled"}, {tag:"InvalidRequest"}, {tag:"TransportError"}]
    }
    function test_terminalOutcomes(data) {
        editingSetup(6, 10)
        subject.editDraft("15"); subject.applySelected()
        var result = writeResult("VALUE", data.tag)
        if (data.tag === "Rejected") {
            updateValue(vehicle, "VALUE", 12)
            result.confirmedValue = 12; result.hasConfirmedValue = true
        }
        transport.parameterWriteFinished(result)
        compare(Object.keys(subject.pendingWrites).length, 0)
        compare(subject.proposedValue, "15")
        compare(subject.selectedRecord.value, data.tag === "Rejected" ? 12 : 10)
        verify(findChild(subject, "parameterFeedback").text.indexOf("Backend " + data.tag) >= 0)
        if (data.tag === "TimedOut") verify(findChild(subject, "parameterFeedback").text.indexOf("may have applied") >= 0)
        if (subject.selectedDraft.review) subject.reviewProposal()
        verify(subject.canApply)
        subject.applySelected()
        compare(transport.submittedWrites.length, 2)
        var nextId = subject.pendingWrites.VALUE.id
        transport.parameterWriteFinished(result) // late result from the earlier attempt
        compare(subject.pendingWrites.VALUE.id, nextId)
    }
    function test_immediateRefusal() {
        editingSetup(6, 10)
        transport.acceptWrites = false
        subject.editDraft("15"); subject.applySelected()
        compare(subject.editorState, "Invalid Request")
        compare(subject.selectedFeedback.reason, transport.refusalReason)
        compare(Object.keys(subject.pendingWrites).length, 0)
        compare(subject.selectedRecord.value, 10)
        compare(subject.proposedValue, "15")
    }
    function test_resultIdentityAndSelectionChanges() {
        editingSetup(6, 10)
        subject.editDraft("15"); subject.applySelected()
        var result = writeResult("VALUE", "Confirmed", 15)
        transport.parameterWriteFinished(Object.assign({}, result, {id:900}))
        transport.parameterWriteFinished(Object.assign({}, result, {generation:900}))
        transport.parameterWriteFinished(Object.assign({}, result, {connectionGeneration:900}))
        transport.parameterWriteFinished(Object.assign({}, result, {systemId:900}))
        transport.parameterWriteFinished(Object.assign({}, result, {componentId:900}))
        transport.parameterWriteFinished(Object.assign({}, result, {type:9}))
        verify(subject.selectedPending !== null)
        subject.selectParameter("OTHER"); subject.editDraft("20")
        updateValue(vehicle, "VALUE", 15); transport.parameterWriteFinished(result)
        compare(subject.selectedName, "OTHER")
        compare(subject.proposedValue, "20")
        compare(subject.modifiedCount, 1)
        subject.selectParameter("VALUE")
        compare(subject.editorState, "Confirmed")
        compare(subject.proposedValue, "15")
    }
    function test_externalUpdateRequiresReview() {
        editingSetup(6, 10)
        subject.editDraft("15")
        updateValue(vehicle, "VALUE", 12)
        compare(subject.proposedValue, "15")
        compare(subject.selectedRecord.value, 12)
        verify(subject.selectedDraft.review); verify(!subject.canApply)
        subject.applySelected(); compare(transport.submittedWrites.length, 0)
        subject.reviewProposal(); verify(subject.canApply)
        subject.applySelected(); compare(transport.submittedWrites.length, 1)
    }
    function test_typeChangeAndRemovalInvalidatesProposal() {
        editingSetup(6, 10)
        subject.editDraft("15")
        publish(vehicle, {VALUE:{name:"VALUE", value:1.5, type:9, index:0}})
        compare(subject.modifiedCount, 0)
        compare(subject.proposedValue, "1.5")
        verify(subject.selectedFeedback.reason.indexOf("type changed") >= 0)
        subject.editDraft("2.5")
        publish(vehicle, {})
        compare(subject.modifiedCount, 0)
        compare(subject.selectedName, "")
    }
    function test_applyRereadsRecord() {
        editingSetup(6, 10)
        subject.editDraft("15")
        transport.records.VALUE = {name:"VALUE", value:10, type:9, index:0} // no notification
        subject.applySelected()
        compare(transport.submittedWrites.length, 0)
        compare(subject.modifiedCount, 0)
        compare(subject.selectedRecord.type, 9)
    }
    function test_discardAllConfirmationKeepsPendingWrites() {
        editingSetup(6, 10)
        subject.editDraft("15"); subject.applySelected()
        subject.selectParameter("OTHER"); subject.editDraft("22")
        findChild(subject, "discardAllParameters").clicked()
        var dialog = findChild(subject, "discardAllParametersDialog")
        tryCompare(dialog, "opened", true)
        dialog.reject(); compare(subject.modifiedCount, 2)
        findChild(subject, "discardAllParameters").clicked()
        tryCompare(dialog, "opened", true)
        subject.active = false
        tryCompare(dialog, "opened", false)
        compare(subject.modifiedCount, 2)
        subject.active = true
        findChild(subject, "discardAllParameters").clicked()
        tryCompare(dialog, "opened", true)
        dialog.standardButton(Dialog.Discard).clicked()
        compare(subject.modifiedCount, 1)
        verify(subject.pendingWrites.VALUE !== undefined)
        compare(transport.submittedWrites.length, 1)
    }
    function test_draftsAndResultsAcrossPageVisibility() {
        editingSetup(6, 10)
        subject.editDraft("15"); subject.applySelected()
        var result = writeResult("VALUE", "Confirmed", 15)
        subject.active = false
        compare(subject.pendingWrites.VALUE.id, result.id)
        updateValue(vehicle, "VALUE", 15)
        transport.parameterWriteFinished(result)
        compare(subject.modifiedCount, 0)
        subject.active = true; wait(20)
        compare(subject.editorState, "Confirmed")
        compare(subject.proposedValue, "15")
        compare(transport.refreshes, 0)
    }
    function test_readinessRefreshAndLateOldVehicleResults() {
        editingSetup(6, 10)
        subject.editDraft("15"); subject.applySelected()
        var result = writeResult("VALUE", "Cancelled")
        manager.parametersReady = false; manager.syncState = 1
        transport.parameterWriteFinished(result)
        compare(subject.editorState, "Cancelled")
        verify(!findChild(subject, "parameterEditor").enabled)
        makeReady(vehicle)
        manager.snapshotProvenance = Object.assign({}, manager.snapshotProvenance, {transferGeneration:2})
        verify(subject.selectedDraft.review); verify(!subject.canApply)
        subject.reviewProposal(); subject.applySelected()
        var oldResult = writeResult("VALUE", "Confirmed", 15)
        var replacement = createTemporaryObject(sourceComponent, test)
        publish(replacement, {VALUE:{name:"VALUE", value:99, type:6, index:0}})
        makeReady(replacement)
        subject.vehicle = replacement; wait(20)
        subject.selectParameter("VALUE"); subject.editDraft("100")
        replacement.parameterManager.snapshotProvenance = Object.assign({}, replacement.parameterManager.snapshotProvenance,
            {transferGeneration:oldResult.generation})
        subject.reviewProposal()
        replacement.mavlink.nextWriteId = oldResult.id - 1
        subject.applySelected()
        compare(subject.pendingWrites.VALUE.id, oldResult.id) // even colliding IDs belong to different sources
        transport.parameterWriteFinished(oldResult)
        compare(subject.selectedRecord.value, 99)
        compare(subject.proposedValue, "100")
        compare(Object.keys(subject.pendingWrites).length, 1)
        compare(subject.editorState, "Pending")
    }
    function test_confirmedResultNeverMutatesSnapshot() {
        editingSetup(6, 10)
        subject.editDraft("15"); subject.applySelected()
        transport.parameterWriteFinished(writeResult("VALUE", "Confirmed", 15))
        compare(subject.selectedRecord.value, 10)
        compare(transport.records.VALUE.value, 10)
        compare(subject.proposedValue, "15")
        verify(subject.selectedDraft.review)
        compare(subject.editorState, "Review required")
    }
    function test_editRestrictionsAndSourceValidation() {
        editingSetup(6, 10)
        subject.editDraft("15")
        transport.connected = false
        subject.applySelected(); compare(transport.submittedWrites.length, 0)
        verify(!findChild(subject, "parameterEditor").enabled)
        transport.connected = true; manager.parametersReady = false
        subject.applySelected(); compare(transport.submittedWrites.length, 0)
        makeReady(vehicle); subject.reviewProposal()
        manager.snapshotProvenance = Object.assign({}, manager.snapshotProvenance, {sourceConnectionGeneration:88})
        subject.applySelected(); compare(transport.submittedWrites.length, 0)
        verify(!subject.writableReady)
        makeReady(vehicle); subject.reviewProposal()
        delete transport.records.VALUE
        subject.applySelected(); compare(transport.submittedWrites.length, 0)
        compare(subject.modifiedCount, 0)
    }
    function test_oldGenerationResultDoesNotClearNewProposal() {
        editingSetup(6, 10)
        subject.editDraft("15"); subject.applySelected()
        var result = writeResult("VALUE", "Confirmed", 15)
        manager.parametersReady = false
        manager.snapshotProvenance = Object.assign({}, manager.snapshotProvenance, {transferGeneration:2, current:true})
        manager.parametersReady = true
        transport.parameterWriteFinished(result)
        compare(subject.selectedRecord.value, 10)
        compare(subject.proposedValue, "15")
        verify(subject.selectedDraft.review)
        verify(subject.editorState.indexOf("Previous snapshot") >= 0)
        verify(!subject.canApply)
    }
    function test_pendingResultSurvivesOemNavigation() {
        subject = createTemporaryObject(oem, test)
        populate(2); makeReady(vehicle)
        subject.selectSection(subject.sectionIndex("Vehicle Settings"))
        findChild(subject, "openFullParameterList").clicked()
        var workspace = findChild(subject, "fullParameterListPage")
        tryCompare(workspace, "observedRevision", manager.snapshotRevision)
        workspace.selectParameter("TEST_0000"); workspace.editDraft("15"); workspace.applySelected()
        var ticket = workspace.pendingWrites.TEST_0000
        verify(ticket !== undefined)
        var result = {id:ticket.id, parameter:"TEST_0000", outcomeName:"Confirmed", reason:"", type:6,
            generation:ticket.identity.transferGeneration, connectionGeneration:ticket.identity.connectionGeneration,
            systemId:42, componentId:1, confirmedValue:15, hasConfirmedValue:true}
        findChild(workspace, "parameterBack").clicked()
        compare(subject.activeDetailPage, "vehicle-settings")
        verify(findChild(subject, "fullParameterListPage") === workspace)
        subject.open = false
        updateValue(vehicle, "TEST_0000", 15)
        transport.parameterWriteFinished(result)
        compare(workspace.modifiedCount, 0)
        compare(workspace.editorState, "Confirmed")
        subject.open = true
        subject.selectSection(subject.sectionIndex("Vehicle Settings"))
        findChild(subject, "openFullParameterList").clicked()
        wait(20)
        verify(findChild(subject, "fullParameterListPage") === workspace)
        compare(workspace.proposedValue, "15")
        compare(workspace.editorState, "Confirmed")
        compare(transport.refreshes, 0)
    }
    function test_editingLayoutsAndThemes() {
        editingSetup(6, 10)
        subject.editDraft("15")
        for (var i = 0; i < 2; ++i) {
            subject.darkTheme = i === 0
            subject.width = i === 0 ? 1300 : 900
            wait(20)
            verify(findChild(subject, "parameterApply").enabled)
            verify(findChild(subject, "parameterEditor").width > 100)
            grabImage(subject).save(decodeURIComponent(Qt.resolvedUrl("../../build/phase4-editor-" + (i === 0 ? "dark" : "light") + ".png").toString().replace(/^file:\/\/\//, "")))
        }
        subject.width = 480; subject.height = 800; wait(20)
        verify(findChild(subject, "parameterEditor").width > 100)
        verify(findChild(subject, "parameterList").height > 100)
        subject.applySelected()
        compare(subject.editorState, "Pending")
    }
    function test_initialCompleteSnapshotAndStagingExclusion() {
        populate(3)
        makeReady(vehicle)
        subject = createTemporaryObject(page, test)
        compare(subject.parameterNames.length, 3)
        compare(transport.reads.maps, 1)
        subject.destroy()
        subject = null
        manager.hasCompleteSnapshot = false
        subject = createTemporaryObject(page, test)
        compare(subject.parameterNames.length, 0)
        compare(transport.reads.maps, 1) // no full-map read without a complete snapshot
        compare(transport.refreshes, 0)
    }
    function test_snapshotReplacementPreservesOrClearsSelection() {
        subject = createTemporaryObject(page, test)
        populate(3)
        subject.selectParameter("TEST_0001")
        publish(vehicle, {TEST_0001: {name:"TEST_0001", value:4.5, type:9, index:0}, Z_LAST: {name:"Z_LAST", value:8, type:1, index:1}})
        compare(subject.selectedName, "TEST_0001")
        compare(subject.selectedRecord.value, 4.5)
        compare(subject.selectedRecord.type, 9)
        compare(subject.parameterNames.join(","), "TEST_0001,Z_LAST")
        publish(vehicle, {Z_LAST: {name:"Z_LAST", value:8, type:1, index:0}})
        compare(subject.selectedName, "")
        compare(subject.selectedRecord.value, undefined)
        publish(vehicle, {})
        compare(subject.parameterNames.length, 0)
        compare(subject.filteredNames.length, 0)
    }
    function test_individualUpdateOnlyReadsAffectedVisibleRow() {
        subject = createTemporaryObject(page, test)
        populate(1500); makeReady(vehicle)
        subject.selectParameter("TEST_0000")
        var list = findChild(subject, "parameterList")
        tryCompare(list, "count", 1500); wait(30)
        var first = list.itemAtIndex(0), second = list.itemAtIndex(1)
        verify(first !== null); verify(second !== null)
        var maps = transport.reads.maps
        var otherReads = transport.reads.values.TEST_0001
        var affectedReads = transport.reads.values.TEST_0000
        var names = subject.parameterNames, filtered = subject.filteredNames
        updateValue(vehicle, "TEST_0000", 123)
        compare(first.record.value, 123)
        compare(subject.selectedRecord.value, 123)
        wait(20)
        compare(transport.reads.maps, maps)
        compare(transport.reads.values.TEST_0001, otherReads)
        verify(transport.reads.values.TEST_0000 > affectedReads)
        verify(subject.parameterNames === names)
        verify(subject.filteredNames === filtered)
        compare(subject.selectedName, "TEST_0000")
    }
    function test_revisionFallbackAndPublicationFanout() {
        subject = createTemporaryObject(page, test)
        populate(1500); wait(30)
        var maps = transport.reads.maps
        var names = subject.parameterNames
        // A revision-only change must be reconciled once, without replacing names.
        transport.records.TEST_0000.value = 77
        manager.snapshotRevision++
        tryCompare(subject, "observedRevision", manager.snapshotRevision)
        compare(transport.reads.maps, maps + 1)
        verify(subject.parameterNames === names)
        var list = findChild(subject, "parameterList")
        compare(list.itemAtIndex(0).record.value, 77)
        subject.selectParameter("TEST_1200")
        list.positionViewAtIndex(1200, ListView.Center)
        var scrollPosition = list.contentY
        maps = transport.reads.maps
        for (var i = 0; i < 3; ++i) publish(vehicle, transport.records)
        wait(20)
        compare(transport.reads.maps, maps + 3)
        verify(subject.parameterNames === names)
        compare(subject.selectedName, "TEST_1200")
        compare(list.contentY, scrollPosition)
        maps = transport.reads.maps
        manager.snapshotPublished(manager.snapshotRevision)
        manager.snapshotChanged()
        wait(20)
        compare(transport.reads.maps, maps)
    }
    function test_revisionGapCannotHideReplacementSnapshot() {
        subject = createTemporaryObject(page, test)
        populate(3)
        subject.selectParameter("TEST_0000")
        // Model C++ reentrancy: a replacement snapshot is installed, then a
        // value arrives before snapshotPublished can reach this observer.
        transport.records = {NEW: {name:"NEW", value:1, type:6, index:0}}
        manager.snapshotRevision++
        updateValue(vehicle, "NEW", 2)
        tryCompare(subject, "parameterNames", ["NEW"])
        compare(subject.selectedName, "")
        subject.selectParameter("NEW")
        compare(subject.selectedRecord.value, 2)
        var maps = transport.reads.maps
        manager.snapshotPublished(manager.snapshotRevision)
        wait(20)
        compare(transport.reads.maps, maps)
        compare(transport.refreshes, 0)
    }
    function test_searchAndScrollingDuringUpdates() {
        subject = createTemporaryObject(page, test)
        populate(1500)
        var search = findChild(subject, "parameterSearch")
        search.text = "test_1"
        compare(subject.filteredNames.length, 500)
        subject.selectedGroup = "TEST"
        subject.selectParameter("TEST_1499")
        var filtered = subject.filteredNames, maps = transport.reads.maps
        updateValue(vehicle, "TEST_1499", 55)
        compare(subject.selectedRecord.value, 55)
        verify(subject.filteredNames === filtered)
        var list = findChild(subject, "parameterList")
        tryCompare(list, "count", 500); wait(20)
        list.positionViewAtIndex(499, ListView.End)
        verify(list.contentY > 0)
        compare(transport.reads.maps, maps)
        compare(transport.refreshes, 0)
    }
    function test_vehicleAssignmentReplacementAndLateSignals() {
        testVehicle = null
        subject = createTemporaryObject(page, test)
        compare(subject.parameterNames.length, 0)
        populate(2)
        subject.vehicle = vehicle
        tryCompare(subject, "observedRevision", manager.snapshotRevision)
        subject.selectParameter("TEST_0000")
        subject.searchText = "test"
        subject.selectedGroup = "TEST"
        var replacement = createTemporaryObject(sourceComponent, test)
        publish(replacement, {OTHER: {name:"OTHER", value:90, type:6, index:0}})
        makeReady(replacement)
        subject.vehicle = replacement
        compare(subject.selectedName, "")
        tryCompare(subject, "parameterNames", ["OTHER"])
        compare(subject.searchText, ""); compare(subject.selectedGroup, "")
        compare(findChild(subject, "parameterSearch").text, "")
        subject.selectParameter("OTHER")
        var maps = replacement.mavlink.reads.maps
        updateValue(vehicle, "TEST_0000", 999)
        manager.snapshotPublished(manager.snapshotRevision)
        manager.snapshotChanged(); wait(20)
        compare(subject.parameterNames.join(","), "OTHER")
        compare(subject.selectedRecord.value, 90)
        compare(replacement.mavlink.reads.maps, maps)
        subject.vehicle = null
        compare(subject.parameterNames.length, 0)
        compare(subject.selectedName, "")
        compare(replacement.mavlink.refreshes, 0); compare(transport.refreshes, 0)
    }
    function test_disconnectHeartbeatAndReconnect_data() {
        return [{tag:"disconnect"}, {tag:"heartbeat-loss"}]
    }
    function test_disconnectHeartbeatAndReconnect(data) {
        subject = createTemporaryObject(page, test)
        populate(4); makeReady(vehicle)
        subject.selectParameter("TEST_0001")
        var names = subject.parameterNames, maps = transport.reads.maps
        manager.parametersReady = false; manager.syncState = 0
        transport.connected = false
        transport.connectionStatusChanged()
        manager.snapshotChanged(); wait(20)
        verify(subject.stale); verify(!subject.ready)
        verify(subject.parameterNames === names)
        compare(subject.selectedRecord.value, 1)
        verify(!findChild(subject, "parameterEditor").enabled)
        manager.snapshotProvenance = {connectionGeneration:2, transferGeneration:5, current:false}
        manager.snapshotChanged()
        transport.connected = true; manager.syncState = 1
        manager.receivedParameterCount = 1; manager.expectedParameterCount = 8; manager.progress = 12
        var status = findChild(subject, "parameterSyncStatus")
        compare(status.progressValue, 12)
        verify(status.countText.indexOf("1 / 8") >= 0)
        manager.syncState = 2
        manager.syncState = 4; manager.syncError = "Missing index 7"
        verify(status.detailText.indexOf("Missing index 7") >= 0)
        compare(subject.parameterNames.length, 4)
        wait(20); compare(transport.reads.maps, maps)
        manager.syncState = 1
        populate(8); makeReady(vehicle)
        verify(subject.ready); verify(!subject.stale)
        compare(subject.selectedName, "TEST_0001")
        compare(subject.parameterNames.length, 8)
        compare(transport.refreshes, 0)
    }
    function test_deactivationAndMissedPublications() {
        subject = createTemporaryObject(page, test)
        populate(3); subject.selectParameter("TEST_0001")
        var maps = transport.reads.maps
        subject.active = false
        populate(5); updateValue(vehicle, "TEST_0001", 88)
        wait(20); compare(transport.reads.maps, maps)
        subject.active = true
        tryCompare(subject, "observedRevision", manager.snapshotRevision)
        compare(subject.parameterNames.length, 5)
        compare(subject.selectedName, "TEST_0001")
        compare(subject.selectedRecord.value, 88)
        compare(transport.reads.maps, maps + 1)
        subject.active = false; subject.active = true; wait(20)
        compare(transport.reads.maps, maps + 1)
        subject.destroy(); subject = createTemporaryObject(page, test)
        compare(subject.parameterNames.length, 5)
        compare(transport.refreshes, 0)
    }
    function test_vehicleReplacementWhileInactive() {
        subject = createTemporaryObject(page, test)
        populate(3); subject.selectParameter("TEST_0001")
        subject.active = false
        var replacement = createTemporaryObject(sourceComponent, test)
        publish(replacement, {NEW: {name:"NEW", value:1, type:1, index:0}})
        subject.vehicle = replacement
        compare(subject.parameterNames.length, 0)
        compare(subject.selectedName, "")
        compare(replacement.mavlink.reads.maps, 0)
        populate(4); wait(20)
        compare(subject.parameterNames.length, 0)
        subject.active = true
        tryCompare(subject, "parameterNames", ["NEW"])
        compare(replacement.mavlink.reads.maps, 1)
        compare(replacement.mavlink.refreshes, 0)
        compare(transport.refreshes, 0)
    }
    function test_navigation() {
        subject = createTemporaryObject(oem, test)
        verify(subject !== null)
        subject.selectSection(subject.sectionIndex("Vehicle Settings"))
        compare(subject.activeDetailPage, "vehicle-settings")
        findChild(subject, "openFullParameterList").clicked()
        compare(subject.activeDetailPage, "full-parameter-list")
        var full = findChild(subject, "fullParameterListPage")
        verify(full !== null); verify(full.height > 100)
        findChild(full, "parameterBack").clicked()
        compare(subject.activeDetailPage, "vehicle-settings")
        findChild(subject, "openFullParameterList").clicked()
        verify(findChild(subject, "fullParameterListPage") !== null)
        subject.open = false; subject.open = true
        compare(subject.activeSectionIndex, -1)
        compare(transport.refreshes, 0)
    }
    function test_states_data() {
        return [
            {tag:"disconnected", state:0, connected:false, ready:false, stale:false},
            {tag:"idle", state:0, connected:true, ready:false, stale:false},
            {tag:"loading", state:1, connected:true, ready:false, stale:false},
            {tag:"recovering", state:2, connected:true, ready:false, stale:false},
            {tag:"ready", state:3, connected:true, ready:true, stale:false},
            {tag:"failed", state:4, connected:true, ready:false, stale:false},
            {tag:"stale", state:1, connected:true, ready:false, stale:true}
        ]
    }
    function test_states(data) {
        transport.connected = data.connected; manager.syncState = data.state
        manager.parametersReady = data.ready; manager.syncError = data.state === 4 ? "Recovery failed" : ""
        subject = createTemporaryObject(page, test)
        compare(subject.filteredNames.length, 0)
        verify(!findChild(subject, "parameterEditor").enabled)
        if (data.ready || data.stale) { populate(2); subject.selectParameter("TEST_0000") }
        if (data.ready) makeReady(vehicle)
        compare(subject.ready, data.ready); compare(subject.stale, data.stale)
        var status = findChild(subject, "parameterSyncStatus")
        compare(status.syncStateValue, data.state)
        if (data.state === 4) verify(status.detailText.indexOf("Recovery failed") >= 0)
        compare(findChild(subject, "parameterEditor").enabled, data.ready)
        verify(!findChild(subject, "parameterApply").enabled)
        compare(transport.refreshes, 0)
    }
    function test_listSearchSelectionAndScroll() {
        subject = createTemporaryObject(page, test)
        populate(1500)
        compare(subject.filteredNames[0], "TEST_0000")
        var list = findChild(subject, "parameterList")
        tryCompare(list, "count", 1500)
        wait(50)
        var first = list.itemAtIndex(0)
        verify(first !== null)
        mouseClick(first)
        compare(subject.selectedName, "TEST_0000")
        list.positionViewAtIndex(1499, ListView.End)
        verify(list.contentY > 0)
        subject.selectParameter("TEST_1499")
        compare(subject.selectedRecord.value, 1499)
        var search = findChild(subject, "parameterSearch")
        search.text = "test_1499"
        compare(subject.filteredNames.length, 1)
        search.text = "missing"; compare(subject.filteredNames.length, 0)
        search.text = ""; compare(subject.filteredNames.length, 1500)
        var names = subject.parameterNames
        manager.snapshotRevision++
        compare(subject.parameterNames, names)
        compare(transport.refreshes, 0)
    }
    function test_resizeAndThemes() {
        subject = createTemporaryObject(page, test)
        populate(10)
        subject.selectParameter("TEST_0000")
        for (var i = 0; i < 2; ++i) {
            subject.darkTheme = i === 0
            subject.width = i === 0 ? 1300 : 900
            subject.height = i === 0 ? 800 : 600
            wait(20)
            verify(findChild(subject, "parameterList").width > 100)
            verify(findChild(subject, "parameterList").height > 100)
            compare(subject.palette.darkMode, subject.darkTheme)
            grabImage(subject).save(decodeURIComponent(Qt.resolvedUrl("../../build/full-parameters-" + (i === 0 ? "dark" : "light") + ".png").toString().replace(/^file:\/\/\//, "")))
        }
        subject.width = 640; wait(20)
        verify(findChild(subject, "parameterList").width > 100)
        subject.width = 480; subject.height = 800; wait(20)
        verify(findChild(subject, "parameterList").width > 100)
        verify(findChild(subject, "parameterList").height > 100)
    }
}
