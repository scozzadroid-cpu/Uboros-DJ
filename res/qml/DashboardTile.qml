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

    implicitHeight: Math.max(96, labels.implicitHeight + 32)

    background: Rectangle {
        border.color: Theme.deckLineColor
        border.width: 1
        color: root.pressed ? Qt.darker(root.accent, 1.8) : Theme.darkGray2
        radius: 14

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 14
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.top: parent.top
            anchors.topMargin: 14
            color: root.accent
            radius: 2
            width: 4
        }
    }
    contentItem: ColumnLayout {
        id: labels

        spacing: 3

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            color: Theme.white
            font.bold: true
            font.pixelSize: 17
            text: root.title
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            color: Theme.midGray
            font.pixelSize: 12
            maximumLineCount: 3
            elide: Text.ElideRight
            text: root.subtitle
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }
    }
}
