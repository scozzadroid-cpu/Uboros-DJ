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

    onClicked: root.switched(!root.on)

    background: Rectangle {
        border.color: Theme.deckLineColor
        border.width: 1
        color: root.pressed ? Theme.darkGray2 : Theme.darkGray3
        radius: 12
    }
    contentItem: RowLayout {
        spacing: 12

        ColumnLayout {
            id: labels

            Layout.fillWidth: true
            Layout.leftMargin: 14
            spacing: 2

            Text {
                Layout.fillWidth: true
                color: root.enabled ? Theme.white : Theme.midGray
                font.bold: true
                font.pixelSize: 15
                text: root.title
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: Theme.midGray
                font.pixelSize: 12
                text: root.subtitle
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }
        }
        // Switch
        Rectangle {
            Layout.rightMargin: 14
            color: root.on ? Theme.accent : Theme.darkGray2
            implicitHeight: 26
            implicitWidth: 46
            opacity: root.enabled ? 1 : 0.4
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
