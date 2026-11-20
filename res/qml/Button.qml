import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import QtQuick.Controls.Basic
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

    // Height of the drawn button when the button itself is taller (a bigger
    // touch target in a bar, see main.qml); 0 = as tall as the button
    property real visualHeight: 0

    implicitHeight: 26
    implicitWidth: Math.max(52, label.implicitWidth + 20)
    // The background is drawn inside the insets: 2 on every side, more above
    // and below when visualHeight is set
    leftInset: 2
    rightInset: 2
    topInset: visualHeight > 0 ? Math.max(2, Math.round((height - visualHeight) / 2)) : 2
    bottomInset: topInset
    opacity: enabled ? 1 : Theme.disabledOpacity

    background: Rectangle {
        id: backgroundRect

        radius: Theme.radiusButton
        // Flat surface; pressed is clearly lighter (it was almost the same
        // color as the normal state)
        color: {
            if (root.pressed)
                return root.active ? Qt.lighter(root.activeFill, 1.25) : Theme.surfacePressedColor;
            return root.active ? root.activeFill : Theme.buttonBackgroundColor;
        }
        border.width: 1
        border.color: root.active ? Qt.lighter(root.activeFill, 1.4) : (root.hovered && !root.pressed ? Qt.lighter(Theme.buttonBorderColor, 1.5) : Theme.buttonBorderColor)

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
