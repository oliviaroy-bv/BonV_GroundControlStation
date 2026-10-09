// // // // // // // // import QtQuick 6.0
// // // // // // // // import QtQuick.Controls 6.0
// // // // // // // // import QtQuick.Layouts 6.0

// // // // // // // // Item {
// // // // // // // //     id: root
// // // // // // // //     implicitHeight: mainCol.implicitHeight

// // // // // // // //     signal fleetChanged()

// // // // // // // //     // ── Edit form state ───────────────────────────────────────────────────────
// // // // // // // //     property bool   showForm:  false
// // // // // // // //     property bool   editMode:  false
// // // // // // // //     property int    editIndex: -1

// // // // // // // //     // Form state (at root scope so openAdd/openEdit can set before form is visible)
// // // // // // // //     property bool capDelivery: true
// // // // // // // //     property bool capDrop:     false
// // // // // // // //     property bool capParadrop: false
// // // // // // // //     property int  rcChannel:   1
// // // // // // // //     property int  rcTriggers:  1
// // // // // // // //     property int  rcCountVal:  1

// // // // // // // //     function openAdd() {
// // // // // // // //         editMode       = false
// // // // // // // //         editIndex      = -1
// // // // // // // //         efName.text    = ""
// // // // // // // //         efRange.value  = 5
// // // // // // // //         efSpeed.value  = 50
// // // // // // // //         capDelivery    = true
// // // // // // // //         capDrop        = false
// // // // // // // //         capParadrop    = false
// // // // // // // //         efRcCh.value   = 1
// // // // // // // //         root.rcCountVal = 1
// // // // // // // //         efTakeoff.value  = 30
// // // // // // // //         efNearHome.value = 60
// // // // // // // //         efCruise.value   = 120
// // // // // // // //         efLand.value     = 5
// // // // // // // //         showForm = true
// // // // // // // //     }

// // // // // // // //     function openEdit(idx) {
// // // // // // // //         var d = droneManager.droneAt(idx)
// // // // // // // //         editMode       = true
// // // // // // // //         editIndex      = idx
// // // // // // // //         efName.text    = d.name
// // // // // // // //         efRange.value  = Math.round(d.rangeKm)
// // // // // // // //         efSpeed.value  = Math.round(d.speedMs * 10)
// // // // // // // //         capDelivery    = d.delivery
// // // // // // // //         capDrop        = d.drop
// // // // // // // //         capParadrop    = d.paradrop
// // // // // // // //         efRcCh.value   = d.rcSwitch > 0 ? d.rcSwitch : 1
// // // // // // // //         root.rcCountVal = d.rcCount > 0 ? d.rcCount : 1
// // // // // // // //         efTakeoff.value  = Math.round(d.takeoffAlt)
// // // // // // // //         efNearHome.value = Math.round(d.nearHomeAlt)
// // // // // // // //         efCruise.value   = Math.round(d.cruiseAlt)
// // // // // // // //         efLand.value     = Math.round(d.landAlt)
// // // // // // // //         showForm = true
// // // // // // // //     }

// // // // // // // //     function saveForm() {
// // // // // // // //         var name = efName.text.trim()
// // // // // // // //         if (name === "") { validationMsg.visible = true; return }
// // // // // // // //         validationMsg.visible = false
// // // // // // // //         var needsRc = capDrop || capParadrop
// // // // // // // //         var rc      = needsRc ? efRcCh.value   : 0
// // // // // // // //         var rcCnt   = needsRc ? root.rcCountVal : 0

// // // // // // // //         if (editMode) {
// // // // // // // //             droneManager.updateDrone(
// // // // // // // //                 editIndex, name,
// // // // // // // //                 efRange.value, efSpeed.value / 10.0,
// // // // // // // //                 capDelivery, capDrop, capParadrop,
// // // // // // // //                 rc, rcCnt,
// // // // // // // //                 efTakeoff.value, efNearHome.value,
// // // // // // // //                 efCruise.value, efLand.value
// // // // // // // //             )
// // // // // // // //         } else {
// // // // // // // //             droneManager.addDrone(
// // // // // // // //                 name,
// // // // // // // //                 efRange.value, efSpeed.value / 10.0,
// // // // // // // //                 capDelivery, capDrop, capParadrop,
// // // // // // // //                 rc, rcCnt,
// // // // // // // //                 efTakeoff.value, efNearHome.value,
// // // // // // // //                 efCruise.value, efLand.value
// // // // // // // //             )
// // // // // // // //         }
// // // // // // // //         showForm = false
// // // // // // // //         root.fleetChanged()
// // // // // // // //     }

// // // // // // // //     // ── Main column ───────────────────────────────────────────────────────────
// // // // // // // //     ColumnLayout {
// // // // // // // //         id: mainCol
// // // // // // // //         anchors { left: parent.left; right: parent.right }
// // // // // // // //         spacing: 0

// // // // // // // //         // Header
// // // // // // // //         RowLayout {
// // // // // // // //             Layout.fillWidth: true
// // // // // // // //             spacing: 8

// // // // // // // //             Text {
// // // // // // // //                 text: "Drone Fleet  (" + droneManager.count + " total)"
// // // // // // // //                 font.family: Theme.fontFamily
// // // // // // // //                 font.pixelSize: Theme.fontBase
// // // // // // // //                 color: Theme.textSecond
// // // // // // // //                 Layout.fillWidth: true
// // // // // // // //             }

// // // // // // // //             FpButton {
// // // // // // // //                 text: "+ Add Drone"
// // // // // // // //                 implicitWidth: 96; implicitHeight: 26
// // // // // // // //                 onClicked: root.openAdd()
// // // // // // // //             }
// // // // // // // //         }

// // // // // // // //         Item { implicitHeight: 8 }

// // // // // // // //         // ── Drone list (scrollable, max 260px) ───────────────────────────────
// // // // // // // //         Item {
// // // // // // // //             Layout.fillWidth: true
// // // // // // // //             implicitHeight: Math.min(listCol.implicitHeight, 260)
// // // // // // // //             clip: true

// // // // // // // //             Flickable {
// // // // // // // //                 anchors.fill: parent
// // // // // // // //                 contentHeight: listCol.implicitHeight
// // // // // // // //                 clip: true

// // // // // // // //                 Column {
// // // // // // // //                     id: listCol
// // // // // // // //                     width: parent.width
// // // // // // // //                     spacing: 4

// // // // // // // //                     Repeater {
// // // // // // // //                         model: droneManager

// // // // // // // //                         delegate: Rectangle {
// // // // // // // //                             width: parent.width
// // // // // // // //                             height: rowContent.implicitHeight + 14
// // // // // // // //                             radius: Theme.radius
// // // // // // // //                             color: droneActive ? Theme.bgSection : "#F8F7F4"
// // // // // // // //                             border.color: droneActive ? Theme.border : Theme.borderLight
// // // // // // // //                             border.width: 0.5
// // // // // // // //                             opacity: droneActive ? 1.0 : 0.65

// // // // // // // //                             RowLayout {
// // // // // // // //                                 id: rowContent
// // // // // // // //                                 anchors {
// // // // // // // //                                     left: parent.left; right: parent.right
// // // // // // // //                                     verticalCenter: parent.verticalCenter
// // // // // // // //                                     margins: 10
// // // // // // // //                                 }
// // // // // // // //                                 spacing: 8

// // // // // // // //                                 // Active dot
// // // // // // // //                                 Rectangle {
// // // // // // // //                                     width: 8; height: 8; radius: 4
// // // // // // // //                                     color: droneActive ? Theme.green : Theme.textHint

// // // // // // // //                                     MouseArea {
// // // // // // // //                                         anchors.fill: parent
// // // // // // // //                                         cursorShape: Qt.PointingHandCursor
// // // // // // // //                                         onClicked: droneManager.setActive(index, !droneActive)
// // // // // // // //                                     }
// // // // // // // //                                 }

// // // // // // // //                                 // Name + badges
// // // // // // // //                                 ColumnLayout {
// // // // // // // //                                     Layout.fillWidth: true
// // // // // // // //                                     spacing: 3

// // // // // // // //                                     Text {
// // // // // // // //                                         text: droneName
// // // // // // // //                                         font.family: Theme.fontFamily
// // // // // // // //                                         font.pixelSize: Theme.fontBase
// // // // // // // //                                         font.weight: Font.Medium
// // // // // // // //                                         color: Theme.textPrimary
// // // // // // // //                                     }

// // // // // // // //                                     Row {
// // // // // // // //                                         spacing: 5

// // // // // // // //                                         Repeater {
// // // // // // // //                                             model: droneCapabilities
// // // // // // // //                                             delegate: Rectangle {
// // // // // // // //                                                 height: 15
// // // // // // // //                                                 width: badgeLabel.implicitWidth + 10
// // // // // // // //                                                 radius: 3
// // // // // // // //                                                 color: modelData === "Delivery" ? Theme.greenLight
// // // // // // // //                                                      : modelData === "Drop"     ? Theme.blueLight
// // // // // // // //                                                                                 : Theme.amberLight
// // // // // // // //                                                 Text {
// // // // // // // //                                                     id: badgeLabel
// // // // // // // //                                                     anchors.centerIn: parent
// // // // // // // //                                                     text: modelData
// // // // // // // //                                                     font.family: Theme.fontFamily
// // // // // // // //                                                     font.pixelSize: 9
// // // // // // // //                                                     font.weight: Font.Medium
// // // // // // // //                                                     color: modelData === "Delivery" ? Theme.green
// // // // // // // //                                                          : modelData === "Drop"     ? Theme.blue
// // // // // // // //                                                                                     : Theme.amber
// // // // // // // //                                                 }
// // // // // // // //                                             }
// // // // // // // //                                         }

// // // // // // // //                                         // RC badge
// // // // // // // //                                         Rectangle {
// // // // // // // //                                             visible: droneNeedsRc
// // // // // // // //                                             height: 15
// // // // // // // //                                             width: rcLabel.implicitWidth + 10
// // // // // // // //                                             radius: 3
// // // // // // // //                                             color: "#F0EAF8"
// // // // // // // //                                             Text {
// // // // // // // //                                                 id: rcLabel
// // // // // // // //                                                 anchors.centerIn: parent
// // // // // // // //                                                 text: "CH" + droneRcSwitch
// // // // // // // //                                                 font.family: Theme.fontFamily
// // // // // // // //                                                 font.pixelSize: 9
// // // // // // // //                                                 font.weight: Font.Medium
// // // // // // // //                                                 color: "#6B3FA0"
// // // // // // // //                                             }
// // // // // // // //                                         }
// // // // // // // //                                     }
// // // // // // // //                                 }

// // // // // // // //                                 // Stats
// // // // // // // //                                 Column {
// // // // // // // //                                     spacing: 1
// // // // // // // //                                     Text {
// // // // // // // //                                         text: (droneRangeKm * 2).toFixed(0) + " km"
// // // // // // // //                                         font.family: Theme.fontFamily
// // // // // // // //                                         font.pixelSize: Theme.fontSm
// // // // // // // //                                         font.weight: Font.Medium
// // // // // // // //                                         color: Theme.textPrimary
// // // // // // // //                                     }
// // // // // // // //                                     Text {
// // // // // // // //                                         text: droneSpeedMs.toFixed(1) + " m/s"
// // // // // // // //                                         font.family: Theme.fontFamily
// // // // // // // //                                         font.pixelSize: 10
// // // // // // // //                                         color: Theme.textMuted
// // // // // // // //                                     }
// // // // // // // //                                 }

// // // // // // // //                                 // Edit
// // // // // // // //                                 FpButton {
// // // // // // // //                                     text: "Edit"
// // // // // // // //                                     implicitWidth: 44; implicitHeight: 24
// // // // // // // //                                     onClicked: root.openEdit(index)
// // // // // // // //                                 }

// // // // // // // //                                 // Deactivate / Reactivate
// // // // // // // //                                 Rectangle {
// // // // // // // //                                     width: 24; height: 24; radius: Theme.radius
// // // // // // // //                                     visible: droneActive
// // // // // // // //                                     color: xArea.containsMouse ? Theme.redLight : "transparent"
// // // // // // // //                                     border.color: xArea.containsMouse ? "#F7C1C1" : Theme.border
// // // // // // // //                                     border.width: 0.5
// // // // // // // //                                     Text {
// // // // // // // //                                         anchors.centerIn: parent
// // // // // // // //                                         text: "✕"; color: Theme.red; font.pixelSize: 11
// // // // // // // //                                     }
// // // // // // // //                                     MouseArea {
// // // // // // // //                                         id: xArea
// // // // // // // //                                         anchors.fill: parent
// // // // // // // //                                         hoverEnabled: true
// // // // // // // //                                         cursorShape: Qt.PointingHandCursor
// // // // // // // //                                         onClicked: { droneManager.deactivate(index); root.fleetChanged() }
// // // // // // // //                                     }
// // // // // // // //                                 }

// // // // // // // //                                 Rectangle {
// // // // // // // //                                     width: 24; height: 24; radius: Theme.radius
// // // // // // // //                                     visible: !droneActive
// // // // // // // //                                     color: plusArea.containsMouse ? Theme.greenLight : "transparent"
// // // // // // // //                                     border.color: plusArea.containsMouse ? "#C0DD97" : Theme.border
// // // // // // // //                                     border.width: 0.5
// // // // // // // //                                     Text {
// // // // // // // //                                         anchors.centerIn: parent
// // // // // // // //                                         text: "+"; color: Theme.green; font.pixelSize: 14
// // // // // // // //                                     }
// // // // // // // //                                     MouseArea {
// // // // // // // //                                         id: plusArea
// // // // // // // //                                         anchors.fill: parent
// // // // // // // //                                         hoverEnabled: true
// // // // // // // //                                         cursorShape: Qt.PointingHandCursor
// // // // // // // //                                         onClicked: { droneManager.setActive(index, true); root.fleetChanged() }
// // // // // // // //                                     }
// // // // // // // //                                 }
// // // // // // // //                             }
// // // // // // // //                         }
// // // // // // // //                     }
// // // // // // // //                 }
// // // // // // // //             }
// // // // // // // //         }

// // // // // // // //         Item { implicitHeight: 10 }

// // // // // // // //         // ── Add / Edit form ───────────────────────────────────────────────────
// // // // // // // //         Rectangle {
// // // // // // // //             Layout.fillWidth: true
// // // // // // // //             visible: root.showForm
// // // // // // // //             height: visible ? formCol.implicitHeight + 24 : 0
// // // // // // // //             clip: true
// // // // // // // //             radius: Theme.radiusLg
// // // // // // // //             color: Theme.bgSection
// // // // // // // //             border.color: Theme.borderFocus
// // // // // // // //             border.width: 1

// // // // // // // //             Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

// // // // // // // //             ColumnLayout {
// // // // // // // //                 id: formCol
// // // // // // // //                 anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
// // // // // // // //                 spacing: 10

// // // // // // // //                 Text {
// // // // // // // //                     text: root.editMode ? "Edit Drone" : "Add New Drone"
// // // // // // // //                     font.family: Theme.fontFamily
// // // // // // // //                     font.pixelSize: Theme.fontBase
// // // // // // // //                     font.weight: Font.Medium
// // // // // // // //                     color: Theme.textPrimary
// // // // // // // //                 }

// // // // // // // //                 // Name
// // // // // // // //                 ColumnLayout {
// // // // // // // //                     Layout.fillWidth: true
// // // // // // // //                     spacing: 4
// // // // // // // //                     Text { text: "Name"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // // // //                     FpInput {
// // // // // // // //                         id: efName
// // // // // // // //                         Layout.fillWidth: true
// // // // // // // //                         placeholderText: "e.g. Eagle-X"
// // // // // // // //                     }
// // // // // // // //                 }

// // // // // // // //                 // Range + Speed side by side
// // // // // // // //                 RowLayout {
// // // // // // // //                     Layout.fillWidth: true
// // // // // // // //                     spacing: 10

// // // // // // // //                     ColumnLayout {
// // // // // // // //                         Layout.fillWidth: true
// // // // // // // //                         spacing: 4
// // // // // // // //                         Text { text: "One-way range"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // // // //                         FpSpinBox {
// // // // // // // //                             id: efRange
// // // // // // // //                             label: ""; unit: "km"
// // // // // // // //                             value: 5; from: 1; to: 500; stepSize: 1
// // // // // // // //                             Layout.fillWidth: true
// // // // // // // //                         }
// // // // // // // //                     }

// // // // // // // //                     ColumnLayout {
// // // // // // // //                         Layout.fillWidth: true
// // // // // // // //                         spacing: 4
// // // // // // // //                         Text { text: "Max speed"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // // // //                         FpSpinBox {
// // // // // // // //                             id: efSpeed
// // // // // // // //                             label: ""; unit: "m/s"
// // // // // // // //                             value: 50; from: 5; to: 200; stepSize: 5
// // // // // // // //                             Layout.fillWidth: true
// // // // // // // //                         }
// // // // // // // //                     }
// // // // // // // //                 }

// // // // // // // //                 // Capabilities
// // // // // // // //                 ColumnLayout {
// // // // // // // //                     spacing: 6
// // // // // // // //                     Text { text: "Mission Capabilities"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // // // // // //                     RowLayout {
// // // // // // // //                         spacing: 20

// // // // // // // //                         // Delivery
// // // // // // // //                         RowLayout {
// // // // // // // //                             spacing: 6
// // // // // // // //                             Rectangle {
// // // // // // // //                                 width: 16; height: 16; radius: 3
// // // // // // // //                                 color: root.capDelivery ? Theme.blueMid : Theme.bgInput
// // // // // // // //                                 border.color: root.capDelivery ? Theme.blueMid : Theme.border
// // // // // // // //                                 border.width: 0.5
// // // // // // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 10; visible: root.capDelivery }
// // // // // // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDelivery = !root.capDelivery }
// // // // // // // //                             }
// // // // // // // //                             Text { text: "Delivery"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // // // // // //                         }

// // // // // // // //                         // Drop
// // // // // // // //                         RowLayout {
// // // // // // // //                             spacing: 6
// // // // // // // //                             Rectangle {
// // // // // // // //                                 width: 16; height: 16; radius: 3
// // // // // // // //                                 color: root.capDrop ? Theme.blueMid : Theme.bgInput
// // // // // // // //                                 border.color: root.capDrop ? Theme.blueMid : Theme.border
// // // // // // // //                                 border.width: 0.5
// // // // // // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 10; visible: root.capDrop }
// // // // // // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDrop = !root.capDrop }
// // // // // // // //                             }
// // // // // // // //                             Text { text: "Drop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // // // // // //                         }

// // // // // // // //                         // Paradrop
// // // // // // // //                         RowLayout {
// // // // // // // //                             spacing: 6
// // // // // // // //                             Rectangle {
// // // // // // // //                                 width: 16; height: 16; radius: 3
// // // // // // // //                                 color: root.capParadrop ? Theme.blueMid : Theme.bgInput
// // // // // // // //                                 border.color: root.capParadrop ? Theme.blueMid : Theme.border
// // // // // // // //                                 border.width: 0.5
// // // // // // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 10; visible: root.capParadrop }
// // // // // // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capParadrop = !root.capParadrop }
// // // // // // // //                             }
// // // // // // // //                             Text { text: "Paradrop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // // // // // //                         }
// // // // // // // //                     }
// // // // // // // //                 }

// // // // // // // //                 // RC Channel — only when Drop or Paradrop
// // // // // // // //                 RowLayout {
// // // // // // // //                     visible: root.capDrop || root.capParadrop
// // // // // // // //                     spacing: 12

// // // // // // // //                     Text {
// // // // // // // //                         text: "RC Channel (trigger)"
// // // // // // // //                         font.family: Theme.fontFamily
// // // // // // // //                         font.pixelSize: Theme.fontSm
// // // // // // // //                         color: Theme.textMuted
// // // // // // // //                         Layout.alignment: Qt.AlignVCenter
// // // // // // // //                     }

// // // // // // // //                     FpSpinBox {
// // // // // // // //                         id: efRcCh
// // // // // // // //                         label: ""; unit: ""
// // // // // // // //                         value: 1; from: 1; to: 16; stepSize: 1
// // // // // // // //                         implicitWidth: 80
// // // // // // // //                     }

// // // // // // // //                     Text {
// // // // // // // //                         text: "Channel number for release"
// // // // // // // //                         font.family: Theme.fontFamily
// // // // // // // //                         font.pixelSize: 10
// // // // // // // //                         color: Theme.textHint
// // // // // // // //                         Layout.alignment: Qt.AlignVCenter
// // // // // // // //                     }
// // // // // // // //                 }

// // // // // // // //                 // Altitude defaults
// // // // // // // //                 ColumnLayout {
// // // // // // // //                     spacing: 6
// // // // // // // //                     Text { text: "Default Altitudes"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // // // // // //                     RowLayout {
// // // // // // // //                         Layout.fillWidth: true
// // // // // // // //                         spacing: 10
// // // // // // // //                         FpSpinBox { id: efTakeoff;  label: "Takeoff";   unit: "m"; value: 30;  from: 10; to: 300; stepSize: 5;  Layout.fillWidth: true }
// // // // // // // //                         FpSpinBox { id: efNearHome; label: "Near-home"; unit: "m"; value: 60;  from: 20; to: 400; stepSize: 5;  Layout.fillWidth: true }
// // // // // // // //                     }
// // // // // // // //                     RowLayout {
// // // // // // // //                         Layout.fillWidth: true
// // // // // // // //                         spacing: 10
// // // // // // // //                         FpSpinBox { id: efCruise; label: "Clearance"; unit: "m"; value: 120; from: 80; to: 800; stepSize: 10; Layout.fillWidth: true }
// // // // // // // //                         FpSpinBox { id: efLand;   label: "Land";      unit: "m"; value: 5;   from: 0;  to: 50;  stepSize: 1;  Layout.fillWidth: true }
// // // // // // // //                     }
// // // // // // // //                 }

// // // // // // // //                 // Validation
// // // // // // // //                 Text {
// // // // // // // //                     id: validationMsg
// // // // // // // //                     visible: false
// // // // // // // //                     text: "Please enter a drone name"
// // // // // // // //                     color: Theme.red
// // // // // // // //                     font.family: Theme.fontFamily
// // // // // // // //                     font.pixelSize: Theme.fontSm
// // // // // // // //                 }

// // // // // // // //                 // Buttons
// // // // // // // //                 RowLayout {
// // // // // // // //                     spacing: 8

// // // // // // // //                     FpButton {
// // // // // // // //                         text: root.editMode ? "Update Drone" : "Add Drone"
// // // // // // // //                         primary: true
// // // // // // // //                         implicitWidth: 110
// // // // // // // //                         onClicked: root.saveForm()
// // // // // // // //                     }

// // // // // // // //                     FpButton {
// // // // // // // //                         text: "Cancel"
// // // // // // // //                         implicitWidth: 70
// // // // // // // //                         onClicked: { root.showForm = false; validationMsg.visible = false }
// // // // // // // //                     }
// // // // // // // //                 }

// // // // // // // //                 Item { implicitHeight: 2 }
// // // // // // // //             }
// // // // // // // //         }
// // // // // // // //     }
// // // // // // // // }



// // // // // // // import QtQuick 6.0
// // // // // // // import QtQuick.Controls 6.0
// // // // // // // import QtQuick.Layouts 6.0

