import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
// import QtQuick.Window
import Qt.labs.settings
import "../accessories"
import "../panels"

Item {
    id: root
    anchors.fill: parent

    Palette {
        id: appTheme
        darkMode: root.darkTheme
    }

    // visible: true
    // visibility: Window.Windowed
    // flags: Qt.Window
    //        | Qt.WindowTitleHint
    //        | Qt.WindowSystemMenuHint
    //        | Qt.WindowMinimizeButtonHint
    //        | Qt.WindowMaximizeButtonHint
    //        | Qt.WindowCloseButtonHint
    // width: 1280
    // height: 760
    // minimumWidth: 1040
    // minimumHeight: 640
    // title: "BonV GCS"

    property string activeTab: "fly"
    property string rtspLink: ""
    property string cameraName: ""
    property bool sidePanelCollapsed: false
    property bool darkTheme: true
    

    Settings {
        id: persistedSettings
        category: "Video"
        property alias rtspLink: root.rtspLink
        property alias cameraName: root.cameraName
    }

    Settings {
        id: persistedAppearanceSettings
        category: "Appearance"
        property alias darkTheme: root.darkTheme
    }

    // color: appTheme.background
    // font.family: appTheme.fontFamily

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // LeftSidePanel {
        //     id: sidePanel
        //     Layout.fillHeight: true
        //     Layout.preferredWidth: root.sidePanelCollapsed ? 24 : 70

        //     currentTab: root.activeTab
        //     theme: appTheme
        //     collapsed: root.sidePanelCollapsed

        //     onTabRequested: function(tabId) {
        //         root.activeTab = tabId
        //     }

        //     onToggleRequested: root.sidePanelCollapsed = !root.sidePanelCollapsed

        //     Behavior on Layout.preferredWidth {
        //         NumberAnimation {
        //             duration: 220
        //             easing.type: Easing.InOutQuad
        //         }
        //     }
        // }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: appTheme.background

            Loader {
                id: tabLoader
                anchors.fill: parent
                sourceComponent: {
                    if (root.activeTab === "configuration")
                        return configurationPage
                    if (root.activeTab === "plan")
                        return planPage
                    if (root.activeTab === "settings")
                        return settingsPage
                    return flyPage
                }
            }
        }
    }

    component StatusChip: Rectangle {
        property string label: ""
        property string value: ""
        property color valueColor: appTheme.text

        Layout.preferredWidth: 136
        Layout.preferredHeight: 34
        radius: 6
        color: appTheme.card
        border.color: appTheme.border

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8

            Text {
                text: label
                color: appTheme.muted
                font.pixelSize: appTheme.fontTiny
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                text: value
                color: valueColor
                font.pixelSize: appTheme.fontSmall
                font.bold: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }
    }

    component PlaceholderPage: Rectangle {
        property string heading: ""
        property string body: ""
        property color accentColor: appTheme.accent

        color: appTheme.background

        Rectangle {
            anchors.fill: parent
            anchors.margins: 22
            radius: 8
            color: appTheme.panelAlt
            border.color: appTheme.border

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 4
                radius: 2
                color: accentColor
            }

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 96, 560)
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: heading
                    color: appTheme.text
                    font.pixelSize: appTheme.fontDisplay
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: body
                    color: appTheme.label
                    font.pixelSize: appTheme.fontMedium
                    lineHeight: 1.25
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Component {
        id: flyPage

        FlyView {
            theme: appTheme
            darkTheme: appTheme.darkMode
            rtspLink: root.rtspLink
            cameraName: root.cameraName
            onCameraLinkRequested: function(rtspLink, cameraName) {
                root.rtspLink = rtspLink
                root.cameraName = cameraName
            }
            onThemeModeRequested: function(darkTheme) {
                persistedAppearanceSettings.darkTheme = darkTheme
            }
        }
    }

    Component {
        id: planPage

        PlaceholderPage {
            heading: "Mission Planner"
            body: "Waypoint editing, route validation, altitude profiles, and upload controls will live in this tab."
            accentColor: appTheme.success
        }
    }

    Component {
        id: configurationPage

        PlaceholderPage {
            heading: "Configuration"
            body: "Vehicle setup, connection profiles, sensor calibration, and parameter tools will live in this tab."
            accentColor: appTheme.text
        }
    }

    Component {
        id: settingsPage

        Rectangle {
            color: appTheme.background

            Rectangle {
                anchors.fill: parent
                anchors.margins: 22
                radius: 8
                color: appTheme.panelAlt
                border.color: appTheme.border

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 4
                    radius: 2
                    color: appTheme.text
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 32
                    spacing: 18

                    Text {
                        Layout.fillWidth: true
                        text: "Settings"
                        color: appTheme.text
                        font.pixelSize: appTheme.fontDisplay
                        font.bold: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 680
                        Layout.preferredHeight: 132
                        radius: 8
                        color: appTheme.card
                        border.color: appTheme.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 10

                            Text {
                                Layout.fillWidth: true
                                text: "RTSP Stream Link"
                                color: appTheme.text
                                font.pixelSize: appTheme.fontMedium
                                font.bold: true
                            }

                            TextField {
                                id: rtspLinkField

                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                text: root.rtspLink
                                placeholderText: "rtsp://username:password@host:port/path"
                                selectByMouse: true
                                color: appTheme.text
                                placeholderTextColor: appTheme.muted
                                font.pixelSize: appTheme.fontBody

                                background: Rectangle {
                                    radius: 6
                                    color: appTheme.background
                                    border.color: rtspLinkField.activeFocus ? appTheme.accent : appTheme.border
                                }

                                onEditingFinished: root.rtspLink = text.trim()
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.rtspLink.length > 0 ? "Current stream: " + root.rtspLink : "Enter an RTSP link to use for the video stream."
                                color: appTheme.label
                                font.pixelSize: appTheme.fontSmall
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }
}




