import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    id: root

    property bool opened: false
    property bool drawerOpen: false
    property int panelWidth: 520
    property int currentStep: 0
    property url outputFolder: ""
    property string generatedPath: ""
    property string statusMessage: ""
    property bool statusIsError: false
    property bool generationComplete: false
    property bool planChoiceVisible: false

    readonly property var stepTitles: [ "Setup", "Coordinates", "Params", "Output", "Review" ]

    signal missionGenerated(string path)

    Component.onCompleted: {
        outputFolder = defaultOutputFolder()
    }

    function defaultOutputFolder() {
        if (typeof backend !== "undefined" && backend.missionsDirectoryUrl)
            return backend.missionsDirectoryUrl()
        return ""
    }

    anchors.fill: parent
    visible: opened || drawerOpen
    z: 135

    function open(hasExistingPlan) {
        closeDelay.stop()
        opened = true
        drawerOpen = false
        Qt.callLater(function() { root.drawerOpen = true })
        planChoiceVisible = hasExistingPlan === true
        resetBuilderState(false)
    }

    function close() {
        drawerOpen = false
        closeDelay.restart()
    }
    function resetBuilderState(resetFields) {
        currentStep = 0
        generatedPath = ""
        statusMessage = ""
        statusIsError = false
        generationComplete = false
        if (resetFields)
            resetPlanFields()
    }

    function resetPlanFields() {
        droneCombo.currentIndex = 0
        missionTypeCombo.currentIndex = 0
        tripModeCombo.currentIndex = 0
        takeoffLatField.text = ""
        takeoffLonField.text = ""
        takeoffNameField.text = ""
        landingLatField.text = ""
        landingLonField.text = ""
        landingNameField.text = ""
        takeoffAltField.text = "40"
        nearHomeAltField.text = "45"
        terrainClearanceField.text = "30"
        droppingAltField.text = "35"
        landAltField.text = "0"
        cruiseSpeedField.text = "12"
        delayTimeField.text = "5"
        hoverTimeField.text = "5"
        outputFolder = defaultOutputFolder()
    }

    function createNewPlan() {
        planChoiceVisible = false
        resetBuilderState(true)
        statusMessage = "New mission plan started."
        statusIsError = false
    }

    function editExistingPlan() {
        planChoiceVisible = false
        resetBuilderState(false)
        statusMessage = "Editing existing mission plan."
        statusIsError = false
    }

    function selectedText(comboBox) {
        return comboBox.currentText
    }

    function numberFrom(field) {
        return Number(field.text.trim())
    }

    function validCoordinate(value, maxAbs) {
        return isFinite(value) && Math.abs(value) <= maxAbs
    }

    function validateInputs() {
        if (takeoffNameField.text.trim().length === 0 || landingNameField.text.trim().length === 0)
            return "Enter both takeoff and landing location names."

        if (selectedText(missionTypeCombo) !== "Delivery" && selectedText(tripModeCombo) !== "Return")
            return "Drop and Paradrop missions require Return trip mode."

        if (!validCoordinate(numberFrom(takeoffLatField), 90) || !validCoordinate(numberFrom(landingLatField), 90))
            return "Latitude must be a number between -90 and 90."

        if (!validCoordinate(numberFrom(takeoffLonField), 180) || !validCoordinate(numberFrom(landingLonField), 180))
            return "Longitude must be a number between -180 and 180."

        var parameterFields = [
            takeoffAltField, nearHomeAltField, terrainClearanceField, droppingAltField,
            landAltField, cruiseSpeedField, delayTimeField, hoverTimeField
        ]
        for (var i = 0; i < parameterFields.length; i++) {
            if (!isFinite(numberFrom(parameterFields[i])))
                return "Flight parameters must be numeric."
        }

        return ""
    }

    function generateMission() {
        var errorText = validateInputs()
        if (errorText.length > 0) {
            statusMessage = errorText
            statusIsError = true
            return
        }

        var outputPath = missionFileWriter.generateMission(
                    outputFolder,
                    selectedText(droneCombo),
                    missionTypeCombo.currentIndex,
                    tripModeCombo.currentIndex,
                    numberFrom(takeoffLatField),
                    numberFrom(takeoffLonField),
                    takeoffNameField.text,
                    numberFrom(landingLatField),
                    numberFrom(landingLonField),
                    landingNameField.text,
                    numberFrom(takeoffAltField),
                    numberFrom(nearHomeAltField),
                    numberFrom(terrainClearanceField),
                    numberFrom(droppingAltField),
                    numberFrom(landAltField),
                    numberFrom(cruiseSpeedField),
                    numberFrom(delayTimeField),
                    numberFrom(hoverTimeField))
        if (outputPath.length === 0) {
            statusMessage = missionFileWriter.lastError
            statusIsError = true
            return
        }

        generatedPath = outputPath
        statusMessage = "Mission generated successfully. Saved: " + outputPath
        statusIsError = false
        generationComplete = true
        missionGenerated(outputPath)
        successPopup.open()
    }

    Timer {
        id: closeDelay
        interval: 220
        repeat: false
        onTriggered: root.opened = false
    }

    Rectangle {
        anchors.fill: parent
        color: "#b0000308"
        opacity: root.drawerOpen ? 1 : 0
        visible: root.visible

        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    Rectangle {
        id: panel

        width: Math.min(root.panelWidth, root.width)
        height: root.height
        x: root.drawerOpen ? root.width - width : root.width
        y: 0
        color: "#f10a0f18"
        border.color: "#3b4b66"
        border.width: 1
        clip: true

        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

        MouseArea {
            anchors.fill: parent
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 18
            anchors.bottomMargin: 24
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: "MISSION BUILDER"
                    color: "#00c7df"
                    font.pixelSize: 14
                    font.bold: true
                    font.letterSpacing: 1.2
                }

                Button {
                    id: closeButton

                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    padding: 0
                    flat: true
                    text: "x"
                    onClicked: root.close()

                    contentItem: Text {
                        text: closeButton.text
                        color: "#8e98aa"
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle { color: closeButton.hovered ? "#141e2b" : "#00000000" }
                }
            }
            RowLayout {
                visible: !root.planChoiceVisible
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: root.stepTitles

                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 25
                            Layout.preferredHeight: 25
                            radius: width / 2
                            color: index <= root.currentStep ? "#138fe2" : "#1b2c38"
                            border.color: index === root.currentStep ? "#75d7ff" : "#304a5c"

                            Text {
                                anchors.centerIn: parent
                                text: index + 1
                                color: "white"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData
                            color: index <= root.currentStep ? "#cdefff" : "#758896"
                            font.pixelSize: 9
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }
            Rectangle {
                visible: !root.planChoiceVisible
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#202a3a"
            }

            PlanChoicePanel {
                visible: root.planChoiceVisible
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            ScrollView {
                id: formScroll

                visible: !root.planChoiceVisible
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: formScroll.availableWidth
                    spacing: 14

                    FormSection {
                        Layout.fillWidth: true
                        title: "1. Mission Setup"
                        expanded: root.currentStep === 0
                        onHeaderClicked: root.currentStep = 0

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 10

                            FieldLabel { text: "Drone" }
                            MissionComboBox {
                                id: droneCombo
                                model: [ "Air Prahar Max", "Air Orca", "Air Hans", "Air Hex" ]
                            }

                            FieldLabel { text: "Mission Type" }
                            MissionComboBox {
                                id: missionTypeCombo
                                model: [ "Delivery", "Drop", "Paradrop" ]
                            }

                            FieldLabel { text: "Trip Mode" }
                            MissionComboBox {
                                id: tripModeCombo
                                model: [ "One Way", "Return" ]
                            }
                        }
                    }

                    FormSection {
                        Layout.fillWidth: true
                        title: "2. Coordinates"
                        expanded: root.currentStep === 1
                        onHeaderClicked: root.currentStep = 1

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 10

                            FieldLabel { text: "Takeoff latitude" }
                            MissionTextField { id: takeoffLatField; placeholderText: "12.971599" }
                            FieldLabel { text: "Takeoff longitude" }
                            MissionTextField { id: takeoffLonField; placeholderText: "77.594566" }
                            FieldLabel { text: "Takeoff Location name" }
                            MissionTextField { id: takeoffNameField; placeholderText: "Takeoff site" }
                            FieldLabel { text: "Landing latitude" }
                            MissionTextField { id: landingLatField; placeholderText: "12.982100" }
                            FieldLabel { text: "Landing longitude" }
                            MissionTextField { id: landingLonField; placeholderText: "77.602300" }
                            FieldLabel { text: "Landing name" }
                            MissionTextField { id: landingNameField; placeholderText: "Landing site" }
                        }
                    }

                    FormSection {
                        Layout.fillWidth: true
                        title: "3. Flight Parameters"
                        expanded: root.currentStep === 2
                        onHeaderClicked: root.currentStep = 2

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 10

                            FieldLabel { text: "Takeoff Alt" }
                            MissionTextField { id: takeoffAltField; text: "40" }
                            FieldLabel { text: "Near-Home Alt" }
                            MissionTextField { id: nearHomeAltField; text: "45" }
                            FieldLabel { text: "Terrain Clearance" }
                            MissionTextField { id: terrainClearanceField; text: "30" }
                            FieldLabel { text: "Dropping Alt" }
                            MissionTextField { id: droppingAltField; text: "35" }
                            FieldLabel { text: "Land Alt" }
                            MissionTextField { id: landAltField; text: "0" }
                            FieldLabel { text: "Cruise Speed" }
                            MissionTextField { id: cruiseSpeedField; text: "12" }
                            FieldLabel { text: "Delay Time" }
                            MissionTextField { id: delayTimeField; text: "5" }
                            FieldLabel { text: "Hover Time" }
                            MissionTextField { id: hoverTimeField; text: "5" }
                        }
                    }

                    FormSection {
                        Layout.fillWidth: true
                        title: "4. Output Folder"
                        expanded: root.currentStep === 3
                        onHeaderClicked: root.currentStep = 3

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                radius: 4
                                color: "#070c13"
                                border.color: "#253047"

                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    text: root.outputFolder.toString().length > 0 ? root.outputFolder.toString() : "Default waypoint folder"
                                    color: root.outputFolder.toString().length > 0 ? "#dceaf5" : "#687d8c"
                                    font.pixelSize: 11
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideMiddle
                                }
                            }

                            MissionButton {
                                Layout.preferredWidth: 84
                                text: "Browse"
                                onClicked: folderDialog.open()
                            }
                        }
                    }

                    FormSection {
                        Layout.fillWidth: true
                        title: "5. Review & Generate"
                        expanded: root.currentStep === 4
                        onHeaderClicked: root.currentStep = 4

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ReviewLine { label: "Drone"; value: root.selectedText(droneCombo) }
                            ReviewLine { label: "Mission"; value: root.selectedText(missionTypeCombo) + " / " + root.selectedText(tripModeCombo) }
                            ReviewLine { label: "Takeoff"; value: takeoffNameField.text + " (" + takeoffLatField.text + ", " + takeoffLonField.text + ")" }
                            ReviewLine { label: "Landing"; value: landingNameField.text + " (" + landingLatField.text + ", " + landingLonField.text + ")" }
                            ReviewLine { label: "Output"; value: root.outputFolder.toString().length > 0 ? root.outputFolder.toString() : "Not selected" }
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: root.statusMessage.length > 0
                text: root.statusMessage
                color: root.statusIsError ? "#ff7777" : "#62e58b"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
            RowLayout {
                visible: !root.planChoiceVisible
                Layout.fillWidth: true
                spacing: 10

                MissionButton {
                    Layout.preferredWidth: 88
                    text: "Back"
                    enabled: root.currentStep > 0
                    onClicked: root.currentStep = Math.max(0, root.currentStep - 1)
                }

                Item { Layout.fillWidth: true }

                MissionButton {
                    Layout.preferredWidth: root.currentStep === 4 ? 132 : 96
                    text: root.currentStep === 4 ? (root.generationComplete ? "Done" : "Generate") : "Next"
                    accent: root.currentStep === 4 ? "#1bbf72" : "#138fe2"
                    onClicked: {
                        if (root.currentStep < 4) {
                            root.currentStep += 1
                        } else if (root.generationComplete) {
                            root.close()
                        } else {
                            root.generateMission()
                        }
                    }
                }
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: "Select waypoint output folder"
        currentFolder: root.outputFolder
        onAccepted: root.outputFolder = selectedFolder
    }

    Popup {
        id: successPopup

        parent: Overlay.overlay
        modal: true
        focus: true
        width: Math.min(460, root.width - 48)
        height: successContent.implicitHeight + 36
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onAboutToShow: {
            x = Math.round((Overlay.overlay.width - width) / 2)
            y = Math.round((Overlay.overlay.height - height) / 2)
        }

        background: Rectangle {
            radius: 6
            color: "#f00c1720"
            border.color: "#2bc278"
            border.width: 1
        }

        contentItem: ColumnLayout {
            id: successContent

            width: successPopup.width - 36
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: "Mission generated successfully"
                color: "#eafff3"
                font.pixelSize: 15
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: root.generatedPath
                color: "#9edfb9"
                font.pixelSize: 11
                wrapMode: Text.WrapAnywhere
                horizontalAlignment: Text.AlignHCenter
                maximumLineCount: 3
                elide: Text.ElideMiddle
            }

            MissionButton {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 96
                text: "OK"
                accent: "#1bbf72"
                onClicked: successPopup.close()
            }
        }
    }
    component PlanChoicePanel: Rectangle {
        color: "#0d121c"
        border.color: "#253047"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: "EXISTING PLAN FOUND"
                color: "#00c7df"
                font.pixelSize: 14
                font.bold: true
                font.letterSpacing: 1.2
            }

            Text {
                Layout.fillWidth: true
                text: "A mission plan is already loaded. Choose whether to start a new plan or continue editing the current one."
                color: "#c8d5ec"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#202a3a"
            }

            MissionChoiceButton {
                Layout.fillWidth: true
                title: "CREATE NEW PLAN"
                detail: "Reset the mission builder fields and start from a blank plan."
                accent: "#00c7df"
                onClicked: root.createNewPlan()
            }

            MissionChoiceButton {
                Layout.fillWidth: true
                title: "EDIT EXISTING PLAN"
                detail: "Keep the current builder values and continue editing."
                accent: "#1fc969"
                onClicked: root.editExistingPlan()
            }

            Item { Layout.fillHeight: true }
        }
    }

    component MissionChoiceButton: Button {
        id: choiceButton
        property string title: ""
        property string detail: ""
        property color accent: "#00c7df"

        Layout.preferredHeight: 76
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.topMargin: 10
            anchors.bottomMargin: 10
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: choiceButton.title
                color: choiceButton.accent
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: choiceButton.detail
                color: "#8794aa"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }

        background: Rectangle {
            color: choiceButton.down ? "#182131" : choiceButton.hovered ? "#111b28" : "#070c13"
            border.color: choiceButton.hovered || choiceButton.down ? choiceButton.accent : "#253047"
            border.width: 1
        }
    }

    component FormSection: ColumnLayout {
        id: section

        property string title: ""
        property bool expanded: false
        default property alias sectionContent: contentColumn.data
        signal headerClicked()

        Layout.fillWidth: true
        width: parent ? parent.width : 0
        spacing: 0

        Button {
            id: sectionButton

            Layout.fillWidth: true
            Layout.preferredHeight: 40
            padding: 0
            flat: true
            text: section.title
            onClicked: section.headerClicked()

            contentItem: RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Text {
                    Layout.fillWidth: true
                    text: section.title
                    color: "#ccefff"
                    font.pixelSize: 12
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    text: section.expanded ? "v" : ">"
                    color: "#7fb6d4"
                    font.pixelSize: 13
                    font.bold: true
                }
            }

            background: Rectangle {
                radius: 4
                color: sectionButton.hovered ? "#111b28" : "#0d121c"
                border.color: section.expanded ? "#00aeca" : "#253047"
            }
        }

        ColumnLayout {
            id: contentColumn

            Layout.fillWidth: true
            visible: section.expanded
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 10
                color: "#00000000"
            }
        }
    }

    component FieldLabel: Text {
        Layout.preferredWidth: 126
        text: ""
        color: "#9fb5c5"
        font.pixelSize: 11
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
    }

    component MissionTextField: TextField {
        Layout.fillWidth: true
        Layout.preferredHeight: 34
        selectByMouse: true
        color: "#edf6fb"
        placeholderTextColor: "#657a89"
        font.pixelSize: 12

        background: Rectangle {
            radius: 4
            color: "#070c13"
            border.color: parent.activeFocus ? "#00aeca" : "#253047"
        }
    }

    component MissionComboBox: ComboBox {
        id: missionCombo

        Layout.fillWidth: true
        Layout.preferredHeight: 34
        font.pixelSize: 12

        contentItem: Text {
            text: missionCombo.displayText
            color: "#edf6fb"
            font: missionCombo.font
            leftPadding: 10
            rightPadding: 26
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 4
            color: "#070c13"
            border.color: parent.activeFocus ? "#00aeca" : "#253047"
        }
    }

    component MissionButton: Button {
        id: missionButton

        property color accent: "#138fe2"

        Layout.preferredHeight: 34
        padding: 0
        font.pixelSize: 11
        font.bold: true

        contentItem: Text {
            text: missionButton.text
            color: missionButton.enabled ? "white" : "#65727b"
            font: missionButton.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 4
            color: missionButton.enabled ? (missionButton.down ? Qt.darker(missionButton.accent, 1.25) : missionButton.hovered ? Qt.lighter(missionButton.accent, 1.12) : missionButton.accent) : "#1d2a33"
            border.color: missionButton.enabled ? "#65cfff" : "#334652"
        }
    }

    component ReviewLine: RowLayout {
        id: reviewLine

        property string label: ""
        property string value: ""

        width: parent ? parent.width : 0
        Layout.fillWidth: true
        spacing: 10

        Text {
            Layout.preferredWidth: 76
            text: reviewLine.label
            color: "#88a8bc"
            font.pixelSize: 10
            font.bold: true
        }

        Text {
            Layout.fillWidth: true
            text: reviewLine.value
            color: "#e5f3fb"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }
}
