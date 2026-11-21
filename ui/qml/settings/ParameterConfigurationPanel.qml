pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as Files

Item {
    id: root
    property var controller: null
    property var colors
    property var draftNames: []
    property bool hasCompleteSnapshot: false
    property bool stale: true
    signal backRequested()
    readonly property bool locked: !controller || controller.busy || controller.applying
    readonly property var summary: controller ? controller.importSummary : ({})
    readonly property var progress: controller ? controller.progress : ({})
    function prepareImport() { unknownAcknowledgement.checked = false; draftAcknowledgement.checked = false; importDialog.open() }
    onControllerChanged: { unknownAcknowledgement.checked = false; draftAcknowledgement.checked = false }
    Rectangle { anchors.fill: parent; color: root.colors.background; border.color: root.colors.border; radius: 5 }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 12; spacing: 8
        RowLayout {
            Layout.fillWidth: true
            ConfigButton { objectName: "configurationBack"; text: "‹ Parameters"; onClicked: root.backRequested() }
            Caption { text: "Configuration comparison"; color: root.colors.text; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
        }
        Flow {
            Layout.fillWidth: true; spacing: 6
            ConfigButton { objectName: "importParameterConfiguration"; text: "Import"; enabled: !root.locked; onClicked: root.prepareImport() }
            ConfigButton { objectName: "exportParameterConfiguration"; text: "Export"; enabled: !root.locked && root.hasCompleteSnapshot; onClicked: exportConfirmation.open() }
            ConfigButton { objectName: "compareParameterConfiguration"; text: "Compare"; enabled: !!root.controller && root.controller.canCompare; onClicked: { unknownAcknowledgement.checked = false; draftAcknowledgement.checked = false; root.controller.compare() } }
            ConfigButton { objectName: "applyParameterConfiguration"; text: "Apply Selected"; enabled: !root.locked && root.controller.comparisonCurrent && root.controller.selectedCount > 0; onClicked: applyConfirmation.open() }
            ConfigButton { objectName: "stopParameterConfiguration"; text: "Stop further writes"; enabled: !!root.controller && root.controller.applying; onClicked: root.controller.stop() }
        }
        Caption {
            Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 11; color: root.colors.muted
            text: root.summary.total !== undefined ? "File: " + root.summary.format + " · " + root.summary.firmware + " · Total " + root.summary.total
                + " · Valid " + root.summary.valid + " · Invalid " + root.summary.invalid + " · Duplicate " + root.summary.duplicates + " · Unsupported " + root.summary.unsupported : "Import a .param or .params file. Importing never writes to the vehicle."
        }
        Caption { Layout.fillWidth: true; text: root.controller ? root.controller.compatibility : "Configuration service unavailable"; wrapMode: Text.Wrap; font.pixelSize: 11; color: root.colors.warning }
        ConfigCheck {
            id: unknownAcknowledgement; objectName: "configurationUnknownAcknowledgement"
            Layout.fillWidth: true
            text: "I acknowledge that file or vehicle firmware identity is unverified."
            enabled: !root.locked
            palette.windowText: root.colors.text
            contentItem: Text { text: unknownAcknowledgement.text; wrapMode: Text.Wrap; leftPadding: 28; color: root.colors.text; font.family: "Segoe UI"; font.pixelSize: 11 }
        }
        ConfigCheck {
            id: draftAcknowledgement; objectName: "configurationDraftAcknowledgement"
            Layout.fillWidth: true
            text: "Apply selected file values over conflicting local proposals; keep draft text for review."
            enabled: !root.locked
            palette.windowText: root.colors.text
            contentItem: Text { text: draftAcknowledgement.text; wrapMode: Text.Wrap; leftPadding: 28; color: root.colors.text; font.family: "Segoe UI"; font.pixelSize: 11 }
        }
        Flow {
            Layout.fillWidth: true; spacing: 6
            ComboBox {
                objectName: "configurationComparisonFilter"
                model: ["All", "Different", "Identical", "Unsupported", "Invalid", "Missing"]
                enabled: !!root.controller
                onActivated: root.controller.filter = currentText
                palette.button: root.colors.panelAlt; palette.buttonText: root.colors.text; palette.text: root.colors.text; palette.base: root.colors.panel
            }
            ConfigButton { text: "Select all valid differences"; enabled: !root.locked && root.controller.comparisonCurrent; onClicked: root.controller.selectAll(true) }
            ConfigButton { text: "Deselect all"; enabled: !root.locked && root.controller.comparisonCurrent; onClicked: root.controller.selectAll(false) }
            Caption { text: root.controller ? root.controller.selectedCount + " selected" : ""; color: root.colors.accent; font.pixelSize: 11; Layout.fillWidth: true }
        }
        ScrollView {
            id: viewport
            Layout.fillWidth: true; Layout.fillHeight: true
            contentWidth: Math.max(850, availableWidth); contentHeight: availableHeight
            ColumnLayout {
                width: viewport.contentWidth; height: viewport.availableHeight; spacing: 4
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Caption { text: "APPLY"; Layout.preferredWidth: 48; color: root.colors.muted }
                    Caption { text: "PARAMETER"; Layout.preferredWidth: 160; color: root.colors.muted }
                    Caption { text: "CURRENT"; Layout.preferredWidth: 130; color: root.colors.muted }
                    Caption { text: "FILE VALUE"; Layout.preferredWidth: 130; color: root.colors.muted }
                    Caption { text: "TYPE"; Layout.preferredWidth: 80; color: root.colors.muted }
                    Caption { text: "COMPARISON / RESULT"; Layout.fillWidth: true; color: root.colors.muted }
                }
                ListView {
                    objectName: "parameterConfigurationRows"
                    Layout.fillWidth: true; Layout.fillHeight: true
                    model: root.controller ? root.controller.rows : null
                    clip: true; reuseItems: true
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: row
                        required property int sourceRow
                        required property string parameterName
                        required property var currentValue
                        required property var fileValue
                        required property string parameterType
                        required property string comparisonStatus
                        required property var detail
                        required property var eligible
                        required property var chosen
                        required property var outcome
                        required property var attempts
                        required property var transactionId
                        width: ListView.view.width; height: 36
                        color: row.chosen ? root.colors.accentSoft : "transparent"
                        RowLayout {
                            anchors.fill: parent; spacing: 8
                            ConfigCheck {
                                Layout.preferredWidth: 48; checked: !!row.chosen
                                enabled: !root.locked && root.controller.comparisonCurrent && !!row.eligible
                                onToggled: root.controller.selectRow(row.sourceRow, checked)
                            }
                            Caption { text: row.parameterName; Layout.preferredWidth: 160; color: root.colors.text; elide: Text.ElideRight }
                            Caption { text: row.currentValue === undefined || row.currentValue === null ? "—" : String(row.currentValue); Layout.preferredWidth: 130; color: root.colors.text; elide: Text.ElideRight }
                            Caption { text: row.fileValue === undefined || row.fileValue === null ? "—" : String(row.fileValue); Layout.preferredWidth: 130; color: root.colors.text; elide: Text.ElideRight }
                            Caption { text: row.parameterType; Layout.preferredWidth: 80; color: root.colors.muted; elide: Text.ElideRight }
                            Caption {
                                text: row.comparisonStatus + (row.outcome ? " · " + row.outcome : "") + (row.attempts ? " · attempts " + row.attempts : "")
                                Layout.fillWidth: true; elide: Text.ElideRight
                                color: row.outcome === "Confirmed" ? root.colors.success : row.eligible ? root.colors.accent : root.colors.warning
                                ToolTip.visible: hover.hovered
                                ToolTip.text: (row.detail || "") + (row.transactionId ? "\nTransaction " + row.transactionId : "")
                                HoverHandler { id: hover }
                            }
                        }
                    }
                    Caption { anchors.centerIn: parent; visible: parent.count === 0; text: "Import and compare a configuration to see results."; color: root.colors.muted }
                }
            }
        }
        BusyIndicator { running: !!root.controller && root.controller.busy; visible: running; implicitHeight: 26 }
        Caption {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: root.colors.text; font.pixelSize: 11
            text: root.controller ? root.controller.message : ""
        }
        Caption {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: root.colors.muted; font.pixelSize: 11
            text: root.progress.total ? "Total " + root.progress.total + " · Submitted " + root.progress.submitted + " · Pending " + root.progress.pending
                + " · Confirmed " + root.progress.Confirmed + " · Rejected " + root.progress.Rejected + " · Timed out " + root.progress.TimedOut
                + " · Cancelled " + root.progress.Cancelled + " · Transport errors " + root.progress.TransportError
                + " · Invalid " + root.progress.InvalidRequest + " · Not submitted " + root.progress["Not submitted"] : ""
        }
    }
    Files.FileDialog {
        id: importDialog; title: "Import parameter configuration"
        fileMode: Files.FileDialog.OpenFile; nameFilters: ["Parameter text (*.param *.params)"]
        onAccepted: root.controller.importFile(selectedFile)
    }
    Files.FileDialog {
        id: exportDialog; title: "Export confirmed parameter configuration"
        fileMode: Files.FileDialog.SaveFile
        nameFilters: root.controller && root.controller.firmware === "ardupilot" ? ["ArduPilot pairs (*.param)"]
            : root.controller && root.controller.firmware === "px4" ? ["QGC/PX4 typed text (*.params)"] : ["Unidentified typed text (*.params)"]
        defaultSuffix: root.controller && root.controller.firmware === "ardupilot" ? "param" : "params"
        onAccepted: root.controller.exportFile(selectedFile, exportStale.checked, exportUnknown.checked)
    }
    Dialog {
        id: exportConfirmation; objectName: "configurationExportConfirmation"
        anchors.centerIn: parent; width: Math.max(200, Math.min(440, root.width - 24)); implicitHeight: 300
        title: "Export confirmed snapshot"; modal: true; standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: exportDialog.open()
        contentItem: ColumnLayout {
            Caption { objectName: "configurationExportWarning"; text: root.stale ? "Warning: this export is a stale retained snapshot, not necessarily the current vehicle state. Drafts and pending proposals are excluded."
                    : "Only the complete confirmed snapshot is exported. Drafts and pending proposals are excluded."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: root.colors.text }
            ConfigCheck { id: exportStale; Layout.fillWidth: true; text: "I acknowledge this may be a stale retained snapshot." }
            ConfigCheck { id: exportUnknown; Layout.fillWidth: true; text: "Allow an explicitly unidentified typed file if firmware is unknown." }
        }
        header: Caption { text: exportConfirmation.title; color: root.colors.text; padding: 12; font.bold: true }
        footer: DialogButtonBox { standardButtons: exportConfirmation.standardButtons; delegate: ConfigButton {} }
        background: Rectangle { color: root.colors.panel; border.color: root.colors.border; radius: 5 }
    }
    Dialog {
        id: applyConfirmation; objectName: "configurationApplyConfirmation"
        anchors.centerIn: parent; width: Math.max(200, Math.min(440, root.width - 24)); implicitHeight: 200
        title: "Apply selected file values?"; modal: true; standardButtons: Dialog.Apply | Dialog.Cancel
        contentItem: Caption {
            text: (root.controller ? root.controller.selectedCount : 0) + " selected differences will be submitted individually. This is not atomic; partial failure is possible. Existing local proposals are preserved."
            wrapMode: Text.Wrap; color: root.colors.text
        }
        onApplied: {
            root.controller.applySelected(root.draftNames, unknownAcknowledgement.checked, draftAcknowledgement.checked)
            close()
        }
        header: Caption { text: applyConfirmation.title; color: root.colors.text; padding: 12; font.bold: true }
        footer: DialogButtonBox { standardButtons: applyConfirmation.standardButtons; delegate: ConfigButton {} }
        background: Rectangle { color: root.colors.panel; border.color: root.colors.border; radius: 5 }
    }
    onVisibleChanged: if (!visible) { applyConfirmation.close(); exportConfirmation.close(); importDialog.close(); exportDialog.close() }
    component ConfigCheck: CheckBox {
        id: check
        font.family: "Segoe UI"; font.pixelSize: 11
        padding: 4; spacing: 8
        implicitHeight: Math.max(28, contentItem.implicitHeight + 8)
        contentItem: Text { text: check.text; font: check.font; color: root.colors.text; leftPadding: 24; wrapMode: Text.Wrap; verticalAlignment: Text.AlignVCenter }
        indicator: Rectangle {
            width: 16; height: 16; radius: 3
            x: check.leftPadding; y: (check.height - height) / 2
            color: check.checked ? root.colors.accentSoft : root.colors.panel
            border.color: check.checked ? root.colors.accent : root.colors.border
            opacity: check.enabled ? 1 : 0.45
            Text { anchors.centerIn: parent; text: check.checked ? "✓" : ""; color: root.colors.accent; font.pixelSize: 12 }
        }
    }
    component Caption: Label { font.family: "Segoe UI"; font.pixelSize: 11 }
    component ConfigButton: Button {
        id: action
        font.family: "Segoe UI"; font.pixelSize: 11; padding: 8
        contentItem: Text { text: action.text; font: action.font; color: action.enabled ? root.colors.text : root.colors.muted; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: action.hovered ? root.colors.panelAlt : root.colors.panel; border.color: root.colors.border; radius: 4 }
    }
}
