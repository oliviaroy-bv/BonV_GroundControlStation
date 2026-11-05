import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0

RowLayout {
    id: root

    property alias label:   lbl.text
    property alias unit:    unitLbl.text
    property alias value:   spin.value
    // property alias from:    spin.from
    // property alias to:      spin.to
    property int from: 0      // ← ADD these at root level
    property int to:   9999 
    property alias stepSize: spin.stepSize
    property alias enabled: spin.enabled
    property real  realValue: spin.value      // convenience
    // For decimal steps, use realStep + decimals
    property int   decimals: 0
    property real  realStep: 1.0
    readonly property bool hasLabel: lbl.text.length > 0
    readonly property bool hasUnit:  unitLbl.text.length > 0

    spacing: 8
    implicitHeight: Theme.isAndroid ? Theme.inputH : 28

    Text {
        id: lbl
        visible: root.hasLabel
        Layout.fillWidth: root.hasLabel
        font.family:    Theme.fontFamily
        font.pixelSize: Theme.fontBase
        color:          root.enabled ? Theme.textSecond : Theme.textHint
        verticalAlignment: Text.AlignVCenter
    }

    // Custom spin using TextField + up/down buttons so we can support decimals
    Row {
        spacing: 0

        Rectangle {
            width:  Theme.spinW
            height: root.implicitHeight
            radius: Theme.radius
            color:  root.enabled ? Theme.bgInput : Theme.bgSection
            // Red border if value is outside allowed range
            property bool outOfRange: spin.value < spin.from || spin.value > spin.to
            border.color: outOfRange ? Theme.red
                        : spin.activeFocus ? Theme.borderFocus
                        : Theme.border
            border.width: outOfRange ? 1.5
                        : spin.activeFocus ? 1 : 0.5

            TextInput {
                id: spin

                property int from:     root.from
                property int to:       root.to
                property int stepSize: 1
                property int value:    0

                onFromChanged: {
                    if (value < from) {
                        value = from
                    }

                    text = root.decimals > 0
                        ? value.toFixed(root.decimals)
                        : value.toString()
                }
                onToChanged: {
                    if (value > to) {
                        value = to
                    }

                    text = root.decimals > 0
                        ? value.toFixed(root.decimals)
                        : value.toString()
                }

                anchors.fill: parent
                anchors.leftMargin: 7
                anchors.rightMargin: 7
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontBase
                color:          root.enabled ? Theme.textPrimary : Theme.textHint
                horizontalAlignment: TextInput.AlignRight
                verticalAlignment:   TextInput.AlignVCenter
                enabled: root.enabled
                text: root.decimals > 0
                      ? value.toFixed(root.decimals)
                      : value.toString()

            //     // validator: IntValidator { bottom: spin.from; top: spin.to }
            //     validator: IntValidator {
            //         id: spinValidator
            //         bottom: root.from
            //         top: root.to
            //     }

            //     onTextEdited: {
            //             var v = parseInt(text)
            //             if (isNaN(v)) return

            //             if (v < root.from) {          // ← ADD this block
            //                 spin.value = root.from
            //                 text = root.from.toString()
            //                 cursorPosition = text.length
            //             }

            //             if (v > root.to) {
            //                 spin.value = root.to
            //                 text = root.to.toString()
            //                 cursorPosition = text.length
            //             }
            //         }

            //     inputMethodHints: Qt.ImhDigitsOnly
            //     onTextChanged: {
            //         var v = parseInt(text)
            //         if (!isNaN(v)) {
            //             var clamped = Math.max(root.from, Math.min(root.to, v))  // ← root.from, not spin.from
            //             value = clamped
            //             if (v !== clamped) {
            //                 text = clamped.toString()
            //                 cursorPosition = text.length
            //             }
            //         }
            //     }

            //     onEditingFinished: {
            //         var v = parseInt(text)
            //         if (isNaN(v)) v = root.from   // ← root.from as fallback
            //         v = Math.max(root.from, Math.min(root.to, v))   // ← root.from
            //         spin.value = v
            //         text = spin.value.toString()
            //     }
            // }


            // Replace IntValidator with this:
            validator: RegularExpressionValidator {
                regularExpression: /^[0-9]*$/
            }

            onTextEdited: {
                var v = parseInt(text)
                if (isNaN(v) || text === "") return

                if (v < root.from) {
                    spin.value = root.from
                    spin.text  = root.from.toString()
                    spin.cursorPosition = spin.text.length
                    return
                }
                if (v > root.to) {
                    spin.value = root.to
                    spin.text  = root.to.toString()
                    spin.cursorPosition = spin.text.length
                    return
                }
                spin.value = v
            }

            onEditingFinished: {
                var v = parseInt(text)
                if (isNaN(v)) v = root.from
                v = Math.max(root.from, Math.min(root.to, v))
                spin.value = v
                spin.text  = v.toString()
            }

            onTextChanged: {
                // Keep value property in sync silently
                var v = parseInt(text)
                if (!isNaN(v)) {
                    value = Math.max(root.from, Math.min(root.to, v))
                }
            }
        }
        }

        // Stepper column
        Column {
            spacing: 0

            Rectangle {
                width: Theme.isAndroid ? 28 : 22; height: root.implicitHeight / 2
                radius: Theme.radius
                color: stepUp.containsMouse ? Theme.bgSection : "transparent"
                border.color: Theme.border; border.width: 0.5
                enabled: root.enabled
                Text {
                    anchors.centerIn: parent
                    text: "▲"
                    font.pixelSize: Theme.isAndroid ? 9 : 7
                    color: root.enabled ? Theme.textMuted : Theme.textHint
                }
                MouseArea {
                    id: stepUp
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (!root.enabled) return
                        // spin.value = Math.min(spin.to, spin.value + spin.stepSize)
                        spin.value = Math.min(root.to, spin.value + spin.stepSize)

                        spin.text = root.decimals > 0
                                ? spin.value.toFixed(root.decimals)
                                : spin.value.toString()
                    }
                }
            }

            Rectangle {
                width: Theme.isAndroid ? 28 : 22; height: root.implicitHeight / 2
                radius: Theme.radius
                color: stepDown.containsMouse ? Theme.bgSection : "transparent"
                border.color: Theme.border; border.width: 0.5
                enabled: root.enabled
                Text {
                    anchors.centerIn: parent
                    text: "▼"
                    font.pixelSize: Theme.isAndroid ? 9 : 7
                    color: root.enabled ? Theme.textMuted : Theme.textHint
                }
                MouseArea {
                    id: stepDown
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (!root.enabled) return
                        // spin.value = Math.max(spin.from, spin.value - spin.stepSize)
                        spin.value = Math.max(root.from, spin.value - spin.stepSize)

                        spin.text = root.decimals > 0
                                ? spin.value.toFixed(root.decimals)
                                : spin.value.toString()
                    }
                }
            }
        }
    }

    Text {
        id: unitLbl
        visible: root.hasUnit
        Layout.preferredWidth: root.hasUnit
                               ? (root.hasLabel ? 52 : Math.max(unitLbl.implicitWidth, 18))
                               : 0
        font.family:    Theme.fontFamily
        font.pixelSize: Theme.fontSm
        color:          root.enabled ? Theme.textMuted : Theme.textHint
    }
}
