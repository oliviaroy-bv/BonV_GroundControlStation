import QtQuick
import QtQuick.Layouts

Column {
    property string iconType: ""
    property string label: ""
    property string value: ""
    property color valueColor: "white"
    property int labelPixelSize: 11
    property int valuePixelSize: 14
    property int iconSize: 13

    Layout.alignment: Qt.AlignVCenter
    Layout.preferredWidth: 82
    spacing: 1

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 3

        TelemetryIcon {
            iconType: parent.parent.iconType
            iconColor: "#fbfbfb"
            width: parent.parent.iconSize
            height: parent.parent.iconSize
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: parent.parent.label
            color: "#fbfbfb"
            font.pixelSize: parent.parent.labelPixelSize
            font.bold: true
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        text: parent.value
        color: parent.valueColor
        font.pixelSize: parent.valuePixelSize
        font.bold: true
    }
}
