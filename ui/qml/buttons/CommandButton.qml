import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: commandButton

    property string label: ""
    property color buttonColor: "#273142"

    width: 55
    height: 42
    Layout.alignment: Qt.AlignVCenter
    text: label

    font.pixelSize: 9
    font.bold: true

    contentItem: Text {
        text: commandButton.text
        color: "white"
        font: commandButton.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 4
        color: commandButton.down ? Qt.darker(commandButton.buttonColor, 1.25) : commandButton.buttonColor
    }
}
