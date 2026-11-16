import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Layouts
import "Theme"

// Shown when the stem separation does not keep up on this phone (or makes
// the audio drop out): proposes the light quality. Not modal: the DJ can go
// on playing and answer later.
Rectangle {
    id: root

    readonly property bool shown: slowControl.value > 0

    color: "#2B2B2B"
    border.color: Theme.yellow
    border.width: 1
    radius: 8
    implicitHeight: row.implicitHeight + 20
    visible: shown

    Mixxx.ControlProxy {
        id: slowControl

        group: "[Stems]"
        key: "slow_detected"
    }
    Mixxx.ControlProxy {
        id: qualityControl

        group: "[Stems]"
        key: "quality"
    }

    component Choice: Rectangle {
        id: choice

        property alias text: label.text
        signal chosen

        Layout.preferredHeight: 44
        Layout.preferredWidth: label.implicitWidth + 32
        color: tap.pressed ? "#555555" : "#3F3F3F"
        radius: 6

        Text {
            id: label

            anchors.centerIn: parent
            color: Theme.white
            font.bold: true
            font.pixelSize: 15
        }
        TapHandler {
            id: tap

            onTapped: choice.chosen()
        }
    }

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.margins: 10
        spacing: 12

        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.pixelSize: 15
            text: qsTr("This phone is too slow for the stem quality in use: the stems arrive late or the audio drops out. Use the light quality?")
            wrapMode: Text.WordWrap
        }
        Choice {
            text: qsTr("Light")

            onChosen: {
                qualityControl.value = 1;
                slowControl.value = 0;
            }
        }
        Choice {
            text: qsTr("Not now")

            onChosen: slowControl.value = 0
        }
    }
}
