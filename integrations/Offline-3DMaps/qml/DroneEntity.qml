import QtQuick
import QtQuick3D

Node {
    id: root

    required property var vehicle
    property real visualScale: 1.0

    position: vehicle.scenePosition
    rotation: vehicle.sceneRotation
    scale: Qt.vector3d(visualScale, visualScale, visualScale)

    PrincipledMaterial {
        id: bodyMaterial
        baseColor: "#d9e3e7"
        roughness: 0.42
        metalness: 0.55
    }

    PrincipledMaterial {
        id: darkMaterial
        baseColor: "#182126"
        roughness: 0.68
        metalness: 0.18
    }

    PrincipledMaterial {
        id: accentMaterial
        baseColor: vehicle.connected ? "#34d399" : "#f2b84b"
        emissiveFactor: Qt.vector3d(0.35, 0.22, 0.05)
        roughness: 0.35
    }

    Model {
        source: "#Cube"
        scale: Qt.vector3d(0.034, 0.012, 0.068)
        materials: [bodyMaterial]
    }

    Model {
        source: "#Sphere"
        z: -4.7
        y: -0.1
        scale: Qt.vector3d(0.017, 0.013, 0.025)
        materials: [darkMaterial]
    }

    Model {
        source: "#Cube"
        eulerRotation.y: 45
        scale: Qt.vector3d(0.112, 0.005, 0.006)
        materials: [darkMaterial]
    }

    Model {
        source: "#Cube"
        eulerRotation.y: -45
        scale: Qt.vector3d(0.112, 0.005, 0.006)
        materials: [darkMaterial]
    }

    Repeater3D {
        model: [
            { "x": -3.9, "z": -3.9, "clockwise": true },
            { "x": 3.9, "z": -3.9, "clockwise": false },
            { "x": -3.9, "z": 3.9, "clockwise": false },
            { "x": 3.9, "z": 3.9, "clockwise": true }
        ]

        delegate: Node {
            required property var modelData
            x: modelData.x
            z: modelData.z

            Model {
                source: "#Cylinder"
                scale: Qt.vector3d(0.012, 0.012, 0.012)
                materials: [accentMaterial]
            }

            Model {
                id: rotor
                y: 0.8
                source: "#Cylinder"
                scale: Qt.vector3d(0.032, 0.001, 0.032)
                materials: [darkMaterial]

                NumberAnimation on eulerRotation.y {
                    running: vehicle.armed
                    from: modelData.clockwise ? 0 : 360
                    to: modelData.clockwise ? 360 : 0
                    duration: 70
                    loops: Animation.Infinite
                }
            }
        }
    }

    Model {
        y: -1.4
        x: -1.9
        source: "#Cube"
        scale: Qt.vector3d(0.005, 0.018, 0.005)
        materials: [darkMaterial]
    }

    Model {
        y: -1.4
        x: 1.9
        source: "#Cube"
        scale: Qt.vector3d(0.005, 0.018, 0.005)
        materials: [darkMaterial]
    }
}
