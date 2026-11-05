import QtQuick 6.0

// Usage:
//   DistanceBadge { oneWayKm: 30.8; tripKm: 30.8; maxKm: 15; visible: hasResult }

Item {
    id: root

    property real   oneWayKm: 0
    property real   tripKm:   0
    property real   maxKm:    0
    property bool   inRange:  tripKm <= maxKm

    implicitHeight: 26
    implicitWidth:  badgeRow.implicitWidth + 16

    Rectangle {
        anchors.fill: parent
        radius:       5
        color:        root.inRange ? Theme.greenLight : Theme.redLight
        border.color: root.inRange ? "#C0DD97" : "#F7C1C1"
        border.width: 0.5
    }

    Row {
        id: badgeRow
        anchors.centerIn: parent
        spacing: 8

        Text {
            text:           "One-way: " + root.oneWayKm.toFixed(1) + " km"
            font.family:    Theme.fontFamily
            font.pixelSize: Theme.fontSm
            font.weight:    Font.Medium
            color:          root.inRange ? Theme.green : Theme.red
        }

        Rectangle { width: 1; height: 14; color: root.inRange ? "#C0DD97" : "#F7C1C1"; anchors.verticalCenter: parent.verticalCenter }

        Text {
            text:           "Trip: " + root.tripKm.toFixed(1) + " km"
            font.family:    Theme.fontFamily
            font.pixelSize: Theme.fontSm
            font.weight:    Font.Medium
            color:          root.inRange ? Theme.green : Theme.red
        }

        Rectangle { width: 1; height: 14; color: root.inRange ? "#C0DD97" : "#F7C1C1"; anchors.verticalCenter: parent.verticalCenter }

        Text {
            text:           root.inRange ? "In range" : "EXCEEDS RANGE"
            font.family:    Theme.fontFamily
            font.pixelSize: Theme.fontSm
            font.weight:    Font.Medium
            color:          root.inRange ? Theme.green : Theme.red
        }
    }
}
