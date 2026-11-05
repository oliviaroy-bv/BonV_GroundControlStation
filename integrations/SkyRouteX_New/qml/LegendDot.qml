import QtQuick 6.0

Row {
    property string dotColor: "#000"
    property string label:    ""
    spacing: 4

    Rectangle {
        width:  10; height: 3; radius: 1.5
        color:  parent.dotColor
        anchors.verticalCenter: parent.verticalCenter
    }
    Text {
        text:           parent.label
        font.family:    Theme.fontFamily
        font.pixelSize: Theme.fontSm
        color:          Theme.textMuted
        anchors.verticalCenter: parent.verticalCenter
    }
}
