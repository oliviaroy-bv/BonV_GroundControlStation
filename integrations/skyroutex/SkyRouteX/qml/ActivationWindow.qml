import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    width: 1060
    height: 720
    color: "#F5F5F5"

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
                    source: "qrc:/Images/bonv_logo.png"
                    width: 40
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
                        "Device Based"
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

                onClicked: {
                    backend.onValidateKey(
                        licenseKeyField.text.trim()
                    )
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
