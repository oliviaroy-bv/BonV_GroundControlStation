pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore as Core
import "../accessories"

Item {
    id: root
    objectName: "fullParameterListPage"
    property var vehicle: null
    property bool darkTheme: true
    property var palette: Palette { darkMode: root.darkTheme }
    property bool active: true
    readonly property var manager: vehicle ? vehicle.parameterManager : null
    readonly property var transport: vehicle ? vehicle.mavlink : null
    readonly property var configuration: transport ? (transport.parameterConfiguration || null) : null
    property bool configurationVisible: false
    property var firmwareIdentity: transport ? transport.console : null
    readonly property bool ready: !!manager && manager.parametersReady && !!transport && transport.connected
    readonly property bool stale: !!manager && manager.hasCompleteSnapshot && !ready
    // Names are a presentation index only. Values remain owned by ParameterManager.
    property var parameterNames: []
    property var filteredNames: []
    property string selectedName: ""
    property string selectedGroup: ""
    property string searchText: ""
    property string navigationView: "all"
    property int selectedType: -1
    // Derived search fields only: no confirmed values or independent parameter model.
    property var filterFields: ({})
    property url favoritesSettingsLocation: ""
    property var favoriteNamespaces: ({})
    property string snapshotFavoriteNamespace: "unknown/classic-v1"
    readonly property var favoriteNames: own(favoriteNamespaces, snapshotFavoriteNamespace) || []
    readonly property int availableFavoriteCount: favoriteNames.filter(function(name) { return parameterNames.indexOf(name) >= 0 }).length
    readonly property int pendingCount: Object.keys(pendingWrites).length
    readonly property int unsubmittedCount: Object.keys(drafts).filter(function(name) { return !own(pendingWrites, name) }).length
    readonly property string modifiedMembership: JSON.stringify(Object.keys(Object.assign(Object.create(null), drafts, pendingWrites)).sort())
    readonly property var typeChoices: [-1, 1, 2, 3, 4, 5, 6, 9]
    readonly property string emptyText: !parameterNames.length ? "No complete parameter snapshot available"
        : searchText.trim() || selectedType !== -1 ? "No matching search results"
        : selectedGroup ? "No parameters in selected group"
        : navigationView === "modified" ? "No modified parameters"
        : navigationView === "favorites" ? (favoriteNames.length ? "No favorite parameters in this snapshot" : "No favorites yet")
        : "No matching parameters"
    property string proposedValue: ""
    // Local proposals and result presentation only; ParameterManager owns all writes.
    property var drafts: ({})
    property var pendingWrites: ({})
    property var writeFeedback: ({})
    readonly property var selectedDraft: own(drafts, selectedName)
    readonly property var selectedPending: own(pendingWrites, selectedName)
    readonly property var selectedFeedback: own(writeFeedback, selectedName)
    readonly property int modifiedCount: Object.keys(drafts).length
    readonly property int discardableCount: Object.keys(drafts).filter(function(name) { return !own(pendingWrites, name) }).length
    readonly property var provenance: manager ? manager.snapshotProvenance : ({})
    readonly property string sourceEpoch: JSON.stringify([provenance.systemId, provenance.componentId,
        provenance.connectionGeneration, provenance.transferGeneration, provenance.encoding, provenance.sourceConnectionGeneration])
    readonly property bool writableReady: active && ready && sourceMatches && provenance.current === true
        && (!configuration || !configuration.applying)
        && provenance.sourceConnectionGeneration === transport.connectionGeneration
    readonly property string inputError: selectedName ? validateText(proposedValue, selectedRecord.type) : ""
    readonly property bool canApply: writableReady && !!selectedDraft && !selectedPending
        && !selectedDraft.review && inputError === ""
    readonly property string editorState: selectedPending ? "Pending" : selectedFeedback ? selectedFeedback.state
        : selectedDraft ? "Modified" : "Unmodified"
    // Presentation bookkeeping, never a second snapshot or synchronization state.
    property var displayedVehicle: null
    property var displayedManager: null
    property var displayedTransport: null
    property var observedRevision: -1
    property int snapshotDisplayRevision: 0
    property int selectedDisplayRevision: 0
    property bool completed: false
    readonly property bool sourceMatches: displayedVehicle === vehicle
        && displayedManager === manager && displayedTransport === transport
    readonly property var selectedRecord: {
        var revision = selectedDisplayRevision
        return sourceMatches && transport && selectedName ? transport.parameterValue(selectedName) : ({})
    }
    readonly property var groupCounts: {
        var found = Object.create(null)
        for (var i = 0; i < parameterNames.length; ++i) {
            var separator = parameterNames[i].indexOf("_")
            if (separator > 0) {
                var prefix = parameterNames[i].substring(0, separator)
                found[prefix] = (found[prefix] || 0) + 1
            }
        }
        return found
    }
    readonly property var groups: Object.keys(groupCounts).sort()
    signal backRequested()
    signal recordChanged(string name)

    function reloadNames() {
        if (!completed || !active) return
        var revision = manager ? manager.snapshotRevision : -1
        if (sourceMatches && observedRevision === revision) return
        displayedVehicle = vehicle
        displayedManager = manager
        displayedTransport = transport
        var snapshot = manager && manager.hasCompleteSnapshot && transport ? transport.parameters() : ({})
        var names = Object.keys(snapshot).sort()
        updateFavoriteNamespace()
        var fields = Object.create(null)
        for (var i = 0; i < names.length; ++i) fields[names[i]] = searchFields(names[i], snapshot[names[i]])
        filterFields = fields
        // Repeated publications with unchanged membership keep the ListView model
        // and its scroll position intact, even if values/types have changed.
        if (names.length !== parameterNames.length || names.some(function(name, index) { return name !== parameterNames[index] })) {
            parameterNames = names
            filterNames()
        }
        if (parameterNames.indexOf(selectedName) < 0) selectedName = ""
        if (groups.indexOf(selectedGroup) < 0) selectedGroup = ""
        filterNames()
        observedRevision = revision
        snapshotDisplayRevision++
        selectedDisplayRevision++
        reconcileDrafts(snapshot)
        syncEditor()
    }
    function resetSource() {
        reconcile.stop()
        if (displayedTransport && displayedTransport.parameterConfiguration && displayedTransport.parameterConfiguration.applying)
            displayedTransport.parameterConfiguration.stop()
        configurationVisible = false
        if (completed) discardDialog.close()
        displayedVehicle = null
        displayedManager = null
        displayedTransport = null
        observedRevision = -1
        selectedName = ""
        selectedGroup = ""
        searchText = ""
        navigationView = "all"
        selectedType = -1
        filterFields = ({})
        snapshotFavoriteNamespace = "unknown/classic-v1"
        proposedValue = ""
        drafts = ({})
        pendingWrites = ({})
        writeFeedback = ({})
        parameterNames = []
        filteredNames = []
        if (completed && active) reconcile.restart()
    }
    function filterNames() {
        var query = searchText.trim().toUpperCase()
        var names = parameterNames.filter(function(name) {
            var fields = own(filterFields, name)
            return (!query || (fields ? fields.text : name.toUpperCase()).indexOf(query) >= 0)
                && (!selectedGroup || name.indexOf(selectedGroup + "_") === 0)
                && (selectedType === -1 || (fields && fields.type === selectedType))
                && (navigationView !== "modified" || !!own(drafts, name) || !!own(pendingWrites, name))
                && (navigationView !== "favorites" || isFavorite(name))
        })
        if (names.length !== filteredNames.length || names.some(function(name, index) { return name !== filteredNames[index] })) filteredNames = names
    }
    function searchFields(name, record) {
        var separator = name.indexOf("_")
        return {type:record.type, text:[name, separator > 0 ? name.substring(0, separator) : "",
            typeof record.description === "string" ? record.description : "",
            typeof record.category === "string" ? record.category : ""].join("\n").toUpperCase()}
    }
    function updateSearchFields(name, record) {
        var previous = own(filterFields, name)
        if (!previous) return
        var fields = searchFields(name, record)
        if (fields.type !== previous.type || fields.text !== previous.text) {
            filterFields = put(filterFields, name, fields)
            filterNames()
        }
    }
    function selectView(view) { navigationView = view; selectedGroup = "" }
    function resetFilters() { searchText = ""; selectedType = -1; selectedGroup = "" }
    function parameterState(name) {
        var status = own(writeFeedback, name)
        return own(pendingWrites, name) ? "Pending" : status ? status.state : own(drafts, name) ? "Modified" : ""
    }
    function updateFavoriteNamespace() {
        // Existing console identity follows only the selected autopilot heartbeat.
        // Never infer firmware from parameter names or wire encoding.
        // Unidentified firmware deliberately shares unknown/classic-v1; it is not
        // automatically migrated to a known family when identity becomes available.
        if (!manager || !manager.parametersReady || !transport || !transport.connected) return
        var console = firmwareIdentity
        snapshotFavoriteNamespace = console && console.commandMode ? "ardupilot/classic-v1"
            : console && console.modeLabel === "PX4 NSH Shell" ? "px4/classic-v1" : "unknown/classic-v1"
    }
    function loadFavorites() {
        favoriteSettings.sync()
        var clean = Object.create(null)
        try {
            var saved = JSON.parse(String(favoriteSettings.value("selections", "{}")))
            if (saved && typeof saved === "object" && !Array.isArray(saved)) {
                Object.keys(saved).forEach(function(namespace) {
                    if (!Array.isArray(saved[namespace])) return
                    var unique = Object.create(null)
                    saved[namespace].forEach(function(name) {
                        if (typeof name === "string" && /^[!-~]{1,16}$/.test(name)) unique[name] = true
                    })
                    clean[namespace] = Object.keys(unique).sort()
                })
            }
        } catch (error) { /* Invalid preferences never invalidate vehicle data. */ }
        favoriteNamespaces = clean
    }
    function isFavorite(name) { return favoriteNames.indexOf(name) >= 0 }
    function setFavorite(name, enabled) {
        if (enabled && parameterNames.indexOf(name) < 0) return
        // Merge the latest settings before an explicit toggle; keystrokes never persist.
        loadFavorites()
        if (isFavorite(name) === enabled) return
        var names = favoriteNames.filter(function(item) { return item !== name })
        if (enabled) names.push(name)
        names.sort()
        favoriteNamespaces = put(favoriteNamespaces, snapshotFavoriteNamespace, names)
        favoriteSettings.setValue("selections", JSON.stringify(favoriteNamespaces))
        favoriteSettings.sync()
    }
    function toggleFavorite(name) { setFavorite(name, !isFavorite(name)) }
    function own(map, name) { return Object.prototype.hasOwnProperty.call(map, name) ? map[name] : null }
    function put(map, name, value) {
        var copy = Object.assign(Object.create(null), map)
        if (value === null) delete copy[name]
        else copy[name] = value
        return copy
    }
    function selectParameter(name) { selectedName = name; syncEditor() }
    function syncEditor() {
        var draft = own(drafts, selectedName)
        proposedValue = draft ? draft.text : selectedRecord.value === undefined ? "" : String(selectedRecord.value)
    }
    function feedback(name, state, reason) { writeFeedback = put(writeFeedback, name, {state:state, reason:reason || ""}) }
    function validateText(text, type) {
        var input = text.trim()
        if (!input) return "Enter a value."
        if (provenance.encoding !== 1 && provenance.encoding !== 2) return "Parameter encoding is unavailable."
        var ranges = {1:[0,255], 2:[-128,127], 3:[0,65535], 4:[-32768,32767], 5:[0,4294967295], 6:[-2147483648,2147483647]}
        if (type !== 9 && !own(ranges, type)) return "Unsupported parameter type."
        if (!/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$/.test(input)) return "Enter a finite decimal number."
        var value = Number(input)
        if (!isFinite(value)) return "NaN and infinity are not valid parameter values."
        if (type !== 9) {
            if (!/^[+-]?\d+$/.test(input)) return "Enter an integer without a fractional part or exponent."
            var range = ranges[type]
            if (value < range[0] || value > range[1]) return "Value must be between " + range[0] + " and " + range[1] + "."
            if (provenance.encoding === 1 && Math.fround(value) !== value) return "This integer cannot be represented exactly by the vehicle's numeric encoding."
        } else {
            var encoded = Math.fround(value)
            if (!isFinite(encoded)) return "Value exceeds the REAL32 range."
            if ((value !== 0 && encoded === 0) || (value === 0 && /[1-9]/.test(input.split(/[eE]/)[0])))
                return "Value is too small for REAL32 encoding."
        }
        return ""
    }
    function editDraft(text) {
        if (!writableReady || !selectedName || selectedPending || selectedRecord.value === undefined) return
        proposedValue = text
        var previous = own(drafts, selectedName)
        var draft = previous ? Object.assign({}, previous) : {type:selectedRecord.type, baseValue:selectedRecord.value, epoch:sourceEpoch, review:false}
        draft.text = text
        if (!validateText(text, draft.type) && Number(text) === selectedRecord.value && !draft.review)
            drafts = put(drafts, selectedName, null)
        else drafts = put(drafts, selectedName, draft)
        writeFeedback = put(writeFeedback, selectedName, null)
    }
    function discardDraft(name) {
        var parameter = name === undefined ? selectedName : name
        if (own(pendingWrites, parameter)) return // discarding is never backend cancellation
        drafts = put(drafts, parameter, null)
        writeFeedback = put(writeFeedback, parameter, null)
        syncEditor()
    }
    function discardAllDrafts() {
        var names = Object.keys(drafts)
        for (var i = 0; i < names.length; ++i) if (!own(pendingWrites, names[i])) discardDraft(names[i])
    }
    function requireReview(reason) {
        var copy = Object.assign(Object.create(null), drafts)
        for (var name in copy) copy[name] = Object.assign({}, copy[name], {review:true, reviewReason:reason})
        drafts = copy
        syncEditor()
    }
    function reconcileDraft(name, record) {
        var draft = own(drafts, name)
        if (!draft) return
        if (!record || record.value === undefined || record.type !== draft.type) {
            drafts = put(drafts, name, null)
            feedback(name, "Unmodified", record && record.value !== undefined ? "Parameter type changed; the old proposal was discarded." : "Parameter no longer exists; the proposal was discarded.")
        } else if (record.value !== draft.baseValue || draft.epoch !== sourceEpoch) {
            drafts = put(drafts, name, Object.assign({}, draft, {review:true,
                reviewReason: draft.epoch !== sourceEpoch ? "Snapshot changed. Review the proposal before applying." : "Confirmed value changed externally. Review the proposal before applying."}))
        }
    }
    function reconcileDrafts(snapshot) {
        var names = Object.keys(drafts)
        for (var i = 0; i < names.length; ++i) reconcileDraft(names[i], snapshot[names[i]])
    }
    function reviewProposal() {
        if (!writableReady || !selectedDraft || selectedPending) return
        var record = transport.parameterValue(selectedName)
        if (record.type !== selectedDraft.type || record.value === undefined) {
            reconcileDraft(selectedName, record); syncEditor(); return
        }
        drafts = put(drafts, selectedName, Object.assign({}, selectedDraft,
            {baseValue:record.value, epoch:sourceEpoch, review:false, reviewReason:""}))
        writeFeedback = put(writeFeedback, selectedName, null)
    }
    function applySelected() {
        var name = selectedName, draft = own(drafts, name)
        if (!name || !draft || own(pendingWrites, name)) return
        if (!writableReady) { feedback(name, "Invalid Request", "A connected vehicle and a current ready snapshot are required."); return }
        var record = transport.parameterValue(name)
        if (record.value === undefined || record.type !== draft.type) {
            reconcileDraft(name, record); selectedDisplayRevision++; syncEditor(); return
        }
        reconcileDraft(name, record)
        draft = own(drafts, name)
        if (draft.review) { feedback(name, "Modified", draft.reviewReason); return }
        var error = validateText(draft.text, record.type)
        if (error) { feedback(name, "Invalid Request", error); return }
        var originVehicle = vehicle, originManager = manager, originTransport = transport, epoch = sourceEpoch
        var identity = Object.assign({}, provenance)
        // The backend validates encoding again and defers transmission/results.
        var result = originTransport.requestParameterWrite(name, Number(draft.text), record.type)
        if (vehicle !== originVehicle || manager !== originManager || transport !== originTransport) return
        if (!result.accepted) { feedback(name, "Invalid Request", result.reason); return }
        pendingWrites = put(pendingWrites, name, {id:result.id, vehicle:originVehicle, manager:originManager,
            transport:originTransport, epoch:epoch, identity:identity, type:record.type, text:draft.text})
        feedback(name, "Pending", "Awaiting vehicle confirmation.")
    }
    function finishWrite(result) {
        var name = result.parameter, ticket = own(pendingWrites, name)
        if (!ticket || String(ticket.id) !== String(result.id) || ticket.vehicle !== vehicle
            || ticket.manager !== manager || ticket.transport !== transport
            || result.generation !== ticket.identity.transferGeneration
            || result.connectionGeneration !== ticket.identity.connectionGeneration
            || result.systemId !== ticket.identity.systemId || result.componentId !== ticket.identity.componentId
            || result.type !== ticket.type) return
        pendingWrites = put(pendingWrites, name, null)
        var labels = {Confirmed:"Confirmed", Rejected:"Rejected", TimedOut:"Timed Out", Cancelled:"Cancelled", InvalidRequest:"Invalid Request", TransportError:"Transport Error"}
        var state = labels[result.outcomeName] || "Invalid Request"
        var reason = result.reason || ""
        if (result.outcomeName === "TimedOut") reason = "Confirmation was not received; the vehicle may have applied the value. " + reason
        var current = ticket.epoch === sourceEpoch && ready && provenance.current === true
            && provenance.sourceConnectionGeneration === transport.connectionGeneration
        var record = transport.parameterValue(name)
        var draft = own(drafts, name)
        var encoded = ticket.type === 9 ? Math.fround(Number(ticket.text)) : Number(ticket.text)
        if (result.outcomeName === "Confirmed" && current && record.type === ticket.type && record.value === encoded
            && draft && draft.text === ticket.text) {
            drafts = put(drafts, name, null)
            reason = "Vehicle confirmed " + String(record.value) + "."
        } else {
            reconcileDraft(name, record)
            if (!current) reason = "Result belongs to the previous snapshot. " + reason
            if (result.hasConfirmedValue) reason += " Vehicle reported " + String(result.confirmedValue) + "."
            if (result.outcomeName === "Confirmed" && current) {
                state = "Review required"
                reason = "The write was confirmed, but the current snapshot differs from the proposal. Review the current value. " + reason
                var remaining = own(drafts, name)
                if (remaining) drafts = put(drafts, name, Object.assign({}, remaining, {review:true, reviewReason:reason}))
            }
        }
        if (!current && result.outcomeName !== "Cancelled") state = "Previous snapshot: " + state
        feedback(name, state, reason)
        if (name === selectedName) { selectedDisplayRevision++; syncEditor() }
    }
    function typeName(type) {
        var types = {1: "UINT8", 2: "INT8", 3: "UINT16", 4: "INT16", 5: "UINT32", 6: "INT32", 9: "REAL32"}
        return types[type] || (type === undefined ? "—" : "Type " + type)
    }
    onVehicleChanged: resetSource()
    onManagerChanged: resetSource()
    onTransportChanged: resetSource()
    onActiveChanged: {
        if (active) reconcile.restart()
        else { reconcile.stop(); if (completed) discardDialog.close() }
    }
    onSearchTextChanged: filterNames()
    onSelectedGroupChanged: filterNames()
    onNavigationViewChanged: filterNames()
    onSelectedTypeChanged: filterNames()
    onFavoriteNamesChanged: if (navigationView === "favorites") filterNames()
    onModifiedMembershipChanged: if (navigationView === "modified") filterNames()
    onReadyChanged: {
        if (!ready && completed) requireReview("Readiness was revoked. Review the proposal after parameters are ready.")
        else updateFavoriteNamespace()
    }
    onSourceEpochChanged: if (completed) requireReview("Snapshot generation changed. Review the proposal before applying.")
    Component.onCompleted: { completed = true; loadFavorites(); reloadNames() }
    Core.Settings { id: favoriteSettings; category: "FullParameterList/Favorites/v1"; location: root.favoritesSettingsLocation }
    Connections {
        target: root.firmwareIdentity || null
        function onChanged() { root.updateFavoriteNamespace() }
    }
    // snapshotChanged precedes parameterUpdated/snapshotPublished in C++.
    // Reconcile only revisions not accounted for by those specific notifications.
    // This also catches a missed/reentrant publication without copying a full map
    // for each value update or for readiness/provenance-only notifications.
    Timer { id: reconcile; interval: 0; onTriggered: root.reloadNames() }
    Connections {
        target: root.manager
        enabled: root.completed
        function onSnapshotChanged() { if (root.active) reconcile.restart() }
        function onSnapshotPublished() { root.reloadNames() }
        function onParameterUpdated(name, parameter) {
            if (!root.sourceMatches) return
            root.reconcileDraft(name, parameter)
            if (!root.active) return
            root.updateSearchFields(name, parameter)
            var revision = root.manager.snapshotRevision
            if (root.observedRevision === revision) return // publication's per-record fan-out
            // A gap can include a replacement snapshot whose publication was
            // interrupted by a reentrant observer. Let reconciliation reload it
            // rather than mistaking its next value notification for the whole change.
            if (revision !== root.observedRevision + 1) return
            root.observedRevision = revision
            root.recordChanged(name)
            if (name === root.selectedName) {
                root.selectedDisplayRevision++
                root.syncEditor()
            }
        }
    }
    Connections {
        target: root.transport
        function onParameterWriteFinished(result) { root.finishWrite(result) }
    }

    Rectangle { anchors.fill: parent; color: root.palette.background; radius: 5; border.color: root.palette.border }
    ColumnLayout {
        visible: !root.configurationVisible
        anchors.fill: parent; anchors.margins: 12; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            ActionButton { objectName: "parameterBack"; text: root.width < 760 ? "‹ Back" : "‹  Vehicle Settings"; onClicked: root.backRequested() }
            Label { text: "Full Parameter List"; color: root.palette.text; font.family: "Segoe UI"; font.pixelSize: root.width < 560 ? 16 : 20; font.bold: true; Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
            Label { objectName: "modifiedCount"; text: root.unsubmittedCount + " modified · " + root.pendingCount + " pending"; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 11 }
            ActionButton { objectName: "discardAllParameters"; text: "Discard All"; enabled: root.discardableCount > 0; onClicked: discardDialog.open() }
            ActionButton { objectName: "parameterConfigurationTools"; text: root.width < 760 ? "Tools" : "Configuration…"; enabled: !!root.configuration; onClicked: root.configurationVisible = true }
            Label { visible: root.width >= 760; text: root.stale ? "STALE · READ ONLY" : ""; color: root.palette.warning; font.family: "Segoe UI"; font.pixelSize: 11 }
        }
        ParameterSyncStatus { objectName: "parameterSyncStatus"; vehicle: root.vehicle; darkTheme: root.darkTheme; Layout.fillWidth: true }
        GridLayout {
            columns: root.width < 560 ? 1 : root.width < 760 ? 2 : 3
            Layout.fillWidth: true; Layout.fillHeight: true; columnSpacing: 10; rowSpacing: 10
            Panel {
                visible: root.width >= 760
                Layout.preferredWidth: root.width < 1000 ? 125 : 170
                Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 6
                    Label { text: "PARAMETERS"; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 10; font.bold: true }
                    ActionButton { objectName: "viewAllParameters"; text: "All Parameters (" + root.parameterNames.length + ")"; highlighted: root.navigationView === "all"; Layout.fillWidth: true; onClicked: root.selectView("all") }
                    ActionButton { objectName: "viewModifiedParameters"; text: "Modified (" + JSON.parse(root.modifiedMembership).length + ")"; highlighted: root.navigationView === "modified"; Layout.fillWidth: true; onClicked: root.selectView("modified") }
                    ActionButton { objectName: "viewFavoriteParameters"; text: "Favorites (" + root.availableFavoriteCount + ")"; highlighted: root.navigationView === "favorites"; Layout.fillWidth: true; onClicked: root.selectView("favorites") }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.Wrap; font.family: "Segoe UI"; font.pixelSize: 10; color: root.palette.muted
                        text: "Favorites: " + (root.snapshotFavoriteNamespace.indexOf("ardupilot/") === 0 ? "ArduPilot" : root.snapshotFavoriteNamespace.indexOf("px4/") === 0 ? "PX4" : "Unidentified firmware")
                    }
                    Label { text: "NAME PREFIXES"; visible: root.groups.length > 0; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 10 }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        model: root.groups
                        delegate: ActionButton {
                            required property string modelData
                            width: ListView.view.width; text: modelData + " (" + root.groupCounts[modelData] + ")"
                            highlighted: root.selectedGroup === modelData
                            onClicked: root.selectedGroup = modelData
                        }
                        ScrollBar.vertical: ScrollBar {}
                    }
                }
            }
            Panel {
                Layout.fillWidth: true; Layout.fillHeight: true
                Layout.minimumHeight: 100
                Layout.preferredHeight: root.width < 560 ? 350 : -1
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 8
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        TextField {
                            objectName: "parameterSearch"; Layout.fillWidth: true; Layout.minimumWidth: 0
                            font.family: "Segoe UI"; font.pixelSize: 12
                            placeholderTextColor: root.palette.muted
                            placeholderText: "Search names, prefixes or available metadata…"
                            text: root.searchText
                            onTextChanged: root.searchText = text
                            color: root.palette.text; selectByMouse: true
                            background: Rectangle { color: root.palette.background; radius: 4; border.color: parent.activeFocus ? root.palette.accent : root.palette.border }
                        }
                        ActionButton { objectName: "clearParameterSearch"; text: "Clear"; enabled: root.searchText !== ""; onClicked: root.searchText = "" }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        FilterCombo {
                            objectName: "parameterViewFilter"; visible: root.width < 760; Layout.fillWidth: true
                            model: ["All Parameters", "Modified", "Favorites"]
                            currentIndex: ["all", "modified", "favorites"].indexOf(root.navigationView)
                            onActivated: root.selectView(["all", "modified", "favorites"][currentIndex])
                        }
                        FilterCombo {
                            objectName: "parameterTypeFilter"; Layout.fillWidth: true
                            model: ["All types", "UINT8", "INT8", "UINT16", "INT16", "UINT32", "INT32", "REAL32"]
                            currentIndex: root.typeChoices.indexOf(root.selectedType)
                            onActivated: root.selectedType = root.typeChoices[currentIndex]
                            Accessible.name: "Parameter type"
                        }
                        ActionButton {
                            objectName: "resetParameterFilters"; text: "Reset filters"
                            enabled: root.searchText !== "" || root.selectedType !== -1 || root.selectedGroup !== ""
                            onClicked: root.resetFilters()
                        }
                    }
                    FilterCombo {
                        objectName: "parameterGroupFilter"; visible: root.width < 760
                        Layout.fillWidth: true
                        model: ["All prefixes"].concat(root.groups.map(function(group) { return group + " (" + root.groupCounts[group] + ")" }))
                        currentIndex: root.groups.indexOf(root.selectedGroup) + 1
                        onActivated: root.selectedGroup = currentIndex === 0 ? "" : root.groups[currentIndex - 1]
                    }
                    Label { text: root.filteredNames.length + " / " + root.parameterNames.length + " parameters"; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 11 }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Item { Layout.preferredWidth: 24 }
                        Label { text: "NAME"; Layout.fillWidth: true; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 10 }
                        Label { text: "VALUE"; Layout.preferredWidth: 90; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 10 }
                        Label { text: "TYPE"; Layout.preferredWidth: 60; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 10 }
                        Label { text: "INDEX"; Layout.preferredWidth: 38; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 10 }
                    }
                    ListView {
                        id: list
                        objectName: "parameterList"
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true; reuseItems: true; boundsBehavior: Flickable.StopAtBounds
                        model: root.filteredNames
                        ScrollBar.vertical: ScrollBar {}
                        delegate: ItemDelegate {
                            id: row
                            required property string modelData
                            property int valueDisplayRevision: 0
                            readonly property var record: {
                                var snapshotRevision = root.snapshotDisplayRevision
                                var valueRevision = valueDisplayRevision
                                return root.sourceMatches && root.transport ? root.transport.parameterValue(modelData) : ({})
                            }
                            Connections {
                                target: root
                                function onRecordChanged(name) {
                                    if (name === row.modelData) row.valueDisplayRevision++
                                }
                            }
                            width: list.width; height: 34; padding: 4
                            onClicked: root.selectParameter(modelData)
                            background: Rectangle {
                                color: root.selectedName === row.modelData ? root.palette.accentSoft : row.hovered ? root.palette.panelAlt : "transparent"
                                border.color: root.selectedName === row.modelData ? root.palette.accent : "transparent"; radius: 3
                            }
                            contentItem: RowLayout {
                                spacing: 8
                                ActionButton {
                                    objectName: "favoriteRow_" + row.modelData
                                    Layout.preferredWidth: 24; Layout.preferredHeight: 26; padding: 0
                                    text: root.isFavorite(row.modelData) ? "★" : "☆"
                                    highlighted: root.isFavorite(row.modelData)
                                    Accessible.name: (root.isFavorite(row.modelData) ? "Remove favorite " : "Favorite ") + row.modelData
                                    onClicked: root.toggleFavorite(row.modelData)
                                }
                                Label { text: row.modelData; Layout.fillWidth: true; elide: Text.ElideRight; color: root.palette.text; font.family: "Segoe UI"; font.pixelSize: 12 }
                                Label {
                                    visible: root.width >= 1000 && root.parameterState(row.modelData) !== ""
                                    text: root.parameterState(row.modelData)
                                    color: text === "Confirmed" ? root.palette.success : text === "Modified" || text === "Pending" ? root.palette.accent : root.palette.warning
                                    font.family: "Segoe UI"; font.pixelSize: 10
                                }
                                Label { text: row.record.value === undefined ? "—" : String(row.record.value); Layout.preferredWidth: 90; elide: Text.ElideRight; color: root.palette.text; font.family: "Segoe UI"; font.pixelSize: 12 }
                                Label { text: root.typeName(row.record.type); Layout.preferredWidth: 60; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 10 }
                                Label { text: row.record.index === undefined ? "—" : row.record.index; Layout.preferredWidth: 38; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 10 }
                            }
                        }
                        Label {
                            anchors.centerIn: parent; width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
                            visible: list.count === 0
                            objectName: "parameterEmptyState"
                            text: root.emptyText
                            color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 12
                        }
                    }
                }
            }
            Panel {
                Layout.fillWidth: root.width < 560
                Layout.preferredWidth: root.width < 560 ? -1 : root.width < 1000 ? 210 : 270
                Layout.preferredHeight: root.width < 560 ? 260 : -1
                Layout.fillHeight: root.width >= 560
                ScrollView {
                    anchors.fill: parent; anchors.margins: 12; contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width; spacing: 12
                        Label { text: "PARAMETER DETAILS"; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 10 }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: root.selectedName || "Select a parameter"; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; color: root.palette.accent; font.family: "Segoe UI"; font.pixelSize: 16; font.bold: true }
                            ActionButton {
                                objectName: "favoriteSelectedParameter"; enabled: root.selectedName !== ""
                                text: root.isFavorite(root.selectedName) ? "★" : "☆"
                                highlighted: root.isFavorite(root.selectedName)
                                Accessible.name: root.isFavorite(root.selectedName) ? "Remove selected favorite" : "Favorite selected parameter"
                                onClicked: root.toggleFavorite(root.selectedName)
                            }
                        }
                        Label { text: "Confirmed value"; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 11 }
                        Label { text: root.selectedRecord.value === undefined ? "—" : String(root.selectedRecord.value); color: root.palette.text; font.family: "Segoe UI"; font.pixelSize: 18 }
                        Label { text: root.typeName(root.selectedRecord.type) + "   ·   Index " + (root.selectedRecord.index === undefined ? "—" : root.selectedRecord.index); color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 11 }
                        Label { text: "Proposed value"; color: root.palette.muted; font.family: "Segoe UI"; font.pixelSize: 11 }
                        TextField {
                            objectName: "parameterEditor"; Layout.fillWidth: true
                            font.family: "Segoe UI"; font.pixelSize: 12
                            enabled: root.writableReady && root.selectedName !== "" && !root.selectedPending
                            text: root.proposedValue; onTextEdited: root.editDraft(text)
                            color: root.palette.text; selectByMouse: true
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            background: Rectangle { color: root.palette.background; radius: 4; border.color: root.selectedDraft ? root.palette.accent : root.palette.border }
                        }
                        Label {
                            objectName: "parameterInputError"; Layout.fillWidth: true; wrapMode: Text.Wrap
                            visible: !!root.selectedDraft && root.inputError !== ""
                            text: root.inputError; color: root.palette.danger; font.family: "Segoe UI"; font.pixelSize: 11
                        }
                        Label {
                            objectName: "parameterReviewReason"; Layout.fillWidth: true; wrapMode: Text.Wrap
                            visible: !!root.selectedDraft && root.selectedDraft.review
                            text: root.selectedDraft && root.selectedDraft.review ? root.selectedDraft.reviewReason : ""
                            color: root.palette.warning; font.family: "Segoe UI"; font.pixelSize: 11
                        }
                        ActionButton {
                            objectName: "parameterReview"; text: "Review proposal"
                            visible: !!root.selectedDraft && root.selectedDraft.review
                            enabled: root.writableReady && !root.selectedPending
                            onClicked: root.reviewProposal()
                        }
                        RowLayout {
                            ActionButton { objectName: "parameterApply"; text: "Apply"; enabled: root.canApply; onClicked: root.applySelected() }
                            ActionButton { objectName: "parameterDiscard"; text: "Discard"; enabled: !!root.selectedDraft && !root.selectedPending; onClicked: root.discardDraft() }
                        }
                        BusyIndicator { running: !!root.selectedPending; visible: running; implicitWidth: 24; implicitHeight: 24 }
                        Label {
                            objectName: "parameterFeedback"; Layout.fillWidth: true; wrapMode: Text.Wrap
                            text: root.editorState + (root.selectedFeedback && root.selectedFeedback.reason ? "\n" + root.selectedFeedback.reason : "")
                                + (!root.writableReady ? "\nEditing requires a connected vehicle and a current ready snapshot." : "")
                            color: root.selectedPending ? root.palette.accent : root.editorState === "Confirmed" ? root.palette.success
                                : root.editorState === "Unmodified" ? root.palette.muted : root.palette.warning
                            font.family: "Segoe UI"; font.pixelSize: 11
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }
    ParameterConfigurationPanel {
        objectName: "parameterConfigurationPanel"
        anchors.fill: parent
        visible: root.configurationVisible
        controller: root.configuration
        colors: root.palette
        draftNames: Object.keys(root.drafts)
        hasCompleteSnapshot: !!root.manager && root.manager.hasCompleteSnapshot
        stale: root.stale
        onBackRequested: root.configurationVisible = false
    }
    component FilterCombo: ComboBox {
        id: filterCombo
        implicitHeight: 30
        font.family: "Segoe UI"; font.pixelSize: 11
        palette.text: root.palette.text
        palette.buttonText: root.palette.text
        palette.base: root.palette.panel
        palette.button: root.palette.panelAlt
        palette.window: root.palette.panel
        palette.highlight: root.palette.accentSoft
        palette.highlightedText: root.palette.text
        background: Rectangle {
            color: root.palette.panelAlt; radius: 4
            border.color: filterCombo.activeFocus ? root.palette.accent : root.palette.border
        }
    }
    Dialog {
        id: discardDialog
        objectName: "discardAllParametersDialog"
        anchors.centerIn: parent
        width: Math.max(120, Math.min(360, root.width - 24))
        implicitHeight: 200
        modal: true
        title: "Discard local proposals?"
        standardButtons: Dialog.Discard | Dialog.Cancel
        onDiscarded: root.discardAllDrafts()
        header: Label { text: discardDialog.title; padding: 12; color: root.palette.text; font.bold: true; wrapMode: Text.Wrap }
        footer: DialogButtonBox { standardButtons: discardDialog.standardButtons; delegate: ActionButton {} }
        contentItem: Label {
            text: "Discard all proposals that are not pending? Pending writes will continue and their proposals will be kept."
            wrapMode: Text.Wrap; color: root.palette.text
        }
        background: Rectangle { color: root.palette.panel; radius: 5; border.color: root.palette.border }
    }
    component ActionButton: Button {
        id: action
        hoverEnabled: true
        padding: 8
        font.family: "Segoe UI"; font.pixelSize: 11
        contentItem: Text {
            text: action.text
            color: !action.enabled ? root.palette.muted : action.highlighted ? root.palette.accent : root.palette.text
            font: action.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 4
            color: action.highlighted ? root.palette.accentSoft : action.hovered && action.enabled ? root.palette.panelAlt : "transparent"
            border.color: action.highlighted ? root.palette.accent : root.palette.border
            opacity: action.enabled ? 1 : 0.5
        }
    }
    component Panel: Rectangle {
        color: Qt.rgba(root.palette.panel.r, root.palette.panel.g, root.palette.panel.b, 0.94)
        border.color: root.palette.border; border.width: 1; radius: 5
    }
}