// // // // // // // Item {
// // // // // // //     id: root
// // // // // // //     implicitHeight: mainCol.implicitHeight

// // // // // // //     signal fleetChanged()

// // // // // // //     // ── Form state (root-scope so openAdd/openEdit can set before form renders) ─
// // // // // // //     property bool showForm:    false
// // // // // // //     property bool editMode:    false
// // // // // // //     property int  editIndex:   -1

// // // // // // //     property bool capDelivery: true
// // // // // // //     property bool capDrop:     false
// // // // // // //     property bool capParadrop: false
// // // // // // //     property int  rcChannel:   1
// // // // // // //     property int  rcTriggers:  1

// // // // // // //     // ── Functions ─────────────────────────────────────────────────────────────
// // // // // // //     function openAdd() {
// // // // // // //         editMode         = false
// // // // // // //         editIndex        = -1
// // // // // // //         efName.text      = ""
// // // // // // //         efRange.value    = 5
// // // // // // //         efSpeed.value    = 50
// // // // // // //         capDelivery      = true
// // // // // // //         capDrop          = false
// // // // // // //         capParadrop      = false
// // // // // // //         rcChannel        = 1
// // // // // // //         rcTriggers       = 1
// // // // // // //         efTakeoff.value  = 30
// // // // // // //         efNearHome.value = 60
// // // // // // //         efCruise.value   = 120
// // // // // // //         efLand.value     = 5
// // // // // // //         showForm         = true
// // // // // // //     }

// // // // // // //     function openEdit(idx) {
// // // // // // //         var d            = droneManager.droneAt(idx)
// // // // // // //         editMode         = true
// // // // // // //         editIndex        = idx
// // // // // // //         efName.text      = d.name
// // // // // // //         efRange.value    = Math.round(d.rangeKm)
// // // // // // //         efSpeed.value    = Math.round(d.speedMs * 10)
// // // // // // //         capDelivery      = d.delivery
// // // // // // //         capDrop          = d.drop
// // // // // // //         capParadrop      = d.paradrop
// // // // // // //         rcChannel        = d.rcSwitch > 0 ? d.rcSwitch : 1
// // // // // // //         rcTriggers       = d.rcCount  > 0 ? d.rcCount  : 1
// // // // // // //         efTakeoff.value  = Math.round(d.takeoffAlt)
// // // // // // //         efNearHome.value = Math.round(d.nearHomeAlt)
// // // // // // //         efCruise.value   = Math.round(d.cruiseAlt)
// // // // // // //         efLand.value     = Math.round(d.landAlt)
// // // // // // //         showForm         = true
// // // // // // //     }

// // // // // // //     function saveForm() {
// // // // // // //         var name = efName.text.trim()
// // // // // // //         if (name === "") { validationMsg.visible = true; return }
// // // // // // //         validationMsg.visible = false

// // // // // // //         var needsRc = capDrop || capParadrop
// // // // // // //         var rc      = needsRc ? rcChannel  : 0
// // // // // // //         var rcCnt   = needsRc ? rcTriggers : 0

// // // // // // //         if (editMode) {
// // // // // // //             droneManager.updateDrone(
// // // // // // //                 editIndex, name,
// // // // // // //                 efRange.value, efSpeed.value / 10.0,
// // // // // // //                 capDelivery, capDrop, capParadrop,
// // // // // // //                 rc, rcCnt,
// // // // // // //                 efTakeoff.value, efNearHome.value,
// // // // // // //                 efCruise.value,  efLand.value
// // // // // // //             )
// // // // // // //         } else {
// // // // // // //             droneManager.addDrone(
// // // // // // //                 name,
// // // // // // //                 efRange.value, efSpeed.value / 10.0,
// // // // // // //                 capDelivery, capDrop, capParadrop,
// // // // // // //                 rc, rcCnt,
// // // // // // //                 efTakeoff.value, efNearHome.value,
// // // // // // //                 efCruise.value,  efLand.value
// // // // // // //             )
// // // // // // //         }
// // // // // // //         showForm = false
// // // // // // //         root.fleetChanged()
// // // // // // //     }

// // // // // // //     // ── Main column ───────────────────────────────────────────────────────────
// // // // // // //     ColumnLayout {
// // // // // // //         id: mainCol
// // // // // // //         anchors { left: parent.left; right: parent.right }
// // // // // // //         spacing: 0

// // // // // // //         // Header
// // // // // // //         RowLayout {
// // // // // // //             Layout.fillWidth: true
// // // // // // //             spacing: 8

// // // // // // //             Text {
// // // // // // //                 text:           "Drone Fleet  (" + droneManager.count + " total)"
// // // // // // //                 font.family:    Theme.fontFamily
// // // // // // //                 font.pixelSize: Theme.fontBase
// // // // // // //                 color:          Theme.textSecond
// // // // // // //                 Layout.fillWidth: true
// // // // // // //             }

// // // // // // //             FpButton {
// // // // // // //                 text:           "+ Add Drone"
// // // // // // //                 implicitWidth:  96
// // // // // // //                 implicitHeight: 26
// // // // // // //                 onClicked:      root.openAdd()
// // // // // // //             }
// // // // // // //         }

// // // // // // //         Item { implicitHeight: 8 }

// // // // // // //         // ── Drone list ────────────────────────────────────────────────────────
// // // // // // //         Item {
// // // // // // //             Layout.fillWidth: true
// // // // // // //             implicitHeight:   Math.min(listCol.implicitHeight, 300)
// // // // // // //             clip:             true

// // // // // // //             Flickable {
// // // // // // //                 anchors.fill:  parent
// // // // // // //                 contentHeight: listCol.implicitHeight
// // // // // // //                 clip:          true

// // // // // // //                 Column {
// // // // // // //                     id:      listCol
// // // // // // //                     width:   parent.width
// // // // // // //                     spacing: 4

// // // // // // //                     Repeater {
// // // // // // //                         model: droneManager

// // // // // // //                         delegate: Rectangle {
// // // // // // //                             width:        listCol.width
// // // // // // //                             height:       rowContent.implicitHeight + 14
// // // // // // //                             radius:       Theme.radius
// // // // // // //                             color:        droneActive ? Theme.bgSection : "#F8F7F4"
// // // // // // //                             border.color: droneActive ? Theme.border    : Theme.borderLight
// // // // // // //                             border.width: 0.5
// // // // // // //                             opacity:      droneActive ? 1.0 : 0.6

// // // // // // //                             RowLayout {
// // // // // // //                                 id: rowContent
// // // // // // //                                 anchors {
// // // // // // //                                     left: parent.left; right: parent.right
// // // // // // //                                     verticalCenter: parent.verticalCenter
// // // // // // //                                     margins: 10
// // // // // // //                                 }
// // // // // // //                                 spacing: 8

// // // // // // //                                 // Active dot
// // // // // // //                                 Rectangle {
// // // // // // //                                     width: 8; height: 8; radius: 4
// // // // // // //                                     color: droneActive ? Theme.green : Theme.textHint
// // // // // // //                                     MouseArea {
// // // // // // //                                         anchors.fill: parent
// // // // // // //                                         cursorShape:  Qt.PointingHandCursor
// // // // // // //                                         onClicked:    droneManager.setActive(index, !droneActive)
// // // // // // //                                     }
// // // // // // //                                 }

// // // // // // //                                 // Name + capability badges
// // // // // // //                                 ColumnLayout {
// // // // // // //                                     Layout.fillWidth: true
// // // // // // //                                     spacing: 3

// // // // // // //                                     Text {
// // // // // // //                                         text:           droneName
// // // // // // //                                         font.family:    Theme.fontFamily
// // // // // // //                                         font.pixelSize: Theme.fontBase
// // // // // // //                                         font.weight:    Font.Medium
// // // // // // //                                         color:          Theme.textPrimary
// // // // // // //                                     }

// // // // // // //                                     Row {
// // // // // // //                                         spacing: 5

// // // // // // //                                         Repeater {
// // // // // // //                                             model: droneCapabilities
// // // // // // //                                             delegate: Rectangle {
// // // // // // //                                                 height: 15
// // // // // // //                                                 width:  badgeTxt.implicitWidth + 10
// // // // // // //                                                 radius: 3
// // // // // // //                                                 color:  modelData === "Delivery" ? Theme.greenLight
// // // // // // //                                                       : modelData === "Drop"     ? Theme.blueLight
// // // // // // //                                                                                  : Theme.amberLight
// // // // // // //                                                 Text {
// // // // // // //                                                     id:             badgeTxt
// // // // // // //                                                     anchors.centerIn: parent
// // // // // // //                                                     text:           modelData
// // // // // // //                                                     font.family:    Theme.fontFamily
// // // // // // //                                                     font.pixelSize: 9
// // // // // // //                                                     font.weight:    Font.Medium
// // // // // // //                                                     color:          modelData === "Delivery" ? Theme.green
// // // // // // //                                                                   : modelData === "Drop"     ? Theme.blue
// // // // // // //                                                                                             : Theme.amber
// // // // // // //                                                 }
// // // // // // //                                             }
// // // // // // //                                         }

// // // // // // //                                         // RC channel badge
// // // // // // //                                         Rectangle {
// // // // // // //                                             visible:      droneNeedsRc
// // // // // // //                                             height:       15
// // // // // // //                                             width:        rcBadgeTxt.implicitWidth + 10
// // // // // // //                                             radius:       3
// // // // // // //                                             color:        "#F0EAF8"
// // // // // // //                                             Text {
// // // // // // //                                                 id:             rcBadgeTxt
// // // // // // //                                                 anchors.centerIn: parent
// // // // // // //                                                 text:           "CH" + droneRcSwitch + " ×" + droneRcCount
// // // // // // //                                                 font.family:    Theme.fontFamily
// // // // // // //                                                 font.pixelSize: 9
// // // // // // //                                                 font.weight:    Font.Medium
// // // // // // //                                                 color:          "#6B3FA0"
// // // // // // //                                             }
// // // // // // //                                         }
// // // // // // //                                     }
// // // // // // //                                 }

// // // // // // //                                 // Range / speed
// // // // // // //                                 Column {
// // // // // // //                                     spacing: 1
// // // // // // //                                     Text {
// // // // // // //                                         text:           (droneRangeKm * 2).toFixed(0) + " km"
// // // // // // //                                         font.family:    Theme.fontFamily
// // // // // // //                                         font.pixelSize: Theme.fontSm
// // // // // // //                                         font.weight:    Font.Medium
// // // // // // //                                         color:          Theme.textPrimary
// // // // // // //                                     }
// // // // // // //                                     Text {
// // // // // // //                                         text:           droneSpeedMs.toFixed(1) + " m/s"
// // // // // // //                                         font.family:    Theme.fontFamily
// // // // // // //                                         font.pixelSize: 10
// // // // // // //                                         color:          Theme.textMuted
// // // // // // //                                     }
// // // // // // //                                 }

// // // // // // //                                 // Edit
// // // // // // //                                 FpButton {
// // // // // // //                                     text:           "Edit"
// // // // // // //                                     implicitWidth:  44
// // // // // // //                                     implicitHeight: 24
// // // // // // //                                     onClicked:      root.openEdit(index)
// // // // // // //                                 }

// // // // // // //                                 // Deactivate
// // // // // // //                                 Rectangle {
// // // // // // //                                     width: 24; height: 24; radius: Theme.radius
// // // // // // //                                     visible:      droneActive
// // // // // // //                                     color:        xArea.containsMouse ? Theme.redLight  : "transparent"
// // // // // // //                                     border.color: xArea.containsMouse ? "#F7C1C1"       : Theme.border
// // // // // // //                                     border.width: 0.5
// // // // // // //                                     Text { anchors.centerIn: parent; text: "✕"; color: Theme.red; font.pixelSize: 11 }
// // // // // // //                                     MouseArea {
// // // // // // //                                         id: xArea
// // // // // // //                                         anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
// // // // // // //                                         onClicked: { droneManager.deactivate(index); root.fleetChanged() }
// // // // // // //                                     }
// // // // // // //                                 }

// // // // // // //                                 // Reactivate
// // // // // // //                                 Rectangle {
// // // // // // //                                     width: 24; height: 24; radius: Theme.radius
// // // // // // //                                     visible:      !droneActive
// // // // // // //                                     color:        plusArea.containsMouse ? Theme.greenLight : "transparent"
// // // // // // //                                     border.color: plusArea.containsMouse ? "#C0DD97"        : Theme.border
// // // // // // //                                     border.width: 0.5
// // // // // // //                                     Text { anchors.centerIn: parent; text: "+"; color: Theme.green; font.pixelSize: 14 }
// // // // // // //                                     MouseArea {
// // // // // // //                                         id: plusArea
// // // // // // //                                         anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
// // // // // // //                                         onClicked: { droneManager.setActive(index, true); root.fleetChanged() }
// // // // // // //                                     }
// // // // // // //                                 }
// // // // // // //                             }
// // // // // // //                         }
// // // // // // //                     }
// // // // // // //                 }
// // // // // // //             }
// // // // // // //         }

// // // // // // //         Item { implicitHeight: 10 }

// // // // // // //         // ── Add / Edit form ───────────────────────────────────────────────────
// // // // // // //         Rectangle {
// // // // // // //             Layout.fillWidth: true
// // // // // // //             visible:      root.showForm
// // // // // // //             height:       visible ? formCol.implicitHeight + 28 : 0
// // // // // // //             clip:         true
// // // // // // //             radius:       Theme.radiusLg
// // // // // // //             color:        Theme.bgSection
// // // // // // //             border.color: Theme.borderFocus
// // // // // // //             border.width: 1

// // // // // // //             Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

// // // // // // //             ColumnLayout {
// // // // // // //                 id: formCol
// // // // // // //                 anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
// // // // // // //                 spacing: 12

// // // // // // //                 // Title
// // // // // // //                 Text {
// // // // // // //                     text:           root.editMode ? "Edit Drone" : "Add New Drone"
// // // // // // //                     font.family:    Theme.fontFamily
// // // // // // //                     font.pixelSize: Theme.fontBase
// // // // // // //                     font.weight:    Font.Medium
// // // // // // //                     color:          Theme.textPrimary
// // // // // // //                 }

// // // // // // //                 // ── Name ──────────────────────────────────────────────────────
// // // // // // //                 ColumnLayout {
// // // // // // //                     Layout.fillWidth: true
// // // // // // //                     spacing: 4
// // // // // // //                     Text { text: "Name"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // // //                     FpInput {
// // // // // // //                         id:              efName
// // // // // // //                         Layout.fillWidth: true
// // // // // // //                         placeholderText: "e.g. Eagle-X"
// // // // // // //                     }
// // // // // // //                 }

// // // // // // //                 // ── Range + Speed ─────────────────────────────────────────────
// // // // // // //                 RowLayout {
// // // // // // //                     Layout.fillWidth: true
// // // // // // //                     spacing: 12

// // // // // // //                     ColumnLayout {
// // // // // // //                         Layout.fillWidth: true
// // // // // // //                         spacing: 4
// // // // // // //                         Text { text: "One-way range (km)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // // //                         FpSpinBox {
// // // // // // //                             id: efRange
// // // // // // //                             label: ""; unit: "km"
// // // // // // //                             value: 5; from: 1; to: 500; stepSize: 1
// // // // // // //                             Layout.fillWidth: true
// // // // // // //                         }
// // // // // // //                     }

// // // // // // //                     ColumnLayout {
// // // // // // //                         Layout.fillWidth: true
// // // // // // //                         spacing: 4
// // // // // // //                         Text { text: "Max speed (m/s)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // // //                         FpSpinBox {
// // // // // // //                             id: efSpeed
// // // // // // //                             label: ""; unit: "m/s"
// // // // // // //                             value: 50; from: 5; to: 200; stepSize: 5
// // // // // // //                             Layout.fillWidth: true
// // // // // // //                         }
// // // // // // //                     }
// // // // // // //                 }

// // // // // // //                 // ── Mission Capabilities (all three independent) ───────────────
// // // // // // //                 ColumnLayout {
// // // // // // //                     spacing: 8

// // // // // // //                     Text { text: "Mission Capabilities"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // // // // //                     RowLayout {
// // // // // // //                         spacing: 24

// // // // // // //                         // Delivery
// // // // // // //                         RowLayout {
// // // // // // //                             spacing: 8
// // // // // // //                             Rectangle {
// // // // // // //                                 width: 18; height: 18; radius: 4
// // // // // // //                                 color:        root.capDelivery ? Theme.blueMid : Theme.bgInput
// // // // // // //                                 border.color: root.capDelivery ? Theme.blueMid : Theme.border
// // // // // // //                                 border.width: 0.5
// // // // // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capDelivery }
// // // // // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDelivery = !root.capDelivery }
// // // // // // //                             }
// // // // // // //                             Text { text: "Delivery"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // // // // //                         }

// // // // // // //                         // Drop
// // // // // // //                         RowLayout {
// // // // // // //                             spacing: 8
// // // // // // //                             Rectangle {
// // // // // // //                                 width: 18; height: 18; radius: 4
// // // // // // //                                 color:        root.capDrop ? Theme.blueMid : Theme.bgInput
// // // // // // //                                 border.color: root.capDrop ? Theme.blueMid : Theme.border
// // // // // // //                                 border.width: 0.5
// // // // // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capDrop }
// // // // // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDrop = !root.capDrop }
// // // // // // //                             }
// // // // // // //                             Text { text: "Drop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // // // // //                         }

// // // // // // //                         // Paradrop
// // // // // // //                         RowLayout {
// // // // // // //                             spacing: 8
// // // // // // //                             Rectangle {
// // // // // // //                                 width: 18; height: 18; radius: 4
// // // // // // //                                 color:        root.capParadrop ? Theme.blueMid : Theme.bgInput
// // // // // // //                                 border.color: root.capParadrop ? Theme.blueMid : Theme.border
// // // // // // //                                 border.width: 0.5
// // // // // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capParadrop }
// // // // // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capParadrop = !root.capParadrop }
// // // // // // //                             }
// // // // // // //                             Text { text: "Paradrop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // // // // //                         }
// // // // // // //                     }
// // // // // // //                 }

// // // // // // //                 // ── RC settings — only when Drop or Paradrop ticked ───────────
// // // // // // //                 ColumnLayout {
// // // // // // //                     visible:  root.capDrop || root.capParadrop
// // // // // // //                     spacing:  8

// // // // // // //                     Text { text: "RC Trigger Settings"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // // // // //                     RowLayout {
// // // // // // //                         spacing: 12

// // // // // // //                         ColumnLayout {
// // // // // // //                             spacing: 4
// // // // // // //                             Text { text: "RC Channel"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // // //                             FpSpinBox {
// // // // // // //                                 id:             efRcCh
// // // // // // //                                 label:          ""
// // // // // // //                                 unit:           ""
// // // // // // //                                 value:          root.rcChannel
// // // // // // //                                 from: 1; to: 16; stepSize: 1
// // // // // // //                                 implicitWidth:  100
// // // // // // //                                 onValueChanged: root.rcChannel = value
// // // // // // //                             }
// // // // // // //                         }

// // // // // // //                         ColumnLayout {
// // // // // // //                             spacing: 4
// // // // // // //                             Text { text: "Trigger Count"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // // //                             FpSpinBox {
// // // // // // //                                 id:             efRcCount
// // // // // // //                                 label:          ""
// // // // // // //                                 unit:           "×"
// // // // // // //                                 value:          root.rcTriggers
// // // // // // //                                 from: 1; to: 10; stepSize: 1
// // // // // // //                                 implicitWidth:  100
// // // // // // //                                 onValueChanged: root.rcTriggers = value
// // // // // // //                             }
// // // // // // //                         }

// // // // // // //                         Text {
// // // // // // //                             text:           "Channel " + root.rcChannel + ", pulsed " + root.rcTriggers + "× at destination"
// // // // // // //                             font.family:    Theme.fontFamily
// // // // // // //                             font.pixelSize: Theme.fontSm
// // // // // // //                             color:          Theme.textMuted
// // // // // // //                             Layout.alignment: Qt.AlignBottom
// // // // // // //                         }
// // // // // // //                     }
// // // // // // //                 }

// // // // // // //                 // ── Default Altitudes ─────────────────────────────────────────
// // // // // // //                 ColumnLayout {
// // // // // // //                     spacing: 8

// // // // // // //                     Text { text: "Default Altitudes"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // // // // //                     RowLayout {
// // // // // // //                         Layout.fillWidth: true
// // // // // // //                         spacing: 12
// // // // // // //                         FpSpinBox { id: efTakeoff;  label: "Takeoff";    unit: "m"; value: 30;  from: 10; to: 300; stepSize: 5;  Layout.fillWidth: true }
// // // // // // //                         FpSpinBox { id: efNearHome; label: "Near-home";  unit: "m"; value: 60;  from: 20; to: 400; stepSize: 5;  Layout.fillWidth: true }
// // // // // // //                         FpSpinBox { id: efCruise;   label: "Clearance";  unit: "m"; value: 120; from: 80; to: 800; stepSize: 10; Layout.fillWidth: true }
// // // // // // //                         FpSpinBox { id: efLand;     label: "Land";       unit: "m"; value: 5;   from: 0;  to: 50;  stepSize: 1;  Layout.fillWidth: true }
// // // // // // //                     }
// // // // // // //                 }

// // // // // // //                 // ── Validation ────────────────────────────────────────────────
// // // // // // //                 Text {
// // // // // // //                     id:             validationMsg
// // // // // // //                     visible:        false
// // // // // // //                     text:           "Please enter a drone name"
// // // // // // //                     color:          Theme.red
// // // // // // //                     font.family:    Theme.fontFamily
// // // // // // //                     font.pixelSize: Theme.fontSm
// // // // // // //                 }

// // // // // // //                 // ── Buttons ───────────────────────────────────────────────────
// // // // // // //                 RowLayout {
// // // // // // //                     spacing: 8

// // // // // // //                     FpButton {
// // // // // // //                         text:           root.editMode ? "Update Drone" : "Add Drone"
// // // // // // //                         primary:        true
// // // // // // //                         implicitWidth:  110
// // // // // // //                         onClicked:      root.saveForm()
// // // // // // //                     }

// // // // // // //                     FpButton {
// // // // // // //                         text:          "Cancel"
// // // // // // //                         implicitWidth: 70
// // // // // // //                         onClicked: {
// // // // // // //                             root.showForm = false
// // // // // // //                             validationMsg.visible = false
// // // // // // //                         }
// // // // // // //                     }
// // // // // // //                 }

// // // // // // //                 Item { implicitHeight: 2 }
// // // // // // //             }
// // // // // // //         }
// // // // // // //     }
// // // // // // // }



// // // // // // import QtQuick 6.0
// // // // // // import QtQuick.Controls 6.0
// // // // // // import QtQuick.Layouts 6.0

// // // // // // Item {
// // // // // //     id: root
// // // // // //     implicitHeight: mainCol.implicitHeight

// // // // // //     signal fleetChanged()

// // // // // //     // ── Form state (root-scope so openAdd/openEdit can set before form renders) ─
// // // // // //     property bool showForm:    false
// // // // // //     property bool editMode:    false
// // // // // //     property int  editIndex:   -1

