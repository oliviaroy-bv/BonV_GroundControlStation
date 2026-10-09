import QtQuick 6.0
import QtQuick.Controls 6.0

ComboBox {
    id: root
    implicitHeight: Theme.inputH

    font.family:    Theme.fontFamily
    font.pixelSize: Theme.fontBase

    contentItem: Text {
        leftPadding:    8
        text:           root.displayText
        font:           root.font
        color:          Theme.textPrimary
        verticalAlignment: Text.AlignVCenter
        elide:          Text.ElideRight
    }

    background: Rectangle {
        color:        root.hovered ? "#F5F4F0" : Theme.bgInput
        border.color: root.down ? Theme.borderFocus : Theme.border
        border.width: root.down ? 1 : 0.5
        radius:       Theme.radius

        // Arrow indicator
        Canvas {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 10; height: 6
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = Theme.textMuted
                ctx.lineWidth = 1.2
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(0, 1)
                ctx.lineTo(5, 5)
                ctx.lineTo(10, 1)
                ctx.stroke()
            }
        }
    }

    popup: Popup {
        y:      root.height + 2
        width:  root.width
        implicitHeight: contentItem.implicitHeight + 2
        padding: 1

        contentItem: ListView {
            clip:         true
            implicitHeight: contentHeight
            model:        root.popup.visible ? root.delegateModel : null
            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            color:        Theme.bg
            border.color: Theme.border
            border.width: 0.5
            radius:       Theme.radius
        }
    }

    delegate: ItemDelegate {
        width: root.width
        contentItem: Text {
            text:           modelData
            font:           root.font
            color:          highlighted ? Theme.blue : Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
        }
        highlighted: root.highlightedIndex === index
        background: Rectangle {
            color: highlighted ? Theme.blueLight : "transparent"
        }
    }
}
