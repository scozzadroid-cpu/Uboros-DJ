import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme"

// On/off option of the dashboard: the whole row is the touch target
AbstractButton {
    id: root

    property bool on: false
    property string subtitle
    property string title

    // Emitted with the new value; the owner stores it
    signal switched(bool value)

    implicitHeight: Math.max(56, labels.implicitHeight + 20)
    opacity: enabled ? 1 : Theme.disabledOpacity

    onClicked: root.switched(!root.on)

    background: Rectangle {
        border.color: root.pressed ? Theme.accentColor : Theme.deckLineColor
        border.width: 1
        color: root.pressed ? Theme.surfacePressedColor : Theme.darkGray3
        radius: Theme.radiusLarge
    }
    contentItem: RowLayout {
        spacing: 12

        ColumnLayout {
            id: labels

            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingXL
            spacing: Theme.spacingXS

            Text {
                Layout.fillWidth: true
                color: Theme.white
                font.bold: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody + 1
                text: root.title
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textMutedColor
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                text: root.subtitle
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }
        }
        // Switch
        Rectangle {
            Layout.rightMargin: Theme.spacingXL
            border.color: root.on ? Theme.accentColor : Theme.buttonBorderColor
            border.width: 1
            color: root.on ? Theme.accentColor : Theme.darkGray2
            implicitHeight: 26
            implicitWidth: 46
            radius: 13

            Rectangle {
                color: Theme.white
                height: 20
                radius: 10
                width: 20
                x: root.on ? parent.width - width - 3 : 3
                y: 3

                Behavior on x {
                    NumberAnimation {
                        duration: 120
                    }
                }
            }
        }
    }
}
