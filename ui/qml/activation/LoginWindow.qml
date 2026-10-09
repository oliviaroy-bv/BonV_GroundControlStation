import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    width: 1060
    height: 720
    color: "#06101a"

    property color accentColor: "#2aa7ee"
    property color textColor: "#182437"
    property color mutedColor: "#687a90"
    property color borderColor: "#d8e4f0"

    signal loginSucceeded()

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
        id: loginCard

        width: Math.min(620, root.width - 42)
        height: Math.min(390, root.height - 42)
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
                Layout.preferredWidth: Math.max(210, Math.min(250, loginCard.width * 0.42))
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
                            text: "Secure Login"
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
                        text: "Welcome Back"
                        color: root.textColor
                        font.pixelSize: 19
                        font.bold: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Enter your operator password to continue"
                        color: root.mutedColor
                        font.pixelSize: 11
                    }

                    Item { Layout.preferredHeight: 6 }

                    AuthTextField {
                        id: passwordField
                        Layout.fillWidth: true
                        placeholderText: "Enter Password"
                        echoMode: TextInput.Password
                        iconType: "lock"
                        onAccepted: loginButton.clicked()
                    }

                    Text {
                        id: errorLabel
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: ""
                        color: "#d84b4b"
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }

                    AuthButton {
                        id: loginButton
                        Layout.fillWidth: true
                        text: "LOGIN"
                        onClicked: {
                            if (backend.verifyLoginPassword(passwordField.text)) {
                                errorLabel.text = ""
                                passwordField.text = ""
                                root.loginSucceeded()
                            } else {
                                errorLabel.text = "Incorrect password. Please try again."
                                passwordField.text = ""
                                passwordField.forceActiveFocus()
                            }
                        }
                    }
                }
            }
        }
    }

    component AuthTextField: TextField {
        id: field

        property string iconType: "lock"

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

        Layout.preferredHeight: 46
        padding: 0
        flat: true
        hoverEnabled: true

        contentItem: Text {
            text: authButton.text
            color: "#ffffff"
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 4
            color: authButton.down ? "#167cc4" : authButton.hovered ? "#2396df" : "#2b9eea"
            border.color: "#2b9eea"
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

            if (iconType === "lock") {
                ctx.strokeRect(w * 0.25, h * 0.48, w * 0.50, h * 0.36)
                ctx.beginPath(); ctx.arc(cx, h * 0.48, w * 0.18, Math.PI, 0); ctx.stroke()
            }
        }
    }
}
