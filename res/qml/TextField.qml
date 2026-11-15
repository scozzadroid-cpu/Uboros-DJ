import QtQuick 2.12
import QtQuick.Controls.Basic
import "Theme"

TextField {
    color: Theme.textColor

    background: Rectangle {
        anchors.fill: parent
        color: Theme.embeddedBackgroundColor
        radius: 5
    }
}
