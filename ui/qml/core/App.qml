// import QtQuick
// import QtQuick.Controls
// import QtQuick.Window
// import "../activation"

// ApplicationWindow {
//     id: root

//     property bool activated: backend.isActivated()

//     width: 1280
//     height: 760
//     minimumWidth: 900
//     minimumHeight: 620
//     visible: true
//     title: "BonVGroundStation"

//     ActivationWindow {
//         anchors.fill: parent
//         visible: !root.activated
//         enabled: !root.activated
//     }

//     LoginWindow {
//         anchors.fill: parent
//         visible: root.activated
//         enabled: root.activated

//         onLoginSucceeded: root.launchMainWindow()
//     }

//     Connections {
//         target: backend

//         function onActivationSucceeded() {
//             successOverlay.visible = true
//         }
//     }

//     Rectangle {
//         id: successOverlay

//         visible: false
//         anchors.fill: parent
//         color: "#80000000"
//         z: 100

//         Rectangle {
//             width: 320
//             height: 220
//             radius: 12
//             color: "white"

//             anchors.centerIn: parent

//             Column {
//                 anchors.centerIn: parent
//                 spacing: 15

//                 Label {
//                     text: "✅"
//                     font.pixelSize: 42
//                     anchors.horizontalCenter: parent.horizontalCenter
//                 }

//                 Label {
//                     text: "Activation Successful"
//                     font.pixelSize: 18
//                     font.bold: true
//                 }

//                 Label {
//                     text: "Your product has been activated."
//                 }

//                 Button {
//                     text: "Continue"

//                     anchors.horizontalCenter: parent.horizontalCenter

//                     onClicked: {
//                         successOverlay.visible = false
//                         root.launchMainWindow()
//                     }
//                 }
//             }
//         }
//     }

//     // function launchMainWindow() {
//     //     mainWindowComponent.createObject(null)
//     //     root.close()
//     // }

//     function launchMainWindow() {

//         var w = mainWindowComponent.createObject(null)

//         console.log("Created:", w)

//         if (w) {
//             w.show()
//         }

//         // root.close()
//     }

//     Component {
//         id: mainWindowComponent

//         MainWindow {
//         }
//     }
// }


import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../activation"

ApplicationWindow {
    id: root

    property bool activated: backend.isActivated()

    width: 1280
    height: 760
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: "BonV GroundStation"

    Loader {
        id: pageLoader
        anchors.fill: parent
    }

    Component {
        id: activationPage

        ActivationWindow {
            anchors.fill: parent

            Connections {
                target: backend

                function onActivationSucceeded() {
                    successOverlay.visible = true
                }
            }
        }
    }

    Component {
        id: loginPage

        LoginWindow {
            anchors.fill: parent

            onLoginSucceeded: {
                pageLoader.sourceComponent = mainPage
            }
        }
    }

    Component {
        id: mainPage

        MainPage {
            anchors.fill: parent
        }
    }

    Component.onCompleted: {
        if (backend.isActivated())
            pageLoader.sourceComponent = loginPage
        else
            pageLoader.sourceComponent = activationPage
    }

    Rectangle {
        id: successOverlay

        anchors.fill: parent
        visible: false
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
                    text: "Continue"
                    anchors.horizontalCenter: parent.horizontalCenter

                    onClicked: {
                        successOverlay.visible = false
                        pageLoader.sourceComponent = loginPage
                    }
                }
            }
        }
    }
}