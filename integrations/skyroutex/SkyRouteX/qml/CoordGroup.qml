import QtQuick 6.0
import QtQuick.Layouts 6.0

// Usage:
//   CoordGroup { label: "Takeoff"; onLatChanged: ... }

Item {
    id: root

    property string label:    "Takeoff"
    property alias  name:     nameField.text
    property alias  lat:      latField.text
    property alias  lon:      lonField.text

    implicitWidth:  parent ? parent.width : 280
    implicitHeight: col.implicitHeight + 20

    Rectangle {
        anchors.fill:   parent
        radius:         Theme.radiusLg
        color:          Theme.bgSection
        border.color:   Theme.borderLight
        border.width:   0.5
    }

    ColumnLayout {
        id: col
        anchors { left: parent.left; right: parent.right; top: parent.top }
        anchors.margins: 10
        spacing: 6

        // Label
        Text {
            text:           root.label.toUpperCase()
            font.family:    Theme.fontFamily
            font.pixelSize: Theme.fontXs
            font.weight:    Font.Medium
            font.letterSpacing: 0.7
            color:          Theme.textMuted
        }

        // Name
        FpInput {
            id: nameField
            Layout.fillWidth: true
            implicitHeight:   28
            placeholderText:  "Location name (max 7 chars)"
            maximumLength:    7
            font.pixelSize:   Theme.fontSm
        }

        // Lat / Lon pair
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            FpInput {
                id: latField
                Layout.fillWidth: true
                implicitHeight:   28
                placeholderText:  "Latitude"
                font.pixelSize:   Theme.fontSm
                validator: DoubleValidator { bottom: -90; top: 90; notation: DoubleValidator.StandardNotation }
            }

            FpInput {
                id: lonField
                Layout.fillWidth: true
                implicitHeight:   28
                placeholderText:  "Longitude"
                font.pixelSize:   Theme.fontSm
                validator: DoubleValidator { bottom: -180; top: 180; notation: DoubleValidator.StandardNotation }
            }
        }
    }
}