// // // // // //     property bool capDelivery: true
// // // // // //     property bool capDrop:     false
// // // // // //     property bool capParadrop: false
// // // // // //     property int  rcChannel:   1
// // // // // //     property int  rcTriggers:  1

// // // // // //     // ── Functions ─────────────────────────────────────────────────────────────
// // // // // //     function openAdd() {
// // // // // //         editMode         = false
// // // // // //         editIndex        = -1
// // // // // //         efName.text      = ""
// // // // // //         efRange.value    = 5
// // // // // //         efSpeed.value    = 50
// // // // // //         capDelivery      = true
// // // // // //         capDrop          = false
// // // // // //         capParadrop      = false
// // // // // //         rcChannel        = 1
// // // // // //         rcTriggers       = 1
// // // // // //         efTakeoff.value  = 30
// // // // // //         efNearHome.value = 60
// // // // // //         efCruise.value   = 120
// // // // // //         efLand.value     = 5
// // // // // //         showForm         = true
// // // // // //     }

// // // // // //     function openEdit(idx) {
// // // // // //         var d            = droneManager.droneAt(idx)
// // // // // //         editMode         = true
// // // // // //         editIndex        = idx
// // // // // //         efName.text      = d.name
// // // // // //         efRange.value    = Math.round(d.rangeKm)
// // // // // //         efSpeed.value    = Math.round(d.speedMs * 10)
// // // // // //         capDelivery      = d.delivery
// // // // // //         capDrop          = d.drop
// // // // // //         capParadrop      = d.paradrop
// // // // // //         rcChannel        = d.rcSwitch > 0 ? d.rcSwitch : 1
// // // // // //         rcTriggers       = d.rcCount  > 0 ? d.rcCount  : 1
// // // // // //         efTakeoff.value  = Math.round(d.takeoffAlt)
// // // // // //         efNearHome.value = Math.round(d.nearHomeAlt)
// // // // // //         efCruise.value   = Math.round(d.cruiseAlt)
// // // // // //         efLand.value     = Math.round(d.landAlt)
// // // // // //         showForm         = true
// // // // // //     }

// // // // // //     function saveForm() {
// // // // // //         var name = efName.text.trim()
// // // // // //         if (name === "") { validationMsg.visible = true; return }
// // // // // //         validationMsg.visible = false

// // // // // //         var needsRc = capDrop || capParadrop
// // // // // //         var rc      = needsRc ? rcChannel  : 0
// // // // // //         var rcCnt   = needsRc ? rcTriggers : 0

// // // // // //         if (editMode) {
// // // // // //             droneManager.updateDrone(
// // // // // //                 editIndex, name,
// // // // // //                 efRange.value, efSpeed.value / 10.0,
// // // // // //                 capDelivery, capDrop, capParadrop,
// // // // // //                 rc, rcCnt,
// // // // // //                 efTakeoff.value, efNearHome.value,
// // // // // //                 efCruise.value,  efLand.value
// // // // // //             )
// // // // // //         } else {
// // // // // //             droneManager.addDrone(
// // // // // //                 name,
// // // // // //                 efRange.value, efSpeed.value / 10.0,
// // // // // //                 capDelivery, capDrop, capParadrop,
// // // // // //                 rc, rcCnt,
// // // // // //                 efTakeoff.value, efNearHome.value,
// // // // // //                 efCruise.value,  efLand.value
// // // // // //             )
// // // // // //         }
// // // // // //         showForm = false
// // // // // //         root.fleetChanged()
// // // // // //     }

// // // // // //     // ── Main column ───────────────────────────────────────────────────────────
// // // // // //     ColumnLayout {
// // // // // //         id: mainCol
// // // // // //         anchors { left: parent.left; right: parent.right }
// // // // // //         spacing: 0

// // // // // //         // Header
// // // // // //         RowLayout {
// // // // // //             Layout.fillWidth: true
// // // // // //             spacing: 8

// // // // // //             Text {
// // // // // //                 text:           "Drone Fleet  (" + droneManager.count + " total)"
// // // // // //                 font.family:    Theme.fontFamily
// // // // // //                 font.pixelSize: Theme.fontBase
// // // // // //                 color:          Theme.textSecond
// // // // // //                 Layout.fillWidth: true
// // // // // //             }

// // // // // //             FpButton {
// // // // // //                 text:           "+ Add Drone"
// // // // // //                 implicitWidth:  96
// // // // // //                 implicitHeight: 26
// // // // // //                 onClicked:      root.openAdd()
// // // // // //             }
// // // // // //         }

// // // // // //         Item { implicitHeight: 8 }

// // // // // //         // ── Drone list — no height cap, parent ScrollView scrolls ──────────
// // // // // //         Column {
// // // // // //             id:               listCol
// // // // // //             Layout.fillWidth: true
// // // // // //             spacing:          4

// // // // // //             Repeater {
// // // // // //                         model: droneManager

// // // // // //                         delegate: Rectangle {
// // // // // //                             width:        parent.width
// // // // // //                             height:       rowContent.implicitHeight + 14
// // // // // //                             radius:       Theme.radius
// // // // // //                             color:        droneActive ? Theme.bgSection : "#F8F7F4"
// // // // // //                             border.color: droneActive ? Theme.border    : Theme.borderLight
// // // // // //                             border.width: 0.5
// // // // // //                             opacity:      droneActive ? 1.0 : 0.6

// // // // // //                             RowLayout {
// // // // // //                                 id: rowContent
// // // // // //                                 anchors {
// // // // // //                                     left: parent.left; right: parent.right
// // // // // //                                     verticalCenter: parent.verticalCenter
// // // // // //                                     margins: 10
// // // // // //                                 }
// // // // // //                                 spacing: 8

// // // // // //                                 // Active dot
// // // // // //                                 Rectangle {
// // // // // //                                     width: 8; height: 8; radius: 4
// // // // // //                                     color: droneActive ? Theme.green : Theme.textHint
// // // // // //                                     MouseArea {
// // // // // //                                         anchors.fill: parent
// // // // // //                                         cursorShape:  Qt.PointingHandCursor
// // // // // //                                         onClicked:    droneManager.setActive(index, !droneActive)
// // // // // //                                     }
// // // // // //                                 }

// // // // // //                                 // Name + capability badges
// // // // // //                                 ColumnLayout {
// // // // // //                                     Layout.fillWidth: true
// // // // // //                                     spacing: 3

// // // // // //                                     Text {
// // // // // //                                         text:           droneName
// // // // // //                                         font.family:    Theme.fontFamily
// // // // // //                                         font.pixelSize: Theme.fontBase
// // // // // //                                         font.weight:    Font.Medium
// // // // // //                                         color:          Theme.textPrimary
// // // // // //                                     }

// // // // // //                                     Row {
// // // // // //                                         spacing: 5

// // // // // //                                         Repeater {
// // // // // //                                             model: droneCapabilities
// // // // // //                                             delegate: Rectangle {
// // // // // //                                                 height: 15
// // // // // //                                                 width:  badgeTxt.implicitWidth + 10
// // // // // //                                                 radius: 3
// // // // // //                                                 color:  modelData === "Delivery" ? Theme.greenLight
// // // // // //                                                       : modelData === "Drop"     ? Theme.blueLight
// // // // // //                                                                                  : Theme.amberLight
// // // // // //                                                 Text {
// // // // // //                                                     id:             badgeTxt
// // // // // //                                                     anchors.centerIn: parent
// // // // // //                                                     text:           modelData
// // // // // //                                                     font.family:    Theme.fontFamily
// // // // // //                                                     font.pixelSize: 9
// // // // // //                                                     font.weight:    Font.Medium
// // // // // //                                                     color:          modelData === "Delivery" ? Theme.green
// // // // // //                                                                   : modelData === "Drop"     ? Theme.blue
// // // // // //                                                                                             : Theme.amber
// // // // // //                                                 }
// // // // // //                                             }
// // // // // //                                         }

// // // // // //                                         // RC channel badge
// // // // // //                                         Rectangle {
// // // // // //                                             visible:      droneNeedsRc
// // // // // //                                             height:       15
// // // // // //                                             width:        rcBadgeTxt.implicitWidth + 10
// // // // // //                                             radius:       3
// // // // // //                                             color:        "#F0EAF8"
// // // // // //                                             Text {
// // // // // //                                                 id:             rcBadgeTxt
// // // // // //                                                 anchors.centerIn: parent
// // // // // //                                                 text:           "CH" + droneRcSwitch + " ×" + droneRcCount
// // // // // //                                                 font.family:    Theme.fontFamily
// // // // // //                                                 font.pixelSize: 9
// // // // // //                                                 font.weight:    Font.Medium
// // // // // //                                                 color:          "#6B3FA0"
// // // // // //                                             }
// // // // // //                                         }
// // // // // //                                     }
// // // // // //                                 }

// // // // // //                                 // Range / speed
// // // // // //                                 Column {
// // // // // //                                     spacing: 1
// // // // // //                                     Text {
// // // // // //                                         text:           (droneRangeKm * 2).toFixed(0) + " km"
// // // // // //                                         font.family:    Theme.fontFamily
// // // // // //                                         font.pixelSize: Theme.fontSm
// // // // // //                                         font.weight:    Font.Medium
// // // // // //                                         color:          Theme.textPrimary
// // // // // //                                     }
// // // // // //                                     Text {
// // // // // //                                         text:           droneSpeedMs.toFixed(1) + " m/s"
// // // // // //                                         font.family:    Theme.fontFamily
// // // // // //                                         font.pixelSize: 10
// // // // // //                                         color:          Theme.textMuted
// // // // // //                                     }
// // // // // //                                 }

// // // // // //                                 // Edit
// // // // // //                                 FpButton {
// // // // // //                                     text:           "Edit"
// // // // // //                                     implicitWidth:  44
// // // // // //                                     implicitHeight: 24
// // // // // //                                     onClicked:      root.openEdit(index)
// // // // // //                                 }

// // // // // //                                 // Deactivate
// // // // // //                                 Rectangle {
// // // // // //                                     width: 24; height: 24; radius: Theme.radius
// // // // // //                                     visible:      droneActive
// // // // // //                                     color:        xArea.containsMouse ? Theme.redLight  : "transparent"
// // // // // //                                     border.color: xArea.containsMouse ? "#F7C1C1"       : Theme.border
// // // // // //                                     border.width: 0.5
// // // // // //                                     Text { anchors.centerIn: parent; text: "✕"; color: Theme.red; font.pixelSize: 11 }
// // // // // //                                     MouseArea {
// // // // // //                                         id: xArea
// // // // // //                                         anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
// // // // // //                                         onClicked: { droneManager.deactivate(index); root.fleetChanged() }
// // // // // //                                     }
// // // // // //                                 }

// // // // // //                                 // Reactivate
// // // // // //                                 Rectangle {
// // // // // //                                     width: 24; height: 24; radius: Theme.radius
// // // // // //                                     visible:      !droneActive
// // // // // //                                     color:        plusArea.containsMouse ? Theme.greenLight : "transparent"
// // // // // //                                     border.color: plusArea.containsMouse ? "#C0DD97"        : Theme.border
// // // // // //                                     border.width: 0.5
// // // // // //                                     Text { anchors.centerIn: parent; text: "+"; color: Theme.green; font.pixelSize: 14 }
// // // // // //                                     MouseArea {
// // // // // //                                         id: plusArea
// // // // // //                                         anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
// // // // // //                                         onClicked: { droneManager.setActive(index, true); root.fleetChanged() }
// // // // // //                                     }
// // // // // //                                 }
// // // // // //                             }
// // // // // //                         }
// // // // // //             }
// // // // // //         }

// // // // // //         Item { implicitHeight: 10 }

// // // // // //         // ── Add / Edit form ───────────────────────────────────────────────────
// // // // // //         Rectangle {
// // // // // //             Layout.fillWidth: true
// // // // // //             visible:      root.showForm
// // // // // //             height:       visible ? formCol.implicitHeight + 28 : 0
// // // // // //             clip:         true
// // // // // //             radius:       Theme.radiusLg
// // // // // //             color:        Theme.bgSection
// // // // // //             border.color: Theme.borderFocus
// // // // // //             border.width: 1

// // // // // //             Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

// // // // // //             ColumnLayout {
// // // // // //                 id: formCol
// // // // // //                 anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
// // // // // //                 spacing: 12

// // // // // //                 // Title
// // // // // //                 Text {
// // // // // //                     text:           root.editMode ? "Edit Drone" : "Add New Drone"
// // // // // //                     font.family:    Theme.fontFamily
// // // // // //                     font.pixelSize: Theme.fontBase
// // // // // //                     font.weight:    Font.Medium
// // // // // //                     color:          Theme.textPrimary
// // // // // //                 }

// // // // // //                 // ── Name ──────────────────────────────────────────────────────
// // // // // //                 ColumnLayout {
// // // // // //                     Layout.fillWidth: true
// // // // // //                     spacing: 4
// // // // // //                     Text { text: "Name"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // //                     FpInput {
// // // // // //                         id:              efName
// // // // // //                         Layout.fillWidth: true
// // // // // //                         placeholderText: "e.g. Eagle-X"
// // // // // //                     }
// // // // // //                 }

// // // // // //                 // ── Range + Speed ─────────────────────────────────────────────
// // // // // //                 RowLayout {
// // // // // //                     Layout.fillWidth: true
// // // // // //                     spacing: 12

// // // // // //                     ColumnLayout {
// // // // // //                         Layout.fillWidth: true
// // // // // //                         spacing: 4
// // // // // //                         Text { text: "One-way range (km)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // //                         FpSpinBox {
// // // // // //                             id: efRange
// // // // // //                             label: ""; unit: "km"
// // // // // //                             value: 5; from: 1; to: 500; stepSize: 1
// // // // // //                             Layout.fillWidth: true
// // // // // //                         }
// // // // // //                     }

// // // // // //                     ColumnLayout {
// // // // // //                         Layout.fillWidth: true
// // // // // //                         spacing: 4
// // // // // //                         Text { text: "Max speed (m/s)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // //                         FpSpinBox {
// // // // // //                             id: efSpeed
// // // // // //                             label: ""; unit: "m/s"
// // // // // //                             value: 50; from: 5; to: 200; stepSize: 5
// // // // // //                             Layout.fillWidth: true
// // // // // //                         }
// // // // // //                     }
// // // // // //                 }

// // // // // //                 // ── Mission Capabilities (all three independent) ───────────────
// // // // // //                 ColumnLayout {
// // // // // //                     spacing: 8

// // // // // //                     Text { text: "Mission Capabilities"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // // // //                     RowLayout {
// // // // // //                         spacing: 24

// // // // // //                         // Delivery
// // // // // //                         RowLayout {
// // // // // //                             spacing: 8
// // // // // //                             Rectangle {
// // // // // //                                 width: 18; height: 18; radius: 4
// // // // // //                                 color:        root.capDelivery ? Theme.blueMid : Theme.bgInput
// // // // // //                                 border.color: root.capDelivery ? Theme.blueMid : Theme.border
// // // // // //                                 border.width: 0.5
// // // // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capDelivery }
// // // // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDelivery = !root.capDelivery }
// // // // // //                             }
// // // // // //                             Text { text: "Delivery"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // // // //                         }

// // // // // //                         // Drop
// // // // // //                         RowLayout {
// // // // // //                             spacing: 8
// // // // // //                             Rectangle {
// // // // // //                                 width: 18; height: 18; radius: 4
// // // // // //                                 color:        root.capDrop ? Theme.blueMid : Theme.bgInput
// // // // // //                                 border.color: root.capDrop ? Theme.blueMid : Theme.border
// // // // // //                                 border.width: 0.5
// // // // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capDrop }
// // // // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDrop = !root.capDrop }
// // // // // //                             }
// // // // // //                             Text { text: "Drop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // // // //                         }

// // // // // //                         // Paradrop
// // // // // //                         RowLayout {
// // // // // //                             spacing: 8
// // // // // //                             Rectangle {
// // // // // //                                 width: 18; height: 18; radius: 4
// // // // // //                                 color:        root.capParadrop ? Theme.blueMid : Theme.bgInput
// // // // // //                                 border.color: root.capParadrop ? Theme.blueMid : Theme.border
// // // // // //                                 border.width: 0.5
// // // // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capParadrop }
// // // // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capParadrop = !root.capParadrop }
// // // // // //                             }
// // // // // //                             Text { text: "Paradrop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // // // //                         }
// // // // // //                     }
// // // // // //                 }

// // // // // //                 // ── RC settings — only when Drop or Paradrop ticked ───────────
// // // // // //                 ColumnLayout {
// // // // // //                     visible:  root.capDrop || root.capParadrop
// // // // // //                     spacing:  8

// // // // // //                     Text { text: "RC Trigger Settings"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // // // //                     RowLayout {
// // // // // //                         spacing: 12

// // // // // //                         ColumnLayout {
// // // // // //                             spacing: 4
// // // // // //                             Text { text: "RC Channel"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // //                             FpSpinBox {
// // // // // //                                 id:             efRcCh
// // // // // //                                 label:          ""
// // // // // //                                 unit:           ""
// // // // // //                                 value:          root.rcChannel
// // // // // //                                 from: 1; to: 16; stepSize: 1
// // // // // //                                 implicitWidth:  100
// // // // // //                                 onValueChanged: root.rcChannel = value
// // // // // //                             }
// // // // // //                         }

// // // // // //                         ColumnLayout {
// // // // // //                             spacing: 4
// // // // // //                             Text { text: "Trigger Count"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // // //                             FpSpinBox {
// // // // // //                                 id:             efRcCount
// // // // // //                                 label:          ""
// // // // // //                                 unit:           "×"
// // // // // //                                 value:          root.rcTriggers
// // // // // //                                 from: 1; to: 10; stepSize: 1
// // // // // //                                 implicitWidth:  100
// // // // // //                                 onValueChanged: root.rcTriggers = value
// // // // // //                             }
// // // // // //                         }

// // // // // //                         Text {
// // // // // //                             text:           "Channel " + root.rcChannel + ", pulsed " + root.rcTriggers + "× at destination"
// // // // // //                             font.family:    Theme.fontFamily
// // // // // //                             font.pixelSize: Theme.fontSm
// // // // // //                             color:          Theme.textMuted
// // // // // //                             Layout.alignment: Qt.AlignBottom
// // // // // //                         }
// // // // // //                     }
// // // // // //                 }

// // // // // //                 // ── Default Altitudes ─────────────────────────────────────────
// // // // // //                 ColumnLayout {
// // // // // //                     spacing: 8

// // // // // //                     Text { text: "Default Altitudes"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // // // //                     RowLayout {
// // // // // //                         Layout.fillWidth: true
// // // // // //                         spacing: 12
// // // // // //                         FpSpinBox { id: efTakeoff;  label: "Takeoff";    unit: "m"; value: 30;  from: 10; to: 300; stepSize: 5;  Layout.fillWidth: true }
// // // // // //                         FpSpinBox { id: efNearHome; label: "Near-home";  unit: "m"; value: 60;  from: 20; to: 400; stepSize: 5;  Layout.fillWidth: true }
// // // // // //                         FpSpinBox { id: efCruise;   label: "Clearance";  unit: "m"; value: 120; from: 80; to: 800; stepSize: 10; Layout.fillWidth: true }
// // // // // //                         FpSpinBox { id: efLand;     label: "Land";       unit: "m"; value: 5;   from: 0;  to: 50;  stepSize: 1;  Layout.fillWidth: true }
// // // // // //                     }
// // // // // //                 }

// // // // // //                 // ── Validation ────────────────────────────────────────────────
// // // // // //                 Text {
// // // // // //                     id:             validationMsg
// // // // // //                     visible:        false
// // // // // //                     text:           "Please enter a drone name"
// // // // // //                     color:          Theme.red
// // // // // //                     font.family:    Theme.fontFamily
// // // // // //                     font.pixelSize: Theme.fontSm
// // // // // //                 }

// // // // // //                 // ── Buttons ───────────────────────────────────────────────────
// // // // // //                 RowLayout {
// // // // // //                     spacing: 8

// // // // // //                     FpButton {
// // // // // //                         text:           root.editMode ? "Update Drone" : "Add Drone"
// // // // // //                         primary:        true
// // // // // //                         implicitWidth:  110
// // // // // //                         onClicked:      root.saveForm()
// // // // // //                     }

// // // // // //                     FpButton {
// // // // // //                         text:          "Cancel"
// // // // // //                         implicitWidth: 70
// // // // // //                         onClicked: {
// // // // // //                             root.showForm = false
// // // // // //                             validationMsg.visible = false
// // // // // //                         }
// // // // // //                     }
// // // // // //                 }

// // // // // //                 Item { implicitHeight: 2 }
// // // // // //             }
// // // // // //         }
// // // // // //     }
// // // // // // }


// // // // // import QtQuick 6.0
// // // // // import QtQuick.Controls 6.0
// // // // // import QtQuick.Layouts 6.0

// // // // // Item {
// // // // //     id: root
// // // // //     implicitHeight: mainCol.implicitHeight

// // // // //     signal fleetChanged()

// // // // //     // ── Form state (root-scope so openAdd/openEdit can set before form renders) ─
// // // // //     property bool showForm:    false
// // // // //     property bool editMode:    false
// // // // //     property int  editIndex:   -1

// // // // //     property bool capDelivery: true
// // // // //     property bool capDrop:     false
// // // // //     property bool capParadrop: false
// // // // //     property int  rcChannel:   1
// // // // //     property int  rcTriggers:  1

// // // // //     // ── Functions ─────────────────────────────────────────────────────────────
// // // // //     function openAdd() {
// // // // //         editMode         = false
// // // // //         editIndex        = -1
// // // // //         efName.text      = ""
// // // // //         efRange.value    = 5
// // // // //         efSpeed.value    = 50
// // // // //         capDelivery      = true
// // // // //         capDrop          = false
// // // // //         capParadrop      = false
// // // // //         rcChannel        = 1
// // // // //         rcTriggers       = 1
// // // // //         efTakeoff.value  = 30
// // // // //         efNearHome.value = 60
// // // // //         efCruise.value   = 120
// // // // //         efLand.value     = 5
// // // // //         showForm         = true
// // // // //         Qt.callLater(function() { root.ensureFormVisible() })
// // // // //     }

// // // // //     function openEdit(idx) {
// // // // //         var d            = droneManager.droneAt(idx)
// // // // //         editMode         = true
// // // // //         editIndex        = idx
// // // // //         efName.text      = d.name
// // // // //         efRange.value    = Math.round(d.rangeKm)
// // // // //         efSpeed.value    = Math.round(d.speedMs * 10)
// // // // //         capDelivery      = d.delivery
// // // // //         capDrop          = d.drop
// // // // //         capParadrop      = d.paradrop
// // // // //         rcChannel        = d.rcSwitch > 0 ? d.rcSwitch : 1
// // // // //         rcTriggers       = d.rcCount  > 0 ? d.rcCount  : 1
// // // // //         efTakeoff.value  = Math.round(d.takeoffAlt)
// // // // //         efNearHome.value = Math.round(d.nearHomeAlt)
// // // // //         efCruise.value   = Math.round(d.cruiseAlt)
// // // // //         efLand.value     = Math.round(d.landAlt)
// // // // //         showForm         = true
// // // // //         Qt.callLater(function() { root.ensureFormVisible() })
// // // // //     }

