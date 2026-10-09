import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    width: 1060
    height: 720
    color: "#06101a"

    property string startupMessage: ""
    property color accentColor: "#2aa7ee"
    property color textColor: "#182437"
    property color mutedColor: "#687a90"
    property color borderColor: "#d8e4f0"

    Image {
        anchors.fill: parent
        source: "qrc:/backgrounds/topo_background.png"
        fillMode: Image.PreserveAspectCrop
        smooth: true
        opacity: 0.35
    }

    Rectangle {
        anchors.fill: parent
        color: "#88020b14"
    }

    Rectangle {
        id: activationCard

        width: Math.min(620, root.width - 42)
        height: Math.min(430, root.height - 42)
        anchors.centerIn: parent
        radius: 18
        color: "#f7fbff"
        border.color: "#dbe7f3"
        border.width: 1
        clip: true

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 17
            color: "#00000000"
            border.color: "#ffffff"
            border.width: 1
        }

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.preferredWidth: Math.max(210, Math.min(250, activationCard.width * 0.42))
                Layout.fillHeight: true
                color: "#eef7ff"

                Rectangle {
                    anchors.fill: parent
                    color: "#55ffffff"
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 38
                    anchors.rightMargin: 26
                    anchors.topMargin: 54
                    anchors.bottomMargin: 28
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Image {
                            Layout.preferredWidth: 42
                            Layout.preferredHeight: 42
                            source: "qrc:/logo/BonVGCS_Icon.png"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Text {
                                text: "BonV"
                                color: "#16263a"
                                font.pixelSize: 22
                                font.bold: true
                            }

                            Text {
                                text: "GROUNDSTATION"
                                color: "#596d82"
                                font.pixelSize: 9
                                font.bold: true
                                font.letterSpacing: 1.2
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    Image {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 178
                        source: "qrc:/backgrounds/drone_model.png"
                        fillMode: Image.PreserveAspectCrop
                        horizontalAlignment: Image.AlignHCenter
                        verticalAlignment: Image.AlignVCenter
                        smooth: true
                        mipmap: true
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 7

                        FieldGlyph {
                            Layout.preferredWidth: 13
                            Layout.preferredHeight: 13
                            iconType: "lock"
                            iconColor: "#61768c"
                        }

                        Text {
                            text: "Secure Activation"
                            color: "#607387"
                            font.pixelSize: 10
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#fbfdff"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 36
                    anchors.rightMargin: 36
                    anchors.topMargin: 54
                    anchors.bottomMargin: 38
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: "Welcome"
                        color: root.textColor
                        font.pixelSize: 19
                        font.bold: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Activate your license to continue"
                        color: root.mutedColor
                        font.pixelSize: 11
                    }

                    Item { Layout.preferredHeight: 6 }

                    AuthTextField {
                        id: licenseKeyField
                        Layout.fillWidth: true
                        placeholderText: "Enter License Key"
                        iconType: "key"
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        radius: 4
                        color: "#fbfdff"
                        border.color: root.borderColor
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 8
                            spacing: 9

                            FieldGlyph {
                                Layout.preferredWidth: 15
                                Layout.preferredHeight: 15
                                iconType: "machine"
                                iconColor: "#667b91"
                            }

                            TextField {
                                id: machineIdField
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: "Machine ID (Auto Detected)"
                                color: "#435469"
                                font.pixelSize: 10
                                readOnly: true
                                selectByMouse: true
                                background: Rectangle { color: "#00000000" }
                                Component.onCompleted: text = backend.activationId().length > 0 ? backend.activationId() : "Machine ID (Auto Detected)"
                            }

                            ToolButton {
                                id: copyButton
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                padding: 0
                                flat: true
                                hoverEnabled: true
                                ToolTip.visible: hovered
                                ToolTip.text: "Copy Machine ID"
                                contentItem: Item {
                                    FieldGlyph {
                                        anchors.centerIn: parent
                                        width: 15
                                        height: 15
                                        iconType: "copy"
                                        iconColor: copyButton.hovered ? root.accentColor : "#667b91"
                                    }
                                }
                                background: Rectangle {
                                    radius: 4
                                    color: copyButton.hovered ? "#edf7ff" : "#00000000"
                                }
                                onClicked: {
                                    machineIdField.text = backend.activationId()
                                    machineIdField.selectAll()
                                    machineIdField.copy()
                                }
                            }
                        }
                    }

                    Text {
                        id: errorLabel
                        Layout.fillWidth: true
                        visible: text.length > 0 || root.startupMessage.length > 0
                        text: root.startupMessage
                        color: "#d84b4b"
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }

                    AuthButton {
                        Layout.fillWidth: true
                        text: "ACTIVATE"
                        primary: true
                        onClicked: backend.onValidateKey(licenseKeyField.text.trim(), "Device Based")
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        var status = backend.activationStatus()
        if (status === "EXPIRED")
            root.startupMessage = "Your license has expired. Please enter a new activation key."
    }

    Connections {
        target: backend
        function onActivationFailed(message) {
            errorLabel.text = message
        }
    }

    component AuthTextField: TextField {
        id: field

        property string iconType: "key"

        Layout.preferredHeight: 38
        leftPadding: 40
        rightPadding: 12
        topPadding: 9
        bottomPadding: 8
        color: root.textColor
        placeholderTextColor: "#8b9bad"
        font.pixelSize: 10
        selectByMouse: true

        background: Rectangle {
            radius: 4
            color: "#fbfdff"
            border.color: field.activeFocus ? root.accentColor : root.borderColor
            border.width: 1
        }

        FieldGlyph {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 14
            width: 15
            height: 15
            iconType: field.iconType
            iconColor: field.activeFocus ? root.accentColor : "#667b91"
        }
    }

    component AuthButton: Button {
        id: authButton

        property bool primary: true

        Layout.preferredHeight: 46
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: Text {
            text: authButton.text
            color: authButton.primary ? "#ffffff" : root.accentColor
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 4
            color: authButton.primary
                   ? authButton.down ? "#167cc4" : authButton.hovered ? "#2396df" : "#2b9eea"
                   : authButton.down ? "#edf7ff" : authButton.hovered ? "#f4fbff" : "#ffffff"
            border.color: authButton.primary ? "#2b9eea" : root.accentColor
            border.width: 1
        }
    }

    component FieldGlyph: Canvas {
        property string iconType: ""
        property color iconColor: "#667b91"

        onIconTypeChanged: requestPaint()
        onIconColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            var cx = w / 2
            var cy = h / 2
            ctx.reset()
            ctx.strokeStyle = iconColor
            ctx.fillStyle = iconColor
            ctx.lineWidth = Math.max(1.3, Math.min(w, h) * 0.09)
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (iconType === "key") {
                ctx.beginPath(); ctx.arc(w * 0.36, cy, w * 0.17, 0, Math.PI * 2); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(w * 0.48, cy); ctx.lineTo(w * 0.84, cy); ctx.moveTo(w * 0.70, cy); ctx.lineTo(w * 0.70, h * 0.64); ctx.moveTo(w * 0.80, cy); ctx.lineTo(w * 0.80, h * 0.58); ctx.stroke()
            } else if (iconType === "machine") {
                ctx.strokeRect(w * 0.16, h * 0.22, w * 0.68, h * 0.46)
                ctx.beginPath(); ctx.moveTo(w * 0.38, h * 0.82); ctx.lineTo(w * 0.62, h * 0.82); ctx.moveTo(cx, h * 0.68); ctx.lineTo(cx, h * 0.82); ctx.stroke()
            } else if (iconType === "copy") {
                ctx.strokeRect(w * 0.32, h * 0.18, w * 0.44, h * 0.56)
                ctx.beginPath()
                ctx.moveTo(w * 0.22, h * 0.36)
                ctx.lineTo(w * 0.22, h * 0.84)
                ctx.lineTo(w * 0.62, h * 0.84)
                ctx.stroke()
            } else if (iconType === "lock") {
                ctx.strokeRect(w * 0.25, h * 0.48, w * 0.50, h * 0.36)
                ctx.beginPath(); ctx.arc(cx, h * 0.48, w * 0.18, Math.PI, 0); ctx.stroke()
            }
        }
    }
}
