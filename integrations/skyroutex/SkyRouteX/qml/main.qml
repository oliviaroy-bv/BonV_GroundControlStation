// import QtQuick 6.0
// import QtQuick.Window 6.0
// import QtQuick.Controls 6.0

// Window {
//     id: root
//     readonly property bool isAndroid: Qt.platform.os === "android"
//     property bool activated: backend.isActivated()
//     width: isAndroid ? Screen.width : 1060
//     height: isAndroid ? Screen.height : 720
//     minimumWidth: isAndroid ? 0 : 900
//     minimumHeight: isAndroid ? 0 : 620
//     visibility: isAndroid ? Window.FullScreen : Window.AutomaticVisibility
//     visible: true
//     title: "BonV — SkyRouteX"

//     MainView {
//         anchors.fill: parent
//     }
// }


import QtQuick 6.0
import QtQuick.Window 6.0
import QtQuick.Controls 6.0

Window {
    id: root

    readonly property bool isAndroid: Qt.platform.os === "android"

    property bool activated: backend.isActivated()

    width: isAndroid ? Screen.width : 1060
    height: isAndroid ? Screen.height : 720

    minimumWidth: isAndroid ? 0 : 900
    minimumHeight: isAndroid ? 0 : 620

    visibility: isAndroid ? Window.FullScreen : Window.AutomaticVisibility

    visible: true

    title: "BonV — SkyRouteX"

    Loader {
        anchors.fill: parent

        sourceComponent:
            root.activated
                ? mainViewComponent
                : activationComponent
    }

    Component {
        id: activationComponent

        ActivationWindow {
        }
    }

    Component {
        id: mainViewComponent

        MainView {
        }
    }

    Connections {
        target: backend

        function onActivationSucceeded()
        {
            successOverlay.visible = true
        }

        function onActivationFailed(message)
        {
            errorOverlay.message = message
            errorOverlay.visible = true
        }
    }

    Rectangle {
        id: successOverlay

        visible: false
        anchors.fill: parent
        color: "#80000000"
        z: 100

        Rectangle {
            width: 320
            height: 220
            radius: 12
            color: "white"

            anchors.centerIn: parent

            Column {
                anchors.centerIn: parent
                spacing: 15

                Label {
                    text: "✅"
                    font.pixelSize: 42
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Label {
                    text: "Activation Successful"
                    font.pixelSize: 18
                    font.bold: true
                }

                Label {
                    text: "Your product has been activated."
                }

                Button {
                    text: "OK"

                    anchors.horizontalCenter: parent.horizontalCenter

                    onClicked: {
                        successOverlay.visible = false
                        root.activated = true
                    }
                }
            }
        }
    }

    Rectangle {
        id: errorOverlay

        property string message: ""

        visible: false
        anchors.fill: parent
        color: "#80000000"
        z: 100

        Rectangle {
            width: 320
            height: 220
            radius: 12
            color: "white"

            anchors.centerIn: parent

            Column {
                anchors.centerIn: parent
                spacing: 15

                Label {
                    text: "❌"
                    font.pixelSize: 42
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Label {
                    text: "Activation Failed"
                    font.pixelSize: 18
                    font.bold: true
                }

                Label {
                    width: 250
                    wrapMode: Text.WordWrap
                    text: errorOverlay.message
                    horizontalAlignment: Text.AlignHCenter
                }

                Button {
                    text: "OK"

                    anchors.horizontalCenter: parent.horizontalCenter

                    onClicked: {
                        errorOverlay.visible = false
                    }
                }
            }
        }
    }
}
