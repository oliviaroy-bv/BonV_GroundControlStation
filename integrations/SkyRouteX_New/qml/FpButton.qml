import QtQuick 6.0
import QtQuick.Controls 6.0

// Usage:
//   FpButton { text: "Generate Waypoints"; primary: true }
//   FpButton { text: "Browse…" }

Button {
    id: root

    property bool primary: false

    implicitHeight: primary ? (Theme.isAndroid ? 48 : 42)
                            : (Theme.isAndroid ? 38 : 30)
    implicitWidth:  80

    font.family:    Theme.fontFamily
    font.pixelSize: primary ? Theme.fontMd : Theme.fontBase
    font.weight:    primary ? Font.Medium : Font.Normal

    contentItem: Text {
        text:               root.text
        font:               root.font
        color:              root.primary
                                ? "#FFFFFF"
                                : (root.hovered ? Theme.blue : Theme.textSecond)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment:   Text.AlignVCenter
        elide:              Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusLg
        color: root.primary
               ? (root.pressed ? Theme.blue : (root.hovered ? "#1A6DB5" : Theme.blue))
               : (root.pressed ? Theme.bgSection
                               : (root.hovered ? Theme.blueLight : Theme.bg))
        border.color: root.primary ? "transparent" : (root.hovered ? Theme.blueBorder : Theme.border)
        border.width: root.primary ? 0 : 0.5

        Behavior on color { ColorAnimation { duration: 100 } }
    }

    scale: root.pressed ? 0.98 : 1.0
    Behavior on scale { NumberAnimation { duration: 80 } }
}