// // // // //     // Scroll the parent Flickable/ScrollView to reveal the edit form
// // // // //     function ensureFormVisible() {
// // // // //         var settingsFlick = null
// // // // //         // Walk up parent chain to find a Flickable
// // // // //         var p = root.parent
// // // // //         while (p) {
// // // // //             if (p.hasOwnProperty("contentY") && p.hasOwnProperty("contentHeight")) {
// // // // //                 settingsFlick = p
// // // // //                 break
// // // // //             }
// // // // //             p = p.parent
// // // // //         }
// // // // //         if (!settingsFlick) return
// // // // //         // Map edit form position to the flickable's content coordinates
// // // // //         var formY = formRect.mapToItem(settingsFlick.contentItem, 0, 0).y
// // // // //         var formH = formRect.height
// // // // //         var viewH = settingsFlick.height
// // // // //         if (formY + formH > settingsFlick.contentY + viewH) {
// // // // //             settingsFlick.contentY = Math.min(
// // // // //                 formY + formH - viewH + 20,
// // // // //                 settingsFlick.contentHeight - viewH
// // // // //             )
// // // // //         }
// // // // //     }

// // // // //     function saveForm() {
// // // // //         var name = efName.text.trim()
// // // // //         if (name === "") { validationMsg.visible = true; return }
// // // // //         validationMsg.visible = false

// // // // //         var needsRc = capDrop || capParadrop
// // // // //         var rc      = needsRc ? rcChannel  : 0
// // // // //         var rcCnt   = needsRc ? rcTriggers : 0

// // // // //         if (editMode) {
// // // // //             droneManager.updateDrone(
// // // // //                 editIndex, name,
// // // // //                 efRange.value, efSpeed.value / 10.0,
// // // // //                 capDelivery, capDrop, capParadrop,
// // // // //                 rc, rcCnt,
// // // // //                 efTakeoff.value, efNearHome.value,
// // // // //                 efCruise.value,  efLand.value
// // // // //             )
// // // // //         } else {
// // // // //             droneManager.addDrone(
// // // // //                 name,
// // // // //                 efRange.value, efSpeed.value / 10.0,
// // // // //                 capDelivery, capDrop, capParadrop,
// // // // //                 rc, rcCnt,
// // // // //                 efTakeoff.value, efNearHome.value,
// // // // //                 efCruise.value,  efLand.value
// // // // //             )
// // // // //         }
// // // // //         showForm = false
// // // // //         root.fleetChanged()
// // // // //     }

// // // // //     // ── Main column ───────────────────────────────────────────────────────────
// // // // //     ColumnLayout {
// // // // //         id: mainCol
// // // // //         anchors { left: parent.left; right: parent.right }
// // // // //         spacing: 0

// // // // //         // Header
// // // // //         RowLayout {
// // // // //             Layout.fillWidth: true
// // // // //             spacing: 8

// // // // //             Text {
// // // // //                 text:           "Drone Fleet  (" + droneManager.count + " total)"
// // // // //                 font.family:    Theme.fontFamily
// // // // //                 font.pixelSize: Theme.fontBase
// // // // //                 color:          Theme.textSecond
// // // // //                 Layout.fillWidth: true
// // // // //             }

// // // // //             FpButton {
// // // // //                 text:           "+ Add Drone"
// // // // //                 implicitWidth:  96
// // // // //                 implicitHeight: 26
// // // // //                 onClicked:      root.openAdd()
// // // // //             }
// // // // //         }

// // // // //         Item { implicitHeight: 8 }

// // // // //         // ── Drone list — no height cap, parent ScrollView scrolls ──────────
// // // // //         Column {
// // // // //             id:               listCol
// // // // //             Layout.fillWidth: true
// // // // //             spacing:          4

// // // // //             Repeater {
// // // // //                         model: droneManager

// // // // //                         delegate: Rectangle {
// // // // //                             width:        parent.width
// // // // //                             height:       rowContent.implicitHeight + 14
// // // // //                             radius:       Theme.radius
// // // // //                             color:        droneActive ? Theme.bgSection : "#F8F7F4"
// // // // //                             border.color: droneActive ? Theme.border    : Theme.borderLight
// // // // //                             border.width: 0.5
// // // // //                             opacity:      droneActive ? 1.0 : 0.6

// // // // //                             RowLayout {
// // // // //                                 id: rowContent
// // // // //                                 anchors {
// // // // //                                     left: parent.left; right: parent.right
// // // // //                                     verticalCenter: parent.verticalCenter
// // // // //                                     leftMargin: 10; rightMargin: 10
// // // // //                                 }
// // // // //                                 spacing: 8

// // // // //                                 // Active dot
// // // // //                                 Rectangle {
// // // // //                                     width: 8; height: 8; radius: 4
// // // // //                                     color: droneActive ? Theme.green : Theme.textHint
// // // // //                                     Layout.alignment: Qt.AlignVCenter
// // // // //                                     MouseArea {
// // // // //                                         anchors.fill: parent
// // // // //                                         cursorShape:  Qt.PointingHandCursor
// // // // //                                         onClicked:    droneManager.setActive(index, !droneActive)
// // // // //                                     }
// // // // //                                 }

// // // // //                                 // Name + capability badges
// // // // //                                 ColumnLayout {
// // // // //                                     Layout.fillWidth: true
// // // // //                                     Layout.alignment: Qt.AlignVCenter
// // // // //                                     spacing: 3

// // // // //                                     Text {
// // // // //                                         text:           droneName
// // // // //                                         font.family:    Theme.fontFamily
// // // // //                                         font.pixelSize: Theme.fontBase
// // // // //                                         font.weight:    Font.Medium
// // // // //                                         color:          Theme.textPrimary
// // // // //                                     }

// // // // //                                     Row {
// // // // //                                         spacing: 5

// // // // //                                         Repeater {
// // // // //                                             model: droneCapabilities
// // // // //                                             delegate: Rectangle {
// // // // //                                                 height: 15
// // // // //                                                 width:  badgeTxt.implicitWidth + 10
// // // // //                                                 radius: 3
// // // // //                                                 color:  modelData === "Delivery" ? Theme.greenLight
// // // // //                                                       : modelData === "Drop"     ? Theme.blueLight
// // // // //                                                                                  : Theme.amberLight
// // // // //                                                 Text {
// // // // //                                                     id:             badgeTxt
// // // // //                                                     anchors.centerIn: parent
// // // // //                                                     text:           modelData
// // // // //                                                     font.family:    Theme.fontFamily
// // // // //                                                     font.pixelSize: 9
// // // // //                                                     font.weight:    Font.Medium
// // // // //                                                     color:          modelData === "Delivery" ? Theme.green
// // // // //                                                                   : modelData === "Drop"     ? Theme.blue
// // // // //                                                                                             : Theme.amber
// // // // //                                                 }
// // // // //                                             }
// // // // //                                         }

// // // // //                                         // RC channel badge
// // // // //                                         Rectangle {
// // // // //                                             visible:      droneNeedsRc
// // // // //                                             height:       15
// // // // //                                             width:        rcBadgeTxt.implicitWidth + 10
// // // // //                                             radius:       3
// // // // //                                             color:        "#F0EAF8"
// // // // //                                             Text {
// // // // //                                                 id:             rcBadgeTxt
// // // // //                                                 anchors.centerIn: parent
// // // // //                                                 text:           "CH" + droneRcSwitch + " ×" + droneRcCount
// // // // //                                                 font.family:    Theme.fontFamily
// // // // //                                                 font.pixelSize: 9
// // // // //                                                 font.weight:    Font.Medium
// // // // //                                                 color:          "#6B3FA0"
// // // // //                                             }
// // // // //                                         }
// // // // //                                     }
// // // // //                                 }

// // // // //                                 // Range / speed
// // // // //                                 Column {
// // // // //                                     spacing: 1
// // // // //                                     Text {
// // // // //                                         text:           (droneRangeKm * 2).toFixed(0) + " km"
// // // // //                                         font.family:    Theme.fontFamily
// // // // //                                         font.pixelSize: Theme.fontSm
// // // // //                                         font.weight:    Font.Medium
// // // // //                                         color:          Theme.textPrimary
// // // // //                                     }
// // // // //                                     Text {
// // // // //                                         text:           droneSpeedMs.toFixed(1) + " m/s"
// // // // //                                         font.family:    Theme.fontFamily
// // // // //                                         font.pixelSize: 10
// // // // //                                         color:          Theme.textMuted
// // // // //                                     }
// // // // //                                 }

// // // // //                                 // Edit
// // // // //                                 FpButton {
// // // // //                                     text:           "Edit"
// // // // //                                     implicitWidth:  44
// // // // //                                     implicitHeight: 24
// // // // //                                     onClicked:      root.openEdit(index)
// // // // //                                 }

// // // // //                                 // Deactivate
// // // // //                                 Rectangle {
// // // // //                                     width: 24; height: 24; radius: Theme.radius
// // // // //                                     visible:      droneActive
// // // // //                                     color:        xArea.containsMouse ? Theme.redLight  : "transparent"
// // // // //                                     border.color: xArea.containsMouse ? "#F7C1C1"       : Theme.border
// // // // //                                     border.width: 0.5
// // // // //                                     Text { anchors.centerIn: parent; text: "✕"; color: Theme.red; font.pixelSize: 11 }
// // // // //                                     MouseArea {
// // // // //                                         id: xArea
// // // // //                                         anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
// // // // //                                         onClicked: { droneManager.deactivate(index); root.fleetChanged() }
// // // // //                                     }
// // // // //                                 }

// // // // //                                 // Reactivate
// // // // //                                 Rectangle {
// // // // //                                     width: 24; height: 24; radius: Theme.radius
// // // // //                                     visible:      !droneActive
// // // // //                                     color:        plusArea.containsMouse ? Theme.greenLight : "transparent"
// // // // //                                     border.color: plusArea.containsMouse ? "#C0DD97"        : Theme.border
// // // // //                                     border.width: 0.5
// // // // //                                     Text { anchors.centerIn: parent; text: "+"; color: Theme.green; font.pixelSize: 14 }
// // // // //                                     MouseArea {
// // // // //                                         id: plusArea
// // // // //                                         anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
// // // // //                                         onClicked: { droneManager.setActive(index, true); root.fleetChanged() }
// // // // //                                     }
// // // // //                                 }
// // // // //                             }
// // // // //                         }
// // // // //             }
// // // // //         }

// // // // //         Item { implicitHeight: 10 }

// // // // //         // ── Add / Edit form ───────────────────────────────────────────────────
// // // // //         Rectangle {
// // // // //             id:           formRect
// // // // //             Layout.fillWidth: true
// // // // //             visible:      root.showForm
// // // // //             height:       visible ? formCol.implicitHeight + 28 : 0
// // // // //             clip:         true
// // // // //             radius:       Theme.radiusLg
// // // // //             color:        Theme.bgSection
// // // // //             border.color: Theme.borderFocus
// // // // //             border.width: 1

// // // // //             Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

// // // // //             ColumnLayout {
// // // // //                 id: formCol
// // // // //                 anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
// // // // //                 spacing: 12

// // // // //                 // Title
// // // // //                 Text {
// // // // //                     text:           root.editMode ? "Edit Drone" : "Add New Drone"
// // // // //                     font.family:    Theme.fontFamily
// // // // //                     font.pixelSize: Theme.fontBase
// // // // //                     font.weight:    Font.Medium
// // // // //                     color:          Theme.textPrimary
// // // // //                 }

// // // // //                 // ── Name ──────────────────────────────────────────────────────
// // // // //                 ColumnLayout {
// // // // //                     Layout.fillWidth: true
// // // // //                     spacing: 4
// // // // //                     Text { text: "Name"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // //                     FpInput {
// // // // //                         id:              efName
// // // // //                         Layout.fillWidth: true
// // // // //                         placeholderText: "e.g. Eagle-X"
// // // // //                     }
// // // // //                 }

// // // // //                 // ── Range + Speed ─────────────────────────────────────────────
// // // // //                 RowLayout {
// // // // //                     Layout.fillWidth: true
// // // // //                     spacing: 12

// // // // //                     ColumnLayout {
// // // // //                         Layout.fillWidth: true
// // // // //                         spacing: 4
// // // // //                         Text { text: "One-way range (km)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // //                         FpSpinBox {
// // // // //                             id: efRange
// // // // //                             label: ""; unit: "km"
// // // // //                             value: 5; from: 1; to: 500; stepSize: 1
// // // // //                             Layout.fillWidth: true
// // // // //                         }
// // // // //                     }

// // // // //                     ColumnLayout {
// // // // //                         Layout.fillWidth: true
// // // // //                         spacing: 4
// // // // //                         Text { text: "Max speed (m/s)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // //                         FpSpinBox {
// // // // //                             id: efSpeed
// // // // //                             label: ""; unit: "m/s"
// // // // //                             value: 50; from: 5; to: 200; stepSize: 5
// // // // //                             Layout.fillWidth: true
// // // // //                         }
// // // // //                     }
// // // // //                 }

// // // // //                 // ── Mission Capabilities (all three independent) ───────────────
// // // // //                 ColumnLayout {
// // // // //                     spacing: 8

// // // // //                     Text { text: "Mission Capabilities"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // // //                     RowLayout {
// // // // //                         spacing: 24

// // // // //                         // Delivery
// // // // //                         RowLayout {
// // // // //                             spacing: 8
// // // // //                             Rectangle {
// // // // //                                 width: 18; height: 18; radius: 4
// // // // //                                 color:        root.capDelivery ? Theme.blueMid : Theme.bgInput
// // // // //                                 border.color: root.capDelivery ? Theme.blueMid : Theme.border
// // // // //                                 border.width: 0.5
// // // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capDelivery }
// // // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDelivery = !root.capDelivery }
// // // // //                             }
// // // // //                             Text { text: "Delivery"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // // //                         }

// // // // //                         // Drop
// // // // //                         RowLayout {
// // // // //                             spacing: 8
// // // // //                             Rectangle {
// // // // //                                 width: 18; height: 18; radius: 4
// // // // //                                 color:        root.capDrop ? Theme.blueMid : Theme.bgInput
// // // // //                                 border.color: root.capDrop ? Theme.blueMid : Theme.border
// // // // //                                 border.width: 0.5
// // // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capDrop }
// // // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDrop = !root.capDrop }
// // // // //                             }
// // // // //                             Text { text: "Drop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // // //                         }

// // // // //                         // Paradrop
// // // // //                         RowLayout {
// // // // //                             spacing: 8
// // // // //                             Rectangle {
// // // // //                                 width: 18; height: 18; radius: 4
// // // // //                                 color:        root.capParadrop ? Theme.blueMid : Theme.bgInput
// // // // //                                 border.color: root.capParadrop ? Theme.blueMid : Theme.border
// // // // //                                 border.width: 0.5
// // // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capParadrop }
// // // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capParadrop = !root.capParadrop }
// // // // //                             }
// // // // //                             Text { text: "Paradrop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // // //                         }
// // // // //                     }
// // // // //                 }

// // // // //                 // ── RC settings — only when Drop or Paradrop ticked ───────────
// // // // //                 ColumnLayout {
// // // // //                     visible:  root.capDrop || root.capParadrop
// // // // //                     spacing:  8

// // // // //                     Text { text: "RC Trigger Settings"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // // //                     RowLayout {
// // // // //                         spacing: 12

// // // // //                         ColumnLayout {
// // // // //                             spacing: 4
// // // // //                             Text { text: "RC Channel"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // //                             FpSpinBox {
// // // // //                                 id:             efRcCh
// // // // //                                 label:          ""
// // // // //                                 unit:           ""
// // // // //                                 value:          root.rcChannel
// // // // //                                 from: 1; to: 16; stepSize: 1
// // // // //                                 implicitWidth:  100
// // // // //                                 onValueChanged: root.rcChannel = value
// // // // //                             }
// // // // //                         }

// // // // //                         ColumnLayout {
// // // // //                             spacing: 4
// // // // //                             Text { text: "Trigger Count"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // // //                             FpSpinBox {
// // // // //                                 id:             efRcCount
// // // // //                                 label:          ""
// // // // //                                 unit:           "×"
// // // // //                                 value:          root.rcTriggers
// // // // //                                 from: 1; to: 10; stepSize: 1
// // // // //                                 implicitWidth:  100
// // // // //                                 onValueChanged: root.rcTriggers = value
// // // // //                             }
// // // // //                         }

// // // // //                         Text {
// // // // //                             text:           "Channel " + root.rcChannel + ", pulsed " + root.rcTriggers + "× at destination"
// // // // //                             font.family:    Theme.fontFamily
// // // // //                             font.pixelSize: Theme.fontSm
// // // // //                             color:          Theme.textMuted
// // // // //                             Layout.alignment: Qt.AlignBottom
// // // // //                         }
// // // // //                     }
// // // // //                 }

// // // // //                 // ── Default Altitudes ─────────────────────────────────────────
// // // // //                 ColumnLayout {
// // // // //                     spacing: 8

// // // // //                     Text { text: "Default Altitudes"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // // //                     RowLayout {
// // // // //                         Layout.fillWidth: true
// // // // //                         spacing: 12
// // // // //                         FpSpinBox { id: efTakeoff;  label: "Takeoff";    unit: "m"; value: 30;  from: 10; to: 300; stepSize: 5;  Layout.fillWidth: true }
// // // // //                         FpSpinBox { id: efNearHome; label: "Near-home";  unit: "m"; value: 60;  from: 20; to: 400; stepSize: 5;  Layout.fillWidth: true }
// // // // //                         FpSpinBox { id: efCruise;   label: "Clearance";  unit: "m"; value: 120; from: 80; to: 800; stepSize: 10; Layout.fillWidth: true }
// // // // //                         FpSpinBox { id: efLand;     label: "Land";       unit: "m"; value: 5;   from: 0;  to: 50;  stepSize: 1;  Layout.fillWidth: true }
// // // // //                     }
// // // // //                 }

// // // // //                 // ── Validation ────────────────────────────────────────────────
// // // // //                 Text {
// // // // //                     id:             validationMsg
// // // // //                     visible:        false
// // // // //                     text:           "Please enter a drone name"
// // // // //                     color:          Theme.red
// // // // //                     font.family:    Theme.fontFamily
// // // // //                     font.pixelSize: Theme.fontSm
// // // // //                 }

// // // // //                 // ── Buttons ───────────────────────────────────────────────────
// // // // //                 RowLayout {
// // // // //                     spacing: 8

// // // // //                     FpButton {
// // // // //                         text:           root.editMode ? "Update Drone" : "Add Drone"
// // // // //                         primary:        true
// // // // //                         implicitWidth:  110
// // // // //                         onClicked:      root.saveForm()
// // // // //                     }

// // // // //                     FpButton {
// // // // //                         text:          "Cancel"
// // // // //                         implicitWidth: 70
// // // // //                         onClicked: {
// // // // //                             root.showForm = false
// // // // //                             validationMsg.visible = false
// // // // //                         }
// // // // //                     }
// // // // //                 }

// // // // //                 Item { implicitHeight: 2 }
// // // // //             }
// // // // //         }
// // // // //     }
// // // // // }


// // // // import QtQuick 6.0
// // // // import QtQuick.Controls 6.0
// // // // import QtQuick.Layouts 6.0

// // // // Item {
// // // //     id: root
// // // //     implicitHeight: mainCol.implicitHeight

// // // //     signal fleetChanged()

// // // //     // ── Form state (root-scope so openAdd/openEdit can set before form renders) ─
// // // //     property bool showForm:    false
// // // //     property bool editMode:    false
// // // //     property int  editIndex:   -1

// // // //     property bool capDelivery: true
// // // //     property bool capDrop:     false
// // // //     property bool capParadrop: false
// // // //     property int  rcChannel:   1
// // // //     property int  rcTriggers:  1

// // // //     // ── Functions ─────────────────────────────────────────────────────────────
// // // //     function openAdd() {
// // // //         editMode         = false
// // // //         editIndex        = -1
// // // //         efName.text      = ""
// // // //         efRange.value    = 5
// // // //         efSpeed.value    = 50
// // // //         capDelivery      = true
// // // //         capDrop          = false
// // // //         capParadrop      = false
// // // //         rcChannel        = 1
// // // //         rcTriggers       = 1
// // // //         efTakeoff.value  = 30
// // // //         efNearHome.value = 60
// // // //         efCruise.value   = 120
// // // //         efLand.value     = 5
// // // //         showForm         = true
// // // //         Qt.callLater(function() { root.ensureFormVisible() })
// // // //     }

// // // //     function openEdit(idx) {
// // // //         var d            = droneManager.droneAt(idx)
// // // //         editMode         = true
// // // //         editIndex        = idx
// // // //         efName.text      = d.name
// // // //         efRange.value    = Math.round(d.rangeKm)
// // // //         efSpeed.value    = Math.round(d.speedMs * 10)
// // // //         capDelivery      = d.delivery
// // // //         capDrop          = d.drop
// // // //         capParadrop      = d.paradrop
// // // //         rcChannel        = d.rcSwitch > 0 ? d.rcSwitch : 1
// // // //         rcTriggers       = d.rcCount  > 0 ? d.rcCount  : 1
// // // //         efTakeoff.value  = Math.round(d.takeoffAlt)
// // // //         efNearHome.value = Math.round(d.nearHomeAlt)
// // // //         efCruise.value   = Math.round(d.cruiseAlt)
// // // //         efLand.value     = Math.round(d.landAlt)
// // // //         showForm         = true
// // // //         Qt.callLater(function() { root.ensureFormVisible() })
// // // //     }

// // // //     // Scroll the parent Flickable/ScrollView to reveal the edit form
// // // //     function ensureFormVisible() {
// // // //         var settingsFlick = null
// // // //         // Walk up parent chain to find a Flickable
// // // //         var p = root.parent
// // // //         while (p) {
// // // //             if (p.hasOwnProperty("contentY") && p.hasOwnProperty("contentHeight")) {
// // // //                 settingsFlick = p
// // // //                 break
// // // //             }
// // // //             p = p.parent
// // // //         }
// // // //         if (!settingsFlick) return
// // // //         // Map edit form position to the flickable's content coordinates
// // // //         var formY = formRect.mapToItem(settingsFlick.contentItem, 0, 0).y
// // // //         var formH = formRect.height
// // // //         var viewH = settingsFlick.height
// // // //         if (formY + formH > settingsFlick.contentY + viewH) {
// // // //             settingsFlick.contentY = Math.min(
// // // //                 formY + formH - viewH + 20,
// // // //                 settingsFlick.contentHeight - viewH
// // // //             )
// // // //         }
// // // //     }

// // // //     function saveForm() {
// // // //         var name = efName.text.trim()
// // // //         if (name === "") { validationMsg.visible = true; return }
// // // //         validationMsg.visible = false

// // // //         var needsRc = capDrop || capParadrop
// // // //         var rc      = needsRc ? rcChannel  : 0
// // // //         var rcCnt   = needsRc ? rcTriggers : 0

