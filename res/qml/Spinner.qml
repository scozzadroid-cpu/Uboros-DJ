import QtQuick
import "Theme"

// "Something is in progress": a dot going round a ring. The animation runs
// only while the item is shown and `running` is on, so a hidden spinner costs nothing.
Item {
    id: root

    property bool running: true
    property real thickness: Math.max(2, width / 9)
    property color tint: Theme.accentColor

    implicitHeight: 20
    implicitWidth: 20

    Rectangle {
        anchors.fill: parent
        border.color: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.28)
        border.width: root.thickness
        color: "transparent"
        radius: width / 2
    }
    Item {
        anchors.fill: parent

        RotationAnimator on rotation {
            duration: 1000
            from: 0
            loops: Animation.Infinite
            running: root.running && root.visible
            to: 360
        }
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.tint
            height: root.thickness * 1.8
            radius: height / 2
            width: height
            y: -root.thickness * 0.4
        }
    }
}
