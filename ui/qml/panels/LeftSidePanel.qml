import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../accessories"

Rectangle {
    id: root

    property string currentTab: "fly"
    property QtObject theme: Palette {}
    property bool collapsed: false
    readonly property string activeTitle: titleFor(currentTab)
    readonly property string activeSubtitle: subtitleFor(currentTab)

    signal tabRequested(string tabId)
    signal toggleRequested()

    color: "#0b151c"
    clip: true

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: "#1d2a33"
        border.width: 1
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 1
        color: "#24343f"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            visible: !root.collapsed
            color: "#101b23"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 0

                Image {
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                    Layout.preferredWidth: Math.max(0, parent.width - 10)
                    Layout.preferredHeight: 48
                    source: "qrc:/logo/BonVGCS_Icon.png"
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            visible: !root.collapsed
            color: "#1d2a33"
        }

        Column {
            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: 0
            Layout.topMargin: 0
            width: root.width
            spacing: 0
            visible: !root.collapsed

            NavButton {
                tabId: "fly"
                label: "Fly"
                iconSource: "qrc:/icons/fly.png"
            }

            NavButton {
                tabId: "plan"
                label: "Plan"
                iconSource: "qrc:/icons/plan.png"
            }

            NavButton {
                tabId: "configuration"
                label: "Config"
                iconSource: "qrc:/icons/config.png"
            }

            NavButton {
                tabId: "settings"
                label: "Settings"
                iconSource: "qrc:/icons/settings.png"
            }
        }

        Item {
            Layout.fillHeight: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: "#16252e"
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 44

            Text {
                anchors.centerIn: parent
                text: root.collapsed ? "\u00BB" : "\u00AB"
                color: "#9dafba"
                font.pixelSize: 22
                font.bold: true
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleRequested()
            }
        }
    }

    component NavButton: Rectangle {
        id: navButton

        property string tabId: ""
        property string label: ""
        property string iconSource: ""
        property color normalTextColor: "#ffffff"
        property color selectedTextColor: "#000000"
        readonly property bool selected: root.currentTab === tabId

        width: root.width
        height: 78
        radius: 6
        color: selected ? "#09518f" : "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 4
            radius: 2
            color: selected ? "#21a8ff" : "transparent"
        }

        Column {
            anchors.centerIn: parent
            spacing: 7

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 25
                height: 25
                source: navButton.iconSource
                fillMode: Image.PreserveAspectFit
                mipmap: true
                opacity: selected ? 1.0 : 0.78
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: navButton.label
                color: navButton.selected ? navButton.selectedTextColor : navButton.normalTextColor
                renderType: Text.NativeRendering
                font.pixelSize: 12
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.tabRequested(navButton.tabId)
        }
    }

    function titleFor(tabId) {
        if (tabId === "configuration")
            return "Configuration"
        if (tabId === "plan")
            return "Plan"
        if (tabId === "settings")
            return "Settings"
        return "Fly"
    }

    function subtitleFor(tabId) {
        if (tabId === "configuration")
            return "Vehicle configuration and setup"
        if (tabId === "plan")
            return "Mission planning workspace"
        if (tabId === "settings")
            return "Application preferences"
        return "Flight control workspace"
    }
}
