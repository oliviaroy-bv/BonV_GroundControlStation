import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.settings
import "../accessories"

Item {
    id: root

    property bool expanded: true
    property bool resizing: false
    property bool armed: false
    property bool darkTheme: true
    property int panelWidth: 300
    property string flightModeText: "--"
    property string relativeAltitudeText: "-- m"
    property string homeDistanceText: "-- m"
    property string climbText: "-- m/s"
    property string groundSpeedText: "-- m/s"
    property string flightTimeText: "--:--"
    property string flightDistanceText: "-- m"
    property string rollText: "-- deg"
    property string pitchText: "-- deg"
    property string yawText: "-- deg"
    property string batteryText: "-- %"
    property string satellitesText: "--"
    property string gpsFixText: "--"
    property string latitudeText: "--"
    property string longitudeText: "--"
    property string clockText: "--:--"
    property var messages: []
    property bool showMessages: false
    property string messageFilter: "all"
    property bool reorderMode: false
    property string draggingFieldId: ""
    property int layoutRows: 3
    property int layoutColumns: 2
    property var defaultFieldIds: [ "climb", "groundSpeed", "flightDistance", "windSpeed", "roll", "yaw" ]
    property var selectedFieldIds: defaultFieldIds.slice()
    readonly property int layoutSlotCount: Math.max(1, layoutRows * layoutColumns)
    readonly property var displayFieldIds: buildDisplayFieldIds()
    readonly property var allTelemetryFields: [
        { "id": "flightMode", "label": "FLIGHT MODE", "icon": "mode" },
        { "id": "relativeAltitude", "label": "ALTITUDE", "icon": "altitude" },
        { "id": "homeDistance", "label": "HOME DIST", "icon": "home" },
        { "id": "climb", "label": "CLIMB RATE", "icon": "climb" },
        { "id": "groundSpeed", "label": "GROUND SPEED", "icon": "speed" },
        { "id": "flightTime", "label": "FLIGHT TIME", "icon": "time" },
        { "id": "flightDistance", "label": "DISTANCE", "icon": "distance" },
        { "id": "windSpeed", "label": "WIND SPEED", "icon": "speed" },
        { "id": "roll", "label": "ROLL", "icon": "attitude" },
        { "id": "pitch", "label": "PITCH", "icon": "attitude" },
        { "id": "yaw", "label": "YAW", "icon": "mode" },
        { "id": "battery", "label": "BATTERY", "icon": "battery" },
        { "id": "satellites", "label": "SATELLITES", "icon": "satellite" },
        { "id": "gpsFix", "label": "GPS FIX", "icon": "gps" },
        { "id": "latitude", "label": "LATITUDE", "icon": "gps" },
        { "id": "longitude", "label": "LONGITUDE", "icon": "gps" },
        { "id": "clock", "label": "CLOCK", "icon": "time" }
    ]
    readonly property color chromeColor: root.darkTheme ? "#f1080d15" : "#f8fbff"
    readonly property color cardColor: root.darkTheme ? "#0a111b" : "#ffffff"
    readonly property color borderColor: root.darkTheme ? "#223145" : "#cfdceb"
    readonly property color cardBorderColor: root.darkTheme ? "#243247" : "#d5e0ee"
    readonly property color textColor: root.darkTheme ? "#f4f8ff" : "#0f213b"
    readonly property color labelColor: root.darkTheme ? "#9fb5d7" : "#526680"
    readonly property color mutedColor: root.darkTheme ? "#8192ad" : "#6f7f95"
    readonly property color accentColor: root.darkTheme ? "#00c7df" : "#008fb8"
    readonly property color warningColor: "#d6a600"
    readonly property color errorColor: "#ff5f5f"
    readonly property color infoColor: "#6bc7ff"
    readonly property color hoverColor: root.darkTheme ? "#121d2b" : "#eef5fc"
    readonly property color downColor: root.darkTheme ? "#1a2738" : "#ddeaf8"
    readonly property color panelColor: root.darkTheme ? "#0b1018" : "#ffffff"

    signal toggleRequested()
    signal popOutRequested()
    signal messagesRequested()

    width: panelWidth
    height: parent ? parent.height : 0
    z: 90

    Behavior on width {
        enabled: !root.resizing
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    onReorderModeChanged: panelMenuIcon.requestPaint()

    Settings {
        id: telemetrySettings
        category: "TelemetryPanel"
        property string selectedFieldsJson: JSON.stringify(root.defaultFieldIds)
        property int rows: 3
        property int columns: 2
    }

    Component.onCompleted: {
        root.layoutRows = Math.max(1, Math.min(6, telemetrySettings.rows))
        root.layoutColumns = Math.max(1, Math.min(4, telemetrySettings.columns))
        loadSelectedFields()
    }

    function loadSelectedFields() {
        try {
            var parsed = JSON.parse(telemetrySettings.selectedFieldsJson)
            if (Array.isArray(parsed)) {
                selectedFieldIds = sanitizeFieldIds(parsed)
                return
            }
        } catch (error) {
        }
        selectedFieldIds = defaultFieldIds.slice()
    }

    function sanitizeFieldIds(ids) {
        var result = []
        for (var i = 0; i < ids.length; i++) {
            if (fieldIndex(ids[i]) >= 0 && result.indexOf(ids[i]) < 0)
                result.push(ids[i])
        }
        return result
    }

    function buildDisplayFieldIds() {
        var ids = selectedFieldIds.slice()
        while (ids.length < layoutSlotCount)
            ids.push("__empty_" + ids.length)
        return ids
    }

    function saveSelectedFields() {
        telemetrySettings.selectedFieldsJson = JSON.stringify(selectedFieldIds)
    }

    function saveLayout() {
        telemetrySettings.rows = layoutRows
        telemetrySettings.columns = layoutColumns
    }

    function fieldIndex(fieldId) {
        for (var i = 0; i < allTelemetryFields.length; i++) {
            if (allTelemetryFields[i].id === fieldId)
                return i
        }
        return -1
    }

    function isPlaceholder(fieldId) {
        return String(fieldId).indexOf("__empty_") === 0
    }

    function fieldMeta(fieldId) {
        if (isPlaceholder(fieldId))
            return { "id": fieldId, "label": "UNCONFIGURED", "icon": "speed" }
        var index = fieldIndex(fieldId)
        return index >= 0 ? allTelemetryFields[index] : { "id": fieldId, "label": fieldId, "icon": "speed" }
    }

    function isFieldSelected(fieldId) {
        return selectedFieldIds.indexOf(fieldId) >= 0
    }

    function setFieldSelected(fieldId, selected) {
        var ids = selectedFieldIds.slice()
        var index = ids.indexOf(fieldId)
        if (selected && index < 0)
            ids.push(fieldId)
        else if (!selected && index >= 0)
            ids.splice(index, 1)
        selectedFieldIds = sanitizeFieldIds(ids)
        saveSelectedFields()
    }

    function moveFieldTo(fieldId, targetIndex) {
        var ids = selectedFieldIds.slice()
        var index = ids.indexOf(fieldId)
        if (index < 0 || ids.length < 2)
            return
        var clampedTarget = Math.max(0, Math.min(ids.length - 1, targetIndex))
        if (index === clampedTarget)
            return
        var item = ids.splice(index, 1)[0]
        ids.splice(clampedTarget, 0, item)
        selectedFieldIds = ids
    }

    function startReorder() {
        reorderMode = true
        telemetryMenu.close()
    }

    function finishReorder() {
        reorderMode = false
        saveSelectedFields()
    }

    function setLayout(rows, columns) {
        layoutRows = Math.max(1, Math.min(6, rows))
        layoutColumns = Math.max(1, Math.min(4, columns))
        saveLayout()
    }

    function resetTelemetryPanel() {
        reorderMode = false
        setLayout(3, 2)
        selectedFieldIds = defaultFieldIds.slice()
        saveSelectedFields()
    }

    function valueForField(fieldId) {
        if (isPlaceholder(fieldId))
            return "0"
        if (fieldId === "flightMode")
            return flightModeText
        if (fieldId === "relativeAltitude")
            return relativeAltitudeText
        if (fieldId === "homeDistance")
            return homeDistanceText
        if (fieldId === "climb")
            return climbText
        if (fieldId === "groundSpeed")
            return groundSpeedText
        if (fieldId === "flightTime")
            return flightTimeText
        if (fieldId === "flightDistance")
            return flightDistanceText
        if (fieldId === "windSpeed")
            return "--"
        if (fieldId === "roll")
            return rollText
        if (fieldId === "pitch")
            return pitchText
        if (fieldId === "yaw")
            return yawText
        if (fieldId === "battery")
            return batteryText
        if (fieldId === "satellites")
            return satellitesText
        if (fieldId === "gpsFix")
            return gpsFixText
        if (fieldId === "latitude")
            return latitudeText
        if (fieldId === "longitude")
            return longitudeText
        if (fieldId === "clock")
            return clockText
        return "--"
    }

    function boundedPixelSize(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value))
    }

    function normalizedSeverity(value) {
        var severity = String(value || "info").toLowerCase()
        if (severity === "warning" || severity === "error")
            return severity
        return "info"
    }

    function severityColor(severity) {
        var normalized = normalizedSeverity(severity)
        if (normalized === "error")
            return root.errorColor
        if (normalized === "warning")
            return root.warningColor
        return root.infoColor
    }

    function filteredMessages() {
        var result = []
        for (var i = 0; i < root.messages.length; i++) {
            var entry = root.messages[i]
            if (root.messageFilter === "all" || root.normalizedSeverity(entry.severity) === root.messageFilter)
                result.push(entry)
        }
        return result
    }

    function latestMessageDateText() {
        var filtered = filteredMessages()
        return filtered.length > 0 ? filtered[0].dateText : Qt.formatDate(new Date(), "M/d/yyyy")
    }

    function openTelemetryMenu() {
        var buttonBottomRight = panelMenuButton.mapToItem(root, panelMenuButton.width, panelMenuButton.height)
        telemetryMenu.x = Math.max(0, Math.min(root.width - telemetryMenu.width, Math.round(buttonBottomRight.x - telemetryMenu.width)))
        telemetryMenu.y = Math.round(buttonBottomRight.y + 4)
        telemetryMenu.open()
    }

    Rectangle {
        id: panelBody
        anchors.fill: parent
        color: root.chromeColor
        border.color: root.borderColor
        border.width: 1
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            spacing: 7

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                spacing: 4

                TelemetryIcon {
                    Layout.preferredWidth: 15
                    Layout.preferredHeight: 15
                    iconType: "speed"
                    iconColor: root.accentColor
                }

                Text {
                    text: root.showMessages ? "MESSAGES" : "TELEMETRY"
                    color: root.accentColor
                    font.pixelSize: 13
                    font.bold: true
                }

                Text {
                    text: root.showMessages ? root.filteredMessages().length + " entries" : root.reorderMode ? "Drag to reorder" : "Live values"
                    color: root.mutedColor
                    font.pixelSize: 10
                }

                Item { Layout.fillWidth: true }

                Button {
                    id: panelMenuButton
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    padding: 0
                    flat: true
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: root.reorderMode ? "Save order" : "Telemetry options"
                    onClicked: root.reorderMode ? root.finishReorder() : root.openTelemetryMenu()

                    contentItem: Canvas {
                        id: panelMenuIcon
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            ctx.strokeStyle = panelMenuButton.hovered ? root.textColor : root.labelColor
                            ctx.fillStyle = panelMenuButton.hovered ? root.textColor : root.labelColor
                            ctx.lineWidth = 2
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            if (root.reorderMode) {
                                ctx.beginPath()
                                ctx.moveTo(3, 9)
                                ctx.lineTo(7, 13)
                                ctx.lineTo(15, 5)
                                ctx.stroke()
                                return
                            }
                            for (var i = 0; i < 3; i++) {
                                ctx.beginPath()
                                ctx.arc(width / 2, 4 + i * 5, 1.3, 0, Math.PI * 2)
                                ctx.fill()
                            }
                        }

                        Connections {
                            target: panelMenuButton
                            function onHoveredChanged() { panelMenuIcon.requestPaint() }
                            function onDownChanged() { panelMenuIcon.requestPaint() }
                        }
                    }

                    background: Rectangle {
                        radius: 3
                        color: panelMenuButton.down ? "#1a2738" : panelMenuButton.hovered ? "#121d2b" : "#00000000"
                    }
                }

                Button {
                    id: telemetryMessagesButton
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    padding: 0
                    flat: true
                    hoverEnabled: true
                    ToolTip.visible: hovered
                    ToolTip.text: root.showMessages ? "Telemetry" : "Messages"
                    onClicked: {
                        root.showMessages = !root.showMessages
                        if (root.showMessages)
                            root.messagesRequested()
                    }

                    contentItem: Canvas {
                        id: telemetryMessagesIcon
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            ctx.strokeStyle = telemetryMessagesButton.hovered ? root.textColor : root.labelColor
                            ctx.lineWidth = 2
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            ctx.beginPath()
                            if (root.showMessages) {
                                ctx.moveTo(12, 4)
                                ctx.lineTo(7, 9)
                                ctx.lineTo(12, 14)
                            } else {
                                ctx.moveTo(6, 4)
                                ctx.lineTo(11, 9)
                                ctx.lineTo(6, 14)
                            }
                            ctx.stroke()
                        }

                        Connections {
                            target: telemetryMessagesButton
                            function onHoveredChanged() { telemetryMessagesIcon.requestPaint() }
                            function onDownChanged() { telemetryMessagesIcon.requestPaint() }
                        }
                        Connections {
                            target: root
                            function onShowMessagesChanged() { telemetryMessagesIcon.requestPaint() }
                        }
                    }

                    background: Rectangle {
                        radius: 3
                        color: telemetryMessagesButton.down ? "#1a2738" : telemetryMessagesButton.hovered ? "#121d2b" : "#00000000"
                    }
                }
            }

            Item {
                id: contentViewport
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                Item {
                    id: slidingPages
                    width: contentViewport.width * 2
                    height: contentViewport.height
                    x: root.showMessages ? -contentViewport.width : 0

                    Behavior on x {
                        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                    }

                    Item {
                        id: telemetryPage
                        x: 0
                        y: 0
                        width: contentViewport.width
                        height: contentViewport.height

                        GridView {
                            id: telemetryGrid
                            anchors.fill: parent
                            clip: true
                            interactive: contentHeight > height
                            boundsBehavior: Flickable.StopAtBounds
                            model: root.displayFieldIds
                            readonly property real columnGap: 8
                            readonly property real rowGap: 8
                            readonly property real fittedCardHeight: Math.max(54, Math.floor((height - rowGap * (root.layoutRows - 1)) / root.layoutRows))
                            cellWidth: Math.max(72, width / root.layoutColumns)
                            cellHeight: fittedCardHeight + rowGap

                            delegate: Item {
                                id: slot
                                width: telemetryGrid.cellWidth
                                height: telemetryGrid.cellHeight
                                property string fieldId: modelData
                                property bool placeholder: root.isPlaceholder(fieldId)

                                TelemetryCard {
                                    id: card
                                    width: Math.max(1, slot.width - (root.layoutColumns > 1 ? telemetryGrid.columnGap : 0))
                                    height: telemetryGrid.fittedCardHeight
                                    fieldId: slot.fieldId
                                    placeholder: slot.placeholder
                                    label: root.fieldMeta(slot.fieldId).label
                                    value: root.valueForField(slot.fieldId)
                                    iconType: root.fieldMeta(slot.fieldId).icon
                                    opacity: slot.placeholder ? 0.72 : 1
                                }

                                MouseArea {
                                    id: reorderDragArea
                                    anchors.fill: parent
                                    enabled: root.reorderMode && !slot.placeholder
                                    cursorShape: enabled ? Qt.OpenHandCursor : Qt.ArrowCursor
                                    drag.target: card
                                    drag.axis: Drag.XAndYAxis
                                    onPressed: {
                                        root.draggingFieldId = slot.fieldId
                                        cursorShape = Qt.ClosedHandCursor
                                        card.z = 20
                                        card.scale = 1.03
                                    }
                                    onPositionChanged: {
                                        var p = mapToItem(telemetryGrid, mouse.x, mouse.y)
                                        var targetIndex = telemetryGrid.indexAt(p.x, p.y)
                                        if (targetIndex >= 0)
                                            root.moveFieldTo(root.draggingFieldId, targetIndex)
                                    }
                                    onReleased: {
                                        root.draggingFieldId = ""
                                        cursorShape = Qt.OpenHandCursor
                                        card.x = 0
                                        card.y = 0
                                        card.z = 0
                                        card.scale = 1
                                    }
                                    onCanceled: {
                                        root.draggingFieldId = ""
                                        cursorShape = Qt.OpenHandCursor
                                        card.x = 0
                                        card.y = 0
                                        card.z = 0
                                        card.scale = 1
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        id: messagesPage
                        x: contentViewport.width
                        y: 0
                        width: contentViewport.width
                        height: contentViewport.height

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 7

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                spacing: 6

                                MessageFilterButton { text: "ALL"; filterValue: "all" }
                                MessageFilterButton { text: "INFO"; filterValue: "info" }
                                MessageFilterButton { text: "WARNING"; filterValue: "warning" }
                                MessageFilterButton { text: "ERROR"; filterValue: "error" }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 18
                                spacing: 6

                                Item { Layout.fillWidth: true }
                                Text {
                                    text: root.latestMessageDateText()
                                    color: root.mutedColor
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                ListView {
                                    id: messagesList
                                    anchors.fill: parent
                                    clip: true
                                    spacing: 4
                                    model: root.filteredMessages()
                                    boundsBehavior: Flickable.StopAtBounds

                                    delegate: Rectangle {
                                        width: messagesList.width
                                        height: Math.max(52, messageColumn.implicitHeight + 16)
                                        radius: 4
                                        color: root.darkTheme ? "#d70b131d" : "#ffffff"
                                        border.color: root.darkTheme ? "#1f2f40" : "#d5e0ee"
                                        border.width: 1

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            anchors.topMargin: 7
                                            anchors.bottomMargin: 7
                                            spacing: 8

                                            Rectangle {
                                                Layout.preferredWidth: 15
                                                Layout.preferredHeight: 15
                                                Layout.alignment: Qt.AlignTop
                                                radius: 7
                                                color: root.severityColor(modelData.severity)

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: root.normalizedSeverity(modelData.severity) === "info" ? "i" : "!"
                                                    color: "#ffffff"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }

                                            ColumnLayout {
                                                id: messageColumn
                                                Layout.fillWidth: true
                                                spacing: 2

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData.timeText || "--:--:--"
                                                    color: root.mutedColor
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData.text || ""
                                                    color: root.textColor
                                                    font.pixelSize: 11
                                                    wrapMode: Text.WordWrap
                                                    maximumLineCount: 2
                                                    elide: Text.ElideRight
                                                }
                                            }

                                            Text {
                                                Layout.alignment: Qt.AlignTop
                                                text: root.normalizedSeverity(modelData.severity).toUpperCase()
                                                color: root.severityColor(modelData.severity)
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                        }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    width: parent.width - 28
                                    visible: messagesList.count === 0
                                    text: "No messages"
                                    color: root.mutedColor
                                    font.pixelSize: 12
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Menu {
        id: telemetryMenu
        width: 184
        padding: 6

        background: Rectangle {
            color: root.darkTheme ? "#f20b1018" : "#ffffff"
            border.color: root.accentColor
            border.width: 1
        }

        TelemetryMenuItem {
            text: "Customize"
            onTriggered: customizeDialog.open()
        }
        TelemetryMenuItem {
            text: "Reorder"
            onTriggered: root.startReorder()
        }
        TelemetryMenuItem {
            text: "Change Layout"
            onTriggered: {
                rowSpin.value = root.layoutRows
                columnSpin.value = root.layoutColumns
                layoutDialog.open()
            }
        }
        TelemetryMenuItem {
            text: "Pop Out"
            onTriggered: root.popOutRequested()
        }
        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: root.borderColor
            }
        }
        TelemetryMenuItem {
            text: "Reset"
            onTriggered: root.resetTelemetryPanel()
        }
    }

    Dialog {
        id: layoutDialog
        modal: true
        focus: true
        width: Math.min(330, root.width - 24)
        anchors.centerIn: parent
        padding: 0
        onAccepted: root.setLayout(rowSpin.value, columnSpin.value)

        background: Rectangle {
            color: root.darkTheme ? "#f20b1018" : "#ffffff"
            border.color: root.accentColor
            border.width: 1
            radius: 0
        }

        header: DialogHeader {
            title: "CHANGE LAYOUT"
            onCloseRequested: layoutDialog.reject()
        }

        contentItem: ColumnLayout {
            width: layoutDialog.width
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 118
                color: root.panelColor
                border.color: root.borderColor
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 14
                    anchors.bottomMargin: 14
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        text: "Select rows and columns visible in the panel."
                        color: root.mutedColor
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }

                    LayoutControlRow {
                        label: "ROWS"
                        SpinBox {
                            id: rowSpin
                            from: 1
                            to: 6
                            value: root.layoutRows
                            editable: true
                            Layout.preferredWidth: 118
                            Layout.preferredHeight: 32
                            font.pixelSize: 12
                            contentItem: TextInput {
                                text: rowSpin.textFromValue(rowSpin.value, rowSpin.locale)
                                color: root.textColor
                                font: rowSpin.font
                                horizontalAlignment: Qt.AlignHCenter
                                verticalAlignment: Qt.AlignVCenter
                                readOnly: !rowSpin.editable
                                validator: rowSpin.validator
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                            }
                            up.indicator: SpinButtonIndicator {
                                x: rowSpin.width - width
                                y: 0
                                height: rowSpin.height / 2
                                up: true
                                pressed: rowSpin.up.pressed
                            }
                            down.indicator: SpinButtonIndicator {
                                x: rowSpin.width - width
                                y: rowSpin.height / 2
                                height: rowSpin.height / 2
                                up: false
                                pressed: rowSpin.down.pressed
                            }
                            background: Rectangle {
                                color: root.darkTheme ? "#070c13" : "#f8fbff"
                                border.color: rowSpin.activeFocus ? root.accentColor : root.borderColor
                                border.width: 1
                            }
                        }
                    }

                    LayoutControlRow {
                        label: "COLUMNS"
                        SpinBox {
                            id: columnSpin
                            from: 1
                            to: 4
                            value: root.layoutColumns
                            editable: true
                            Layout.preferredWidth: 118
                            Layout.preferredHeight: 32
                            font.pixelSize: 12
                            contentItem: TextInput {
                                text: columnSpin.textFromValue(columnSpin.value, columnSpin.locale)
                                color: root.textColor
                                font: columnSpin.font
                                horizontalAlignment: Qt.AlignHCenter
                                verticalAlignment: Qt.AlignVCenter
                                readOnly: !columnSpin.editable
                                validator: columnSpin.validator
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                            }
                            up.indicator: SpinButtonIndicator {
                                x: columnSpin.width - width
                                y: 0
                                height: columnSpin.height / 2
                                up: true
                                pressed: columnSpin.up.pressed
                            }
                            down.indicator: SpinButtonIndicator {
                                x: columnSpin.width - width
                                y: columnSpin.height / 2
                                height: columnSpin.height / 2
                                up: false
                                pressed: columnSpin.down.pressed
                            }
                            background: Rectangle {
                                color: root.darkTheme ? "#070c13" : "#f8fbff"
                                border.color: columnSpin.activeFocus ? root.accentColor : root.borderColor
                                border.width: 1
                            }
                        }
                    }
                }
            }
        }

        footer: DialogFooter {
            TelemetryDialogButton { text: "CANCEL"; onClicked: layoutDialog.reject() }
            TelemetryDialogButton { text: "APPLY"; accent: root.accentColor; onClicked: layoutDialog.accept() }
        }
    }

    Dialog {
        id: customizeDialog
        modal: true
        focus: true
        width: Math.min(360, root.width - 24)
        height: Math.min(520, root.height - 40)
        anchors.centerIn: parent
        padding: 0

        background: Rectangle {
            color: root.darkTheme ? "#f20b1018" : "#ffffff"
            border.color: root.accentColor
            border.width: 1
            radius: 0
        }

        header: DialogHeader {
            title: "CUSTOMIZE TELEMETRY"
            onCloseRequested: customizeDialog.accept()
        }

        contentItem: ScrollView {
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            background: Rectangle {
                color: root.panelColor
                border.color: root.borderColor
                border.width: 1
            }

            ColumnLayout {
                width: customizeDialog.availableWidth
                spacing: 0

                Repeater {
                    model: root.allTelemetryFields

                    CheckBox {
                        id: fieldCheck
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        text: modelData.label
                        checked: root.isFieldSelected(modelData.id)
                        onToggled: root.setFieldSelected(modelData.id, checked)
                        hoverEnabled: true

                        indicator: Rectangle {
                            implicitWidth: 15
                            implicitHeight: 15
                            x: 12
                            y: Math.round((fieldCheck.height - height) / 2)
                            radius: 1
                            color: fieldCheck.checked ? root.accentColor : fieldCheck.hovered ? root.hoverColor : "#00000000"
                            border.color: fieldCheck.checked || fieldCheck.hovered ? root.accentColor : root.borderColor
                            border.width: 1

                            Canvas {
                                anchors.fill: parent
                                anchors.margins: 3
                                visible: fieldCheck.checked
                                onVisibleChanged: requestPaint()
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.strokeStyle = root.darkTheme ? "#061018" : "#ffffff"
                                    ctx.lineWidth = 2
                                    ctx.lineCap = "round"
                                    ctx.lineJoin = "round"
                                    ctx.beginPath()
                                    ctx.moveTo(width * 0.10, height * 0.55)
                                    ctx.lineTo(width * 0.40, height * 0.82)
                                    ctx.lineTo(width * 0.88, height * 0.18)
                                    ctx.stroke()
                                }
                            }
                        }

                        contentItem: Text {
                            text: fieldCheck.text
                            color: root.textColor
                            font.pixelSize: 12
                            font.bold: true
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 38
                            rightPadding: 12
                            elide: Text.ElideRight
                        }

                        background: Rectangle {
                            color: fieldCheck.down ? root.downColor : fieldCheck.hovered ? root.hoverColor : root.panelColor
                            border.color: root.borderColor
                            border.width: 1
                        }
                    }
                }
            }
        }

        footer: DialogFooter {
            TelemetryDialogButton { text: "DONE"; accent: root.accentColor; onClicked: customizeDialog.accept() }
        }
    }

    component DialogHeader: Rectangle {
        id: headerRoot

        property string title: ""
        signal closeRequested()

        implicitHeight: 48
        color: root.darkTheme ? "#090e16" : "#f8fbff"
        border.color: root.borderColor
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 8
            spacing: 10

            TelemetryIcon {
                Layout.preferredWidth: 17
                Layout.preferredHeight: 17
                Layout.alignment: Qt.AlignVCenter
                iconType: "speed"
                iconColor: root.accentColor
            }

            Text {
                Layout.fillWidth: true
                text: headerRoot.title
                color: root.textColor
                font.pixelSize: 14
                font.bold: true
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            Button {
                id: headerCloseButton
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                padding: 0
                flat: true
                hoverEnabled: true
                contentItem: Text {
                    text: "x"
                    color: headerCloseButton.hovered ? root.textColor : root.mutedColor
                    font.pixelSize: 14
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: headerCloseButton.down ? root.downColor : headerCloseButton.hovered ? root.hoverColor : "#00000000"
                    border.color: headerCloseButton.hovered ? root.borderColor : "#00000000"
                    border.width: 1
                }
                onClicked: headerRoot.closeRequested()
            }
        }
    }

    component DialogFooter: Rectangle {
        id: footerRoot

        default property alias footerContent: footerLayout.data

        implicitHeight: 52
        color: root.panelColor
        border.color: root.borderColor
        border.width: 1

        RowLayout {
            id: footerLayout
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 8

            Item { Layout.fillWidth: true }
        }
    }

    component MessageFilterButton: Button {
        id: filterButton

        property string filterValue: "all"
        readonly property bool active: root.messageFilter === filterValue

        Layout.preferredWidth: filterValue === "warning" ? 72 : 48
        Layout.preferredHeight: 24
        padding: 0
        flat: true
        hoverEnabled: true
        onClicked: root.messageFilter = filterValue

        contentItem: Text {
            text: filterButton.text
            color: filterButton.active ? root.accentColor : filterButton.hovered ? root.textColor : root.labelColor
            font.pixelSize: 9
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 2
            color: filterButton.active ? (root.darkTheme ? "#101f2b" : "#e3f7fb")
                  : filterButton.down ? root.downColor
                  : filterButton.hovered ? root.hoverColor
                  : root.panelColor
            border.color: filterButton.active ? root.accentColor : root.borderColor
            border.width: 1
        }
    }

    component TelemetryDialogButton: Button {
        id: dialogButton

        property color accent: root.mutedColor

        Layout.preferredWidth: 86
        Layout.preferredHeight: 32
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: Text {
            text: dialogButton.text
            color: dialogButton.enabled ? dialogButton.accent : root.mutedColor
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            color: !dialogButton.enabled ? root.panelColor : dialogButton.down ? root.downColor : dialogButton.hovered ? root.hoverColor : root.panelColor
            border.color: dialogButton.enabled ? dialogButton.accent : root.borderColor
            border.width: 1
        }
    }

    component TelemetryMenuItem: MenuItem {
        id: menuItem

        implicitWidth: 172
        implicitHeight: 34
        padding: 0

        contentItem: Text {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            text: menuItem.text
            color: menuItem.enabled ? root.textColor : root.mutedColor
            font.pixelSize: 11
            font.bold: true
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: menuItem.down ? root.downColor : menuItem.highlighted ? root.hoverColor : "#00000000"
            border.color: menuItem.highlighted ? root.borderColor : "#00000000"
            border.width: 1
        }
    }

    component LayoutControlRow: RowLayout {
        id: controlRow

        property string label: ""

        Layout.fillWidth: true
        spacing: 12

        Text {
            Layout.fillWidth: true
            text: controlRow.label
            color: root.labelColor
            font.pixelSize: 11
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }
    }

    component SpinButtonIndicator: Rectangle {
        property bool up: true
        property bool pressed: false

        implicitWidth: 28
        implicitHeight: 16
        color: pressed ? root.downColor : root.hoverColor
        border.color: root.borderColor
        border.width: 1

        Canvas {
            anchors.centerIn: parent
            width: 10
            height: 8
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = root.accentColor
                ctx.lineWidth = 1.6
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                if (parent.up) {
                    ctx.moveTo(2, 6)
                    ctx.lineTo(5, 2)
                    ctx.lineTo(8, 6)
                } else {
                    ctx.moveTo(2, 2)
                    ctx.lineTo(5, 6)
                    ctx.lineTo(8, 2)
                }
                ctx.stroke()
            }
        }
    }

    component GripIcon: Canvas {
        property color gripColor: root.labelColor
        width: 16
        height: 18
        onGripColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = gripColor
            for (var column = 0; column < 2; column++) {
                for (var row = 0; row < 3; row++) {
                    ctx.beginPath()
                    ctx.arc(5 + column * 6, 4 + row * 5, 1.25, 0, Math.PI * 2)
                    ctx.fill()
                }
            }
        }
    }

    component TelemetryCard: Rectangle {
        id: card

        property string fieldId: ""
        property bool placeholder: false
        property string label: ""
        property string value: ""
        property string iconType: "speed"
        property color valueColor: placeholder ? root.mutedColor : root.textColor
        readonly property real contentScale: Math.max(0.58, Math.min(1, Math.min(width / 132, height / 78)))
        readonly property int horizontalPadding: Math.round(10 * contentScale)
        readonly property int verticalPadding: Math.round(8 * contentScale)
        readonly property int iconSize: Math.round(18 * contentScale)
        readonly property int gripWidth: Math.round(16 * contentScale)
        readonly property int headerHeight: Math.max(12, Math.round(20 * contentScale))
        readonly property int labelPixelSize: root.boundedPixelSize(Math.min(width * 0.09, height * 0.18), 7, 10)
        readonly property int valuePixelSize: root.boundedPixelSize(Math.min(width * 0.18, height * 0.29), 9, 20)

        radius: 6
        color: root.cardColor
        border.color: root.reorderMode && !placeholder ? root.accentColor : root.cardBorderColor
        border.width: root.reorderMode && !placeholder ? 1.5 : 1

        Behavior on scale { NumberAnimation { duration: 110 } }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: card.horizontalPadding
            anchors.rightMargin: card.horizontalPadding
            anchors.topMargin: card.verticalPadding
            anchors.bottomMargin: card.verticalPadding
            spacing: Math.max(2, Math.round(6 * card.contentScale))

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: card.headerHeight
                spacing: Math.max(3, Math.round(7 * card.contentScale))

                GripIcon {
                    visible: root.reorderMode && !card.placeholder
                    Layout.preferredWidth: card.gripWidth
                    Layout.preferredHeight: card.iconSize
                    gripColor: root.accentColor
                }

                TelemetryIcon {
                    visible: !root.reorderMode || card.placeholder
                    Layout.preferredWidth: card.iconSize
                    Layout.preferredHeight: card.iconSize
                    iconType: card.iconType
                    iconColor: card.placeholder ? root.mutedColor : root.accentColor
                }

                Text {
                    Layout.fillWidth: true
                    text: card.label
                    color: card.placeholder ? root.mutedColor : root.labelColor
                    font.pixelSize: card.labelPixelSize
                    minimumPixelSize: 7
                    fontSizeMode: Text.Fit
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: card.value
                color: card.valueColor
                font.pixelSize: card.valuePixelSize
                minimumPixelSize: 8
                fontSizeMode: Text.Fit
                font.bold: true
                font.family: "Consolas"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideLeft
            }
        }
    }
}
