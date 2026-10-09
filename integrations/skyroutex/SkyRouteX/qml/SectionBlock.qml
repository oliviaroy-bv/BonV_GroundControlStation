import QtQuick 6.0
import QtQuick.Layouts 6.0

Item {
    id: sb

    property string title: ""
    default property alias content: innerCol.children

    implicitHeight: hdr.height + 10 + innerCol.implicitHeight + 14

    Rectangle {
        anchors.bottom: parent.bottom
        height: 0.5; width: parent.width
        color: Theme.borderLight
    }

    Text {
        id: hdr
        anchors {
            top: parent.top; topMargin: 14
            left: parent.left; leftMargin: 16
        }
        text:               sb.title.toUpperCase()
        font.family:        Theme.fontFamily
        font.pixelSize:     Theme.fontXs
        font.weight:        Font.Medium
        font.letterSpacing: 0.8
        color:              Theme.textMuted
    }

    ColumnLayout {
        id: innerCol
        anchors {
            top: hdr.bottom; topMargin: 10
            left: parent.left; right: parent.right
            leftMargin: 16; rightMargin: 16
        }
        spacing: 0
    }
}
