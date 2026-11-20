import Mixxx 1.0 as Mixxx
import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import "Theme"

Rectangle {
    id: root

    required property string group
    required property string key

    // Android: no OpacityMask (two offscreen buffers re-rendered at the VU
    // rate, 30 Hz, for every meter); the gradient is clipped by a plain Item
    readonly property bool cheapEffects: Qt.platform.os === "android"

    radius: width / 2
    color: "black"

    Mixxx.ControlProxy {
        id: control

        group: root.group
        key: root.key
        // Hidden meter (mixer closed, screen asleep): at most once a second
        // instead of at the VU rate; caught up as soon as it is shown
        updateInterval: root.visible ? 0 : 1000
    }

    Item {
        id: meterMask

        anchors.fill: parent
        visible: false

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 1
            antialiasing: false // for performance reasons
            height: control.parameter * (parent.height - 2 * anchors.margins)
            radius: width / 2
        }
    }

    Rectangle {
        id: meterGradient

        antialiasing: false // for performance reasons
        anchors.fill: parent
        visible: false

        gradient: Gradient {
            GradientStop {
                position: 0.1
                color: Theme.red
            }

            GradientStop {
                position: 0.15
                color: Theme.yellow
            }

            GradientStop {
                position: 0.25
                color: Theme.yellow
            }

            GradientStop {
                position: 0.3
                color: Theme.green
            }

            GradientStop {
                position: 1
                color: Theme.green
            }
        }
    }

    Loader {
        anchors.fill: parent
        active: !root.cheapEffects

        sourceComponent: OpacityMask {
            source: meterGradient
            maskSource: meterMask
        }
    }

    Item {
        id: cheapMeter

        anchors.fill: parent
        anchors.margins: 1
        visible: root.cheapEffects

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.round(control.parameter * cheapMeter.height)
            clip: true

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: cheapMeter.height
                radius: width / 2
                antialiasing: false
                gradient: meterGradient.gradient
            }
        }
    }
}
