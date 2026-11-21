import QtQuick
import QtQuick.Shapes

// Consistent vector outlines in a 24-unit grid, independent of installed fonts.
Item {
    id: root
    property string name: "link"
    property color color: "#00d6eb"
    width: 20; height: 20
    readonly property string outline: {
        switch (name) {
        case "signal": return "M4 19V16 M9 19V12 M14 19V8 M19 19V4"
        case "layers": return "M3 8L12 3L21 8L12 13Z M3 12L12 17L21 12 M3 16L12 21L21 16"
        case "pulse": return "M2 12H7L10 4L14 20L17 12H22"
        case "clock": return "M12 2A10 10 0 1 1 11.99 2 M12 6V12L16 14"
        case "radio": return "M12 20V10 M9 20H15 M8 15A6 6 0 1 1 16 15 M5 18A10 10 0 1 1 19 18 M11 10A1 1 0 1 0 13 10A1 1 0 1 0 11 10"
        case "up": return "M6 15L12 9L18 15"
        case "down": return "M6 9L12 15L18 9"
        case "close": return "M6 6L18 18 M18 6L6 18"
        case "camera": return "M3 7H7L9 4H15L17 7H21V20H3Z M15.5 13A3.5 3.5 0 1 1 8.5 13A3.5 3.5 0 1 1 15.5 13"
        case "gps": return "M12 5A7 7 0 1 1 11.99 5 M12 9A3 3 0 1 1 11.99 9 M12 2V5 M12 19V22 M2 12H5 M19 12H22"
        case "battery": return "M3 7H19V17H3Z M22 10V14 M7 10V14 M11 10V14 M15 10V14"
        default: return "M9 15L15 9 M8 12L5 15A4 4 0 0 0 11 21L14 18 M10 6L13 3A4 4 0 0 1 19 9L16 12"
        }
    }
    Shape {
        width: 24; height: 24
        transform: Scale { xScale: root.width / 24; yScale: root.height / 24 }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.8; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.outline }
        }
    }
}