// // // //         if (editMode) {
// // // //             droneManager.updateDrone(
// // // //                 editIndex, name,
// // // //                 efRange.value, efSpeed.value / 10.0,
// // // //                 capDelivery, capDrop, capParadrop,
// // // //                 rc, rcCnt,
// // // //                 efTakeoff.value, efNearHome.value,
// // // //                 efCruise.value,  efLand.value
// // // //             )
// // // //         } else {
// // // //             droneManager.addDrone(
// // // //                 name,
// // // //                 efRange.value, efSpeed.value / 10.0,
// // // //                 capDelivery, capDrop, capParadrop,
// // // //                 rc, rcCnt,
// // // //                 efTakeoff.value, efNearHome.value,
// // // //                 efCruise.value,  efLand.value
// // // //             )
// // // //         }
// // // //         showForm = false
// // // //         root.fleetChanged()
// // // //     }

// // // //     // ── Main column ───────────────────────────────────────────────────────────
// // // //     ColumnLayout {
// // // //         id: mainCol
// // // //         anchors { left: parent.left; right: parent.right }
// // // //         spacing: 0

// // // //         // Header
// // // //         RowLayout {
// // // //             Layout.fillWidth: true
// // // //             spacing: 8

// // // //             Text {
// // // //                 text:           "Drone Fleet  (" + droneManager.count + " total)"
// // // //                 font.family:    Theme.fontFamily
// // // //                 font.pixelSize: Theme.fontBase
// // // //                 color:          Theme.textSecond
// // // //                 Layout.fillWidth: true
// // // //             }

// // // //             FpButton {
// // // //                 text:           "+ Add Drone"
// // // //                 implicitWidth:  96
// // // //                 implicitHeight: 26
// // // //                 onClicked:      root.openAdd()
// // // //             }
// // // //         }

// // // //         Item { implicitHeight: 8 }

// // // //         // ── Drone list — no height cap, parent ScrollView scrolls ──────────
// // // //         Column {
// // // //             id:               listCol
// // // //             Layout.fillWidth: true
// // // //             spacing:          4

// // // //             Repeater {
// // // //                         model: droneManager

// // // //                         delegate: Rectangle {
// // // //                             width:        parent.width
// // // //                             height:       rowContent.implicitHeight + 14
// // // //                             radius:       Theme.radius
// // // //                             color:        droneActive ? Theme.bgSection : "#F8F7F4"
// // // //                             border.color: droneActive ? Theme.border    : Theme.borderLight
// // // //                             border.width: 0.5
// // // //                             opacity:      droneActive ? 1.0 : 0.6

// // // //                             RowLayout {
// // // //                                 id: rowContent
// // // //                                 anchors {
// // // //                                     left: parent.left; right: parent.right
// // // //                                     verticalCenter: parent.verticalCenter
// // // //                                     leftMargin: 10; rightMargin: 10
// // // //                                 }
// // // //                                 spacing: 8

// // // //                                 // Active dot
// // // //                                 Rectangle {
// // // //                                     width: 8; height: 8; radius: 4
// // // //                                     color: droneActive ? Theme.green : Theme.textHint
// // // //                                     Layout.alignment: Qt.AlignVCenter
// // // //                                     MouseArea {
// // // //                                         anchors.fill: parent
// // // //                                         cursorShape:  Qt.PointingHandCursor
// // // //                                         onClicked:    droneManager.setActive(index, !droneActive)
// // // //                                     }
// // // //                                 }

// // // //                                 // Name + capability badges
// // // //                                 ColumnLayout {
// // // //                                     Layout.fillWidth: true
// // // //                                     Layout.alignment: Qt.AlignVCenter
// // // //                                     spacing: 3

// // // //                                     Text {
// // // //                                         text:           droneName
// // // //                                         font.family:    Theme.fontFamily
// // // //                                         font.pixelSize: Theme.fontBase
// // // //                                         font.weight:    Font.Medium
// // // //                                         color:          Theme.textPrimary
// // // //                                     }

// // // //                                     Row {
// // // //                                         spacing: 5

// // // //                                         Repeater {
// // // //                                             model: droneCapabilities
// // // //                                             delegate: Rectangle {
// // // //                                                 height: 15
// // // //                                                 width:  badgeTxt.implicitWidth + 10
// // // //                                                 radius: 3
// // // //                                                 color:  modelData === "Delivery" ? Theme.greenLight
// // // //                                                       : modelData === "Drop"     ? Theme.blueLight
// // // //                                                                                  : Theme.amberLight
// // // //                                                 Text {
// // // //                                                     id:             badgeTxt
// // // //                                                     anchors.centerIn: parent
// // // //                                                     text:           modelData
// // // //                                                     font.family:    Theme.fontFamily
// // // //                                                     font.pixelSize: 9
// // // //                                                     font.weight:    Font.Medium
// // // //                                                     color:          modelData === "Delivery" ? Theme.green
// // // //                                                                   : modelData === "Drop"     ? Theme.blue
// // // //                                                                                             : Theme.amber
// // // //                                                 }
// // // //                                             }
// // // //                                         }

// // // //                                         // RC channel badge
// // // //                                         Rectangle {
// // // //                                             visible:      droneNeedsRc
// // // //                                             height:       15
// // // //                                             width:        rcBadgeTxt.implicitWidth + 10
// // // //                                             radius:       3
// // // //                                             color:        "#F0EAF8"
// // // //                                             Text {
// // // //                                                 id:             rcBadgeTxt
// // // //                                                 anchors.centerIn: parent
// // // //                                                 text:           "CH" + droneRcSwitch + " ×" + droneRcCount
// // // //                                                 font.family:    Theme.fontFamily
// // // //                                                 font.pixelSize: 9
// // // //                                                 font.weight:    Font.Medium
// // // //                                                 color:          "#6B3FA0"
// // // //                                             }
// // // //                                         }
// // // //                                     }
// // // //                                 }

// // // //                                 // Range / speed
// // // //                                 Column {
// // // //                                     spacing: 1
// // // //                                     Text {
// // // //                                         text:           (droneRangeKm * 2).toFixed(0) + " km"
// // // //                                         font.family:    Theme.fontFamily
// // // //                                         font.pixelSize: Theme.fontSm
// // // //                                         font.weight:    Font.Medium
// // // //                                         color:          Theme.textPrimary
// // // //                                     }
// // // //                                     Text {
// // // //                                         text:           droneSpeedMs.toFixed(1) + " m/s"
// // // //                                         font.family:    Theme.fontFamily
// // // //                                         font.pixelSize: 10
// // // //                                         color:          Theme.textMuted
// // // //                                     }
// // // //                                 }

// // // //                                 // Edit
// // // //                                 FpButton {
// // // //                                     text:           "Edit"
// // // //                                     implicitWidth:  44
// // // //                                     implicitHeight: 24
// // // //                                     onClicked:      root.openEdit(index)
// // // //                                 }

// // // //                                 // Deactivate
// // // //                                 Rectangle {
// // // //                                     width: 24; height: 24; radius: Theme.radius
// // // //                                     visible:      droneActive
// // // //                                     color:        xArea.containsMouse ? Theme.redLight  : "transparent"
// // // //                                     border.color: xArea.containsMouse ? "#F7C1C1"       : Theme.border
// // // //                                     border.width: 0.5
// // // //                                     Text { anchors.centerIn: parent; text: "✕"; color: Theme.red; font.pixelSize: 11 }
// // // //                                     MouseArea {
// // // //                                         id: xArea
// // // //                                         anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
// // // //                                         onClicked: { droneManager.deactivate(index); root.fleetChanged() }
// // // //                                     }
// // // //                                 }

// // // //                                 // Reactivate
// // // //                                 Rectangle {
// // // //                                     width: 24; height: 24; radius: Theme.radius
// // // //                                     visible:      !droneActive
// // // //                                     color:        plusArea.containsMouse ? Theme.greenLight : "transparent"
// // // //                                     border.color: plusArea.containsMouse ? "#C0DD97"        : Theme.border
// // // //                                     border.width: 0.5
// // // //                                     Text { anchors.centerIn: parent; text: "+"; color: Theme.green; font.pixelSize: 14 }
// // // //                                     MouseArea {
// // // //                                         id: plusArea
// // // //                                         anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
// // // //                                         onClicked: { droneManager.setActive(index, true); root.fleetChanged() }
// // // //                                     }
// // // //                                 }
// // // //                             }
// // // //                         }
// // // //             }
// // // //         }

// // // //         Item { implicitHeight: 10 }

// // // //         // ── Add / Edit form ───────────────────────────────────────────────────
// // // //         Rectangle {
// // // //             id:           formRect
// // // //             Layout.fillWidth: true
// // // //             visible:      root.showForm
// // // //             height:       visible ? formCol.implicitHeight + 28 : 0
// // // //             clip:         true
// // // //             radius:       Theme.radiusLg
// // // //             color:        Theme.bgSection
// // // //             border.color: Theme.borderFocus
// // // //             border.width: 1

// // // //             Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

// // // //             ColumnLayout {
// // // //                 id: formCol
// // // //                 anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
// // // //                 spacing: 12

// // // //                 // Title
// // // //                 Text {
// // // //                     text:           root.editMode ? "Edit Drone" : "Add New Drone"
// // // //                     font.family:    Theme.fontFamily
// // // //                     font.pixelSize: Theme.fontBase
// // // //                     font.weight:    Font.Medium
// // // //                     color:          Theme.textPrimary
// // // //                 }

// // // //                 // ── Name ──────────────────────────────────────────────────────
// // // //                 ColumnLayout {
// // // //                     Layout.fillWidth: true
// // // //                     spacing: 4
// // // //                     Text { text: "Name"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // //                     FpInput {
// // // //                         id:              efName
// // // //                         Layout.fillWidth: true
// // // //                         placeholderText: "e.g. Eagle-X"
// // // //                     }
// // // //                 }

// // // //                 // ── Range + Speed ─────────────────────────────────────────────
// // // //                 RowLayout {
// // // //                     Layout.fillWidth: true
// // // //                     spacing: 12

// // // //                     ColumnLayout {
// // // //                         Layout.fillWidth: true
// // // //                         spacing: 4
// // // //                         Text { text: "One-way range (km)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // //                         FpSpinBox {
// // // //                             id: efRange
// // // //                             label: ""; unit: "km"
// // // //                             value: 5; from: 1; to: 500; stepSize: 1
// // // //                             Layout.fillWidth: true
// // // //                         }
// // // //                     }

// // // //                     ColumnLayout {
// // // //                         Layout.fillWidth: true
// // // //                         spacing: 4
// // // //                         Text { text: "Max speed (m/s)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // //                         FpSpinBox {
// // // //                             id: efSpeed
// // // //                             label: ""; unit: "m/s"
// // // //                             value: 50; from: 5; to: 200; stepSize: 5
// // // //                             Layout.fillWidth: true
// // // //                         }
// // // //                     }
// // // //                 }

// // // //                 // ── Mission Capabilities (all three independent) ───────────────
// // // //                 ColumnLayout {
// // // //                     spacing: 8

// // // //                     Text { text: "Mission Capabilities"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // //                     RowLayout {
// // // //                         spacing: 24

// // // //                         // Delivery
// // // //                         RowLayout {
// // // //                             spacing: 8
// // // //                             Rectangle {
// // // //                                 width: 18; height: 18; radius: 4
// // // //                                 color:        root.capDelivery ? Theme.blueMid : Theme.bgInput
// // // //                                 border.color: root.capDelivery ? Theme.blueMid : Theme.border
// // // //                                 border.width: 0.5
// // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capDelivery }
// // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDelivery = !root.capDelivery }
// // // //                             }
// // // //                             Text { text: "Delivery"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // //                         }

// // // //                         // Drop
// // // //                         RowLayout {
// // // //                             spacing: 8
// // // //                             Rectangle {
// // // //                                 width: 18; height: 18; radius: 4
// // // //                                 color:        root.capDrop ? Theme.blueMid : Theme.bgInput
// // // //                                 border.color: root.capDrop ? Theme.blueMid : Theme.border
// // // //                                 border.width: 0.5
// // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capDrop }
// // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDrop = !root.capDrop }
// // // //                             }
// // // //                             Text { text: "Drop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // //                         }

// // // //                         // Paradrop
// // // //                         RowLayout {
// // // //                             spacing: 8
// // // //                             Rectangle {
// // // //                                 width: 18; height: 18; radius: 4
// // // //                                 color:        root.capParadrop ? Theme.blueMid : Theme.bgInput
// // // //                                 border.color: root.capParadrop ? Theme.blueMid : Theme.border
// // // //                                 border.width: 0.5
// // // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capParadrop }
// // // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capParadrop = !root.capParadrop }
// // // //                             }
// // // //                             Text { text: "Paradrop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // // //                         }
// // // //                     }
// // // //                 }

// // // //                 // ── RC settings — only when Drop or Paradrop ticked ───────────
// // // //                 ColumnLayout {
// // // //                     visible:  root.capDrop || root.capParadrop
// // // //                     spacing:  8

// // // //                     Text { text: "RC Trigger Settings"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // //                     RowLayout {
// // // //                         spacing: 12

// // // //                         ColumnLayout {
// // // //                             spacing: 4
// // // //                             Text { text: "RC Channel"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // //                             FpSpinBox {
// // // //                                 id:             efRcCh
// // // //                                 label:          ""
// // // //                                 unit:           ""
// // // //                                 value:          root.rcChannel
// // // //                                 from: 1; to: 16; stepSize: 1
// // // //                                 implicitWidth:  100
// // // //                                 onValueChanged: root.rcChannel = value
// // // //                             }
// // // //                         }

// // // //                         ColumnLayout {
// // // //                             spacing: 4
// // // //                             Text { text: "Trigger Count"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // // //                             FpSpinBox {
// // // //                                 id:             efRcCount
// // // //                                 label:          ""
// // // //                                 unit:           "×"
// // // //                                 value:          root.rcTriggers
// // // //                                 from: 1; to: 10; stepSize: 1
// // // //                                 implicitWidth:  100
// // // //                                 onValueChanged: root.rcTriggers = value
// // // //                             }
// // // //                         }

// // // //                         Text {
// // // //                             text:           "Channel " + root.rcChannel + ", pulsed " + root.rcTriggers + "× at destination"
// // // //                             font.family:    Theme.fontFamily
// // // //                             font.pixelSize: Theme.fontSm
// // // //                             color:          Theme.textMuted
// // // //                             Layout.alignment: Qt.AlignBottom
// // // //                         }
// // // //                     }
// // // //                 }

// // // //                 // ── Default Altitudes ─────────────────────────────────────────
// // // //                 ColumnLayout {
// // // //                     spacing: 8

// // // //                     Text { text: "Default Altitudes"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // // //                     RowLayout {
// // // //                         Layout.fillWidth: true
// // // //                         spacing: 12
// // // //                         FpSpinBox { id: efTakeoff;  label: "Takeoff";    unit: "m"; value: 30;  from: 10; to: 300; stepSize: 5;  Layout.fillWidth: true }
// // // //                         FpSpinBox { id: efNearHome; label: "Near-home";  unit: "m"; value: 60;  from: 20; to: 400; stepSize: 5;  Layout.fillWidth: true }
// // // //                         FpSpinBox { id: efCruise;   label: "Clearance";  unit: "m"; value: 120; from: 80; to: 800; stepSize: 10; Layout.fillWidth: true }
// // // //                         FpSpinBox { id: efLand;     label: "Land";       unit: "m"; value: 5;   from: 0;  to: 50;  stepSize: 1;  Layout.fillWidth: true }
// // // //                     }
// // // //                 }

// // // //                 // ── Validation ────────────────────────────────────────────────
// // // //                 Text {
// // // //                     id:             validationMsg
// // // //                     visible:        false
// // // //                     text:           "Please enter a drone name"
// // // //                     color:          Theme.red
// // // //                     font.family:    Theme.fontFamily
// // // //                     font.pixelSize: Theme.fontSm
// // // //                 }

// // // //                 // ── Buttons ───────────────────────────────────────────────────
// // // //                 RowLayout {
// // // //                     spacing: 8

// // // //                     FpButton {
// // // //                         text:           root.editMode ? "Update Drone" : "Add Drone"
// // // //                         primary:        true
// // // //                         implicitWidth:  110
// // // //                         onClicked:      root.saveForm()
// // // //                     }

// // // //                     FpButton {
// // // //                         text:          "Cancel"
// // // //                         implicitWidth: 70
// // // //                         onClicked: {
// // // //                             root.showForm = false
// // // //                             validationMsg.visible = false
// // // //                         }
// // // //                     }
// // // //                 }

// // // //                 Item { implicitHeight: 2 }
// // // //             }
// // // //         }
// // // //     }
// // // // }



// // // import QtQuick 6.0
// // // import QtQuick.Controls 6.0
// // // import QtQuick.Layouts 6.0

// // // Item {
// // //     id: root
// // //     implicitHeight: mainCol.implicitHeight

// // //     signal fleetChanged()

// // //     // ── Form state ────────────────────────────────────────────────────────────
// // //     property bool showForm:    false
// // //     property bool editMode:    false
// // //     property int  editIndex:   -1

// // //     property bool capDelivery: true
// // //     property bool capDrop:     false
// // //     property bool capParadrop: false
// // //     property int  rcChannel:   1
// // //     property int  rcTriggers:  1

// // //     // ── Functions ─────────────────────────────────────────────────────────────
// // //     function openAdd() {
// // //         editMode         = false
// // //         editIndex        = -1
// // //         efName.text      = ""
// // //         efRange.value    = 5
// // //         efSpeed.value    = 50
// // //         capDelivery      = true
// // //         capDrop          = false
// // //         capParadrop      = false
// // //         rcChannel        = 1
// // //         rcTriggers       = 1
// // //         efTakeoff.value  = 30
// // //         efNearHome.value = 60
// // //         efCruise.value   = 120
// // //         efLand.value     = 5
// // //         showForm         = true
// // //         Qt.callLater(function() { root.ensureFormVisible() })
// // //     }

// // //     function openEdit(idx) {
// // //         var d            = droneManager.droneAt(idx)
// // //         editMode         = true
// // //         editIndex        = idx
// // //         efName.text      = d.name
// // //         efRange.value    = Math.round(d.rangeKm)
// // //         efSpeed.value    = Math.round(d.speedMs * 10)
// // //         capDelivery      = d.delivery
// // //         capDrop          = d.drop
// // //         capParadrop      = d.paradrop
// // //         rcChannel        = d.rcSwitch > 0 ? d.rcSwitch : 1
// // //         rcTriggers       = d.rcCount  > 0 ? d.rcCount  : 1
// // //         efTakeoff.value  = Math.round(d.takeoffAlt)
// // //         efNearHome.value = Math.round(d.nearHomeAlt)
// // //         efCruise.value   = Math.round(d.cruiseAlt)
// // //         efLand.value     = Math.round(d.landAlt)
// // //         showForm         = true
// // //         Qt.callLater(function() { root.ensureFormVisible() })
// // //     }

// // //     // Walk up parent chain to find settingsScroll's internal Flickable
// // //     // (MainView.qml wraps the settings page in a ScrollView named settingsScroll)
// // //     function ensureFormVisible() {
// // //         var flick = null
// // //         var p = root.parent
// // //         while (p) {
// // //             if (p.hasOwnProperty("contentY") && p.hasOwnProperty("contentHeight")) {
// // //                 flick = p
// // //                 break
// // //             }
// // //             p = p.parent
// // //         }
// // //         if (!flick) return

// // //         var formY = formRect.mapToItem(flick.contentItem, 0, 0).y
// // //         var formH = formRect.height
// // //         var viewH = flick.height

// // //         if (formY + formH > flick.contentY + viewH) {
// // //             flick.contentY = Math.min(
// // //                 formY + formH - viewH + 20,
// // //                 flick.contentHeight - viewH
// // //             )
// // //         }
// // //     }

// // //     function saveForm() {
// // //         var name = efName.text.trim()
// // //         if (name === "") { validationMsg.visible = true; return }
// // //         validationMsg.visible = false

// // //         var needsRc = capDrop || capParadrop
// // //         var rc      = needsRc ? rcChannel  : 0
// // //         var rcCnt   = needsRc ? rcTriggers : 0

// // //         if (editMode) {
// // //             droneManager.updateDrone(
// // //                 editIndex, name,
// // //                 efRange.value, efSpeed.value / 10.0,
// // //                 capDelivery, capDrop, capParadrop,
// // //                 rc, rcCnt,
// // //                 efTakeoff.value, efNearHome.value,
// // //                 efCruise.value,  efLand.value
// // //             )
// // //         } else {
// // //             droneManager.addDrone(
// // //                 name,
// // //                 efRange.value, efSpeed.value / 10.0,
// // //                 capDelivery, capDrop, capParadrop,
// // //                 rc, rcCnt,
// // //                 efTakeoff.value, efNearHome.value,
// // //                 efCruise.value,  efLand.value
// // //             )
// // //         }
// // //         showForm = false
// // //         root.fleetChanged()
// // //     }

// // //     // ── Main column ───────────────────────────────────────────────────────────
// // //     ColumnLayout {
// // //         id: mainCol
// // //         anchors { left: parent.left; right: parent.right }
// // //         spacing: 0

// // //         // ── Header row ────────────────────────────────────────────────────────
// // //         RowLayout {
// // //             Layout.fillWidth: true
// // //             spacing: 8

// // //             Text {
// // //                 // FIX: was clipping to just "D" because there was no fillWidth +
// // //                 // the SectionBlock title row was consuming the space.
// // //                 // elide prevents overflow on very narrow panels.
// // //                 text:             "Drone Fleet  (" + droneManager.count + " total)"
// // //                 font.family:      Theme.fontFamily
// // //                 font.pixelSize:   Theme.fontBase
// // //                 color:            Theme.textSecond
// // //                 Layout.fillWidth: true
// // //                 elide:            Text.ElideRight
// // //             }

// // //             FpButton {
// // //                 text:           "+ Add Drone"
// // //                 implicitWidth:  96
// // //                 implicitHeight: 26
// // //                 onClicked:      root.openAdd()
// // //             }
// // //         }

// // //         Item { implicitHeight: 8 }

// // //         // ── Drone list ────────────────────────────────────────────────────────
// // //         Column {
// // //             id:               listCol
// // //             Layout.fillWidth: true
// // //             spacing:          4

// // //             Repeater {
// // //                 model: droneManager

// // //                 delegate: Rectangle {
// // //                     // FIX: explicit parent-width binding so rows don't overflow
// // //                     width:        listCol.width
// // //                     height:       rowContent.implicitHeight + 14
// // //                     radius:       Theme.radius
// // //                     color:        droneActive ? Theme.bgSection : "#F8F7F4"
// // //                     border.color: droneActive ? Theme.border    : Theme.borderLight
// // //                     border.width: 0.5
// // //                     opacity:      droneActive ? 1.0 : 0.6

// // //                     RowLayout {
// // //                         id: rowContent
// // //                         anchors {
// // //                             left: parent.left; right: parent.right
// // //                             verticalCenter: parent.verticalCenter
// // //                             leftMargin: 10; rightMargin: 10
// // //                         }
// // //                         spacing: 8

// // //                         // Active indicator dot
// // //                         Rectangle {
// // //                             width: 8; height: 8; radius: 4
// // //                             color: droneActive ? Theme.green : Theme.textHint
// // //                             Layout.alignment: Qt.AlignVCenter
// // //                             MouseArea {
// // //                                 anchors.fill: parent
// // //                                 cursorShape:  Qt.PointingHandCursor
// // //                                 onClicked:    droneManager.setActive(index, !droneActive)
// // //                             }
// // //                         }

