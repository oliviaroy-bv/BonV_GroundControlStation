import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0

// PinGate.qml
// Overlays its parent with a PIN entry screen.
// Once the correct PIN is entered, unlocked = true and the overlay disappears.
// The PIN is stored as a simple SHA-256 hash in QSettings via the backend.
//
// Usage:
//   Item {
//       SomeProtectedContent { visible: gate.unlocked }
//       PinGate { id: gate; sectionName: "Drone Catalogue" }
//   }
//
// The default PIN is "1234". Change it from the Settings page.
// backend must expose:
//   Q_INVOKABLE bool verifyPin(const QString& section, const QString& pin)
//   Q_INVOKABLE void setPin(const QString& section, const QString& pin)

Item {
    id: root

    property string sectionName: "Protected"
    property bool   unlocked:    false
    readonly property bool isAndroid: Qt.platform.os === "android"
    readonly property int cardMargin: isAndroid ? 14 : 24
    readonly property int padW: isAndroid ? Math.max(46, Math.min(56, (pinCard.width - cardMargin * 2 - 16) / 3)) : 64
    readonly property int padH: isAndroid ? 34 : 42

    // Auto-lock after this many seconds of inactivity (0 = never)
    property int    autoLockSecs: 0

    anchors.fill: parent
    visible:      !unlocked
    z:            100    // always on top of sibling content

    // ── Lock background ───────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color:        Theme.bgPage
        opacity:      0.97
    }

    // ── PIN card ──────────────────────────────────────────────────────────────
    Rectangle {
        id: pinCard
        anchors.centerIn: parent
        width:   Math.min(parent.width - 24, isAndroid ? 240 : 280)
        height:  pinCol.implicitHeight + cardMargin * 2
        radius:  Theme.radiusLg
        color:   Theme.bg
        border.color: Theme.border
        border.width: 0.5

        ColumnLayout {
            id: pinCol
            anchors {
                left: parent.left; right: parent.right
                top:  parent.top
                margins: root.cardMargin
            }
            spacing: root.isAndroid ? 8 : 16

            // Lock icon
            Canvas {
                Layout.alignment: Qt.AlignHCenter
                visible: !root.isAndroid
                Layout.preferredHeight: visible ? 36 : 0
                width: 32; height: 36
                onPaint: {
                    var c = getContext("2d")
                    c.clearRect(0, 0, width, height)
                    // shackle
                    c.strokeStyle = Theme.textMuted
                    c.lineWidth   = 2.5
                    c.lineCap     = "round"
                    c.beginPath()
                    c.arc(16, 13, 8, Math.PI, 0)
                    c.stroke()
                    // body (manual rounded rect — roundRect not in Qt6 Canvas)
                    c.fillStyle   = Theme.bgSection
                    c.strokeStyle = Theme.border
                    c.lineWidth   = 1
                    var r = 3, x = 4, y = 18, w = 24, h = 16
                    c.beginPath()
                    c.moveTo(x + r, y)
                    c.lineTo(x + w - r, y)
                    c.arcTo(x + w, y, x + w, y + r, r)
                    c.lineTo(x + w, y + h - r)
                    c.arcTo(x + w, y + h, x + w - r, y + h, r)
                    c.lineTo(x + r, y + h)
                    c.arcTo(x, y + h, x, y + h - r, r)
                    c.lineTo(x, y + r)
                    c.arcTo(x, y, x + r, y, r)
                    c.closePath()
                    c.fill()
                    c.stroke()
                    // keyhole
                    c.fillStyle = Theme.textMuted
                    c.beginPath()
                    c.arc(16, 25, 3, 0, Math.PI * 2)
                    c.fill()
                }
            }

            // Title
            Text {
                Layout.alignment:   Qt.AlignHCenter
                text:               root.sectionName
                font.family:        Theme.fontFamily
                font.pixelSize:     root.isAndroid ? Theme.fontBase : Theme.fontMd
                font.weight:        Font.Medium
                color:              Theme.textPrimary
            }

            Text {
                Layout.alignment:   Qt.AlignHCenter
                text:               "Enter PIN to access"
                font.family:        Theme.fontFamily
                font.pixelSize:     root.isAndroid ? Theme.fontXs : Theme.fontSm
                color:              Theme.textMuted
            }

            // PIN dots display
            Row {
                Layout.alignment: Qt.AlignHCenter
                spacing: root.isAndroid ? 8 : 10

                Repeater {
                    model: 4
                    delegate: Rectangle {
                        width:  root.isAndroid ? 10 : 12; height: width; radius: width / 2
                        color:  index < pinInput.text.length
                                    ? Theme.blueMid
                                    : Theme.bgSection
                        border.color: Theme.border
                        border.width: 0.5
                    }
                }
            }

            // Hidden text input (captures keyboard)
            TextInput {
                id: pinInput
                maximumLength:  4
                echoMode:       TextInput.NoEcho
                validator:      RegularExpressionValidator {
                    regularExpression: /[0-9]*/
                }
                focus:          true
                width:          1; height: 1   // invisible but focusable
                Layout.preferredWidth: 1
                Layout.preferredHeight: 1

                onTextChanged: {
                    errorText.visible = false
                    if (text.length === 4) {
                        root.tryUnlock(text)
                    }
                }

                // Accept Enter key too (for 4+ digit PINs in future)
                Keys.onReturnPressed: {
                    if (text.length > 0) root.tryUnlock(text)
                }
            }

            // Number pad
            GridLayout {
                Layout.alignment: Qt.AlignHCenter
                columns:    3
                columnSpacing: root.isAndroid ? 6 : 8
                rowSpacing:    root.isAndroid ? 6 : 8

                Repeater {
                    model: ["1","2","3","4","5","6","7","8","9","⌫","0","✓"]
                    delegate: Rectangle {
                        width:  root.padW; height: root.padH
                        radius: Theme.radius
                        color:  padArea.pressed
                                    ? Theme.blueMid
                                    : padArea.containsMouse
                                        ? Theme.blueLight
                                        : Theme.bgSection
                        border.color: Theme.border
                        border.width: 0.5

                        Behavior on color { ColorAnimation { duration: 80 } }

                        Text {
                            anchors.centerIn: parent
                            text:           modelData
                            font.family:    Theme.fontFamily
                            font.pixelSize: root.isAndroid ? Theme.fontBase : Theme.fontMd
                            font.weight:    Font.Medium
                            color:          padArea.pressed
                                                ? "#FFFFFF"
                                                : modelData === "✓"
                                                    ? Theme.green
                                                    : modelData === "⌫"
                                                        ? Theme.red
                                                        : Theme.textPrimary
                        }

                        MouseArea {
                            id: padArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape:  Qt.PointingHandCursor
                            onClicked: {
                                pinInput.forceActiveFocus()
                                if (modelData === "⌫") {
                                    if (pinInput.text.length > 0)
                                        pinInput.text = pinInput.text.slice(0, -1)
                                    errorText.visible = false
                                } else if (modelData === "✓") {
                                    root.tryUnlock(pinInput.text)
                                } else {
                                    if (pinInput.text.length < 4)
                                        pinInput.text += modelData
                                }
                            }
                        }
                    }
                }
            }

            // Error message
            Text {
                id: errorText
                Layout.alignment: Qt.AlignHCenter
                text:    "Incorrect PIN"
                color:   Theme.red
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontSm
                visible: false
            }

            Item { implicitHeight: 4 }   // bottom padding
        }
    }

    // ── Auto-lock timer ───────────────────────────────────────────────────────
    Timer {
        id: autoLockTimer
        interval: root.autoLockSecs * 1000
        running:  root.unlocked && root.autoLockSecs > 0
        onTriggered: root.lock()
    }

    // ── Functions ─────────────────────────────────────────────────────────────

    function tryUnlock(pin) {
        if (backend.verifyPin(root.sectionName, pin)) {
            unlocked = true
            pinInput.text = ""
            if (autoLockSecs > 0) autoLockTimer.restart()
        } else {
            errorText.visible = true
            // Shake animation
            shakeAnim.start()
            pinInput.text = ""
        }
    }

    function lock() {
        unlocked      = false
        pinInput.text = ""
        errorText.visible = false
        pinInput.forceActiveFocus()
    }

    // Shake animation on wrong PIN
    SequentialAnimation {
        id: shakeAnim
        property real origX: 0

        ScriptAction  { script: { shakeAnim.origX = pinCard.x } }
        NumberAnimation { target: pinCard; property: "x"; to: pinCard.x - 8; duration: 50 }
        NumberAnimation { target: pinCard; property: "x"; to: pinCard.x + 8; duration: 50 }
        NumberAnimation { target: pinCard; property: "x"; to: pinCard.x - 6; duration: 50 }
        NumberAnimation { target: pinCard; property: "x"; to: pinCard.x + 6; duration: 50 }
        NumberAnimation { target: pinCard; property: "x"; to: shakeAnim.origX; duration: 50 }
    }

    // Ensure PIN input is focused when gate becomes visible
    onVisibleChanged: {
        if (visible) {
            pinInput.text = ""
            errorText.visible = false
            pinInput.forceActiveFocus()
        }
    }
}
