import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import QtQuick.Controls 2.12
import "Theme"

// Flat, rounded button. The previous look used two inner shadows, a drop
// shadow and a glow per button, which also cost GPU time on phones.
AbstractButton {
    id: root

    required property color activeColor
    property bool highlight: false
    property color normalColor: Theme.buttonNormalColor
    property color pressedColor: activeColor
    readonly property bool active: root.highlight || root.checked
    // Toolbar buttons (white active color) fill with the accent color, the
    // others (hotcues, sync...) with their own color
    readonly property color activeFill: Qt.colorEqual(root.activeColor, Theme.white) ? Theme.accentColor : Qt.darker(root.activeColor, 1.7)

    implicitHeight: 26
    implicitWidth: Math.max(52, label.implicitWidth + 20)

    background: Rectangle {
        id: backgroundRect

        anchors.fill: parent
        anchors.margins: 2
        radius: 6
        color: {
            if (root.pressed)
                return root.active ? Qt.lighter(root.activeFill, 1.2) : Theme.darkGray2;
            return root.active ? root.activeFill : Theme.buttonBackgroundColor;
        }
        border.width: 1
        border.color: root.active ? Qt.lighter(root.activeFill, 1.4) : Theme.buttonBorderColor

        Behavior on color {
            ColorAnimation {
                duration: 80
            }
        }
    }
    contentItem: Item {
        anchors.fill: parent

        Label {
            id: label
            anchors.fill: parent
            color: root.pressed ? root.pressedColor : (root.active ? root.activeColor : root.normalColor)
            font.bold: true
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: 0.5
            font.pixelSize: Theme.buttonFontPixelSize
            horizontalAlignment: Text.AlignHCenter
            text: root.text
            verticalAlignment: Text.AlignVCenter
            visible: root.text != null
        }
        Image {
            id: image
            anchors.centerIn: parent
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            height: icon.height
            source: icon.source
            visible: false
            width: icon.width
        }
        ColorOverlay {
            anchors.fill: image
            antialiasing: true
            color: root.active ? root.activeColor : root.normalColor
            source: image
            visible: icon.source != null && icon.source != ""
        }
    }
}