// // //                         // Name + capability badges
// // //                         ColumnLayout {
// // //                             Layout.fillWidth: true
// // //                             Layout.alignment: Qt.AlignVCenter
// // //                             spacing: 3

// // //                             Text {
// // //                                 text:             droneName
// // //                                 font.family:      Theme.fontFamily
// // //                                 font.pixelSize:   Theme.fontBase
// // //                                 font.weight:      Font.Medium
// // //                                 color:            Theme.textPrimary
// // //                                 elide:            Text.ElideRight
// // //                                 Layout.fillWidth: true
// // //                             }

// // //                             // FIX: Flow instead of Row — badges wrap gracefully
// // //                             // when the panel is narrow instead of overflowing
// // //                             Flow {
// // //                                 Layout.fillWidth: true
// // //                                 spacing: 5

// // //                                 Repeater {
// // //                                     model: droneCapabilities
// // //                                     delegate: Rectangle {
// // //                                         height: 15
// // //                                         width:  badgeTxt.implicitWidth + 10
// // //                                         radius: 3
// // //                                         color:  modelData === "Delivery" ? Theme.greenLight
// // //                                               : modelData === "Drop"     ? Theme.blueLight
// // //                                                                          : Theme.amberLight
// // //                                         Text {
// // //                                             id:             badgeTxt
// // //                                             anchors.centerIn: parent
// // //                                             text:           modelData
// // //                                             font.family:    Theme.fontFamily
// // //                                             font.pixelSize: 9
// // //                                             font.weight:    Font.Medium
// // //                                             color:          modelData === "Delivery" ? Theme.green
// // //                                                           : modelData === "Drop"     ? Theme.blue
// // //                                                                                     : Theme.amber
// // //                                         }
// // //                                     }
// // //                                 }

// // //                                 // RC channel badge
// // //                                 Rectangle {
// // //                                     visible:      droneNeedsRc
// // //                                     height:       15
// // //                                     width:        rcBadgeTxt.implicitWidth + 10
// // //                                     radius:       3
// // //                                     color:        "#F0EAF8"
// // //                                     Text {
// // //                                         id:             rcBadgeTxt
// // //                                         anchors.centerIn: parent
// // //                                         text:           "CH" + droneRcSwitch + " ×" + droneRcCount
// // //                                         font.family:    Theme.fontFamily
// // //                                         font.pixelSize: 9
// // //                                         font.weight:    Font.Medium
// // //                                         color:          "#6B3FA0"
// // //                                     }
// // //                                 }
// // //                             }
// // //                         }

// // //                         // Range / speed
// // //                         Column {
// // //                             spacing: 1
// // //                             // FIX: minimum width prevents this column from collapsing
// // //                             // when the panel is narrow and the badge row takes extra space
// // //                             Layout.minimumWidth: 54

// // //                             Text {
// // //                                 text:           (droneRangeKm * 2).toFixed(0) + " km"
// // //                                 font.family:    Theme.fontFamily
// // //                                 font.pixelSize: Theme.fontSm
// // //                                 font.weight:    Font.Medium
// // //                                 color:          Theme.textPrimary
// // //                             }
// // //                             Text {
// // //                                 text:           droneSpeedMs.toFixed(1) + " m/s"
// // //                                 font.family:    Theme.fontFamily
// // //                                 font.pixelSize: 10
// // //                                 color:          Theme.textMuted
// // //                             }
// // //                         }

// // //                         // Edit button
// // //                         FpButton {
// // //                             text:           "Edit"
// // //                             implicitWidth:  44
// // //                             implicitHeight: 24
// // //                             onClicked:      root.openEdit(index)
// // //                         }

// // //                         // Deactivate (✕) — visible only when active
// // //                         Rectangle {
// // //                             width: 24; height: 24; radius: Theme.radius
// // //                             visible:      droneActive
// // //                             color:        xArea.containsMouse ? Theme.redLight  : "transparent"
// // //                             border.color: xArea.containsMouse ? "#F7C1C1"       : Theme.border
// // //                             border.width: 0.5
// // //                             Text { anchors.centerIn: parent; text: "✕"; color: Theme.red; font.pixelSize: 11 }
// // //                             MouseArea {
// // //                                 id: xArea
// // //                                 anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
// // //                                 onClicked: { droneManager.deactivate(index); root.fleetChanged() }
// // //                             }
// // //                         }

// // //                         // Reactivate (+) — visible only when inactive
// // //                         Rectangle {
// // //                             width: 24; height: 24; radius: Theme.radius
// // //                             visible:      !droneActive
// // //                             color:        plusArea.containsMouse ? Theme.greenLight : "transparent"
// // //                             border.color: plusArea.containsMouse ? "#C0DD97"        : Theme.border
// // //                             border.width: 0.5
// // //                             Text { anchors.centerIn: parent; text: "+"; color: Theme.green; font.pixelSize: 14 }
// // //                             MouseArea {
// // //                                 id: plusArea
// // //                                 anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
// // //                                 onClicked: { droneManager.setActive(index, true); root.fleetChanged() }
// // //                             }
// // //                         }
// // //                     }
// // //                 }
// // //             }
// // //         }

// // //         Item { implicitHeight: 10 }

// // //         // ── Add / Edit form ───────────────────────────────────────────────────
// // //         Rectangle {
// // //             id:               formRect
// // //             Layout.fillWidth: true
// // //             visible:          root.showForm
// // //             height:           visible ? formCol.implicitHeight + 28 : 0
// // //             clip:             true
// // //             radius:           Theme.radiusLg
// // //             color:            Theme.bgSection
// // //             border.color:     Theme.borderFocus
// // //             border.width:     1

// // //             Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

// // //             ColumnLayout {
// // //                 id:      formCol
// // //                 anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
// // //                 spacing: 12

// // //                 Text {
// // //                     text:           root.editMode ? "Edit Drone" : "Add New Drone"
// // //                     font.family:    Theme.fontFamily
// // //                     font.pixelSize: Theme.fontBase
// // //                     font.weight:    Font.Medium
// // //                     color:          Theme.textPrimary
// // //                 }

// // //                 // ── Name ──────────────────────────────────────────────────────
// // //                 ColumnLayout {
// // //                     Layout.fillWidth: true
// // //                     spacing: 4
// // //                     Text { text: "Name"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // //                     FpInput {
// // //                         id:               efName
// // //                         Layout.fillWidth: true
// // //                         placeholderText:  "e.g. Eagle-X"
// // //                     }
// // //                 }

// // //                 // ── Range + Speed ─────────────────────────────────────────────
// // //                 RowLayout {
// // //                     Layout.fillWidth: true
// // //                     spacing: 12

// // //                     ColumnLayout {
// // //                         Layout.fillWidth: true
// // //                         spacing: 4
// // //                         Text { text: "One-way range (km)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // //                         FpSpinBox {
// // //                             id: efRange
// // //                             label: ""; unit: "km"
// // //                             value: 5; from: 1; to: 500; stepSize: 1
// // //                             Layout.fillWidth: true
// // //                         }
// // //                     }

// // //                     ColumnLayout {
// // //                         Layout.fillWidth: true
// // //                         spacing: 4
// // //                         Text { text: "Max speed (m/s)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // //                         FpSpinBox {
// // //                             id: efSpeed
// // //                             label: ""; unit: "m/s"
// // //                             value: 50; from: 5; to: 200; stepSize: 5
// // //                             Layout.fillWidth: true
// // //                         }
// // //                     }
// // //                 }

// // //                 // ── Mission Capabilities ──────────────────────────────────────
// // //                 ColumnLayout {
// // //                     spacing: 8

// // //                     Text { text: "Mission Capabilities"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // //                     RowLayout {
// // //                         spacing: 24

// // //                         // Delivery
// // //                         RowLayout {
// // //                             spacing: 8
// // //                             Rectangle {
// // //                                 width: 18; height: 18; radius: 4
// // //                                 color:        root.capDelivery ? Theme.blueMid : Theme.bgInput
// // //                                 border.color: root.capDelivery ? Theme.blueMid : Theme.border
// // //                                 border.width: 0.5
// // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capDelivery }
// // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDelivery = !root.capDelivery }
// // //                             }
// // //                             Text { text: "Delivery"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // //                         }

// // //                         // Drop
// // //                         RowLayout {
// // //                             spacing: 8
// // //                             Rectangle {
// // //                                 width: 18; height: 18; radius: 4
// // //                                 color:        root.capDrop ? Theme.blueMid : Theme.bgInput
// // //                                 border.color: root.capDrop ? Theme.blueMid : Theme.border
// // //                                 border.width: 0.5
// // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capDrop }
// // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDrop = !root.capDrop }
// // //                             }
// // //                             Text { text: "Drop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // //                         }

// // //                         // Paradrop
// // //                         RowLayout {
// // //                             spacing: 8
// // //                             Rectangle {
// // //                                 width: 18; height: 18; radius: 4
// // //                                 color:        root.capParadrop ? Theme.blueMid : Theme.bgInput
// // //                                 border.color: root.capParadrop ? Theme.blueMid : Theme.border
// // //                                 border.width: 0.5
// // //                                 Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capParadrop }
// // //                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capParadrop = !root.capParadrop }
// // //                             }
// // //                             Text { text: "Paradrop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// // //                         }
// // //                     }
// // //                 }

// // //                 // ── RC settings — only when Drop or Paradrop ticked ───────────
// // //                 ColumnLayout {
// // //                     visible:  root.capDrop || root.capParadrop
// // //                     spacing:  8

// // //                     Text { text: "RC Trigger Settings"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // //                     RowLayout {
// // //                         spacing: 12

// // //                         ColumnLayout {
// // //                             spacing: 4
// // //                             Text { text: "RC Channel"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // //                             FpSpinBox {
// // //                                 id:             efRcCh
// // //                                 label: ""; unit: ""
// // //                                 value:          root.rcChannel
// // //                                 from: 1; to: 16; stepSize: 1
// // //                                 implicitWidth:  100
// // //                                 onValueChanged: root.rcChannel = value
// // //                             }
// // //                         }

// // //                         ColumnLayout {
// // //                             spacing: 4
// // //                             Text { text: "Trigger Count"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// // //                             FpSpinBox {
// // //                                 id:             efRcCount
// // //                                 label: ""; unit: "×"
// // //                                 value:          root.rcTriggers
// // //                                 from: 1; to: 10; stepSize: 1
// // //                                 implicitWidth:  100
// // //                                 onValueChanged: root.rcTriggers = value
// // //                             }
// // //                         }

// // //                         Text {
// // //                             text:             "Channel " + root.rcChannel + ", pulsed " + root.rcTriggers + "× at destination"
// // //                             font.family:      Theme.fontFamily
// // //                             font.pixelSize:   Theme.fontSm
// // //                             color:            Theme.textMuted
// // //                             Layout.alignment: Qt.AlignBottom
// // //                             wrapMode:         Text.WordWrap
// // //                             Layout.fillWidth: true
// // //                         }
// // //                     }
// // //                 }

// // //                 // ── Default Altitudes ─────────────────────────────────────────
// // //                 ColumnLayout {
// // //                     spacing: 8

// // //                     Text { text: "Default Altitudes"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// // //                     RowLayout {
// // //                         Layout.fillWidth: true
// // //                         spacing: 12
// // //                         FpSpinBox { id: efTakeoff;  label: "Takeoff";   unit: "m"; value: 30;  from: 10; to: 300; stepSize: 5;  Layout.fillWidth: true }
// // //                         FpSpinBox { id: efNearHome; label: "Near-home"; unit: "m"; value: 60;  from: 20; to: 400; stepSize: 5;  Layout.fillWidth: true }
// // //                         FpSpinBox { id: efCruise;   label: "Clearance"; unit: "m"; value: 120; from: 80; to: 800; stepSize: 10; Layout.fillWidth: true }
// // //                         FpSpinBox { id: efLand;     label: "Land";      unit: "m"; value: 5;   from: 0;  to: 50;  stepSize: 1;  Layout.fillWidth: true }
// // //                     }
// // //                 }

// // //                 // ── Validation message ────────────────────────────────────────
// // //                 Text {
// // //                     id:             validationMsg
// // //                     visible:        false
// // //                     text:           "Please enter a drone name"
// // //                     color:          Theme.red
// // //                     font.family:    Theme.fontFamily
// // //                     font.pixelSize: Theme.fontSm
// // //                 }

// // //                 // ── Action buttons ────────────────────────────────────────────
// // //                 RowLayout {
// // //                     spacing: 8

// // //                     FpButton {
// // //                         text:          root.editMode ? "Update Drone" : "Add Drone"
// // //                         primary:       true
// // //                         implicitWidth: 110
// // //                         onClicked:     root.saveForm()
// // //                     }

// // //                     FpButton {
// // //                         text:          "Cancel"
// // //                         implicitWidth: 70
// // //                         onClicked: {
// // //                             root.showForm         = false
// // //                             validationMsg.visible = false
// // //                         }
// // //                     }
// // //                 }

// // //                 Item { implicitHeight: 2 }
// // //             }
// // //         }
// // //     }
// // // }





// // import QtQuick 6.0
// // import QtQuick.Controls 6.0
// // import QtQuick.Layouts 6.0

// // // ─────────────────────────────────────────────────────────────────────────────
// // // DroneFleetSection.qml
// // //
// // // Drop this directly into settingsCol in MainView.qml — do NOT wrap it in a
// // // SectionBlock, because SectionBlock adds internal padding that prevents
// // // this component from measuring its own width correctly.
// // //
// // // Usage in MainView.qml (replace the old SectionBlock wrapping DroneFleetPanel):
// // //
// // //   DroneFleetSection {
// // //       Layout.fillWidth: true
// // //       onFleetChanged: root.refreshDroneCombo()
// // //   }
// // // ─────────────────────────────────────────────────────────────────────────────

// // Item {
// //     id: root
// //     implicitWidth:  mainCol.implicitWidth
// //     implicitHeight: mainCol.implicitHeight

// //     signal fleetChanged()

// //     // ── Form state ────────────────────────────────────────────────────────────
// //     property bool showForm:    false
// //     property bool editMode:    false
// //     property int  editIndex:   -1

// //     property bool capDelivery: true
// //     property bool capDrop:     false
// //     property bool capParadrop: false
// //     property int  rcChannel:   1
// //     property int  rcTriggers:  1

// //     // ── Open / populate helpers ───────────────────────────────────────────────
// //     function openAdd() {
// //         editMode         = false
// //         editIndex        = -1
// //         efName.text      = ""
// //         efRange.value    = 5
// //         efSpeed.value    = 50
// //         capDelivery      = true
// //         capDrop          = false
// //         capParadrop      = false
// //         rcChannel        = 1
// //         rcTriggers       = 1
// //         efTakeoff.value  = 30
// //         efNearHome.value = 60
// //         efCruise.value   = 120
// //         efLand.value     = 5
// //         showForm         = true
// //         Qt.callLater(function() { root.ensureFormVisible() })
// //     }

// //     function openEdit(idx) {
// //         var d            = droneManager.droneAt(idx)
// //         editMode         = true
// //         editIndex        = idx
// //         efName.text      = d.name
// //         efRange.value    = Math.round(d.rangeKm)
// //         efSpeed.value    = Math.round(d.speedMs * 10)
// //         capDelivery      = d.delivery
// //         capDrop          = d.drop
// //         capParadrop      = d.paradrop
// //         rcChannel        = d.rcSwitch > 0 ? d.rcSwitch : 1
// //         rcTriggers       = d.rcCount  > 0 ? d.rcCount  : 1
// //         efTakeoff.value  = Math.round(d.takeoffAlt)
// //         efNearHome.value = Math.round(d.nearHomeAlt)
// //         efCruise.value   = Math.round(d.cruiseAlt)
// //         efLand.value     = Math.round(d.landAlt)
// //         showForm         = true
// //         Qt.callLater(function() { root.ensureFormVisible() })
// //     }

// //     // Walk up to settingsScroll's Flickable and scroll formRect into view
// //     function ensureFormVisible() {
// //         var flick = null
// //         var p = root.parent
// //         while (p) {
// //             if (p.hasOwnProperty("contentY") && p.hasOwnProperty("contentHeight")) {
// //                 flick = p
// //                 break
// //             }
// //             p = p.parent
// //         }
// //         if (!flick) return

// //         var formY = formRect.mapToItem(flick.contentItem, 0, 0).y
// //         var formH = formRect.height
// //         var viewH = flick.height

// //         if (formY + formH > flick.contentY + viewH) {
// //             flick.contentY = Math.min(
// //                 formY + formH - viewH + 20,
// //                 flick.contentHeight - viewH
// //             )
// //         }
// //     }

// //     function saveForm() {
// //         var name = efName.text.trim()
// //         if (name === "") { validationMsg.visible = true; return }
// //         validationMsg.visible = false

// //         var needsRc = capDrop || capParadrop
// //         var rc      = needsRc ? rcChannel  : 0
// //         var rcCnt   = needsRc ? rcTriggers : 0

// //         if (editMode) {
// //             droneManager.updateDrone(
// //                 editIndex, name,
// //                 efRange.value, efSpeed.value / 10.0,
// //                 capDelivery, capDrop, capParadrop,
// //                 rc, rcCnt,
// //                 efTakeoff.value, efNearHome.value,
// //                 efCruise.value,  efLand.value
// //             )
// //         } else {
// //             droneManager.addDrone(
// //                 name,
// //                 efRange.value, efSpeed.value / 10.0,
// //                 capDelivery, capDrop, capParadrop,
// //                 rc, rcCnt,
// //                 efTakeoff.value, efNearHome.value,
// //                 efCruise.value,  efLand.value
// //             )
// //         }
// //         showForm = false
// //         root.fleetChanged()
// //     }

// //     // ── Outer card (matches SectionBlock visual style) ────────────────────────
// //     Rectangle {
// //         id:           card
// //         anchors.fill: parent
// //         radius:       Theme.radiusLg
// //         color:        Theme.bgSection
// //         border.color: Theme.border
// //         border.width: 0.5
// //     }

// //     // ── Main column (all content) ─────────────────────────────────────────────
// //     ColumnLayout {
// //         id:      mainCol
// //         // Anchor to fill root so width is always correct
// //         anchors { left: parent.left; right: parent.right; top: parent.top }
// //         spacing: 0

// //         // ── Section title bar ─────────────────────────────────────────────────
// //         Item {
// //             Layout.fillWidth: true
// //             implicitHeight:   titleRow.implicitHeight + 20

// //             RowLayout {
// //                 id:      titleRow
// //                 anchors {
// //                     left: parent.left; right: parent.right
// //                     verticalCenter: parent.verticalCenter
// //                     leftMargin: 16; rightMargin: 16
// //                 }
// //                 spacing: 8

// //                 Text {
// //                     text:             "DRONE FLEET"
// //                     font.family:      Theme.fontFamily
// //                     font.pixelSize:   Theme.fontXs
// //                     font.weight:      Font.Medium
// //                     font.letterSpacing: 0.8
// //                     color:            Theme.textMuted
// //                     Layout.fillWidth: true
// //                 }

// //                 // Drone count chip
// //                 Rectangle {
// //                     height:       18
// //                     width:        countTxt.implicitWidth + 12
// //                     radius:       9
// //                     color:        Theme.blueLight
// //                     visible:      droneManager.count > 0
// //                     Text {
// //                         id:               countTxt
// //                         anchors.centerIn: parent
// //                         text:             droneManager.count + " drone" + (droneManager.count === 1 ? "" : "s")
// //                         font.family:      Theme.fontFamily
// //                         font.pixelSize:   Theme.fontXs
// //                         font.weight:      Font.Medium
// //                         color:            Theme.blue
// //                     }
// //                 }

// //                 FpButton {
// //                     text:           "+ Add Drone"
// //                     implicitWidth:  96
// //                     implicitHeight: 26
// //                     onClicked:      root.openAdd()
// //                 }
// //             }

// //             // Bottom divider
// //             Rectangle {
// //                 anchors.bottom: parent.bottom
// //                 width: parent.width; height: 0.5
// //                 color: Theme.border
// //             }
// //         }

// //         // ── Drone list ────────────────────────────────────────────────────────
// //         ColumnLayout {
// //             id:               listWrapper
// //             Layout.fillWidth: true
// //             Layout.topMargin: 8
// //             Layout.bottomMargin: 4
// //             Layout.leftMargin:  12
// //             Layout.rightMargin: 12
// //             spacing: 4

// //             Repeater {
// //                 model: droneManager

// //                 delegate: Rectangle {
// //                     Layout.fillWidth: true
// //                     height:           rowContent.implicitHeight + 14
// //                     radius:           Theme.radius
// //                     color:            droneActive ? Theme.bg : "#F8F7F4"
// //                     border.color:     droneActive ? Theme.border : Theme.borderLight
// //                     border.width:     0.5
// //                     opacity:          droneActive ? 1.0 : 0.6

// //                     RowLayout {
// //                         id:      rowContent
// //                         anchors {
// //                             left: parent.left; right: parent.right
// //                             verticalCenter: parent.verticalCenter
// //                             leftMargin: 10; rightMargin: 10
// //                         }
// //                         spacing: 8

// //                         // Active dot
// //                         Rectangle {
// //                             width: 8; height: 8; radius: 4
// //                             color: droneActive ? Theme.green : Theme.textHint
// //                             Layout.alignment: Qt.AlignVCenter
// //                             MouseArea {
// //                                 anchors.fill: parent
// //                                 cursorShape:  Qt.PointingHandCursor
// //                                 onClicked:    droneManager.setActive(index, !droneActive)
// //                             }
// //                         }

// //                         // Name + badges
// //                         ColumnLayout {
// //                             Layout.fillWidth: true
// //                             Layout.alignment: Qt.AlignVCenter
// //                             spacing: 3

// //                             Text {
// //                                 text:             droneName
// //                                 font.family:      Theme.fontFamily
// //                                 font.pixelSize:   Theme.fontBase
// //                                 font.weight:      Font.Medium
// //                                 color:            Theme.textPrimary
// //                                 elide:            Text.ElideRight
// //                                 Layout.fillWidth: true
// //                             }

// //                             Flow {
// //                                 Layout.fillWidth: true
// //                                 spacing: 4

// //                                 Repeater {
// //                                     model: droneCapabilities
// //                                     delegate: Rectangle {
// //                                         height: 15
// //                                         width:  badgeTxt.implicitWidth + 10
// //                                         radius: 3
// //                                         color:  modelData === "Delivery" ? Theme.greenLight
// //                                               : modelData === "Drop"     ? Theme.blueLight
// //                                                                          : Theme.amberLight
// //                                         Text {
// //                                             id:             badgeTxt
// //                                             anchors.centerIn: parent
// //                                             text:           modelData
// //                                             font.family:    Theme.fontFamily
// //                                             font.pixelSize: 9
// //                                             font.weight:    Font.Medium
// //                                             color:          modelData === "Delivery" ? Theme.green
// //                                                           : modelData === "Drop"     ? Theme.blue
// //                                                                                     : Theme.amber
// //                                         }
// //                                     }
// //                                 }

