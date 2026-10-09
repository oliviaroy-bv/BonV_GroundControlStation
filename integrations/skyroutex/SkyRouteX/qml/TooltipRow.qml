import QtQuick 6.0
import QtQuick.Layouts 6.0

RowLayout {
    property string label:     ""
    property string value:     ""
    property bool   highlight: false

    Text {
        text:                  label + ":"
        font.family:           Theme.fontFamily
        font.pixelSize:        10
        color:                 Theme.textMuted
        Layout.preferredWidth: 58
    }
    Text {
        text:        value
        font.family: Theme.fontFamily
        font.pixelSize: 10
        font.weight: Font.Medium
        color:       highlight ? Theme.red : Theme.textPrimary
    }
}
