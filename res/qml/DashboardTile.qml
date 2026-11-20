import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme"

// A category of the Android hub (PortraitDashboard.qml): big touch target with a
// title and one line of state. No animation, no shadow.
AbstractButton {
    id: root

    property string subtitle
    property string title
    property color accent: Theme.accentColor

    implicitHeight: Math.max(96, labels.implicitHeight + 2 * Theme.spacingXL)
    opacity: enabled ? 1 : Theme.disabledOpacity
    // The labels sit inside the card, clear of the accent bar and of the edges
    leftPadding: Theme.spacingXL + Theme.spacingXS
    rightPadding: Theme.spacingL
    topPadding: Theme.spacingL
    bottomPadding: Theme.spacingL

    background: Rectangle {
        border.color: root.pressed ? root.accent : Theme.deckLineColor
        border.width: 1
        color: root.pressed ? Qt.darker(root.accent, 1.8) : Theme.darkGray2
        radius: Theme.radiusLarge

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.spacingL
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.top: parent.top
            anchors.topMargin: Theme.spacingL
            color: root.accent
            radius: 2
            width: 4
        }
    }
    contentItem: ColumnLayout {
        id: labels

        spacing: Theme.spacingXS

        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeTitle + 1
            text: root.title
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            color: Theme.textMutedColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
            maximumLineCount: 3
            elide: Text.ElideRight
            text: root.subtitle
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }
    }
}
