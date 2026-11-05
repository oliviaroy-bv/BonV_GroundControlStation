import QtQuick 6.0

// Usage:
//   StatusBar { statusText: "Ready"; srtmInfo: "N23 E077 cached"; droneInfo: "Air-Orca · 7.5 km" }

Item {
    id: root

    property string statusText: "Ready"
    property bool   busy:       false
    property string srtmInfo:   ""
    property string droneInfo:  ""

    implicitHeight: Theme.statusH

    Rectangle {
        anchors.fill: parent
        color:        Theme.bg
        Rectangle { anchors.top: parent.top; height: 0.5; color: Theme.border; width: parent.width }
    }

    Row {
        anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
        spacing: 10

        // Status dot
        Rectangle {
            width: 6; height: 6; radius: 3
            anchors.verticalCenter: parent.verticalCenter
            color: root.busy ? Theme.amber : Theme.green

            SequentialAnimation on opacity {
                running:  root.busy
                loops:    Animation.Infinite
                NumberAnimation { to: 0.3; duration: 500 }
                NumberAnimation { to: 1.0; duration: 500 }
            }
        }

        Text {
            text:           root.statusText
            font.family:    Theme.fontFamily
            font.pixelSize: Theme.fontSm
            color:          Theme.textSecond
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 0.5; height: 12; color: Theme.border
            anchors.verticalCenter: parent.verticalCenter
            visible: root.srtmInfo !== ""
        }

        Rectangle {
            visible:        root.srtmInfo !== ""
            color:          Theme.greenLight
            radius:         4
            height:         18
            width:          srtmTxt.implicitWidth + 12
            anchors.verticalCenter: parent.verticalCenter
            Text {
                id: srtmTxt
                anchors.centerIn: parent
                text:           root.srtmInfo
                font.family:    Theme.fontFamily
                font.pixelSize: 10
                color:          Theme.green
                font.weight:    Font.Medium
            }
        }

        Rectangle {
            width: 0.5; height: 12; color: Theme.border
            anchors.verticalCenter: parent.verticalCenter
            visible: root.droneInfo !== ""
        }

        Text {
            visible:        root.droneInfo !== ""
            text:           root.droneInfo
            font.family:    Theme.fontFamily
            font.pixelSize: Theme.fontSm
            color:          Theme.textHint
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
