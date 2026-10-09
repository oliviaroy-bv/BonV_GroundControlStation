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

        // App icon (house / waypoint shape)
        Canvas {
            width: 22; height: 22; anchors.verticalCenter: parent.verticalCenter
            onPaint: {
                var c = getContext("2d")
                c.clearRect(0, 0, width, height)
                // roof fill
                c.fillStyle = "#D4EAFB"
                c.beginPath(); c.moveTo(11,2); c.lineTo(21,9); c.lineTo(21,20); c.lineTo(1,20); c.lineTo(1,9); c.closePath(); c.fill()
                // roof stroke
                c.strokeStyle = Theme.blueMid; c.lineWidth = 1.2; c.lineJoin = "round"
                c.beginPath(); c.moveTo(11,2); c.lineTo(21,9); c.lineTo(21,20); c.lineTo(1,20); c.lineTo(1,9); c.closePath(); c.stroke()
                // peak dot
                c.fillStyle = Theme.blueMid
                c.beginPath(); c.arc(11, 2, 1.5, 0, Math.PI*2); c.fill()
                // nav line
                c.strokeStyle = Theme.blue; c.lineWidth = 1
                c.beginPath(); c.moveTo(5, 15); c.lineTo(11, 11); c.lineTo(17, 15); c.stroke()
            }
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
