import QtQuick 6.0

// Usage:
//   TripToggle { onModeChanged: function(isReturn) { ... } }

Item {
    id: root

    property bool isReturn: false
    signal modeChanged(bool isReturn)

    function forceReturn() {
        root.isReturn = true
        root.modeChanged(true)
    }

    implicitHeight: Theme.inputH
    implicitWidth:  180

    Rectangle {
        anchors.fill: parent
        radius:       Theme.radius
        color:        Theme.bgInput
        border.color: Theme.border
        border.width: 0.5

        Row {
            anchors.fill: parent

            Repeater {
                model: ["One-Way", "Return"]
                delegate: Rectangle {
                    width:  root.width / 2
                    height: root.height
                    radius: index === 0 ? Theme.radius : 0
                    // Right item needs right-side radius only
                    // QML trick: clip with custom radius via layer
                    color: {
                        if (index === 0 && !root.isReturn) return Theme.blueMid
                        if (index === 1 &&  root.isReturn) return Theme.blueMid
                        return "transparent"
                    }

                    Behavior on color { ColorAnimation { duration: 120 } }

                    Text {
                        anchors.centerIn: parent
                        text:           modelData
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontBase
                        font.weight:    {
                            if (index === 0 && !root.isReturn) return Font.Medium
                            if (index === 1 &&  root.isReturn) return Font.Medium
                            return Font.Normal
                        }
                        color: {
                            if (index === 0 && !root.isReturn) return "#FFFFFF"
                            if (index === 1 &&  root.isReturn) return "#FFFFFF"
                            return Theme.textSecond
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.isReturn = (index === 1)
                            root.modeChanged(root.isReturn)
                        }
                    }
                }
            }

            // centre divider
            Rectangle {
                width: 0.5; height: parent.height
                color: Theme.border
                x: root.width / 2
            }
        }
    }
}