// //                                 Rectangle {
// //                                     visible: droneNeedsRc
// //                                     height:  15
// //                                     width:   rcBadgeTxt.implicitWidth + 10
// //                                     radius:  3
// //                                     color:   "#F0EAF8"
// //                                     Text {
// //                                         id:             rcBadgeTxt
// //                                         anchors.centerIn: parent
// //                                         text:           "CH" + droneRcSwitch + " ×" + droneRcCount
// //                                         font.family:    Theme.fontFamily
// //                                         font.pixelSize: 9
// //                                         font.weight:    Font.Medium
// //                                         color:          "#6B3FA0"
// //                                     }
// //                                 }
// //                             }
// //                         }

// //                         // Range / speed stats
// //                         Column {
// //                             spacing: 1
// //                             Layout.minimumWidth: 54
// //                             Layout.alignment: Qt.AlignVCenter

// //                             Text {
// //                                 text:           (droneRangeKm * 2).toFixed(0) + " km"
// //                                 font.family:    Theme.fontFamily
// //                                 font.pixelSize: Theme.fontSm
// //                                 font.weight:    Font.Medium
// //                                 color:          Theme.textPrimary
// //                             }
// //                             Text {
// //                                 text:           droneSpeedMs.toFixed(1) + " m/s"
// //                                 font.family:    Theme.fontFamily
// //                                 font.pixelSize: 10
// //                                 color:          Theme.textMuted
// //                             }
// //                         }

// //                         FpButton {
// //                             text:           "Edit"
// //                             implicitWidth:  44
// //                             implicitHeight: 24
// //                             onClicked:      root.openEdit(index)
// //                         }

// //                         // Deactivate ✕
// //                         Rectangle {
// //                             width: 24; height: 24; radius: Theme.radius
// //                             visible:      droneActive
// //                             color:        xArea.containsMouse ? Theme.redLight  : "transparent"
// //                             border.color: xArea.containsMouse ? "#F7C1C1"       : Theme.border
// //                             border.width: 0.5
// //                             Text { anchors.centerIn: parent; text: "✕"; color: Theme.red; font.pixelSize: 11 }
// //                             MouseArea {
// //                                 id: xArea
// //                                 anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
// //                                 onClicked: { droneManager.deactivate(index); root.fleetChanged() }
// //                             }
// //                         }

// //                         // Reactivate +
// //                         Rectangle {
// //                             width: 24; height: 24; radius: Theme.radius
// //                             visible:      !droneActive
// //                             color:        plusArea.containsMouse ? Theme.greenLight : "transparent"
// //                             border.color: plusArea.containsMouse ? "#C0DD97"        : Theme.border
// //                             border.width: 0.5
// //                             Text { anchors.centerIn: parent; text: "+"; color: Theme.green; font.pixelSize: 14 }
// //                             MouseArea {
// //                                 id: plusArea
// //                                 anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
// //                                 onClicked: { droneManager.setActive(index, true); root.fleetChanged() }
// //                             }
// //                         }
// //                     }
// //                 }
// //             }
// //         }

// //         // ── Add / Edit form ───────────────────────────────────────────────────
// //         Rectangle {
// //             id:               formRect
// //             Layout.fillWidth: true
// //             Layout.leftMargin:  12
// //             Layout.rightMargin: 12
// //             Layout.bottomMargin: visible ? 12 : 0
// //             visible:          root.showForm
// //             height:           visible ? formCol.implicitHeight + 28 : 0
// //             clip:             true
// //             radius:           Theme.radius
// //             color:            Theme.bg
// //             border.color:     Theme.borderFocus
// //             border.width:     1

// //             Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

// //             ColumnLayout {
// //                 id:      formCol
// //                 anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
// //                 spacing: 12

// //                 Text {
// //                     text:           root.editMode ? "Edit Drone" : "Add New Drone"
// //                     font.family:    Theme.fontFamily
// //                     font.pixelSize: Theme.fontBase
// //                     font.weight:    Font.Medium
// //                     color:          Theme.textPrimary
// //                 }

// //                 // Name
// //                 ColumnLayout {
// //                     Layout.fillWidth: true
// //                     spacing: 4
// //                     Text { text: "Name"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// //                     FpInput {
// //                         id:               efName
// //                         Layout.fillWidth: true
// //                         placeholderText:  "e.g. Eagle-X"
// //                     }
// //                 }

// //                 // Range + Speed
// //                 RowLayout {
// //                     Layout.fillWidth: true
// //                     spacing: 12

// //                     ColumnLayout {
// //                         Layout.fillWidth: true; spacing: 4
// //                         Text { text: "One-way range (km)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// //                         FpSpinBox { id: efRange; label: ""; unit: "km"; value: 5; from: 1; to: 500; stepSize: 1; Layout.fillWidth: true }
// //                     }

// //                     ColumnLayout {
// //                         Layout.fillWidth: true; spacing: 4
// //                         Text { text: "Max speed (m/s)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// //                         FpSpinBox { id: efSpeed; label: ""; unit: "m/s"; value: 50; from: 5; to: 200; stepSize: 5; Layout.fillWidth: true }
// //                     }
// //                 }

// //                 // Mission Capabilities
// //                 ColumnLayout {
// //                     spacing: 8
// //                     Text { text: "Mission Capabilities"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// //                     RowLayout {
// //                         spacing: 24

// //                         Repeater {
// //                             model: [
// //                                 { label: "Delivery", get: function() { return root.capDelivery }, set: function(v) { root.capDelivery = v } },
// //                                 { label: "Drop",     get: function() { return root.capDrop },     set: function(v) { root.capDrop     = v } },
// //                                 { label: "Paradrop", get: function() { return root.capParadrop }, set: function(v) { root.capParadrop = v } }
// //                             ]
// //                             delegate: RowLayout {
// //                                 spacing: 8
// //                                 // Watch all three so binding refreshes on any change
// //                                 property bool checked: index === 0 ? root.capDelivery
// //                                                      : index === 1 ? root.capDrop
// //                                                                    : root.capParadrop
// //                                 Rectangle {
// //                                     width: 18; height: 18; radius: 4
// //                                     color:        checked ? Theme.blueMid : Theme.bgInput
// //                                     border.color: checked ? Theme.blueMid : Theme.border
// //                                     border.width: 0.5
// //                                     Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: checked }
// //                                     MouseArea {
// //                                         anchors.fill: parent; cursorShape: Qt.PointingHandCursor
// //                                         onClicked: {
// //                                             if      (index === 0) root.capDelivery = !root.capDelivery
// //                                             else if (index === 1) root.capDrop     = !root.capDrop
// //                                             else                  root.capParadrop = !root.capParadrop
// //                                         }
// //                                     }
// //                                 }
// //                                 Text { text: modelData.label; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
// //                             }
// //                         }
// //                     }
// //                 }

// //                 // RC settings
// //                 ColumnLayout {
// //                     visible: root.capDrop || root.capParadrop
// //                     spacing: 8
// //                     Text { text: "RC Trigger Settings"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

// //                     RowLayout {
// //                         spacing: 12

// //                         ColumnLayout {
// //                             spacing: 4
// //                             Text { text: "RC Channel"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// //                             FpSpinBox { id: efRcCh; label: ""; unit: ""; value: root.rcChannel; from: 1; to: 16; stepSize: 1; implicitWidth: 100; onValueChanged: root.rcChannel = value }
// //                         }

// //                         ColumnLayout {
// //                             spacing: 4
// //                             Text { text: "Trigger Count"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// //                             FpSpinBox { id: efRcCount; label: ""; unit: "×"; value: root.rcTriggers; from: 1; to: 10; stepSize: 1; implicitWidth: 100; onValueChanged: root.rcTriggers = value }
// //                         }

// //                         Text {
// //                             text:             "Channel " + root.rcChannel + ", pulsed " + root.rcTriggers + "× at destination"
// //                             font.family:      Theme.fontFamily
// //                             font.pixelSize:   Theme.fontSm
// //                             color:            Theme.textMuted
// //                             Layout.alignment: Qt.AlignBottom
// //                             wrapMode:         Text.WordWrap
// //                             Layout.fillWidth: true
// //                         }
// //                     }
// //                 }

// //                 // Default Altitudes
// //                 ColumnLayout {
// //                     spacing: 8
// //                     Text { text: "Default Altitudes"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
// //                     RowLayout {
// //                         Layout.fillWidth: true; spacing: 12
// //                         FpSpinBox { id: efTakeoff;  label: "Takeoff";   unit: "m"; value: 30;  from: 10; to: 300; stepSize: 5;  Layout.fillWidth: true }
// //                         FpSpinBox { id: efNearHome; label: "Near-home"; unit: "m"; value: 60;  from: 20; to: 400; stepSize: 5;  Layout.fillWidth: true }
// //                         FpSpinBox { id: efCruise;   label: "Clearance"; unit: "m"; value: 120; from: 80; to: 800; stepSize: 10; Layout.fillWidth: true }
// //                         FpSpinBox { id: efLand;     label: "Land";      unit: "m"; value: 5;   from: 0;  to: 50;  stepSize: 1;  Layout.fillWidth: true }
// //                     }
// //                 }

// //                 // Validation
// //                 Text {
// //                     id: validationMsg; visible: false
// //                     text: "Please enter a drone name"
// //                     color: Theme.red; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
// //                 }

// //                 // Buttons
// //                 RowLayout {
// //                     spacing: 8
// //                     FpButton {
// //                         text:          root.editMode ? "Update Drone" : "Add Drone"
// //                         primary:       true
// //                         implicitWidth: 110
// //                         onClicked:     root.saveForm()
// //                     }
// //                     FpButton {
// //                         text:          "Cancel"
// //                         implicitWidth: 70
// //                         onClicked:     { root.showForm = false; validationMsg.visible = false }
// //                     }
// //                 }

// //                 Item { implicitHeight: 2 }
// //             }
// //         }

// //         Item { implicitHeight: 12 }
// //     }
// // }







// import QtQuick 6.0
// import QtQuick.Controls 6.0
// import QtQuick.Layouts 6.0

// // Drop directly into settingsCol in MainView.qml — do NOT wrap in SectionBlock.
// //
// //   DroneFleetSection {
// //       Layout.fillWidth: true
// //       onFleetChanged: root.refreshDroneCombo()
// //   }

// Item {
//     id: root
//     implicitWidth:  mainCol.implicitWidth
//     implicitHeight: mainCol.implicitHeight

//     signal fleetChanged()

//     // ── Form state ────────────────────────────────────────────────────────────
//     property bool showForm:    false
//     property bool editMode:    false
//     property int  editIndex:   -1

//     property bool capDelivery: true
//     property bool capDrop:     false
//     property bool capParadrop: false
//     property int  rcChannel:   1
//     property int  rcTriggers:  1

//     // ── Helpers ───────────────────────────────────────────────────────────────
//     function openAdd() {
//         editMode         = false
//         editIndex        = -1
//         efName.text      = ""
//         efRange.value    = 5
//         efSpeed.value    = 50
//         capDelivery      = true
//         capDrop          = false
//         capParadrop      = false
//         rcChannel        = 1
//         rcTriggers       = 1
//         efTakeoff.value  = 30
//         efNearHome.value = 60
//         efCruise.value   = 120
//         efLand.value     = 5
//         showForm         = true
//         Qt.callLater(function() { root.ensureFormVisible() })
//     }

//     function openEdit(idx) {
//         var d            = droneManager.droneAt(idx)
//         editMode         = true
//         editIndex        = idx
//         efName.text      = d.name
//         efRange.value    = Math.round(d.rangeKm)
//         efSpeed.value    = Math.round(d.speedMs * 10)
//         capDelivery      = d.delivery
//         capDrop          = d.drop
//         capParadrop      = d.paradrop
//         rcChannel        = d.rcSwitch > 0 ? d.rcSwitch : 1
//         rcTriggers       = d.rcCount  > 0 ? d.rcCount  : 1
//         efTakeoff.value  = Math.round(d.takeoffAlt)
//         efNearHome.value = Math.round(d.nearHomeAlt)
//         efCruise.value   = Math.round(d.cruiseAlt)
//         efLand.value     = Math.round(d.landAlt)
//         showForm         = true
//         Qt.callLater(function() { root.ensureFormVisible() })
//     }

//     function ensureFormVisible() {
//         var flick = null
//         var p = root.parent
//         while (p) {
//             if (p.hasOwnProperty("contentY") && p.hasOwnProperty("contentHeight")) {
//                 flick = p; break
//             }
//             p = p.parent
//         }
//         if (!flick) return
//         var formY = formCard.mapToItem(flick.contentItem, 0, 0).y
//         var formH = formCard.height
//         var viewH = flick.height
//         if (formY + formH > flick.contentY + viewH)
//             flick.contentY = Math.min(formY + formH - viewH + 20, flick.contentHeight - viewH)
//     }

//     function saveForm() {
//         var name = efName.text.trim()
//         if (name === "") { validationMsg.visible = true; return }
//         validationMsg.visible = false
//         var needsRc = capDrop || capParadrop
//         var rc      = needsRc ? rcChannel  : 0
//         var rcCnt   = needsRc ? rcTriggers : 0
//         if (editMode) {
//             droneManager.updateDrone(editIndex, name,
//                 efRange.value, efSpeed.value / 10.0,
//                 capDelivery, capDrop, capParadrop, rc, rcCnt,
//                 efTakeoff.value, efNearHome.value, efCruise.value, efLand.value)
//         } else {
//             droneManager.addDrone(name,
//                 efRange.value, efSpeed.value / 10.0,
//                 capDelivery, capDrop, capParadrop, rc, rcCnt,
//                 efTakeoff.value, efNearHome.value, efCruise.value, efLand.value)
//         }
//         showForm = false
//         root.fleetChanged()
//     }

//     // ── Outer card ────────────────────────────────────────────────────────────
//     Rectangle {
//         anchors.fill: parent
//         radius:       Theme.radiusLg
//         color:        Theme.bgSection
//         border.color: Theme.border
//         border.width: 0.5
//     }

//     ColumnLayout {
//         id:      mainCol
//         anchors { left: parent.left; right: parent.right; top: parent.top }
//         spacing: 0

//         // ── Section title bar ─────────────────────────────────────────────────
//         Item {
//             Layout.fillWidth: true
//             implicitHeight:   titleRow.implicitHeight + 20

//             RowLayout {
//                 id: titleRow
//                 anchors {
//                     left: parent.left; right: parent.right
//                     verticalCenter: parent.verticalCenter
//                     leftMargin: 16; rightMargin: 16
//                 }
//                 spacing: 8

//                 Text {
//                     text:               "DRONE FLEET"
//                     font.family:        Theme.fontFamily
//                     font.pixelSize:     Theme.fontXs
//                     font.weight:        Font.Medium
//                     font.letterSpacing: 0.8
//                     color:              Theme.textMuted
//                     Layout.fillWidth:   true
//                 }

//                 Rectangle {
//                     height: 18; width: countTxt.implicitWidth + 12; radius: 9
//                     color:   Theme.blueLight
//                     visible: droneManager.count > 0
//                     Text {
//                         id: countTxt; anchors.centerIn: parent
//                         text:           droneManager.count + " drone" + (droneManager.count === 1 ? "" : "s")
//                         font.family:    Theme.fontFamily
//                         font.pixelSize: Theme.fontXs
//                         font.weight:    Font.Medium
//                         color:          Theme.blue
//                     }
//                 }

//                 FpButton {
//                     text: "+ Add Drone"; implicitWidth: 96; implicitHeight: 26
//                     onClicked: root.openAdd()
//                 }
//             }

//             Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 0.5; color: Theme.border }
//         }

//         // ── Drone list ────────────────────────────────────────────────────────
//         ColumnLayout {
//             Layout.fillWidth:    true
//             Layout.topMargin:    8
//             Layout.bottomMargin: 4
//             Layout.leftMargin:   12
//             Layout.rightMargin:  12
//             spacing: 4

//             Repeater {
//                 model: droneManager
//                 delegate: Rectangle {
//                     Layout.fillWidth: true
//                     height:       rowContent.implicitHeight + 14
//                     radius:       Theme.radius
//                     color:        droneActive ? Theme.bg : "#F8F7F4"
//                     border.color: droneActive ? Theme.border : Theme.borderLight
//                     border.width: 0.5
//                     opacity:      droneActive ? 1.0 : 0.6

//                     RowLayout {
//                         id: rowContent
//                         anchors {
//                             left: parent.left; right: parent.right
//                             verticalCenter: parent.verticalCenter
//                             leftMargin: 10; rightMargin: 10
//                         }
//                         spacing: 8

//                         Rectangle {
//                             width: 8; height: 8; radius: 4
//                             color: droneActive ? Theme.green : Theme.textHint
//                             Layout.alignment: Qt.AlignVCenter
//                             MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: droneManager.setActive(index, !droneActive) }
//                         }

//                         ColumnLayout {
//                             Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter; spacing: 3

//                             Text {
//                                 text: droneName; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase
//                                 font.weight: Font.Medium; color: Theme.textPrimary; elide: Text.ElideRight
//                                 Layout.fillWidth: true
//                             }

//                             Flow {
//                                 Layout.fillWidth: true; spacing: 4
//                                 Repeater {
//                                     model: droneCapabilities
//                                     delegate: Rectangle {
//                                         height: 15; width: badgeTxt.implicitWidth + 10; radius: 3
//                                         color: modelData === "Delivery" ? Theme.greenLight : modelData === "Drop" ? Theme.blueLight : Theme.amberLight
//                                         Text { id: badgeTxt; anchors.centerIn: parent; text: modelData; font.family: Theme.fontFamily; font.pixelSize: 9; font.weight: Font.Medium; color: modelData === "Delivery" ? Theme.green : modelData === "Drop" ? Theme.blue : Theme.amber }
//                                     }
//                                 }
//                                 Rectangle {
//                                     visible: droneNeedsRc; height: 15; width: rcBadgeTxt.implicitWidth + 10; radius: 3; color: "#F0EAF8"
//                                     Text { id: rcBadgeTxt; anchors.centerIn: parent; text: "CH" + droneRcSwitch + " ×" + droneRcCount; font.family: Theme.fontFamily; font.pixelSize: 9; font.weight: Font.Medium; color: "#6B3FA0" }
//                                 }
//                             }
//                         }

//                         Column {
//                             spacing: 1; Layout.minimumWidth: 54; Layout.alignment: Qt.AlignVCenter
//                             Text { text: (droneRangeKm * 2).toFixed(0) + " km"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Medium; color: Theme.textPrimary }
//                             Text { text: droneSpeedMs.toFixed(1) + " m/s";       font.family: Theme.fontFamily; font.pixelSize: 10; color: Theme.textMuted }
//                         }

//                         FpButton { text: "Edit"; implicitWidth: 44; implicitHeight: 24; onClicked: root.openEdit(index) }

//                         Rectangle {
//                             width: 24; height: 24; radius: Theme.radius; visible: droneActive
//                             color: xArea.containsMouse ? Theme.redLight : "transparent"; border.color: xArea.containsMouse ? "#F7C1C1" : Theme.border; border.width: 0.5
//                             Text { anchors.centerIn: parent; text: "✕"; color: Theme.red; font.pixelSize: 11 }
//                             MouseArea { id: xArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { droneManager.deactivate(index); root.fleetChanged() } }
//                         }
//                         Rectangle {
//                             width: 24; height: 24; radius: Theme.radius; visible: !droneActive
//                             color: plusArea.containsMouse ? Theme.greenLight : "transparent"; border.color: plusArea.containsMouse ? "#C0DD97" : Theme.border; border.width: 0.5
//                             Text { anchors.centerIn: parent; text: "+"; color: Theme.green; font.pixelSize: 14 }
//                             MouseArea { id: plusArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { droneManager.setActive(index, true); root.fleetChanged() } }
//                         }
//                     }
//                 }
//             }
//         }

//         // ── Add / Edit form — styled card ─────────────────────────────────────
//         Item {
//             id:               formCard
//             Layout.fillWidth: true
//             Layout.leftMargin:   12
//             Layout.rightMargin:  12
//             Layout.bottomMargin: root.showForm ? 12 : 0
//             visible:          root.showForm
//             // Animate height open/closed; formCardRect drives the true size
//             height:           root.showForm ? formCardRect.implicitHeight : 0
//             clip:             true

//             Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

//             Rectangle {
//                 id:           formCardRect
//                 anchors { left: parent.left; right: parent.right; top: parent.top }
//                 implicitHeight: formHeader.height + formBody.implicitHeight + 32
//                 radius:       Theme.radius
//                 color:        Theme.bg
//                 border.color: Theme.borderFocus
//                 border.width: 1

//                 // ── Form header bar ───────────────────────────────────────────
//                 Rectangle {
//                     id:     formHeader
//                     anchors { left: parent.left; right: parent.right; top: parent.top }
//                     height: 40
//                     color:  Theme.bgSection
//                     radius: Theme.radius

//                     // Square off bottom corners so it sits flush on the body
//                     Rectangle {
//                         anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
//                         height: Theme.radius; color: parent.color
//                     }

//                     RowLayout {
//                         anchors {
//                             left: parent.left; right: parent.right
//                             verticalCenter: parent.verticalCenter
//                             leftMargin: 14; rightMargin: 14
//                         }
//                         spacing: 8

//                         // Amber = edit, green = add
//                         Rectangle {
//                             width: 8; height: 8; radius: 4
//                             color: root.editMode ? Theme.amber : Theme.green
//                             Layout.alignment: Qt.AlignVCenter
//                         }

//                         Text {
//                             text:             root.editMode ? "Edit Drone" : "Add New Drone"
//                             font.family:      Theme.fontFamily
//                             font.pixelSize:   Theme.fontBase
//                             font.weight:      Font.Medium
//                             color:            Theme.textPrimary
//                             Layout.fillWidth: true
//                         }

//                         Rectangle {
//                             width: 22; height: 22; radius: Theme.radius
//                             color:        closeHover.containsMouse ? Theme.redLight  : "transparent"
//                             border.color: closeHover.containsMouse ? "#F7C1C1"       : Theme.borderLight
//                             border.width: 0.5
//                             Text { anchors.centerIn: parent; text: "✕"; color: Theme.red; font.pixelSize: 10 }
//                             MouseArea { id: closeHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { root.showForm = false; validationMsg.visible = false } }
//                         }
//                     }

//                     Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 0.5; color: Theme.border }
//                 }

//                 // ── Form body ─────────────────────────────────────────────────
//                 ColumnLayout {
//                     id:      formBody
//                     anchors {
//                         left: parent.left; right: parent.right; top: formHeader.bottom
//                         leftMargin: 16; rightMargin: 16; topMargin: 16
//                     }
//                     spacing: 16

//                     // Name
//                     ColumnLayout {
//                         Layout.fillWidth: true; spacing: 6
//                         Text { text: "Name"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Medium; color: Theme.textMuted }
//                         FpInput { id: efName; Layout.fillWidth: true; placeholderText: "e.g. Eagle-X" }
//                     }

//                     // Range + Speed
//                     RowLayout {
//                         Layout.fillWidth: true; spacing: 12

//                         ColumnLayout {
//                             Layout.fillWidth: true; spacing: 6
//                             Text { text: "One-way range"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Medium; color: Theme.textMuted }
//                             FpSpinBox { id: efRange; label: ""; unit: "km"; value: 5; from: 1; to: 500; stepSize: 1; Layout.fillWidth: true }
//                         }
//                         ColumnLayout {
//                             Layout.fillWidth: true; spacing: 6
//                             Text { text: "Max speed"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Medium; color: Theme.textMuted }
//                             FpSpinBox { id: efSpeed; label: ""; unit: "m/s"; value: 50; from: 5; to: 200; stepSize: 5; Layout.fillWidth: true }
//                         }
//                     }

