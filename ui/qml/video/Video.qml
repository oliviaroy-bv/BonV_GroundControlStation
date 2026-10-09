import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import BonV.Video 1.0

Rectangle {
    id: root

    property string rtspLink: ""
    property string cameraName: ""
    property var videoObject: null

    width: 360
    height: 210

    color: "#55000000"
    border.color: "#33ffffff"
    border.width: 1

    clip: true

    VideoItem {
        anchors.fill: parent
        video: root.videoObject
        visible: root.videoObject && root.videoObject.streaming
    }

    Rectangle {
        id: cameraNameBadge

        z: 3
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 10

        width: Math.min(parent.width - 20, Math.max(96, cameraNameText.implicitWidth + 24))
        height: 28
        radius: 6
        color: "#99000000"
        border.color: "#33ffffff"
        border.width: 1

        Text {
            id: cameraNameText
            anchors.fill: parent
            anchors.leftMargin: 5
            anchors.rightMargin: 10
            text: root.cameraName.length > 0 ? root.cameraName : "CAMERA"
            color: "white"
            font.pixelSize: 12
            font.bold: true
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    Item {
        id: cameraStatusBadge

        z: 3
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10

        width: 86
        height: 28

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 0
            anchors.rightMargin: 0
            spacing: 0

            Button {
                id: recordButton

                Layout.preferredWidth: 76
                Layout.preferredHeight: 24
                padding: 0
                flat: true
                hoverEnabled: true
                enabled: root.videoObject && (root.videoObject.streaming || root.videoObject.recording)
                visible: root.videoObject !== null
                text: root.videoObject && root.videoObject.recording ? "STOP" : "REC"
                ToolTip.visible: hovered
                ToolTip.text: root.videoObject && root.videoObject.recording
                              ? root.videoObject.recordingFilePath
                              : root.videoObject && root.videoObject.errorString.length > 0 ? root.videoObject.errorString : "Record video"

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    Rectangle {
                        Layout.preferredWidth: 9
                        Layout.preferredHeight: 9
                        Layout.alignment: Qt.AlignVCenter
                        radius: root.videoObject && root.videoObject.recording ? 2 : 5
                        color: root.videoObject && root.videoObject.recording ? "#ffffff" : "#ff3b30"
                    }

                    Text {
                        Layout.fillWidth: true
                        text: recordButton.text
                        color: recordButton.enabled ? "#ffffff" : "#84919a"
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                background: Rectangle {
                    radius: 5
                    color: !recordButton.enabled ? "#5510151d"
                           : recordButton.down ? "#9f1f26"
                           : recordButton.hovered ? "#b92730"
                           : root.videoObject && root.videoObject.recording ? "#c52d35" : "#7710151d"
                    border.color: root.videoObject && root.videoObject.recording ? "#ff7078" : "#66ffffff"
                    border.width: 1
                }

                onClicked: if (root.videoObject) root.videoObject.toggleRecording()
            }
        }
    }
    Rectangle {
        id: recordingStatusBubble

        z: 3
        anchors.right: cameraStatusBadge.right
        anchors.bottom: cameraStatusBadge.top
        anchors.bottomMargin: 6

        width: Math.min(root.width - 20, recordingStatusText.implicitWidth + 18)
        height: recordingStatusText.implicitHeight + 10
        radius: 4
        color: "#cc15191f"
        border.color: "#aaef5350"
        border.width: 1
        visible: root.videoObject
                 && !root.videoObject.recording
                 && root.videoObject.errorString.length > 0
                 && (root.videoObject.errorString.indexOf("Recording") === 0
                     || root.videoObject.errorString.indexOf("Wait for the first video frame") === 0
                     || root.videoObject.errorString.indexOf("Failed to create GStreamer recording") === 0
                     || root.videoObject.errorString.indexOf("Failed to start GStreamer recording") === 0)

        Text {
            id: recordingStatusText
            anchors.centerIn: parent
            width: parent.width - 14
            text: root.videoObject ? root.videoObject.errorString : ""
            color: "#ffe4e4"
            font.pixelSize: 11
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }


    Rectangle {
        z: 1
        visible: !root.videoObject || !root.videoObject.streaming

        anchors.fill: parent

        color: "#66000000"

        Column {
            anchors.centerIn: parent
            width: parent.width
            spacing: 10

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: true
                width: 40
                height: 40
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.rtspLink === ""
                      ? "No Camera Connected"
                      : root.videoObject && root.videoObject.errorString.length > 0
                        ? root.videoObject.errorString
                        : "Streaming with GStreamer..."

                color: "white"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    onRtspLinkChanged: {
        if (root.videoObject) {
            root.videoObject.rtspLink = rtspLink
            root.videoObject.cameraName = root.cameraName
            if (rtspLink !== "") {
                root.videoObject.start()
            } else {
                root.videoObject.stop()
            }
        }
    }

    Component.onCompleted: {
        if (root.videoObject) {
            root.videoObject.rtspLink = rtspLink
            root.videoObject.cameraName = root.cameraName
            if (rtspLink !== "") {
                root.videoObject.start()
            }
        }
    }

    onCameraNameChanged: if (root.videoObject) root.videoObject.cameraName = root.cameraName
}
