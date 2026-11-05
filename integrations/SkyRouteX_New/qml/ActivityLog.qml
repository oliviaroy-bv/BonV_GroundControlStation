import QtQuick 6.0
import QtQuick.Controls 6.0

// Usage:
//   ActivityLog { id: log }
//   log.appendLine("INFO",  "Mission started")
//   log.appendLine("OK",    "Tile ready: N23 E077")
//   log.appendLine("WARN",  "Battery margin tight")
//   log.appendLine("ERROR", "Write failed")

Item {
    id: root

    function appendLine(level, message) {
        var now = new Date()
        var hh  = String(now.getHours()).padStart(2,"0")
        var mm  = String(now.getMinutes()).padStart(2,"0")
        var ss  = String(now.getSeconds()).padStart(2,"0")
        logModel.append({ ts: hh+":"+mm+":"+ss, lvl: level, msg: message })
        listView.positionViewAtEnd()
    }

    function clear() { logModel.clear() }

    // ── Header ────────────────────────────────────────────────────────────────
    Rectangle {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: Theme.logHeaderH
        color:  Theme.bg
        border.color: Theme.border; border.width: 0
        Rectangle { anchors.bottom: parent.bottom; height: 0.5; color: Theme.border; width: parent.width }

        Row {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 14 }
            spacing: 7

            // Log icon
            Canvas {
                width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter
                onPaint: {
                    var c = getContext("2d")
                    c.strokeStyle = Theme.textMuted
                    c.lineWidth = 1
                    c.strokeRect(0.5, 1.5, 13, 11)
                    c.beginPath(); c.moveTo(3, 5.5); c.lineTo(11, 5.5); c.stroke()
                    c.beginPath(); c.moveTo(3, 8);   c.lineTo(8,  8);   c.stroke()
                }
            }

            Text {
                text:           "Activity Log"
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontBase
                font.weight:    Font.Medium
                color:          Theme.textPrimary
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Clear button
        Rectangle {
            anchors { right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
            width: clearBtn.implicitWidth + 12; height: 22; radius: 4
            color: clearArea.containsMouse ? Theme.bgSection : "transparent"
            border.color: clearArea.containsMouse ? Theme.border : "transparent"; border.width: 0.5

            Text {
                id: clearBtn
                anchors.centerIn: parent
                text:           "Clear"
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontSm
                color:          Theme.textMuted
            }
            MouseArea {
                id: clearArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape:  Qt.PointingHandCursor
                onClicked:    root.clear()
            }
        }
    }

    // ── Log list ──────────────────────────────────────────────────────────────
    ListView {
        id: listView
        anchors {
            top:    header.bottom
            left:   parent.left
            right:  parent.right
            bottom: parent.bottom
        }
        clip: true
        model: ListModel { id: logModel }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            width:  6
        }

        delegate: Item {
            width:  listView.width
            height: row.implicitHeight + 2

            Row {
                id: row
                anchors { left: parent.left; leftMargin: 14; right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
                spacing: 12

                Text {
                    text:           model.ts
                    font.family:    Theme.fontMono
                    font.pixelSize: Theme.fontSm
                    color:          Theme.textHint
                }

                Text {
                    text:       model.msg
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontSm
                    wrapMode:   Text.WrapAtWordBoundaryOrAnywhere
                    width:      row.width - 60
                    color: {
                        switch(model.lvl) {
                            case "OK":    return Theme.green
                            case "WARN":  return Theme.amber
                            case "ERROR": return Theme.red
                            default:      return Theme.textSecond
                        }
                    }
                }
            }
        }
    }
}
