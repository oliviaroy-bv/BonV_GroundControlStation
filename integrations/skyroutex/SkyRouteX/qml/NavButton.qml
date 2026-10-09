// import QtQuick 6.0

// Item {
//     id: root

//     property bool   active:     false
//     property string iconPath:   ""
//     property bool   isDrone:    false
//     property bool   isSettings: false
//     property bool   is3D:       false
//     property string tooltip:    ""

//     signal clicked

//     implicitWidth:  38
//     implicitHeight: 38

//     Rectangle {
//         anchors.fill: parent
//         radius:       Theme.radiusLg
//         color:        root.active
//                           ? Theme.blueLight
//                           : (ma.containsMouse ? Theme.bgSection : "transparent")
//         Behavior on color { ColorAnimation { duration: 100 } }
//     }

//     // Mission / route icon
//     Canvas {
//         anchors.centerIn: parent
//         width: 18; height: 18
//         visible: root.iconPath !== "" && !root.isDrone && !root.isSettings && !root.is3D
//         onPaint: {
//             var c = getContext("2d")
//             c.clearRect(0, 0, width, height)
//             c.strokeStyle = root.active ? Theme.blueMid : Theme.textMuted
//             c.lineWidth   = 1.5
//             c.lineCap     = "round"
//             c.lineJoin    = "round"
//             c.beginPath()
//             c.moveTo(2, 14); c.lineTo(7, 9); c.lineTo(10, 12); c.lineTo(16, 4)
//             c.stroke()
//             c.fillStyle = root.active ? Theme.blueMid : Theme.textMuted
//             c.beginPath(); c.arc(16, 4, 1.5, 0, Math.PI * 2); c.fill()
//         }
//     }

//     // Drone icon (4-arm quad)
//     Canvas {
//         anchors.centerIn: parent
//         width: 18; height: 18
//         visible: root.isDrone && !root.is3D
//         onPaint: {
//             var c   = getContext("2d")
//             var col = root.active ? Theme.blueMid : Theme.textMuted
//             c.clearRect(0, 0, width, height)
//             c.strokeStyle = col; c.fillStyle = col; c.lineWidth = 1.2
//             c.strokeRect(7, 7, 4, 4)
//             var arms = [[9,7,9,3],[9,11,9,15],[7,9,3,9],[11,9,15,9]]
//             arms.forEach(function(l) {
//                 c.beginPath(); c.moveTo(l[0],l[1]); c.lineTo(l[2],l[3]); c.stroke()
//             })
//             var dots = [[9,3],[9,15],[3,9],[15,9]]
//             dots.forEach(function(p) {
//                 c.beginPath(); c.arc(p[0],p[1],1.2,0,Math.PI*2); c.fill()
//             })
//         }
//     }

//     // Settings gear icon
//     Canvas {
//         anchors.centerIn: parent
//         width: 18; height: 18
//         visible: root.isSettings
//         onPaint: {
//             var c   = getContext("2d")
//             var col = root.active ? Theme.blueMid : Theme.textMuted
//             c.clearRect(0, 0, width, height)
//             c.strokeStyle = col; c.lineWidth = 1.2; c.lineCap = "round"
//             c.beginPath(); c.arc(9, 9, 2.5, 0, Math.PI * 2); c.stroke()
//             var pts = [[9,1],[9,17],[1,9],[17,9],[3.2,3.2],[14.8,14.8],[3.2,14.8],[14.8,3.2]]
//             pts.forEach(function(p) {
//                 c.beginPath(); c.arc(p[0],p[1],0.8,0,Math.PI*2)
//                 c.fillStyle = col; c.fill()
//             })
//         }
//     }

//     // 3D cube icon
//     Canvas {
//         anchors.centerIn: parent
//         width: 18; height: 18
//         visible: root.is3D
//         onPaint: {
//             var c   = getContext("2d")
//             var col = root.active ? Theme.blueMid : Theme.textMuted
//             c.clearRect(0, 0, 18, 18)
//             c.strokeStyle = col; c.lineWidth = 1.3; c.lineCap = "round"; c.lineJoin = "round"
//             // Front face
//             c.strokeRect(2, 6, 10, 10)
//             // Top face
//             c.beginPath(); c.moveTo(2,6); c.lineTo(6,2); c.lineTo(16,2); c.lineTo(12,6); c.closePath(); c.stroke()
//             // Right face
//             c.beginPath(); c.moveTo(12,6); c.lineTo(16,2); c.lineTo(16,12); c.lineTo(12,16); c.closePath(); c.stroke()
//         }
//     }

//     MouseArea {
//         id: ma
//         anchors.fill: parent
//         hoverEnabled: true
//         cursorShape:  Qt.PointingHandCursor
//         onClicked:    root.clicked()
//     }
// }



import QtQuick 6.0