//                     // Mission Capabilities — coloured tile toggles
//                     ColumnLayout {
//                         Layout.fillWidth: true; spacing: 10

//                         Text { text: "Mission Capabilities"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Medium; color: Theme.textMuted }

//                         RowLayout {
//                             Layout.fillWidth: true; spacing: 8

//                             // Delivery tile
//                             Rectangle {
//                                 Layout.fillWidth: true
//                                 height: capRowDel.implicitHeight + 16; radius: Theme.radius
//                                 color:        root.capDelivery ? Theme.greenLight  : Theme.bgSection
//                                 border.color: root.capDelivery ? Theme.green       : Theme.border
//                                 border.width: root.capDelivery ? 1 : 0.5
//                                 Behavior on color { ColorAnimation { duration: 120 } }
//                                 RowLayout { id: capRowDel; anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 10; rightMargin: 8 } spacing: 8
//                                     Rectangle { width: 16; height: 16; radius: 3; color: root.capDelivery ? Theme.green : Theme.bgInput; border.color: root.capDelivery ? "transparent" : Theme.border; border.width: 0.5
//                                         Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 10; visible: root.capDelivery } }
//                                     Text { text: "Delivery"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Medium; color: root.capDelivery ? Theme.green : Theme.textMuted; Layout.fillWidth: true }
//                                 }
//                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDelivery = !root.capDelivery }
//                             }

//                             // Drop tile
//                             Rectangle {
//                                 Layout.fillWidth: true
//                                 height: capRowDrop.implicitHeight + 16; radius: Theme.radius
//                                 color:        root.capDrop ? Theme.blueLight  : Theme.bgSection
//                                 border.color: root.capDrop ? Theme.blue       : Theme.border
//                                 border.width: root.capDrop ? 1 : 0.5
//                                 Behavior on color { ColorAnimation { duration: 120 } }
//                                 RowLayout { id: capRowDrop; anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 10; rightMargin: 8 } spacing: 8
//                                     Rectangle { width: 16; height: 16; radius: 3; color: root.capDrop ? Theme.blue : Theme.bgInput; border.color: root.capDrop ? "transparent" : Theme.border; border.width: 0.5
//                                         Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 10; visible: root.capDrop } }
//                                     Text { text: "Drop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Medium; color: root.capDrop ? Theme.blue : Theme.textMuted; Layout.fillWidth: true }
//                                 }
//                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDrop = !root.capDrop }
//                             }

//                             // Paradrop tile
//                             Rectangle {
//                                 Layout.fillWidth: true
//                                 height: capRowPara.implicitHeight + 16; radius: Theme.radius
//                                 color:        root.capParadrop ? Theme.amberLight : Theme.bgSection
//                                 border.color: root.capParadrop ? Theme.amber      : Theme.border
//                                 border.width: root.capParadrop ? 1 : 0.5
//                                 Behavior on color { ColorAnimation { duration: 120 } }
//                                 RowLayout { id: capRowPara; anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 10; rightMargin: 8 } spacing: 8
//                                     Rectangle { width: 16; height: 16; radius: 3; color: root.capParadrop ? Theme.amber : Theme.bgInput; border.color: root.capParadrop ? "transparent" : Theme.border; border.width: 0.5
//                                         Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 10; visible: root.capParadrop } }
//                                     Text { text: "Paradrop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Medium; color: root.capParadrop ? Theme.amber : Theme.textMuted; Layout.fillWidth: true }
//                                 }
//                                 MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capParadrop = !root.capParadrop }
//                             }
//                         }
//                     }

//                     // RC Trigger Settings — purple tinted card, only for Drop/Paradrop
//                     Rectangle {
//                         Layout.fillWidth: true
//                         visible:          root.capDrop || root.capParadrop
//                         implicitHeight:   rcInner.implicitHeight + 24
//                         radius:           Theme.radius
//                         color:            "#F5F0FB"
//                         border.color:     "#D4C2EE"
//                         border.width:     0.5

//                         ColumnLayout {
//                             id:      rcInner
//                             anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
//                             spacing: 10

//                             RowLayout {
//                                 spacing: 6
//                                 Rectangle { width: 8; height: 8; radius: 4; color: "#6B3FA0" }
//                                 Text { text: "RC Trigger Settings"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Medium; color: "#6B3FA0" }
//                             }

//                             RowLayout {
//                                 Layout.fillWidth: true; spacing: 12

//                                 ColumnLayout {
//                                     spacing: 4
//                                     Text { text: "RC Channel"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
//                                     FpSpinBox { id: efRcCh; label: ""; unit: ""; value: root.rcChannel; from: 1; to: 16; stepSize: 1; implicitWidth: 110; onValueChanged: root.rcChannel = value }
//                                 }

//                                 ColumnLayout {
//                                     spacing: 4
//                                     Text { text: "Trigger count"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
//                                     FpSpinBox { id: efRcCount; label: ""; unit: "×"; value: root.rcTriggers; from: 1; to: 10; stepSize: 1; implicitWidth: 110; onValueChanged: root.rcTriggers = value }
//                                 }

//                                 Text {
//                                     text:             "CH" + root.rcChannel + " pulsed " + root.rcTriggers + "× at destination"
//                                     font.family:      Theme.fontFamily; font.pixelSize: Theme.fontSm; color: "#6B3FA0"
//                                     wrapMode:         Text.WordWrap; Layout.fillWidth: true; Layout.alignment: Qt.AlignBottom
//                                 }
//                             }
//                         }
//                     }

//                     // Default Altitudes — four spinboxes in a row with centred labels
//                     ColumnLayout {
//                         Layout.fillWidth: true; spacing: 10

//                         Text { text: "Default Altitudes"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Medium; color: Theme.textMuted }

//                         RowLayout {
//                             Layout.fillWidth: true; spacing: 8

//                             ColumnLayout {
//                                 Layout.fillWidth: true; spacing: 4
//                                 Text { text: "Takeoff"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
//                                 FpSpinBox { id: efTakeoff; label: ""; unit: "m"; value: 30; from: 10; to: 300; stepSize: 5; Layout.fillWidth: true }
//                             }
//                             ColumnLayout {
//                                 Layout.fillWidth: true; spacing: 4
//                                 Text { text: "Near-home"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
//                                 FpSpinBox { id: efNearHome; label: ""; unit: "m"; value: 60; from: 20; to: 400; stepSize: 5; Layout.fillWidth: true }
//                             }
//                             ColumnLayout {
//                                 Layout.fillWidth: true; spacing: 4
//                                 Text { text: "Clearance"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
//                                 FpSpinBox { id: efCruise; label: ""; unit: "m"; value: 120; from: 80; to: 800; stepSize: 10; Layout.fillWidth: true }
//                             }
//                             ColumnLayout {
//                                 Layout.fillWidth: true; spacing: 4
//                                 Text { text: "Land"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
//                                 FpSpinBox { id: efLand; label: ""; unit: "m"; value: 5; from: 0; to: 50; stepSize: 1; Layout.fillWidth: true }
//                             }
//                         }
//                     }

//                     // Validation banner
//                     Rectangle {
//                         Layout.fillWidth: true
//                         visible:          validationMsg.visible
//                         height:           validationMsg.visible ? 34 : 0
//                         radius:           Theme.radius
//                         color:            Theme.redLight
//                         border.color:     Theme.red; border.width: 0.5
//                         Text {
//                             id:               validationMsg
//                             anchors.centerIn: parent
//                             visible:          false
//                             text:             "⚠  Please enter a drone name"
//                             color:            Theme.red; font.family: Theme.fontFamily
//                             font.pixelSize:   Theme.fontSm; font.weight: Font.Medium
//                         }
//                     }

//                     // Action buttons
//                     RowLayout {
//                         Layout.fillWidth: true; spacing: 8

//                         FpButton {
//                             text:          root.editMode ? "Update Drone" : "Add Drone"
//                             primary:       true; implicitWidth: 120
//                             onClicked:     root.saveForm()
//                         }
//                         FpButton {
//                             text: "Cancel"; implicitWidth: 80
//                             onClicked: { root.showForm = false; validationMsg.visible = false }
//                         }
//                         Item { Layout.fillWidth: true }
//                     }

//                     Item { implicitHeight: 4 }
//                 }
//             }
//         }

//         Item { implicitHeight: 12 }
//     }
// }


import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0

Item {
    id: root
    implicitHeight: mainCol.implicitHeight

    signal fleetChanged()

    // ── Form state (root-scope so openAdd/openEdit can set before form renders) ─
    property bool showForm:    false
    property bool editMode:    false
    property int  editIndex:   -1

    property bool capDelivery: true
    property bool capDrop:     false
    property bool capParadrop: false
    property int  rcChannel:   1
    property int  rcTriggers:  1

    // ── Functions ─────────────────────────────────────────────────────────────
    function openAdd() {
        editMode         = false
        editIndex        = -1
        efName.text      = ""
        efRange.value    = 5
        efSpeed.value    = 50
        capDelivery      = true
        capDrop          = false
        capParadrop      = false
        rcChannel        = 1
        rcTriggers       = 1
        efTakeoff.value  = 30
        efNearHome.value = 60
        efCruise.value   = 120
        efLand.value     = 5
        showForm         = true
        Qt.callLater(function() { root.ensureFormVisible() })
    }

    function openEdit(idx) {
        var d            = droneManager.droneAt(idx)
        editMode         = true
        editIndex        = idx
        efName.text      = d.name
        efRange.value    = Math.round(d.rangeKm)
        efSpeed.value    = Math.round(d.speedMs * 10)
        capDelivery      = d.delivery
        capDrop          = d.drop
        capParadrop      = d.paradrop
        rcChannel        = d.rcSwitch > 0 ? d.rcSwitch : 1
        rcTriggers       = d.rcCount  > 0 ? d.rcCount  : 1
        efTakeoff.value  = Math.round(d.takeoffAlt)
        efNearHome.value = Math.round(d.nearHomeAlt)
        efCruise.value   = Math.round(d.cruiseAlt)
        efLand.value     = Math.round(d.landAlt)
        showForm         = true
        Qt.callLater(function() { root.ensureFormVisible() })
    }

    // Scroll the parent Flickable/ScrollView to reveal the edit form
    function ensureFormVisible() {
        var settingsFlick = null
        // Walk up parent chain to find a Flickable
        var p = root.parent
        while (p) {
            if (p.hasOwnProperty("contentY") && p.hasOwnProperty("contentHeight")) {
                settingsFlick = p
                break
            }
            p = p.parent
        }
        if (!settingsFlick) return
        // Map edit form position to the flickable's content coordinates
        var formY = formRect.mapToItem(settingsFlick.contentItem, 0, 0).y
        var formH = formRect.height
        var viewH = settingsFlick.height
        if (formY + formH > settingsFlick.contentY + viewH) {
            settingsFlick.contentY = Math.min(
                formY + formH - viewH + 20,
                settingsFlick.contentHeight - viewH
            )
        }
    }

    function saveForm() {
        var name = efName.text.trim()
        if (name === "") { validationMsg.visible = true; return }
        validationMsg.visible = false

        var needsRc = capDrop || capParadrop
        var rc      = needsRc ? rcChannel  : 0
        var rcCnt   = needsRc ? rcTriggers : 0

        if (editMode) {
            droneManager.updateDrone(
                editIndex, name,
                efRange.value, efSpeed.value / 10.0,
                capDelivery, capDrop, capParadrop,
                rc, rcCnt,
                efTakeoff.value, efNearHome.value,
                efCruise.value,  efLand.value
            )
        } else {
            droneManager.addDrone(
                name,
                efRange.value, efSpeed.value / 10.0,
                capDelivery, capDrop, capParadrop,
                rc, rcCnt,
                efTakeoff.value, efNearHome.value,
                efCruise.value,  efLand.value
            )
        }
        showForm = false
        root.fleetChanged()
    }

    // ── Main column ───────────────────────────────────────────────────────────
    ColumnLayout {
        id: mainCol
        anchors { left: parent.left; right: parent.right }
        spacing: 0

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text:           "Drone Fleet  (" + droneManager.count + " total)"
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontBase
                color:          Theme.textSecond
                Layout.fillWidth: true
            }

            FpButton {
                text:           "Reset Defaults"
                implicitWidth:  110
                implicitHeight: 26
                onClicked: {
                    droneManager.resetToDefaults()
                    root.fleetChanged()
                }
            }

            FpButton {
                text:           "+ Add Drone"
                implicitWidth:  96
                implicitHeight: 26
                onClicked:      root.openAdd()
            }
        }

        Item { implicitHeight: 8 }

        // ── Drone list — no height cap, parent ScrollView scrolls ──────────
        Column {
            id:               listCol
            Layout.fillWidth: true
            spacing:          4

            Repeater {
                        model: droneManager

                        delegate: Rectangle {
                            width:        parent.width
                            height:       rowContent.implicitHeight + 14
                            radius:       Theme.radius
                            color:        droneActive ? Theme.bgSection : "#F8F7F4"
                            border.color: droneActive ? Theme.border    : Theme.borderLight
                            border.width: 0.5
                            opacity:      droneActive ? 1.0 : 0.6

                            RowLayout {
                                id: rowContent
                                anchors {
                                    left: parent.left; right: parent.right
                                    verticalCenter: parent.verticalCenter
                                    leftMargin: 10; rightMargin: 10
                                }
                                spacing: 8

                                // Active dot
                                Rectangle {
                                    width: 8; height: 8; radius: 4
                                    color: droneActive ? Theme.green : Theme.textHint
                                    Layout.alignment: Qt.AlignVCenter
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape:  Qt.PointingHandCursor
                                        onClicked:    droneManager.setActive(index, !droneActive)
                                    }
                                }

                                // Name + capability badges
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: 3

                                    Text {
                                        text:           droneName
                                        font.family:    Theme.fontFamily
                                        font.pixelSize: Theme.fontBase
                                        font.weight:    Font.Medium
                                        color:          Theme.textPrimary
                                    }

                                    Row {
                                        spacing: 5

                                        Repeater {
                                            model: droneCapabilities
                                            delegate: Rectangle {
                                                height: 15
                                                width:  badgeTxt.implicitWidth + 10
                                                radius: 3
                                                color:  modelData === "Delivery" ? Theme.greenLight
                                                      : modelData === "Drop"     ? Theme.blueLight
                                                                                 : Theme.amberLight
                                                Text {
                                                    id:             badgeTxt
                                                    anchors.centerIn: parent
                                                    text:           modelData
                                                    font.family:    Theme.fontFamily
                                                    font.pixelSize: 9
                                                    font.weight:    Font.Medium
                                                    color:          modelData === "Delivery" ? Theme.green
                                                                  : modelData === "Drop"     ? Theme.blue
                                                                                            : Theme.amber
                                                }
                                            }
                                        }

                                        // RC channel badge
                                        Rectangle {
                                            visible:      droneNeedsRc
                                            height:       15
                                            width:        rcBadgeTxt.implicitWidth + 10
                                            radius:       3
                                            color:        "#F0EAF8"
                                            Text {
                                                id:             rcBadgeTxt
                                                anchors.centerIn: parent
                                                text:           "CH" + droneRcSwitch + " ×" + droneRcCount
                                                font.family:    Theme.fontFamily
                                                font.pixelSize: 9
                                                font.weight:    Font.Medium
                                                color:          "#6B3FA0"
                                            }
                                        }
                                    }
                                }

                                // Range / speed
                                Column {
                                    spacing: 1
                                    Text {
                                        text:           (droneRangeKm * 2).toFixed(0) + " km"
                                        font.family:    Theme.fontFamily
                                        font.pixelSize: Theme.fontSm
                                        font.weight:    Font.Medium
                                        color:          Theme.textPrimary
                                    }
                                    Text {
                                        text:           droneSpeedMs.toFixed(1) + " m/s"
                                        font.family:    Theme.fontFamily
                                        font.pixelSize: 10
                                        color:          Theme.textMuted
                                    }
                                }

                                // Edit
                                FpButton {
                                    text:           "Edit"
                                    implicitWidth:  44
                                    implicitHeight: 24
                                    onClicked:      root.openEdit(index)
                                }

                                // Deactivate
                                Rectangle {
                                    width: 24; height: 24; radius: Theme.radius
                                    visible:      droneActive
                                    color:        xArea.containsMouse ? Theme.redLight  : "transparent"
                                    border.color: xArea.containsMouse ? "#F7C1C1"       : Theme.border
                                    border.width: 0.5
                                    Text { anchors.centerIn: parent; text: "✕"; color: Theme.red; font.pixelSize: 11 }
                                    MouseArea {
                                        id: xArea
                                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: { droneManager.deactivate(index); root.fleetChanged() }
                                    }
                                }

                                // Reactivate
                                Rectangle {
                                    width: 24; height: 24; radius: Theme.radius
                                    visible:      !droneActive
                                    color:        plusArea.containsMouse ? Theme.greenLight : "transparent"
                                    border.color: plusArea.containsMouse ? "#C0DD97"        : Theme.border
                                    border.width: 0.5
                                    Text { anchors.centerIn: parent; text: "+"; color: Theme.green; font.pixelSize: 14 }
                                    MouseArea {
                                        id: plusArea
                                        anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                        onClicked: { droneManager.setActive(index, true); root.fleetChanged() }
                                    }
                                }
                            }
                        }
            }
        }

        Item { implicitHeight: 10 }

        // ── Add / Edit form ───────────────────────────────────────────────────
        Rectangle {
            id:           formRect
            Layout.fillWidth: true
            visible:      root.showForm
            height:       visible ? formCol.implicitHeight + 28 : 0
            clip:         true
            radius:       Theme.radiusLg
            color:        Theme.bgSection
            border.color: Theme.borderFocus
            border.width: 1

            Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

            ColumnLayout {
                id: formCol
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
                spacing: 12

                // Title
                Text {
                    text:           root.editMode ? "Edit Drone" : "Add New Drone"
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontBase
                    font.weight:    Font.Medium
                    color:          Theme.textPrimary
                }

                // ── Name ──────────────────────────────────────────────────────
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "Name"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                    FpInput {
                        id:              efName
                        Layout.fillWidth: true
                        placeholderText: "e.g. Eagle-X"
                    }
                }

                // ── Range + Speed ─────────────────────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: "One-way range (km)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        FpSpinBox {
                            id: efRange
                            label: ""; unit: "km"
                            value: 5; from: 1; to: 500; stepSize: 1
                            Layout.fillWidth: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: "Max speed (m/s)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        FpSpinBox {
                            id: efSpeed
                            label: ""; unit: "m/s"
                            value: 50; from: 5; to: 200; stepSize: 5
                            Layout.fillWidth: true
                        }
                    }
                }

                // ── Mission Capabilities (all three independent) ───────────────
                ColumnLayout {
                    spacing: 8

                    Text { text: "Mission Capabilities"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

                    RowLayout {
                        spacing: 24

                        // Delivery
                        RowLayout {
                            spacing: 8
                            Rectangle {
                                width: 18; height: 18; radius: 4
                                color:        root.capDelivery ? Theme.blueMid : Theme.bgInput
                                border.color: root.capDelivery ? Theme.blueMid : Theme.border
                                border.width: 0.5
                                Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capDelivery }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDelivery = !root.capDelivery }
                            }
                            Text { text: "Delivery"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
                        }

                        // Drop
                        RowLayout {
                            spacing: 8
                            Rectangle {
                                width: 18; height: 18; radius: 4
                                color:        root.capDrop ? Theme.blueMid : Theme.bgInput
                                border.color: root.capDrop ? Theme.blueMid : Theme.border
                                border.width: 0.5
                                Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capDrop }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capDrop = !root.capDrop }
                            }
                            Text { text: "Drop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
                        }

                        // Paradrop
                        RowLayout {
                            spacing: 8
                            Rectangle {
                                width: 18; height: 18; radius: 4
                                color:        root.capParadrop ? Theme.blueMid : Theme.bgInput
                                border.color: root.capParadrop ? Theme.blueMid : Theme.border
                                border.width: 0.5
                                Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: root.capParadrop }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.capParadrop = !root.capParadrop }
                            }
                            Text { text: "Paradrop"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
                        }
                    }
                }

                // ── RC settings — only when Drop or Paradrop ticked ───────────
                ColumnLayout {
                    visible:  root.capDrop || root.capParadrop
                    spacing:  8

                    Text { text: "RC Trigger Settings"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

                    RowLayout {
                        spacing: 12

                        ColumnLayout {
                            spacing: 4
                            Text { text: "RC Channel"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                            FpSpinBox {
                                id:             efRcCh
                                label:          ""
                                unit:           ""
                                value:          root.rcChannel
                                from: 1; to: 16; stepSize: 1
                                implicitWidth:  100
                                onValueChanged: root.rcChannel = value
                            }
                        }

                        ColumnLayout {
                            spacing: 4
                            Text { text: "Trigger Count"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                            FpSpinBox {
                                id:             efRcCount
                                label:          ""
                                unit:           "×"
                                value:          root.rcTriggers
                                from: 1; to: 10; stepSize: 1
                                implicitWidth:  100
                                onValueChanged: root.rcTriggers = value
                            }
                        }

                        Text {
                            text:           "Channel " + root.rcChannel + ", pulsed " + root.rcTriggers + "× at destination"
                            font.family:    Theme.fontFamily
                            font.pixelSize: Theme.fontSm
                            color:          Theme.textMuted
                            Layout.alignment: Qt.AlignBottom
                        }
                    }
                }

                // ── Default Altitudes ─────────────────────────────────────────
                ColumnLayout {
                    spacing: 8

                    Text { text: "Default Altitudes"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        FpSpinBox { id: efTakeoff;  label: "Takeoff";    unit: "m"; value: 30;  from: 10; to: 300; stepSize: 5;  Layout.fillWidth: true }
                        FpSpinBox { id: efNearHome; label: "Near-home";  unit: "m"; value: 60;  from: 20; to: 400; stepSize: 5;  Layout.fillWidth: true }
                        FpSpinBox { id: efCruise;   label: "Clearance";  unit: "m"; value: 120; from: 80; to: 800; stepSize: 10; Layout.fillWidth: true }
                        FpSpinBox { id: efLand;     label: "Land";       unit: "m"; value: 5;   from: 0;  to: 50;  stepSize: 1;  Layout.fillWidth: true }
                    }
                }

                // ── Validation ────────────────────────────────────────────────
                Text {
                    id:             validationMsg
                    visible:        false
                    text:           "Please enter a drone name"
                    color:          Theme.red
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSm
                }

                // ── Buttons ───────────────────────────────────────────────────
                RowLayout {
                    spacing: 8

                    FpButton {
                        text:           root.editMode ? "Update Drone" : "Add Drone"
                        primary:        true
                        implicitWidth:  110
                        onClicked:      root.saveForm()
                    }

                    FpButton {
                        text:          "Cancel"
                        implicitWidth: 70
                        onClicked: {
                            root.showForm = false
                            validationMsg.visible = false
                        }
                    }
                }

                Item { implicitHeight: 2 }
            }
        }
    }
}
