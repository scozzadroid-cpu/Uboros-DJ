import QtQuick
import QtQuick.Layouts
import "Theme"

// Button of the banners (update, stem quality): the main answer is filled
// with the color of the banner, the other one is an outline. 44 dp tall for a finger.
Rectangle {
    id: root

    property bool primary: false
    property alias text: label.text
    property color tint: Theme.accentColor
    signal clicked

    Layout.preferredHeight: 44
    Layout.preferredWidth: label.implicitWidth + 32
    border.color: !enabled ? Theme.cardBorderColor : (primary ? "transparent" : Qt.rgba(tint.r, tint.g, tint.b, 0.6))
    border.width: 1
    color: {
        if (!enabled)
            return "#23272e";
        if (primary)
            return tap.pressed ? Qt.lighter(tint, 1.25) : tint;
        return tap.pressed ? Qt.rgba(tint.r, tint.g, tint.b, 0.25) : "transparent";
    }
    radius: Theme.radiusMedium

    Behavior on color {
        ColorAnimation {
            duration: 90
        }
    }

    Text {
        id: label

        anchors.centerIn: parent
        color: !root.enabled ? Theme.midGray : (root.primary ? "#0b0c0f" : Theme.white)
        font.bold: true
        font.family: Theme.fontFamily
        font.pixelSize: 15
    }
    TapHandler {
        id: tap

        enabled: root.enabled

        onTapped: root.clicked()
    }
}
