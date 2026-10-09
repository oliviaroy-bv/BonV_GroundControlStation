import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0

Item {
    id: root
    implicitHeight: mainCol.implicitHeight

    signal fleetChanged()

    property bool showForm:    false
    property bool editMode:    false
    property int  editIndex:   -1
    property bool capDelivery: true
    property bool capDrop:     false
    property bool capParadrop: false
    property int  rcChannel:   9
    property int  rcTriggers:  1
    readonly property bool isAndroid: Qt.platform.os === "android"
    readonly property bool compactLayout: isAndroid && width < 900

    function openAdd() {
        editMode = false; editIndex = -1
        efName.text = ""; efRange.value = 5
        efMinSpeed.value = 2; efMaxSpeed.value = 8
        capDelivery = true; capDrop = false; capParadrop = false
        rcChannel = 9; rcTriggers = 1
        efTakeoffDef.value = 30; efTakeoffMin.value = 10; efTakeoffMax.value = 100
        efNearDef.value = 60;    efNearMin.value = 20;    efNearMax.value = 200
        efCruiseDef.value = 120; efCruiseMin.value = 80;  efCruiseMax.value = 300
        efLandDef.value = 80;    efLandMin.value = 80;    efLandMax.value = 150
        showForm = true
    }

    function openEdit(idx) {
        var d = droneManager.droneAt(idx)
        editMode = true; editIndex = idx
        efName.text = d.name
        efRange.value = Math.round(d.rangeKm)
        efMinSpeed.value = Math.round(d.minSpeedMs)
        efMaxSpeed.value = Math.round(d.maxSpeedMs)
        capDelivery = d.delivery; capDrop = d.drop; capParadrop = d.paradrop
        rcChannel = d.rcSwitch >= 9 ? d.rcSwitch : 9
        rcTriggers = d.rcCount > 0 ? d.rcCount : 1
        efTakeoffDef.value = Math.round(d.takeoffAlt)
        efTakeoffMin.value = Math.round(d.minTakeoffAlt)
        efTakeoffMax.value = Math.round(d.maxTakeoffAlt)
        efNearDef.value = Math.round(d.nearHomeAlt)
        efNearMin.value = Math.round(d.minNearHomeAlt)
        efNearMax.value = Math.round(d.maxNearHomeAlt)
        efCruiseDef.value = Math.round(d.cruiseAlt)
        efCruiseMin.value = Math.round(d.minCruiseAlt)
        efCruiseMax.value = Math.round(d.maxCruiseAlt)
        efLandDef.value = Math.round(d.landAlt)
        efLandMin.value = Math.round(d.minLandAlt)
        efLandMax.value = Math.round(d.maxLandAlt)
        showForm = true
    }

    function saveForm() {
        var name = efName.text.trim()
        if (name === "") { validationMsg.visible = true; return }
        validationMsg.visible = false
        var needsRc = capDrop || capParadrop
        if (editMode)
            droneManager.updateDrone(editIndex, name,
                efRange.value, efMinSpeed.value, efMaxSpeed.value,
                capDelivery, capDrop, capParadrop,
                needsRc ? rcChannel : 0, needsRc ? rcTriggers : 0,
                efTakeoffDef.value, efTakeoffMin.value, efTakeoffMax.value,
                efNearDef.value,    efNearMin.value,    efNearMax.value,
                efCruiseDef.value,  efCruiseMin.value,  efCruiseMax.value,
                efLandDef.value,    efLandMin.value,    efLandMax.value)
        else
            droneManager.addDrone(name,
                efRange.value, efMinSpeed.value, efMaxSpeed.value,
                capDelivery, capDrop, capParadrop,
                needsRc ? rcChannel : 0, needsRc ? rcTriggers : 0,
                efTakeoffDef.value, efTakeoffMin.value, efTakeoffMax.value,
                efNearDef.value,    efNearMin.value,    efNearMax.value,
                efCruiseDef.value,  efCruiseMin.value,  efCruiseMax.value,
                efLandDef.value,    efLandMin.value,    efLandMax.value)
        showForm = false; root.fleetChanged()
    }

    // ── Helpers ───────────────────────────────────────────────────────────────
    // A row with 3 spinboxes: Min | Default | Max with sub-labels
    component AltRow: Item {
        id: altRowRoot
        property alias minVal:  spinMin.value
        property alias defVal:  spinDef.value
        property alias maxVal:  spinMax.value
        property int   minFrom: 0
        property int   maxTo:   1000
        property int   step:    5
        property string unit:   "m"
        property alias minId: spinMin
        property alias defId: spinDef
        property alias maxId: spinMax

        height: spinRow.height
        width: parent.width

        Row {
            id: spinRow
            width: parent.width
            spacing: 0

            // Min column
            Column {
                width: (parent.width - 2) / 3
                spacing: 2
                Text { text: "Min"; font.family: Theme.fontFamily; font.pixelSize: 10
                    color: Theme.textMuted; leftPadding: 2 }
                FpSpinBox {
                    id: spinMin; width: parent.width - 2; label: ""; unit: altRowRoot.unit
                    value: altRowRoot.minFrom + altRowRoot.step
                    from: altRowRoot.minFrom; to: altRowRoot.maxTo; stepSize: altRowRoot.step
                }
            }
            Item { width: 1; height: 1 }

            // Default column
            Column {
                width: (parent.width - 2) / 3
                spacing: 2
                Text { text: "Default"; font.family: Theme.fontFamily; font.pixelSize: 10
                    color: Theme.blueMid; leftPadding: 2 }
                FpSpinBox {
                    id: spinDef; width: parent.width - 2; label: ""; unit: altRowRoot.unit
                    value: altRowRoot.minFrom + altRowRoot.step * 4
                    from: altRowRoot.minFrom; to: altRowRoot.maxTo; stepSize: altRowRoot.step
                }
            }
            Item { width: 1; height: 1 }

            // Max column
            Column {
                width: (parent.width - 2) / 3
                spacing: 2
                Text { text: "Max"; font.family: Theme.fontFamily; font.pixelSize: 10
                    color: Theme.textMuted; leftPadding: 2 }
                FpSpinBox {
                    id: spinMax; width: parent.width; label: ""; unit: altRowRoot.unit
                    value: altRowRoot.maxTo
                    from: altRowRoot.minFrom; to: altRowRoot.maxTo; stepSize: altRowRoot.step
                }
            }
        }
    }

    Column {
        id: mainCol
        width: parent.width
        spacing: 8

        // Header
        Item {
            width: parent.width; height: 30
            Text { anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                text: "Drone Fleet  (" + droneManager.count + " total)"
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
            Row {
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                spacing: 8
                FpButton { text: "Reset Defaults"; implicitWidth: 110; implicitHeight: 26
                    onClicked: { droneManager.resetToDefaults(); root.fleetChanged() } }
                FpButton { text: "+ Add Drone"; implicitWidth: 96; implicitHeight: 26
                    onClicked: root.openAdd() }
            }
        }

        // Drone rows
        Repeater {
            model: droneManager
            delegate: Rectangle {
                width: mainCol.width; height: 50; radius: Theme.radius
                color: droneActive ? Theme.bgSection : "#F8F7F4"
                border.color: droneActive ? Theme.border : Theme.borderLight
                border.width: 0.5; opacity: droneActive ? 1.0 : 0.6

                Rectangle {
                    id: rowDot; width: 8; height: 8; radius: 4
                    anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                    color: droneActive ? Theme.green : Theme.textHint
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: droneManager.setActive(index, !droneActive) }
                }
                Rectangle {
                    id: rowAction; width: 26; height: 26; radius: Theme.radius
                    anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                    color: droneActive ? (xm.containsMouse ? Theme.redLight : "transparent")
                                       : (pm.containsMouse ? Theme.greenLight : "transparent")
                    border.color: droneActive ? (xm.containsMouse ? "#F7C1C1" : Theme.border)
                                             : (pm.containsMouse ? "#C0DD97" : Theme.border)
                    border.width: 0.5
                    Text { anchors.centerIn: parent; text: droneActive ? "✕" : "+"
                        color: droneActive ? Theme.red : Theme.green; font.pixelSize: droneActive ? 11 : 14 }
                    MouseArea { id: xm; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        visible: droneActive; onClicked: { droneManager.deactivate(index); root.fleetChanged() } }
                    MouseArea { id: pm; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        visible: !droneActive; onClicked: { droneManager.setActive(index, true); root.fleetChanged() } }
                }
                FpButton {
                    id: rowEdit; text: "Edit"; implicitWidth: 48; implicitHeight: 26
                    anchors { right: rowAction.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    onClicked: root.openEdit(index)
                }
                Column {
                    id: rowStats
                    anchors { right: rowEdit.left; rightMargin: 16; verticalCenter: parent.verticalCenter }
                    spacing: 2
                    Text { text: (droneRangeKm * 2).toFixed(0) + " km"; width: 55; horizontalAlignment: Text.AlignRight
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Medium; color: Theme.textPrimary }
                    Text { text: droneMinSpeedMs.toFixed(0) + "–" + droneSpeedMs.toFixed(0) + " m/s"; width: 70; horizontalAlignment: Text.AlignRight
                        font.family: Theme.fontFamily; font.pixelSize: 10; color: Theme.textMuted }
                }
                Column {
                    anchors { left: rowDot.right; leftMargin: 10; right: rowStats.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
                    spacing: 4
                    Text { text: droneName; width: parent.width; elide: Text.ElideRight
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; font.weight: Font.Medium; color: Theme.textPrimary }
                    Row {
                        spacing: 4
                        Repeater {
                            model: droneCapabilities
                            delegate: Rectangle {
                                height: 16; width: bl.implicitWidth + 10; radius: 3
                                color: modelData === "Delivery" ? Theme.greenLight : modelData === "Drop" ? Theme.blueLight : Theme.amberLight
                                Text { id: bl; anchors.centerIn: parent; text: modelData
                                    font.family: Theme.fontFamily; font.pixelSize: 9; font.weight: Font.Medium
                                    color: modelData === "Delivery" ? Theme.green : modelData === "Drop" ? Theme.blue : Theme.amber }
                            }
                        }
                        Rectangle {
                            visible: droneNeedsRc; height: 16; width: rl.implicitWidth + 10; radius: 3; color: "#F0EAF8"
                            Text { id: rl; anchors.centerIn: parent; text: "CH" + droneRcSwitch + " ×" + droneRcCount
                                font.family: Theme.fontFamily; font.pixelSize: 9; font.weight: Font.Medium; color: "#6B3FA0" }
                        }
                    }
                }
            }
        }

        // Form
        Rectangle {
            width: mainCol.width
            height: showForm ? formInner.implicitHeight + 28 : 0
            visible: showForm; clip: true
            radius: Theme.radiusLg; color: Theme.bgSection
            border.color: Theme.borderFocus; border.width: 1
            Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

            Column {
                id: formInner
                anchors { left: parent.left; top: parent.top; margins: 16 }
                width: Math.min(parent.width - 32, root.compactLayout ? parent.width - 32 : 920)
                spacing: root.isAndroid ? 16 : 14

                Text { text: root.editMode ? "Edit Drone" : "Add New Drone"
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; font.weight: Font.Medium; color: Theme.textPrimary }

                // Name
                Column {
                    width: formInner.width; spacing: 4
                    Text { text: "Name"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                    FpInput { id: efName; width: parent.width; placeholderText: "e.g. Eagle-X" }
                }

                // Range + Speed
                Row {
                    width: formInner.width
                    spacing: root.compactLayout ? 12 : 28
                    Column {
                        id: rCol
                        width: root.compactLayout ? 190 : 220
                        spacing: 4
                        Text { text: "One-way range (km)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        FpSpinBox { id: efRange; width: 150; label: ""; unit: "km"; value: 5; from: 1; to: 500; stepSize: 1 }
                    }
                    Column {
                        width: root.compactLayout ? Math.max(300, formInner.width - rCol.width - 12) : 360
                        spacing: 4
                        // Speed: Min + Max side by side
                        Text { text: "Speed (m/s) - Min / Max"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Row {
                            width: parent.width; spacing: root.compactLayout ? 8 : 18
                            Column {
                                width: root.compactLayout ? (parent.width - 8) / 2 : 150; spacing: 2
                                Text { text: "Min"; font.family: Theme.fontFamily; font.pixelSize: 10; color: Theme.textMuted }
                                FpSpinBox { id: efMinSpeed; width: parent.width; label: ""; unit: "m/s"; value: 2; from: 1; to: efMaxSpeed.value; stepSize: 1 }
                            }
                            Column {
                                width: root.compactLayout ? (parent.width - 8) / 2 : 150; spacing: 2
                                Text { text: "Max"; font.family: Theme.fontFamily; font.pixelSize: 10; color: Theme.textMuted }
                                FpSpinBox { id: efMaxSpeed; width: parent.width; label: ""; unit: "m/s"; value: 8; from: efMinSpeed.value; to: 30; stepSize: 1 }
                            }
                        }
                    }
                }

                // Capabilities
                Column {
                    width: formInner.width; spacing: 6
                    Text { text: "Mission Capabilities"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                    Row {
                        spacing: 24
                        Repeater {
                            model: ["Delivery", "Drop", "Paradrop"]
                            delegate: Row {
                                spacing: 8
                                property bool chk: index === 0 ? root.capDelivery : index === 1 ? root.capDrop : root.capParadrop
                                Rectangle {
                                    width: 18; height: 18; radius: 4
                                    color: chk ? Theme.blueMid : Theme.bgInput
                                    border.color: chk ? Theme.blueMid : Theme.border; border.width: 0.5
                                    Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 11; visible: chk }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                        onClicked: { if (index===0) root.capDelivery = !root.capDelivery
                                                     else if (index===1) root.capDrop = !root.capDrop
                                                     else root.capParadrop = !root.capParadrop } }
                                }
                                Text { text: modelData; font.family: Theme.fontFamily; font.pixelSize: Theme.fontBase; color: Theme.textSecond }
                            }
                        }
                    }
                }

                // RC settings
                Column {
                    width: formInner.width; spacing: 6
                    visible: root.capDrop || root.capParadrop
                    Text { text: "RC Trigger Settings"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                    Row {
                        spacing: 12
                        Column {
                            spacing: 2
                            Text { text: "RC Channel (9–16)"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                            FpSpinBox { id: efRcCh; width: 110; label: ""; unit: ""
                                value: root.rcChannel; from: 9; to: 16; stepSize: 1
                                onValueChanged: root.rcChannel = value }
                        }
                        Column {
                            spacing: 2
                            Text { text: "Trigger Count"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                            FpSpinBox { id: efRcCount; width: 110; label: ""; unit: "×"
                                value: root.rcTriggers; from: 1; to: 10; stepSize: 1
                                onValueChanged: root.rcTriggers = value }
                        }
                        Text {
                            text: "CH" + root.rcChannel + ", pulsed " + root.rcTriggers + "× at drop"
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted
                            anchors.bottom: parent.bottom; bottomPadding: 4
                        }
                    }
                }

                // Altitudes — each row: label | Min | Default | Max
                Column {
                    id: altitudeGrid
                    property int labelColW: root.compactLayout ? 170 : 220
                    property int spinColW: root.compactLayout ? 150 : 148
                    property int gapW: root.compactLayout ? 8 : 18

                    width: Math.min(formInner.width, labelColW + spinColW * 3 + gapW * 3)
                    spacing: 8

                    Text { text: "Flight Parameters — Allowable Ranges"
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; color: Theme.textMuted }

                    // Column headers
                    Row {
                        width: altitudeGrid.width
                        height: 16
                        spacing: altitudeGrid.gapW

                        Text { width: altitudeGrid.labelColW; text: "Parameter"
                            font.family: Theme.fontFamily; font.pixelSize: 10; font.weight: Font.Medium; color: Theme.textMuted }
                        Text { width: altitudeGrid.spinColW
                            text: "Min"; horizontalAlignment: Text.AlignHCenter
                            font.family: Theme.fontFamily; font.pixelSize: 10; color: Theme.textMuted }
                        Text { width: altitudeGrid.spinColW
                            text: "Default"; horizontalAlignment: Text.AlignHCenter
                            font.family: Theme.fontFamily; font.pixelSize: 10; color: Theme.blueMid; font.weight: Font.Medium }
                        Text { width: altitudeGrid.spinColW
                            text: "Max"; horizontalAlignment: Text.AlignHCenter
                            font.family: Theme.fontFamily; font.pixelSize: 10; color: Theme.textMuted }
                    }

                    // Each altitude parameter row
                    Repeater {
                        model: [
                            { label: "Takeoff Alt (m AGL)",    minId: "efTakeoffMin", defId: "efTakeoffDef", maxId: "efTakeoffMax" },
                            { label: "Near-Home Alt (m AGL)",  minId: "efNearMin",    defId: "efNearDef",    maxId: "efNearMax"    },
                            { label: "Terrain Clearance (m)",  minId: "efCruiseMin",  defId: "efCruiseDef",  maxId: "efCruiseMax"  },
                            { label: "Land Alt (m AGL)",       minId: "efLandMin",    defId: "efLandDef",    maxId: "efLandMax"    },
                        ]
                        delegate: Item {
                            width: altitudeGrid.width
                            height: Math.max(labelTxt.implicitHeight, 28) + 6

                            Text {
                                id: labelTxt
                                width: altitudeGrid.labelColW; text: modelData.label
                                anchors.verticalCenter: parent.verticalCenter
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                color: Theme.textSecond; wrapMode: Text.WordWrap
                            }

                            Row {
                                id: rowSpins
                                anchors { left: labelTxt.right; leftMargin: altitudeGrid.gapW; verticalCenter: parent.verticalCenter }
                                spacing: altitudeGrid.gapW


                                FpSpinBox {
                                    id: minSpin

                                    width: altitudeGrid.spinColW
                                    label: ""
                                    unit: "m"

                                    value: index === 0 ? efTakeoffMin.value :
                                        index === 1 ? efNearMin.value :
                                        index === 2 ? efCruiseMin.value :
                                                        efLandMin.value

                                    // Global minimum allowed
                                    from: index === 3 ? 80 :
                                        index === 2 ? 80 : 5

                                    // Cannot exceed Default
                                    to: defSpin.value

                                    stepSize: 5

                                    onValueChanged: {

                                        // Safety clamp
                                        if (value > defSpin.value)
                                            value = defSpin.value

                                        if (index === 0)
                                            efTakeoffMin.value = value
                                        else if (index === 1)
                                            efNearMin.value = value
                                        else if (index === 2)
                                            efCruiseMin.value = value
                                        else
                                            efLandMin.value = value
                                    }
                                }

                                FpSpinBox {
                                    id: defSpin

                                    width: altitudeGrid.spinColW
                                    label: ""
                                    unit: "m"

                                    value: index === 0 ? efTakeoffDef.value :
                                        index === 1 ? efNearDef.value :
                                        index === 2 ? efCruiseDef.value :
                                                        efLandDef.value

                                    // Must stay above Min
                                    from: minSpin.value

                                    // Must stay below Max
                                    to: maxSpin.value

                                    stepSize: 5

                                    onValueChanged: {

                                        if (index === 0)
                                            efTakeoffDef.value = value
                                        else if (index === 1)
                                            efNearDef.value = value
                                        else if (index === 2)
                                            efCruiseDef.value = value
                                        else
                                            efLandDef.value = value
                                    }
                                }
                           
                           
                                FpSpinBox {
                                    id: maxSpin

                                    width: altitudeGrid.spinColW
                                    label: ""
                                    unit: "m"

                                    value: index === 0 ? efTakeoffMax.value :
                                        index === 1 ? efNearMax.value :
                                        index === 2 ? efCruiseMax.value :
                                                        efLandMax.value

                                    // Cannot go below Default
                                    from: defSpin.value

                                    // Global maximum allowed
                                    to: index === 3 ? 150 : 1000

                                    stepSize: 5

                                    onValueChanged: {

                                        // Safety clamp
                                        if (value < defSpin.value)
                                            value = defSpin.value

                                        if (index === 0)
                                            efTakeoffMax.value = value
                                        else if (index === 1)
                                            efNearMax.value = value
                                        else if (index === 2)
                                            efCruiseMax.value = value
                                        else
                                            efLandMax.value = value
                                    }
                                }
                            }
                        }
                    }

                    // Hidden binding targets for values
                    FpSpinBox { id: efTakeoffMin; visible: false; label: ""; unit: "m"; value: 10;  from: 5;  to: 800; stepSize: 5 }
                    FpSpinBox { id: efTakeoffDef; visible: false; label: ""; unit: "m"; value: 30;  from: 5;  to: 800; stepSize: 5 }
                    FpSpinBox { id: efTakeoffMax; visible: false; label: ""; unit: "m"; value: 100; from: 5;  to: 800; stepSize: 5 }
                    FpSpinBox { id: efNearMin;    visible: false; label: ""; unit: "m"; value: 20;  from: 5;  to: 800; stepSize: 5 }
                    FpSpinBox { id: efNearDef;    visible: false; label: ""; unit: "m"; value: 60;  from: 5;  to: 800; stepSize: 5 }
                    FpSpinBox { id: efNearMax;    visible: false; label: ""; unit: "m"; value: 200; from: 5;  to: 800; stepSize: 5 }
                    FpSpinBox { id: efCruiseMin;  visible: false; label: ""; unit: "m"; value: 80;  from: 80; to: 1000; stepSize: 10 }
                    FpSpinBox { id: efCruiseDef;  visible: false; label: ""; unit: "m"; value: 120; from: 80; to: 1000; stepSize: 10 }
                    FpSpinBox { id: efCruiseMax;  visible: false; label: ""; unit: "m"; value: 300; from: 80; to: 1000; stepSize: 10 }
                    FpSpinBox { id: efLandMin;    visible: false; label: ""; unit: "m"; value: 80;  from: 80; to: 150; stepSize: 5 }
                    FpSpinBox { id: efLandDef;    visible: false; label: ""; unit: "m"; value: 80;  from: 80; to: 150; stepSize: 5 }
                    FpSpinBox { id: efLandMax;    visible: false; label: ""; unit: "m"; value: 150; from: 80; to: 150; stepSize: 5 }
                }

                Text { id: validationMsg; visible: false; text: "Please enter a drone name"
                    color: Theme.red; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm }

                Row {
                    spacing: 8
                    FpButton { text: root.editMode ? "Update Drone" : "Add Drone"; primary: true; implicitWidth: 110; onClicked: root.saveForm() }
                    FpButton { text: "Cancel"; implicitWidth: 70; onClicked: { root.showForm = false; validationMsg.visible = false } }
                }
                Item { height: 2 }
            }
        }
    }
}
