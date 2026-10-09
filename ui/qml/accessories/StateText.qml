import QtQuick
import QtQuick.Layouts

Column {
    property string iconType: ""
    property string label: ""
    property string value: ""
    property color valueColor: "white"

    Layout.alignment: Qt.AlignVCenter
    Layout.preferredWidth: 78
    spacing: 1

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 3

        TelemetryIcon {
            iconType: parent.parent.iconType
            iconColor: parent.parent.valueColor
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: parent.parent.label
            color: parent.parent.valueColor
            font.pixelSize: 13
            font.bold: true
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        visible: parent.value.length > 0
        text: parent.value
        color: "white"
        font.pixelSize: 11
        font.bold: true
    }
}
