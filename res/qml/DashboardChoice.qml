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
    implicitHeight: column.implicitHeight + 2 * Theme.spacingL
    radius: Theme.radiusLarge

    ColumnLayout {
        id: column

        anchors.left: parent.left
        anchors.margins: Theme.spacingXL
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacingM

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
        Flow {
            Layout.fillWidth: true
            spacing: Theme.spacingM

            Repeater {
                model: root.model

                delegate: Rectangle {
                    id: chip

                    required property var modelData
                    readonly property bool selected: root.current === modelData.value

                    border.color: selected ? Qt.lighter(Theme.accentColor, 1.3) : (tap.pressed ? Theme.accentColor : Theme.buttonBorderColor)
                    border.width: 1
                    // Pressed: lighter (the two branches were the same color)
                    color: selected ? Theme.accentColor : (tap.pressed ? Theme.surfacePressedColor : Theme.darkGray3)
                    height: Theme.touchTarget
                    radius: Theme.radiusMedium + 2
                    width: chipLabel.implicitWidth + 28

                    Text {
                        id: chipLabel

                        anchors.centerIn: parent
                        color: Theme.white
                        font.bold: chip.selected
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
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
