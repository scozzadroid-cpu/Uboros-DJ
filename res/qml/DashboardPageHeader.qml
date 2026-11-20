import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme"

// Top bar of a page of the Android hub: back arrow (big touch target) and the title
Rectangle {
    id: root

    property string title
    signal back

    color: Theme.toolbarBackgroundColor
    implicitHeight: 60

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Theme.deckLineColor
        height: 1
    }
    AbstractButton {
        id: backButton

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.top: parent.top
        width: 72

        // Pressed: a round highlight behind the arrow, not the whole block
        background: Item {
            Rectangle {
                anchors.centerIn: parent
                color: Qt.darker(Theme.accentColor, 1.6)
                height: Theme.touchTarget
                radius: height / 2
                visible: backButton.pressed
                width: height
            }
        }
        contentItem: Text {
            color: Theme.white
            font.bold: true
            font.pixelSize: 34
            horizontalAlignment: Text.AlignHCenter
            text: "‹"
            verticalAlignment: Text.AlignVCenter
        }

        onClicked: root.back()
    }
    Text {
        anchors.left: backButton.right
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.white
        elide: Text.ElideRight
        font.bold: true
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeHeadline
        text: root.title
    }
}
