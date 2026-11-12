import QtQuick
import QtQuick.Controls

Button {
    id: root
    objectName: "mapParameterStatus"
    property var vehicle: null
    property var mavlinkObject: vehicle ? vehicle.mavlink : null
    readonly property bool parametersReady: details.ready
    readonly property int progressValue: details.progressValue
    readonly property color statusColor: details.statusColor
    readonly property string statusText: details.detailText
    width: 32
    height: 32
    padding: 0
    flat: true
    hoverEnabled: true
    Accessible.name: "Parameter status: " + statusText
    ToolTip.visible: hovered && !statusPopup.opened
    ToolTip.text: statusText
    onClicked: statusPopup.opened ? statusPopup.close() : statusPopup.open()
    onVehicleChanged: if (statusPopup) statusPopup.close()
    onMavlinkObjectChanged: if (statusPopup) statusPopup.close()
    onParametersReadyChanged: if (progressCanvas) progressCanvas.requestPaint()
    onProgressValueChanged: if (progressCanvas) progressCanvas.requestPaint()
    onStatusColorChanged: if (progressCanvas) progressCanvas.requestPaint()
    onStatusTextChanged: if (progressCanvas) progressCanvas.requestPaint()

    contentItem: Item {
        Canvas {
            id: progressCanvas
            anchors.centerIn: parent
            width: 28
            height: 28
            visible: !waitingSpinner.running
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.scale(width / 18, height / 18)
                ctx.lineWidth = 3.2
                ctx.lineCap = "round"
                ctx.strokeStyle = root.statusColor
                if (root.parametersReady) {
                    ctx.beginPath()
                    ctx.moveTo(3, 9); ctx.lineTo(7, 13); ctx.lineTo(15, 5)
                    ctx.stroke()
                } else {
                    ctx.strokeStyle = "#405467"
                    ctx.beginPath(); ctx.arc(9, 9, 6.5, 0, Math.PI * 2); ctx.stroke()
                    ctx.strokeStyle = root.statusColor
                    if (details.active) {
                        ctx.beginPath()
                        ctx.arc(9, 9, 6.5, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * root.progressValue / 100)
                        ctx.stroke()
                    } else {
                        ctx.beginPath()
                        if (details.connected && details.syncStateValue === 4) {
                            ctx.moveTo(9, 5); ctx.lineTo(9, 9)
                            ctx.moveTo(9, 12); ctx.lineTo(9, 12.2)
                        } else { ctx.moveTo(6, 9); ctx.lineTo(12, 9) }
                        ctx.stroke()
                    }
                }
            }
        }
        BusyIndicator {
            id: waitingSpinner
            anchors.centerIn: parent
            width: 28
            height: 28
            running: details.active && !!details.manager && details.manager.expectedParameterCount === 0
            visible: running
        }

    }
    background: Rectangle {
        radius: 5
        color: root.down ? "#ee008fb8" : root.hovered ? "#ee152231" : "#dd0b151e"
        border.color: "#88dff6ff"
        border.width: 1
    }
    Popup {
        id: statusPopup
        objectName: "parameterStatusPopup"
        x: -width - 8
        y: 0
        width: 340
        padding: 12
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: "#ee0c1720"
            radius: 6
            border.color: "#355a78"
        }
        contentItem: ParameterSyncStatus {
            id: details
            vehicle: root.vehicle
            mavlinkObject: root.mavlinkObject
        }
    }
}
