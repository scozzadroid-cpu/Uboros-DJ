import QtQuick 2.12
import "Theme"

// Deck state indicator like the ones on the CDJ display (MASTER, SYNC,
// KEY LOCK): shown, lit in its color, only while on (the deck bar on a phone
// has no room for dim indicators).
Rectangle {
    id: root

    property alias text: chipText.text
    property color tint: Theme.white
    property bool active: false

    visible: active

    implicitHeight: 15
    implicitWidth: chipText.implicitWidth + 6
    radius: 3
    color: active ? Qt.darker(tint, 2.6) : "transparent"
    border.width: 1
    border.color: active ? tint : Qt.rgba(1, 1, 1, 0.14)

    Text {
        id: chipText

        anchors.centerIn: parent
        font.pixelSize: 9
        font.bold: true
        color: root.active ? root.tint : Qt.rgba(1, 1, 1, 0.28)
    }
}
