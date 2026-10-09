import QtQuick

QtObject {
    id: palette

    property bool darkMode: true

    readonly property color background: darkMode ? "#0f1115" : "#f4f7fb"
    readonly property color panel: darkMode ? "#171a20" : "#ffffff"
    readonly property color panelAlt: darkMode ? "#1e232b" : "#e8edf4"
    readonly property color card: darkMode ? "#242a33" : "#ffffff"
    readonly property color border: darkMode ? "#343c48" : "#ccd6e4"

    readonly property color text: darkMode ? "#f0f3f6" : "#101722"
    readonly property color label: darkMode ? "#b5bec9" : "#334155"
    readonly property color muted: darkMode ? "#778391" : "#66758a"

    readonly property color accent: darkMode ? "#34c3d6" : "#008fb0"
    readonly property color accentSoft: darkMode ? "#17383f" : "#d7f3f8"
    readonly property color success: darkMode ? "#63c085" : "#218f56"
    readonly property color warning: darkMode ? "#d8a94d" : "#b67810"
    readonly property color danger: darkMode ? "#d96868" : "#c94a4a"
    readonly property color accentText: darkMode ? "#061f24" : "#ffffff"

    readonly property string fontFamily: "Inter, Segoe UI, Arial, sans-serif"
    readonly property int fontTiny: 10
    readonly property int fontSmall: 11
    readonly property int fontBody: 12
    readonly property int fontMedium: 14
    readonly property int fontBrand: 16
    readonly property int fontTitle: 20
    readonly property int fontDisplay: 24

    readonly property real letterSpacingSection: 1.6
    readonly property real letterSpacingCaption: 1.4
}
