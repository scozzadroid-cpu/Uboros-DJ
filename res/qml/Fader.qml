import Mixxx.Controls 1.0 as MixxxControls
import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import "Theme"

MixxxControls.Slider {
    id: root

    property alias fg: handleImage.source
    property alias bg: backgroundImage.source

    bar: true
    barMargin: 10
    implicitWidth: backgroundImage.implicitWidth
    implicitHeight: backgroundImage.implicitHeight

    Image {
        id: handleImage

        visible: false
        source: Theme.imgSliderHandle
        fillMode: Image.PreserveAspectFit
    }

    handle: Item {
        id: handleItem

        width: handleImage.paintedWidth
        height: handleImage.paintedHeight
        x: root.horizontal ? (root.visualPosition * (root.width - width)) : ((root.width - width) / 2)
        y: root.vertical ? (root.visualPosition * (root.height - height)) : ((root.height - height) / 2)

        // DropShadow is a Qt5Compat ShaderEffectSource + FBO per fader. On
        // Android draw the handle directly over an offset translucent rectangle
        // (no offscreen pass); desktop keeps the blurred shadow.
        Loader {
            width: parent.width + 5
            height: parent.height + 5
            sourceComponent: Qt.platform.os === "android" ? cheapShadowComponent : dropShadowComponent
        }

        Component {
            id: dropShadowComponent

            DropShadow {
                source: handleImage
                radius: 5
                verticalOffset: 5
                color: "#80000000"
            }
        }

        Component {
            id: cheapShadowComponent

            Item {
                Rectangle {
                    x: 1
                    y: 4
                    width: parent.width - 7
                    height: parent.height - 5
                    radius: 3
                    color: "#59000000"
                }

                Image {
                    anchors.fill: parent
                    source: handleImage.source
                    fillMode: Image.Stretch
                }
            }
        }
    }

    background: Image {
        id: backgroundImage

        anchors.fill: parent
        anchors.margins: root.barMargin
    }
}
