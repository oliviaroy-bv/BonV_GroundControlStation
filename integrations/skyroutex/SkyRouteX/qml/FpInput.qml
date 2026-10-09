import QtQuick 6.0
import QtQuick.Controls 6.0

// Usage:
//   FpInput { placeholder: "Latitude"; width: 120 }
//   FpInput { text: "28.7381"; validator: DoubleValidator {} }

TextField {
    id: root

    property bool   tinted: false   // blue-tinted background (output dir etc.)

    implicitHeight: Theme.inputH
    implicitWidth:  160

    font.family:    Theme.fontFamily
    font.pixelSize: Theme.fontBase
    color:          Theme.textPrimary
    placeholderTextColor: Theme.textHint

    leftPadding:    8
    rightPadding:   8
    topPadding:     0
    bottomPadding:  0
    verticalAlignment: TextInput.AlignVCenter

    background: Rectangle {
        color:        root.tinted
                          ? (root.activeFocus ? "#F0F7FD" : "#EBF4FB")
                          : (root.activeFocus ? Theme.bgInputFocus : Theme.bgInput)
        border.color: root.activeFocus ? Theme.borderFocus : Theme.border
        border.width: root.activeFocus ? 1 : 0.5
        radius:       Theme.radius
    }
}
