// import QtQuick 6.0

// Item {
//     id: root

//     property int currentIndex: 0
//     signal indexChanged(int index)

//     implicitWidth: Theme.sidebarW

//     // Background + right border
//     Rectangle {
//         anchors.fill: parent
//         color:        Theme.bg

//         Rectangle {
//             anchors.right:  parent.right
//             width: 0.5; height: parent.height
//             color: Theme.border
//         }
//     }

//     Column {
//         anchors {
//             top: parent.top
//             horizontalCenter: parent.horizontalCenter
//             topMargin: 12
//         }
//         spacing: 4

//         // Mission view
//         NavButton {
//             active:    root.currentIndex === 0
//             iconPath:  "route"
//             tooltip:   "Mission"
//             onClicked: { root.currentIndex = 0; root.indexChanged(0) }
//         }

//         // Drone catalogue
//         NavButton {
//             active:    root.currentIndex === 1
//             isDrone:   true
//             tooltip:   "Drones"
//             onClicked: { root.currentIndex = 1; root.indexChanged(1) }
//         }

//         // 3D View
//         NavButton {
//             active:    root.currentIndex === 2
//             is3D:      true
//             tooltip:   "3D View"
//             onClicked: { root.currentIndex = 2; root.indexChanged(2) }
//         }

//         // Separator
//         Rectangle {
//             width:  24
//             height: 0.5
//             color:  Theme.border
//             anchors.horizontalCenter: parent.horizontalCenter
//         }

//         // Settings
//         NavButton {
//             active:      root.currentIndex === 3
//             isSettings:  true
//             tooltip:     "Settings"
//             onClicked:   { root.currentIndex = 3; root.indexChanged(3) }
//         }
//     }
// }


import QtQuick 6.0

Item {
    id: root

    property int currentIndex: 0
    signal indexChanged(int index)

    implicitWidth: Theme.sidebarW

    // Background + right border
    Rectangle {
        anchors.fill: parent
        color:        Theme.bg

        Rectangle {
            anchors.right:  parent.right
            width: 0.5; height: parent.height
            color: Theme.border
        }
    }

    Column {
        anchors {
            top: parent.top
            horizontalCenter: parent.horizontalCenter
            topMargin: 12
        }
        spacing: 4

        // 0 — Mission
        NavButton {
            active:    root.currentIndex === 0
            iconPath:  "route"
            tooltip:   "Mission"
            onClicked: { root.currentIndex = 0; root.indexChanged(0) }
        }

        // 1 — Drone Fleet (PIN protected)
        NavButton {
            active:    root.currentIndex === 1
            isFleet:   true
            tooltip:   "Drone Fleet"
            onClicked: { root.currentIndex = 1; root.indexChanged(1) }
        }

        // 2 — Drone Catalogue
        NavButton {
            active:    root.currentIndex === 2
            isDrone:   true
            tooltip:   "Drone Catalogue"
            onClicked: { root.currentIndex = 2; root.indexChanged(2) }
        }

        // 3 — 3D View
        NavButton {
            active:    root.currentIndex === 3
            is3D:      true
            tooltip:   "3D View"
            onClicked: { root.currentIndex = 3; root.indexChanged(3) }
        }

        // Separator
        Rectangle {
            width:  24; height: 0.5; color: Theme.border
            anchors.horizontalCenter: parent.horizontalCenter
        }

        // 4 — Settings
        NavButton {
            active:      root.currentIndex === 4
            isSettings:  true
            tooltip:     "Settings"
            onClicked:   { root.currentIndex = 4; root.indexChanged(4) }
        }
    }
}