Item {
    id: root

    property bool   active:     false
    property string iconPath:   ""
    property bool   isDrone:    false
    property bool   isSettings: false
    property bool   is3D:       false
    property bool   isFleet:    false
    property string tooltip:    ""

    signal clicked

    implicitWidth:  Theme.isAndroid ? 48 : 38
    implicitHeight: Theme.isAndroid ? 48 : 38

    Rectangle {
        anchors.fill: parent
        radius:       Theme.radiusLg
        color:        root.active
                          ? Theme.blueLight
                          : (ma.containsMouse ? Theme.bgSection : "transparent")
        Behavior on color { ColorAnimation { duration: 100 } }
    }

    // Mission / route icon
    Canvas {
        anchors.centerIn: parent
        width: Theme.isAndroid ? 22 : 18; height: width
        visible: root.iconPath !== "" && !root.isDrone && !root.isSettings && !root.is3D && !root.isFleet
        onPaint: {
            var c = getContext("2d")
            c.clearRect(0, 0, width, height)
            c.strokeStyle = root.active ? Theme.blueMid : Theme.textMuted
            c.lineWidth   = 1.5
            c.lineCap     = "round"
            c.lineJoin    = "round"
            c.beginPath()
            c.moveTo(2, 14); c.lineTo(7, 9); c.lineTo(10, 12); c.lineTo(16, 4)
            c.stroke()
            c.fillStyle = root.active ? Theme.blueMid : Theme.textMuted
            c.beginPath(); c.arc(16, 4, 1.5, 0, Math.PI * 2); c.fill()
        }
    }

    // Drone icon (4-arm quad)
    Canvas {
        anchors.centerIn: parent
        width: Theme.isAndroid ? 22 : 18; height: width
        visible: root.isDrone && !root.is3D && !root.isFleet
        onPaint: {
            var c   = getContext("2d")
            var col = root.active ? Theme.blueMid : Theme.textMuted
            c.clearRect(0, 0, width, height)
            c.strokeStyle = col; c.fillStyle = col; c.lineWidth = 1.2
            c.strokeRect(7, 7, 4, 4)
            var arms = [[9,7,9,3],[9,11,9,15],[7,9,3,9],[11,9,15,9]]
            arms.forEach(function(l) {
                c.beginPath(); c.moveTo(l[0],l[1]); c.lineTo(l[2],l[3]); c.stroke()
            })
            var dots = [[9,3],[9,15],[3,9],[15,9]]
            dots.forEach(function(p) {
                c.beginPath(); c.arc(p[0],p[1],1.2,0,Math.PI*2); c.fill()
            })
        }
    }

    // Settings gear icon
    Canvas {
        anchors.centerIn: parent
        width: Theme.isAndroid ? 22 : 18; height: width
        visible: root.isSettings && !root.isFleet
        onPaint: {
            var c   = getContext("2d")
            var col = root.active ? Theme.blueMid : Theme.textMuted
            c.clearRect(0, 0, width, height)
            c.strokeStyle = col; c.lineWidth = 1.2; c.lineCap = "round"
            c.beginPath(); c.arc(9, 9, 2.5, 0, Math.PI * 2); c.stroke()
            var pts = [[9,1],[9,17],[1,9],[17,9],[3.2,3.2],[14.8,14.8],[3.2,14.8],[14.8,3.2]]
            pts.forEach(function(p) {
                c.beginPath(); c.arc(p[0],p[1],0.8,0,Math.PI*2)
                c.fillStyle = col; c.fill()
            })
        }
    }

    // 3D cube icon
    Canvas {
        anchors.centerIn: parent
        width: Theme.isAndroid ? 22 : 18; height: width
        visible: root.is3D && !root.isFleet
        onPaint: {
            var c   = getContext("2d")
            var col = root.active ? Theme.blueMid : Theme.textMuted
            c.clearRect(0, 0, 18, 18)
            c.strokeStyle = col; c.lineWidth = 1.3; c.lineCap = "round"; c.lineJoin = "round"
            // Front face
            c.strokeRect(2, 6, 10, 10)
            // Top face
            c.beginPath(); c.moveTo(2,6); c.lineTo(6,2); c.lineTo(16,2); c.lineTo(12,6); c.closePath(); c.stroke()
            // Right face
            c.beginPath(); c.moveTo(12,6); c.lineTo(16,2); c.lineTo(16,12); c.lineTo(12,16); c.closePath(); c.stroke()
        }
    }

    // Fleet icon (list with drone symbol)
    Canvas {
        anchors.centerIn: parent
        width: Theme.isAndroid ? 22 : 18; height: width
        visible: root.isFleet
        onPaint: {
            var c   = getContext("2d")
            var col = root.active ? Theme.blueMid : Theme.textMuted
            c.clearRect(0, 0, 18, 18)
            c.strokeStyle = col; c.fillStyle = col; c.lineWidth = 1.3; c.lineCap = "round"
            // Three list rows
            var rows = [3, 8, 13]
            rows.forEach(function(y) {
                // Dot
                c.beginPath(); c.arc(3, y, 1.2, 0, Math.PI * 2); c.fill()
                // Line
                c.beginPath(); c.moveTo(6, y); c.lineTo(16, y); c.stroke()
            })
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape:  Qt.PointingHandCursor
        onClicked:    root.clicked()
    }
}
