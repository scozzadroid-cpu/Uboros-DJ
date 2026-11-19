import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme"

// One setting with a few choices, as touch-sized chips (Android hub, Decks page).
// model: [{"label": "...", "value": 1}, ...]; picked(value) when a chip is tapped.
Rectangle {
    id: root

    property string title
    property string subtitle
    property var model: []
    property var current
    signal picked(var value)

    Layout.fillWidth: true
    border.color: Theme.deckLineColor
    border.width: 1
    color: Theme.darkGray2
    implicitHeight: column.implicitHeight + 24
    radius: 12

    ColumnLayout {
        id: column

        anchors.left: parent.left
        anchors.margins: 14
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

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
        Flow {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: root.model

                delegate: Rectangle {
                    id: chip

                    required property var modelData
                    readonly property bool selected: root.current === modelData.value

                    border.color: selected ? Qt.lighter(Theme.accentColor, 1.3) : Theme.deckLineColor
                    border.width: 1
                    color: selected ? Theme.accentColor : (tap.pressed ? Theme.darkGray3 : Theme.darkGray3)
                    height: 44
                    radius: 10
                    width: chipLabel.implicitWidth + 28

                    Text {
                        id: chipLabel

                        anchors.centerIn: parent
                        color: Theme.white
                        font.bold: chip.selected
                        font.pixelSize: 14
                        text: chip.modelData.label
                    }
                    MouseArea {
                        id: tap

                        anchors.fill: parent

                        onClicked: root.picked(chip.modelData.value)
                    }
                }
            }
        }
    }
}
