import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme"

// Large touch target used by PortraitDashboard
AbstractButton {
    id: root

    property string subtitle
    property string title

    implicitHeight: Math.max(64, labels.implicitHeight + 24)
    opacity: enabled ? 1 : Theme.disabledOpacity

    background: Rectangle {
        border.color: root.pressed ? Theme.accentColor : Theme.deckLineColor
        border.width: 1
        // Pressed: a tint of the accent (a full accent fill flashed the whole row)
        color: root.pressed ? Qt.darker(Theme.accentColor, 1.8) : Theme.darkGray3
        radius: Theme.radiusLarge

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.spacingL
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.top: parent.top
            anchors.topMargin: Theme.spacingL
            color: Theme.accentColor
            radius: 2
            width: 3
        }
    }
    contentItem: RowLayout {
        spacing: 12

        ColumnLayout {
            id: labels

            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingXL + Theme.spacingXS
            spacing: Theme.spacingXS

            Text {
                Layout.fillWidth: true
                color: Theme.white
                font.bold: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTitle
                text: root.title
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textMutedColor
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall + 1
                text: root.subtitle
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }
        }
        Text {
            Layout.rightMargin: Theme.spacingXL
            color: Theme.accentColor
            font.pixelSize: 26
            text: "›"
        }
    }
}
