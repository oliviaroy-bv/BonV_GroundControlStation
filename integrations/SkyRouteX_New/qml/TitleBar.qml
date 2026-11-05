import QtQuick 6.0

Item {
    id: root
    implicitHeight: Theme.titleH

    Rectangle {
        anchors.fill: parent
        color:        Theme.bg
        Rectangle { anchors.bottom: parent.bottom; height: 0.5; color: Theme.border; width: parent.width }
    }

    Row {
        anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
        spacing: 10

        Image {
            width: 62
            height: 28
            anchors.verticalCenter: parent.verticalCenter
            source: "qrc:/images/bonv_logo.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        Text {
            text:           "SkyrouteX"
            font.family:    Theme.fontFamily
            font.pixelSize: Theme.fontMd
            font.weight:    Font.Medium
            color:          Theme.textPrimary
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text:           "v2.0"
            font.family:    Theme.fontFamily
            font.pixelSize: Theme.fontSm
            color:          Theme.textMuted
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Row {
        anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
        spacing: 6

        // SRTM1 badge
        Rectangle {
            height: 20; width: srtmT.implicitWidth + 14; radius: 4
            color: Theme.blueLight; border.color: Theme.blueBorder; border.width: 0.5
            Text { id: srtmT; anchors.centerIn: parent; text: "Terrain"; font.family: Theme.fontFamily; font.pixelSize: 10; font.weight: Font.Medium; color: Theme.blue }
        }

        // ArduPilot badge
        Rectangle {
            height: 20; width: apT.implicitWidth + 14; radius: 4
            color: Theme.greenLight; border.color: "#C0DD97"; border.width: 0.5
            Text { id: apT; anchors.centerIn: parent; text: "BonV"; font.family: Theme.fontFamily; font.pixelSize: 10; font.weight: Font.Medium; color: Theme.green }
        }
    }
}
