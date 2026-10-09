pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var mavlinkObject: null
    readonly property var inspector: mavlinkObject ? mavlinkObject.inspector : null
    property bool darkTheme: true
    property color panelColor: darkTheme ? "#171d26" : "#ffffff"
    property color borderColor: darkTheme ? "#2d3b4c" : "#ccd6e4"
    property color textColor: darkTheme ? "#edf3f8" : "#101722"
    property color mutedColor: darkTheme ? "#8292a4" : "#66758a"
    property color accentColor: darkTheme ? "#24c7dc" : "#008fb0"
    property color accentSoftColor: darkTheme ? "#123d4a" : "#d7f3f8"
    readonly property color liveColor: darkTheme ? "#37d67a" : "#15964d"
    readonly property color pausedColor: darkTheme ? "#c9a65a" : "#9a7117"
    readonly property color workspaceColor: darkTheme ? "#111821" : "#f2f5f8"
    readonly property bool hasSelection: inspector ? inspector.hasSelection : false
    readonly property var selectedDetails: inspector && inspector.hasSelection ? inspector.selectedDetails : ({})
    readonly property bool hasRawPacket: inspector ? inspector.hasRawPacket : false
    readonly property var selectedRawPacket: inspector && inspector.hasRawPacket ? inspector.selectedRawPacket : ({})
    readonly property var detailFields: selectedDetails.fields ? selectedDetails.fields : []
    readonly property var graph: inspector ? inspector.graphModel : null
    readonly property var graphFields: inspector && inspector.hasSelection ? inspector.availableGraphFields : []
    readonly property bool graphHasData: graph && graph.active && graph.sampleCount > 0
    property bool showHex: true

    // The Settings viewport owns our height. This prevents the Inspector from
    // advertising a tall page to the outer Flickable.
    implicitHeight: parent ? parent.height : 0

    function rateText(value) { var number = Number(value); return isFinite(number) ? number.toFixed(number >= 10 ? 0 : 1) + " Hz" : "--" }
    function bytesText(value) { var number = Number(value); if (!isFinite(number)) return "--"; if (number >= 1048576) return (number / 1048576).toFixed(1) + " MB/s"; if (number >= 1024) return (number / 1024).toFixed(1) + " KB/s"; return number.toFixed(0) + " B/s" }
    function timestampText(value) { if (!value) return "--:--"; var date = value instanceof Date ? value : new Date(value); return isNaN(date.getTime()) ? "--:--" : Qt.formatTime(date, "hh:mm:ss") }
    function valueText(value, fallback) { return value === undefined || value === null ? fallback : String(value) }
    function fieldValue(field) { if (!field || !field.available || field.value === undefined || field.value === null) return "--"; return Array.isArray(field.value) ? JSON.stringify(field.value) : String(field.value) }
    function rowIsSelected(link, sysid, compid, messageId) { return hasSelection && String(selectedDetails.link) === String(link) && Number(selectedDetails.sysid) === Number(sysid) && Number(selectedDetails.compid) === Number(compid) && Number(selectedDetails.messageId) === Number(messageId) }
    function clearFilters() { if (root.inspector) root.inspector.clearFilters(); vehicleFilter.currentIndex = 0; componentFilter.currentIndex = 0; messageFilter.currentIndex = 0; linkFilter.currentIndex = 0; searchField.clear() }

    Rectangle { anchors.fill: parent; color: root.workspaceColor; border.color: root.borderColor; border.width: 1; radius: 4 }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 62; color: root.panelColor; border.color: root.borderColor; border.width: 1; radius: 4
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 12; spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 1
                    Text { text: "MAVLink Inspector"; color: root.textColor; font.pixelSize: 21; font.bold: true }
                    Text { text: "Live MAVLink traffic diagnostics and message analysis"; color: root.mutedColor; font.pixelSize: 11 }
                }
                Rectangle {
                    Layout.preferredWidth: 78; Layout.preferredHeight: 28; radius: 3
                    color: root.inspector && root.inspector.paused ? "transparent" : Qt.rgba(root.liveColor.r, root.liveColor.g, root.liveColor.b, 0.10)
                    border.color: root.inspector && root.inspector.paused ? root.pausedColor : root.liveColor; border.width: 1
                    Row { anchors.centerIn: parent; spacing: 6
                        Rectangle { width: 7; height: 7; radius: 4; anchors.verticalCenter: parent.verticalCenter; color: root.inspector && root.inspector.paused ? root.pausedColor : root.liveColor }
                        Text { anchors.verticalCenter: parent.verticalCenter; text: root.inspector && root.inspector.paused ? "PAUSED" : "LIVE"; color: root.inspector && root.inspector.paused ? root.pausedColor : root.liveColor; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1 }
                    }
                }
                HeaderStat { label: "RX RATE"; value: root.inspector ? root.rateText(root.inspector.totalRxRate) : "--" }
                HeaderStat { label: "RX DATA"; value: root.inspector ? root.bytesText(root.inspector.totalRxBytesPerSecond) : "--" }
                HeaderStat { label: "STREAMS"; value: root.inspector ? String(root.inspector.streamCount) : "0" }
                ToolAction { text: root.inspector && root.inspector.paused ? "RESUME" : "PAUSE"; emphasized: true; onClicked: if (root.inspector) root.inspector.paused ? root.inspector.resume() : root.inspector.pause() }
                ToolAction { text: "CLEAR"; destructive: true; onClicked: if (root.inspector) root.inspector.clear() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Math.max(340, content.height * 0.52)
            Layout.minimumHeight: 340
            spacing: 10
            Rectangle {
                Layout.preferredWidth: 258; Layout.minimumWidth: 228; Layout.fillHeight: true; color: root.panelColor; border.color: root.borderColor; border.width: 1; radius: 4
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 4
                    Text { text: "FILTERS"; color: root.textColor; font.pixelSize: 13; font.bold: true; font.letterSpacing: 0.6 }
                    Text { text: "Narrow displayed traffic"; color: root.mutedColor; font.pixelSize: 10 }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: root.borderColor }
                    FilterLabel { text: "VEHICLE" }
                    FilterCombo { id: vehicleFilter; model: root.inspector ? root.inspector.vehicleFilterOptions : []; onActivated: if (root.inspector) root.inspector.setVehicleFilter(currentValue) }
                    FilterLabel { text: "COMPONENT" }
                    FilterCombo { id: componentFilter; model: root.inspector ? root.inspector.componentFilterOptions : []; onActivated: if (root.inspector) root.inspector.setComponentFilter(currentValue) }
                    FilterLabel { text: "MESSAGE" }
                    FilterCombo { id: messageFilter; model: root.inspector ? root.inspector.messageFilterOptions : []; onActivated: if (root.inspector) root.inspector.setMessageFilter(currentValue) }
                    FilterLabel { text: "LINK" }
                    FilterCombo { id: linkFilter; model: root.inspector ? root.inspector.linkFilterOptions : []; onActivated: if (root.inspector) root.inspector.setLinkFilter(currentValue) }
                    FilterLabel { text: "SEARCH" }
                    TextField {
                        id: searchField
                        Layout.fillWidth: true; Layout.preferredHeight: 28; placeholderText: "Message / ID / field..."; color: root.textColor; font.pixelSize: 11; selectByMouse: true
                        onTextChanged: if (root.inspector) root.inspector.setSearchText(text)
                        background: Rectangle { color: root.workspaceColor; border.color: searchField.activeFocus ? root.accentColor : root.borderColor; border.width: 1; radius: 3 }
                    }
                    Item { Layout.fillHeight: true }
                    ToolAction { Layout.fillWidth: true; text: "CLEAR FILTERS"; onClicked: root.clearFilters() }
                }
            }

            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; color: root.panelColor; border.color: root.borderColor; border.width: 1; radius: 4
                Text { id: trafficTitle; anchors.left: parent.left; anchors.leftMargin: 13; anchors.top: parent.top; anchors.topMargin: 11; text: "MAVLINK TRAFFIC"; color: root.textColor; font.pixelSize: 13; font.bold: true; font.letterSpacing: 0.5 }
                Text { anchors.right: parent.right; anchors.rightMargin: 13; anchors.verticalCenter: trafficTitle.verticalCenter; text: root.inspector ? String(root.inspector.filteredStreamCount) + " displayed" : "0 displayed"; color: root.mutedColor; font.pixelSize: 10 }
                Rectangle { id: trafficRule; anchors.left: parent.left; anchors.right: parent.right; anchors.top: trafficTitle.bottom; anchors.topMargin: 9; height: 1; color: root.borderColor }
                RowLayout {
                    id: trafficHeader
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: trafficRule.bottom; anchors.leftMargin: 12; anchors.rightMargin: 14; height: 28; spacing: 6
                    TrafficHeader { Layout.preferredWidth: 68; text: "TIME" }
                    TrafficHeader { Layout.preferredWidth: 104; text: "LINK" }
                    TrafficHeader { Layout.preferredWidth: 31; text: "SYS"; horizontalAlignment: Text.AlignRight }
                    TrafficHeader { Layout.preferredWidth: 37; text: "COMP"; horizontalAlignment: Text.AlignRight }
                    TrafficHeader { Layout.fillWidth: true; text: "MESSAGE" }
                    TrafficHeader { Layout.preferredWidth: 35; text: "ID"; horizontalAlignment: Text.AlignRight }
                    TrafficHeader { Layout.preferredWidth: 57; text: "RATE"; horizontalAlignment: Text.AlignRight }
                    TrafficHeader { Layout.preferredWidth: 54; text: "COUNT"; horizontalAlignment: Text.AlignRight }
                    TrafficHeader { Layout.preferredWidth: 45; text: "SIZE"; horizontalAlignment: Text.AlignRight }
                }
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: trafficHeader.bottom; height: 1; color: root.borderColor }
                ListView {
                    id: trafficList
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: trafficHeader.bottom; anchors.bottom: parent.bottom; anchors.topMargin: 1; anchors.bottomMargin: 4; clip: true
                    model: root.inspector ? root.inspector.filteredModel : null
                    interactive: contentHeight > height
                    reuseItems: true
                    boundsBehavior: Flickable.StopAtBounds
                    flickDeceleration: 3500
                    maximumFlickVelocity: 1800
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; interactive: true }
                    delegate: Item {
                        id: trafficDelegate
                        required property int index
                        required property string link
                        required property int sysid
                        required property int compid
                        required property int messageId
                        required property string messageName
                        required property int count
                        required property real rateHz
                        required property int packetSize
                        required property var timestamp
                        width: trafficList.width; height: 27
                        readonly property bool selected: root.rowIsSelected(link, sysid, compid, messageId)
                        Rectangle { anchors.fill: parent; color: trafficDelegate.selected ? root.accentSoftColor : (trafficMouse.containsMouse ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.035) : (trafficDelegate.index % 2 ? "transparent" : Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.018))) }
                        Rectangle { visible: trafficDelegate.selected; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 3; color: root.accentColor }
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 14; spacing: 6
                            TrafficCell { Layout.preferredWidth: 68; text: root.timestampText(trafficDelegate.timestamp) }
                            TrafficCell { Layout.preferredWidth: 104; text: trafficDelegate.link; color: root.accentColor; elide: Text.ElideRight }
                            TrafficCell { Layout.preferredWidth: 31; text: String(trafficDelegate.sysid); horizontalAlignment: Text.AlignRight }
                            TrafficCell { Layout.preferredWidth: 37; text: String(trafficDelegate.compid); horizontalAlignment: Text.AlignRight }
                            TrafficCell { Layout.fillWidth: true; text: trafficDelegate.messageName; color: root.textColor; font.bold: true; elide: Text.ElideRight }
                            TrafficCell { Layout.preferredWidth: 35; text: String(trafficDelegate.messageId); horizontalAlignment: Text.AlignRight }
                            TrafficCell { Layout.preferredWidth: 57; text: root.rateText(trafficDelegate.rateHz); horizontalAlignment: Text.AlignRight }
                            TrafficCell { Layout.preferredWidth: 54; text: String(trafficDelegate.count); horizontalAlignment: Text.AlignRight }
                            TrafficCell { Layout.preferredWidth: 45; text: String(trafficDelegate.packetSize) + " B"; horizontalAlignment: Text.AlignRight }
                        }
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Qt.rgba(root.borderColor.r, root.borderColor.g, root.borderColor.b, 0.45) }
                        MouseArea { id: trafficMouse; anchors.fill: parent; hoverEnabled: true; onClicked: if (root.inspector) root.inspector.selectRow(trafficDelegate.index) }
                    }
                    Text { anchors.centerIn: parent; visible: trafficList.count === 0; text: "NO MAVLINK TRAFFIC"; color: root.mutedColor; font.pixelSize: 12; font.letterSpacing: 1 }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Math.max(270, content.height * 0.48)
            Layout.minimumHeight: 270
            spacing: 10
            Rectangle {
                Layout.fillWidth: true; Layout.preferredWidth: 350; Layout.minimumWidth: 270; Layout.fillHeight: true; color: root.panelColor; border.color: root.borderColor; border.width: 1; radius: 4
                Text { id: detailsTitle; anchors.left: parent.left; anchors.leftMargin: 12; anchors.top: parent.top; anchors.topMargin: 11; text: "MESSAGE DETAILS"; color: root.textColor; font.pixelSize: 13; font.bold: true; font.letterSpacing: 0.5 }
                Rectangle { id: detailsRule; anchors.left: parent.left; anchors.right: parent.right; anchors.top: detailsTitle.bottom; anchors.topMargin: 9; height: 1; color: root.borderColor }
                Item {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: detailsRule.bottom; anchors.bottom: parent.bottom; anchors.margins: 12; visible: root.hasSelection
                    ColumnLayout {
                        anchors.fill: parent; spacing: 6
                        Text { Layout.fillWidth: true; text: root.valueText(root.selectedDetails.messageName, "--") + "  (" + root.valueText(root.selectedDetails.messageId, "--") + ")"; color: root.textColor; font.pixelSize: 16; font.bold: true; elide: Text.ElideRight }
                        GridLayout {
                            Layout.fillWidth: true; columns: 2; rowSpacing: 3; columnSpacing: 14
                            DetailMetric { label: "PACKETS"; value: root.valueText(root.selectedDetails.count, "--") }
                            DetailMetric { label: "SEQUENCE"; value: root.valueText(root.selectedDetails.sequence, "--") }
                        }
                        Text { Layout.fillWidth: true; text: "LINK  " + root.valueText(root.selectedDetails.link, "--") + "    •    " + root.timestampText(root.selectedDetails.timestamp); color: root.mutedColor; font.pixelSize: 10; elide: Text.ElideRight }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: root.borderColor }
                        Text { text: "DECODED FIELDS"; color: root.textColor; font.pixelSize: 11; font.bold: true; font.letterSpacing: 0.5 }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            FieldHeader { Layout.preferredWidth: 112; text: "FIELD" }
                            FieldHeader { Layout.preferredWidth: 58; text: "TYPE" }
                            FieldHeader { Layout.fillWidth: true; text: "VALUE" }
                        }
                        ListView {
                            id: fieldsList
                            Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 72; clip: true; model: root.detailFields
                            interactive: true
                            reuseItems: true
                            boundsBehavior: Flickable.StopAtBounds
                            flickDeceleration: 3000
                            maximumFlickVelocity: 1600
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; interactive: true; width: 8 }
                            delegate: RowLayout {
                                id: fieldDelegate
                                required property var modelData
                                width: fieldsList.width; height: 21; spacing: 5
                                Text { Layout.preferredWidth: 112; text: root.valueText(fieldDelegate.modelData.name, "--"); color: root.textColor; font.pixelSize: 11; elide: Text.ElideRight }
                                Text { Layout.preferredWidth: 58; text: root.valueText(fieldDelegate.modelData.type, "--"); color: root.mutedColor; font.pixelSize: 10; elide: Text.ElideRight }
                                Text { Layout.fillWidth: true; text: root.fieldValue(fieldDelegate.modelData); color: fieldDelegate.modelData.available ? root.accentColor : root.mutedColor; font.pixelSize: 11; font.bold: fieldDelegate.modelData.available; elide: Text.ElideRight }
                            }
                        }
                    }
                }
                EmptyState { anchors.fill: parent; anchors.topMargin: 30; visible: !root.hasSelection; title: "NO MESSAGE SELECTED"; detail: "Select a MAVLink stream from the traffic table." }
            }

            Rectangle {
                Layout.fillWidth: true; Layout.preferredWidth: 370; Layout.minimumWidth: 285; Layout.fillHeight: true; color: root.panelColor; border.color: root.borderColor; border.width: 1; radius: 4
                Text { id: graphTitle; anchors.left: parent.left; anchors.leftMargin: 12; anchors.top: parent.top; anchors.topMargin: 11; text: "LIVE GRAPH"; color: root.textColor; font.pixelSize: 13; font.bold: true; font.letterSpacing: 0.5 }
                ToolAction { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: graphTitle.verticalCenter; width: 92; height: 25; text: "CLEAR GRAPH"; onClicked: if (root.inspector) root.inspector.clearGraph() }
                Rectangle { id: graphRule; anchors.left: parent.left; anchors.right: parent.right; anchors.top: graphTitle.bottom; anchors.topMargin: 9; height: 1; color: root.borderColor }
                ColumnLayout {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: graphRule.bottom; anchors.bottom: parent.bottom; anchors.margins: 12; spacing: 7
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        Text { Layout.fillWidth: true; text: root.hasSelection ? root.valueText(root.selectedDetails.messageName, "STREAM") : "SELECT STREAM"; color: root.textColor; font.pixelSize: 11; elide: Text.ElideRight }
                        ComboBox {
                            id: graphFieldCombo
                            Layout.preferredWidth: 108; Layout.preferredHeight: 27; model: root.graphFields; textRole: "label"; valueRole: "value"; enabled: root.hasSelection && count > 0
                            currentIndex: {
                                for (var fieldIndex = 0; fieldIndex < root.graphFields.length; ++fieldIndex) {
                                    if (root.graphFields[fieldIndex].value === (root.inspector ? root.inspector.selectedGraphField : ""))
                                        return fieldIndex
                                }
                                return -1
                            }
                            onActivated: function(index) {
                                var field = root.graphFields[index]
                                if (root.inspector && field)
                                    root.inspector.selectGraphField(field.value)
                            }
                            contentItem: Text { leftPadding: 8; rightPadding: 20; text: graphFieldCombo.displayText; color: root.textColor; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            background: Rectangle { color: root.workspaceColor; border.color: graphFieldCombo.activeFocus ? root.accentColor : root.borderColor; border.width: 1; radius: 3 }
                        }
                        Text { text: "30 s"; color: root.mutedColor; font.pixelSize: 10 }
                    }
                    Canvas {
                        id: graphCanvas
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 130; visible: root.graphHasData
                        onPaint: {
                            var context = getContext("2d"); context.reset(); context.fillStyle = root.workspaceColor; context.fillRect(0, 0, width, height)
                            var padLeft = 34, padRight = 8, padTop = 9, padBottom = 22; var plotW = Math.max(1, width - padLeft - padRight), plotH = Math.max(1, height - padTop - padBottom)
                            context.strokeStyle = root.borderColor; context.lineWidth = 1
                            for (var g = 0; g <= 4; ++g) { var gy = padTop + plotH * g / 4; context.beginPath(); context.moveTo(padLeft, gy); context.lineTo(width - padRight, gy); context.stroke() }
                            for (var xg = 0; xg <= 5; ++xg) { var gx = padLeft + plotW * xg / 5; context.beginPath(); context.moveTo(gx, padTop); context.lineTo(gx, height - padBottom); context.stroke() }
                            var points = root.graph.points, min = Number(root.graph.minimum), max = Number(root.graph.maximum)
                            if (!isFinite(min) || !isFinite(max)) return
                            if (min === max) { min -= 0.5; max += 0.5 }
                            var start = Number(root.graph.visibleStartTime), end = Number(root.graph.visibleEndTime); if (!isFinite(start) || !isFinite(end) || end <= start) { start = 0; end = Math.max(1, points.length - 1) }
                            var timeRange = end - start, valueRange = max - min
                            if (!isFinite(timeRange) || !isFinite(valueRange) || timeRange <= 0 || valueRange <= 0) return
                            context.strokeStyle = root.accentColor; context.lineWidth = 1.5; context.beginPath()
                            var lastPx = -1, lastPy = -1
                            for (var i = 0; i < points.length; ++i) {
                                var point = points[i], t = Number(point[0]), value = Number(point[1])
                                if (!isFinite(t) || !isFinite(value)) continue
                                var normalizedX = (t - start) / timeRange, normalizedY = (value - min) / valueRange
                                if (!isFinite(normalizedX) || !isFinite(normalizedY)) continue
                                var px = padLeft + Math.max(0, Math.min(1, normalizedX)) * plotW
                                var py = padTop + (1 - Math.max(0, Math.min(1, normalizedY))) * plotH
                                if (lastPx < 0) context.moveTo(px, py); else context.lineTo(px, py)
                                lastPx = px; lastPy = py
                            }
                            context.stroke()
                            if (lastPx >= 0) { context.fillStyle = root.accentColor; context.beginPath(); context.arc(lastPx, lastPy, 2.5, 0, Math.PI * 2); context.fill() }
                            context.fillStyle = root.mutedColor; context.font = "10px sans-serif"; context.fillText(max.toFixed(3), 1, padTop + 8); context.fillText(min.toFixed(3), 1, height - padBottom); context.fillText("TIME (s)", width / 2 - 20, height - 5)
                        }
                        Connections { target: root.graph; function onChanged() { graphCanvas.requestPaint() } }
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                    }
                    Item {
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 130; visible: !root.graphHasData
                        Rectangle { anchors.fill: parent; color: root.workspaceColor; border.color: root.borderColor; border.width: 1 }
                        Text { anchors.centerIn: parent; text: "SELECT A NUMERIC FIELD"; color: root.mutedColor; font.pixelSize: 11; font.letterSpacing: 0.8 }
                    }
                    RowLayout { Layout.fillWidth: true; visible: root.graphHasData; spacing: 12
                        GraphMetric { Layout.fillWidth: true; label: "MIN"; value: root.graph ? Number(root.graph.minimum).toFixed(3) : "--" }
                        GraphMetric { Layout.fillWidth: true; label: "MAX"; value: root.graph ? Number(root.graph.maximum).toFixed(3) : "--" }
                        GraphMetric { Layout.fillWidth: true; label: "CURRENT"; value: root.graph ? Number(root.graph.latestValue).toFixed(3) : "--"; accent: true }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true; Layout.preferredWidth: 330; Layout.minimumWidth: 260; Layout.fillHeight: true; color: root.panelColor; border.color: root.borderColor; border.width: 1; radius: 4
                Text { id: rawTitle; anchors.left: parent.left; anchors.leftMargin: 12; anchors.top: parent.top; anchors.topMargin: 11; text: "RAW PACKET"; color: root.textColor; font.pixelSize: 13; font.bold: true; font.letterSpacing: 0.5 }
                ToolAction { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: rawTitle.verticalCenter; width: 76; height: 25; text: "COPY HEX"; enabled: root.hasRawPacket; onClicked: if (root.inspector) root.inspector.copySelectedRawHex() }
                Rectangle { id: rawRule; anchors.left: parent.left; anchors.right: parent.right; anchors.top: rawTitle.bottom; anchors.topMargin: 9; height: 1; color: root.borderColor }
                ColumnLayout {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: rawRule.bottom; anchors.bottom: parent.bottom; anchors.margins: 12; spacing: 7; visible: root.hasRawPacket
                    Text { Layout.fillWidth: true; text: root.valueText(root.selectedRawPacket.messageName, root.valueText(root.selectedDetails.messageName, "--")) + "  (" + root.valueText(root.selectedRawPacket.messageId, root.valueText(root.selectedDetails.messageId, "--")) + ")"; color: root.textColor; font.pixelSize: 15; font.bold: true; elide: Text.ElideRight }
                    GridLayout {
                        Layout.fillWidth: true; columns: 2; rowSpacing: 3; columnSpacing: 12
                        DetailMetric { label: "SYSID"; value: root.valueText(root.selectedRawPacket.sysid, "--") }
                        DetailMetric { label: "COMPID"; value: root.valueText(root.selectedRawPacket.compid, "--") }
                        DetailMetric { label: "LENGTH"; value: root.valueText(root.selectedRawPacket.packetLength, "--") + " B" }
                        DetailMetric { label: "SEQUENCE"; value: root.valueText(root.selectedRawPacket.sequence, "--") }
                        DetailMetric { label: "PROTOCOL"; value: root.valueText(root.selectedRawPacket.mavlinkVersion, "MAVLink --") }
                        DetailMetric { label: "LINK"; value: root.valueText(root.selectedRawPacket.link, "--") }
                    }
                    RowLayout { Layout.fillWidth: true; spacing: 0
                        PacketTab { Layout.fillWidth: true; text: "HEX"; selected: root.showHex; onClicked: root.showHex = true }
                        PacketTab { Layout.fillWidth: true; text: "ASCII"; selected: !root.showHex; onClicked: root.showHex = false }
                    }
                    ScrollView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        background: Rectangle { color: root.workspaceColor; border.color: root.borderColor; border.width: 1 }
                        TextArea { readOnly: true; selectByMouse: true; wrapMode: TextArea.NoWrap; text: root.showHex ? root.valueText(root.selectedRawPacket.formatted, "") : root.valueText(root.selectedRawPacket.ascii, ""); color: root.textColor; font.family: "Consolas"; font.pixelSize: 11; leftPadding: 9; rightPadding: 9; topPadding: 8; bottomPadding: 8; background: null }
                    }
                }
                EmptyState { anchors.fill: parent; anchors.topMargin: 30; visible: !root.hasRawPacket; title: "NO RAW PACKET"; detail: "Select a MAVLink stream to inspect its latest packet." }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 28; color: "transparent"; border.color: root.borderColor; border.width: 1; radius: 3
            RowLayout { anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10; spacing: 14
                Text { text: root.inspector ? "SHOWING " + root.inspector.filteredStreamCount + " / " + root.inspector.streamCount + " STREAMS" : "SHOWING 0 / 0 STREAMS"; color: root.mutedColor; font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.4 }
                Text { text: root.inspector ? root.rateText(root.inspector.totalRxRate) : "--"; color: root.textColor; font.pixelSize: 10 }
                Text { text: root.inspector ? root.bytesText(root.inspector.totalRxBytesPerSecond) : "--"; color: root.textColor; font.pixelSize: 10 }
                Item { Layout.fillWidth: true }
                Text { text: root.inspector && root.inspector.paused ? "PAUSED" : "LIVE"; color: root.inspector && root.inspector.paused ? root.pausedColor : root.liveColor; font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.8 }
            }
        }
    }

    component HeaderStat: ColumnLayout {
        id: headerStat
        property string label: ""; property string value: "--"; Layout.preferredWidth: 67; spacing: 1
        Text { text: headerStat.label; color: root.mutedColor; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.6 }
        Text { text: headerStat.value; color: root.textColor; font.pixelSize: 12; font.bold: true }
    }
    component ToolAction: Button {
        id: toolAction
        property bool emphasized: false; property bool destructive: false
        implicitWidth: 76; implicitHeight: 28; font.pixelSize: 10; font.bold: true
        contentItem: Text { text: toolAction.text; color: toolAction.destructive ? "#ef8b91" : (toolAction.emphasized ? root.accentColor : root.textColor); font: toolAction.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 3; color: toolAction.down ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.16) : (toolAction.hovered ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.06) : "transparent"); border.color: toolAction.destructive ? "#b74f58" : (toolAction.emphasized ? root.accentColor : root.borderColor); border.width: 1 }
    }
    component FilterLabel: Text { color: root.mutedColor; font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.6 }
    component FilterCombo: ComboBox {
        id: filterCombo
        Layout.fillWidth: true; Layout.preferredHeight: 28; textRole: "label"; valueRole: "value"
        contentItem: Text { leftPadding: 8; rightPadding: 20; text: filterCombo.displayText; color: root.textColor; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
        background: Rectangle { color: root.workspaceColor; border.color: filterCombo.activeFocus ? root.accentColor : root.borderColor; border.width: 1; radius: 3 }
    }
    component TrafficHeader: Text { color: root.mutedColor; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.4; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
    component TrafficCell: Text { color: root.textColor; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
    component FieldHeader: Text { color: root.mutedColor; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.4; elide: Text.ElideRight }
    component DetailMetric: ColumnLayout {
        id: detailMetric
        Layout.fillWidth: true; spacing: 0; property string label: ""; property string value: "--"
        Text { text: detailMetric.label; color: root.mutedColor; font.pixelSize: 9; font.bold: true; font.letterSpacing: 0.35 }
        Text { text: detailMetric.value; color: root.textColor; font.pixelSize: 11; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
    }
    component GraphMetric: ColumnLayout {
        id: graphMetric
        property string label: ""; property string value: "--"; property bool accent: false; spacing: 0
        Text { text: graphMetric.label; color: root.mutedColor; font.pixelSize: 9; font.bold: true }
        Text { text: graphMetric.value; color: graphMetric.accent ? root.liveColor : root.textColor; font.pixelSize: 12; font.bold: true }
    }
    component PacketTab: Button {
        id: packetTab
        property bool selected: false; implicitHeight: 25; font.pixelSize: 10; font.bold: true
        contentItem: Text { text: packetTab.text; color: packetTab.selected ? root.workspaceColor : root.mutedColor; font: packetTab.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: packetTab.selected ? root.accentColor : root.workspaceColor; border.color: root.borderColor; border.width: 1; radius: 2 }
    }
    component EmptyState: Item {
        id: emptyState
        property string title: ""; property string detail: ""
        ColumnLayout { anchors.centerIn: parent; width: Math.min(parent.width - 30, 260); spacing: 4
            Text { Layout.fillWidth: true; text: emptyState.title; color: root.mutedColor; font.pixelSize: 11; font.bold: true; font.letterSpacing: 0.8; horizontalAlignment: Text.AlignHCenter }
            Text { Layout.fillWidth: true; text: emptyState.detail; color: root.mutedColor; font.pixelSize: 10; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
        }
    }
}
