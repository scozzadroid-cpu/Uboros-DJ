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

    background: Rectangle {
        border.color: Theme.deckLineColor
        border.width: 1
        color: root.pressed ? Theme.accentColor : Theme.darkGray3
        radius: 14

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 16
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.top: parent.top
            anchors.topMargin: 16
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
            Layout.leftMargin: 14
            spacing: 2

            Text {
                Layout.fillWidth: true
                color: Theme.white
                font.bold: true
                font.pixelSize: 16
                text: root.title
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: Theme.midGray
                font.pixelSize: 13
                text: root.subtitle
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }
        }
        Text {
            Layout.rightMargin: 14
            color: Theme.accentColor
            font.pixelSize: 26
            text: "›"
        }
    }
}
