import Mixxx.Controls 1.0 as MixxxControls
import QtQuick 2.12
import QtQuick.Shapes
import "Theme"

// Flat knob: a plain disc with a thin edge, the indicator line and the value
// arc over a dim track of the whole range. No images and no shadow: the disc
// and the track are static, only the indicator and the value arc move.
MixxxControls.Knob {
    id: root

    required property color color
    // Size when the owner sets none (MiniKnob.qml: 40)
    property int defaultSize: 48
    // Diameter of the disc relative to the knob (the arc runs around it)
    property real faceScale: 0.74

    implicitWidth: defaultSize
    implicitHeight: implicitWidth
    arc: true
    arcRadius: width * 0.45
    arcColor: root.color
    arcWidth: 2.5
    angle: 116

    background: Item {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: width

        // Track of the arc over the whole range. Curve renderer: smooth
        // edges without multisampling (the geometry is made once)
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                capStyle: ShapePath.FlatCap
                fillColor: "transparent"
                strokeColor: Theme.knobTrackColor
                strokeWidth: root.arcWidth

                PathAngleArc {
                    centerX: root.width / 2
                    centerY: root.width / 2
                    radiusX: root.arcRadius
                    radiusY: root.arcRadius
                    startAngle: -90 - root.angle
                    sweepAngle: 2 * root.angle
                }
            }
        }
        Rectangle {
            anchors.centerIn: parent
            border.color: Theme.knobEdgeColor
            border.width: 1
            color: Theme.knobFaceColor
            height: width
            radius: width / 2
            width: Math.round(root.width * root.faceScale)
        }
    }

    foreground: Item {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: width

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 2
            height: Math.round(root.width * 0.24)
            y: Math.round(root.width * (1 - root.faceScale) / 2) + 2
            radius: 1
            color: root.color
        }
    }
}
