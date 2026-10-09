import QtQuick 6.0
import QtQuick.Layouts 6.0

RowLayout {
    id: fr

    property string label: ""
    default property alias trailing: trailSlot.children

    spacing: 8
    Layout.fillWidth: true

    Text {
        text:           fr.label
        font.family:    Theme.fontFamily
        font.pixelSize: Theme.fontBase
        color:          Theme.textSecond
        width:          Theme.isAndroid ? 104 : 86
        Layout.preferredWidth: Theme.isAndroid ? 104 : 86
    }

    Item {
        id: trailSlot
        Layout.fillWidth: true
        implicitHeight: Theme.inputH
    }
}
