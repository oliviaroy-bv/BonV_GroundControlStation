pragma Singleton
import QtQuick 6.0

QtObject {
    readonly property bool   isAndroid:    Qt.platform.os === "android"

    // ── Palette ────────────────────────────────────────────────────────────────
    readonly property color bg:           "#FFFFFF"
    readonly property color bgPage:       "#FAFAF8"
    readonly property color bgSection:    "#F5F4F0"
    readonly property color bgInput:      "#FAFAF8"
    readonly property color bgInputFocus: "#FFFFFF"

    readonly property color border:       "#D3D1C7"
    readonly property color borderLight:  "#F1EFE8"
    readonly property color borderFocus:  "#378ADD"

    readonly property color textPrimary:  "#2C2C2A"
    readonly property color textSecond:   "#5F5E5A"
    readonly property color textMuted:    "#888780"
    readonly property color textHint:     "#B4B2A9"

    readonly property color blue:         "#185FA5"
    readonly property color blueMid:      "#378ADD"
    readonly property color blueLight:    "#E6F1FB"
    readonly property color blueBorder:   "#B5D4F4"

    readonly property color green:        "#3B6D11"
    readonly property color greenLight:   "#EAF3DE"

    readonly property color amber:        "#854F0B"
    readonly property color amberLight:   "#FAEEDA"

    readonly property color red:          "#A32D2D"
    readonly property color redLight:     "#FCEBEB"

    // ── Typography ─────────────────────────────────────────────────────────────
    readonly property string fontFamily:  "Segoe UI"
    readonly property string fontMono:    "Cascadia Code, Consolas, monospace"

    readonly property int    fontXs:      10
    readonly property int    fontSm:      11
    readonly property int    fontBase:    12
    readonly property int    fontMd:      13
    readonly property int    fontLg:      14

    // ── Geometry ───────────────────────────────────────────────────────────────
    readonly property int    radius:       isAndroid ? 8 : 6
    readonly property int    radiusLg:     8
    readonly property int    sidebarW:     52
    readonly property int    leftPanelW:   360
    readonly property int    titleH:       42
    readonly property int    statusH:      isAndroid ? 24 : 26
    readonly property int    logHeaderH:   isAndroid ? 30 : 38
    readonly property int    sectionPad:   14
    readonly property int    inputH:       30
    readonly property int    spinW:        80
}
