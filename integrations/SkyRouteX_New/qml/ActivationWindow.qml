// import QtQuick 6.0

// Item {

// }
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    width: 1060
    height: 720
    color: "#F5F5F5"

    property string startupMessage: ""

    Rectangle {
        id: activationCard

        width: 440
        height: 620

        anchors.centerIn: parent

        radius: 12
        color: "#F3F3F3"

        Column {
            anchors.fill: parent
            anchors.margins: 25

            spacing: 20

            Item {
                width: parent.width
                height: 50

                Image {
                    source: "qrc:/images/bonv_logo.png"
                    width: 120
                    height: 40
                    anchors.horizontalCenter: parent.horizontalCenter
                    fillMode: Image.PreserveAspectFit
                }
            }

            Label {
                text: "Product Activation"
                font.pixelSize: 20
                font.bold: true

                anchors.horizontalCenter: parent.horizontalCenter
            }

            Column {
                width: parent.width
                spacing: 10

                Label {
                    text: "Activation Type:"
                    font.pixelSize: 14
                }

                ComboBox {
                    id: activationType

                    width: parent.width

                    model: [
                        "Device Based",
                        "Drone Based"
                    ]
                }
            }

            TextField {
                id: licenseKeyField

                width: parent.width
                height: 50

                placeholderText: "Enter License Key"

                selectByMouse: true
            }

            Row {
                spacing: 40

                CheckBox {
                    text: "BonV"
                }

                CheckBox {
                    text: "Manufacturer"
                }
            }

            Item {
                height: 10
            }

            // Label {
            //     text: "Activation ID"
            //     font.bold: true
            // }

            // Column {
            //     visible: backend.isAndroid()

            //     width: parent.width
            //     spacing: 6

            //     Label {
            //         text: "Device ID"
            //         font.bold: true
            //         font.pixelSize: 14
            //     }

            //     Rectangle {
            //         width: parent.width
            //         height: 44

            //         radius: 6
            //         color: "#F7F7F7"
            //         border.color: "#D0D0D0"

            //         RowLayout {
            //             anchors.fill: parent
            //             anchors.margins: 8

            //             TextField {
            //                 id: deviceIdField

            //                 Layout.fillWidth: true

            //                 text: backend.activationId()
            //                 readOnly: true

            //                 background: Rectangle {
            //                     color: "transparent"
            //                 }
            //             }

            //             ToolButton {
            //                 text: "📋"

            //                 onClicked: {
            //                     deviceIdField.selectAll()
            //                     deviceIdField.copy()
            //                 }
            //             }

            //             Label {
            //                 id: copiedText
            //                 visible: false
            //                 text: "Copied"
            //                 color: "green"
            //             }

            //             Timer {
            //                 id: copiedTimer
            //                 interval: 1500
            //                 repeat: false
            //                 onTriggered: copiedText.visible = false
            //             }
            //         }
            //     }
            // }

            Column {
                visible: backend.isAndroid()

                width: parent.width
                spacing: 6

                Label {
                    text: "Device ID"
                    font.bold: true
                }

                Row {
                    width: parent.width
                    spacing: 8

                    TextField {
                        id: deviceIdField

                        width: parent.width - 50

                        text: backend.activationId()

                        readOnly: true
                    }

                    ToolButton {
                        text: "📋"

                        onClicked: {
                            deviceIdField.selectAll()
                            deviceIdField.copy()
                        }
                    }
                }
            }

            // TextArea {
            //     text: backend.activationId()

            //     readOnly: true

            //     wrapMode: Text.WrapAnywhere

            //     width: parent.width
            //     height: 80
            // }

            Label {
                visible: startupMessage.length > 0

                text: startupMessage

                color: "#D32F2F"

                wrapMode: Text.WordWrap

                width: parent.width

                horizontalAlignment: Text.AlignHCenter
            }

            Component.onCompleted: {
                let status = backend.activationStatus()

                console.log("Activation status =", status)

                if (status === "EXPIRED") {
                    startupMessage =
                        "Your license has expired. Please enter a new activation key."
                }
            }

            Button {
                id: activateButton

                width: parent.width
                height: 60

                text: "Activate Product"

                font.pixelSize: 16
                font.bold: true

                background: Rectangle {
                    radius: 8
                    color: "#000000"
                }

                contentItem: Text {
                    text: activateButton.text

                    color: "white"

                    font.pixelSize: 16
                    font.bold: true

                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter

                    anchors.fill: parent
                }

                // onClicked: {
                //     backend.onValidateKey(
                //         licenseKeyField.text.trim()
                //     )
                // }
                onClicked: {
                    backend.onValidateKey(
                        licenseKeyField.text.trim(),
                        activationType.currentText)
                }
            }

            Item {
                Layout.fillHeight: true
                height: 250
            }

            Label {
                text: "Powered by BonV Aero"

                color: "#666666"
                font.pixelSize: 14

                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    Connections {
        target: backend

        function onActivationFailed(message) {
            errorLabel.text = message
            errorLabel.visible = true
        }
    }

    Label {
        id: errorLabel

        visible: false

        anchors.horizontalCenter: activationCard.horizontalCenter
        anchors.top: activationCard.bottom
        anchors.topMargin: 15

        color: "red"
        font.pixelSize: 14
    }
}
