import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme"

// First-run tutorial: a few cards, Skip at any time. Closing it in any way
// (Skip, Done, tap outside, Back key) marks it as seen: it is not shown again.
// Settings > Maintenance can show it again.
Popup {
    id: root

    property int step: 0
    readonly property var steps: [
        {
            title: qsTr("Welcome to Ubøros DJ"),
            text: qsTr("Free and open source DJ software: no account, no subscription, no ads. A 40-second tour; you can skip it now and find it again in Settings.")
        },
        {
            title: qsTr("Plug in your controller"),
            text: qsTr("Connect the controller to the USB port with an OTG cable (a hub if the phone can't power it). It is recognised and set up by itself: nothing to install, no root. The Pioneer DDJ-FLX4 is the tested reference; other models use the Mixxx mappings.")
        },
        {
            title: qsTr("Load a track"),
            text: qsTr("Turn BROWSE to scroll the library and press LOAD on the deck you want. On a phone the library is hidden to leave room for the waveforms: press BROWSE (or the Library button) to show it. Without a controller you can drag a track onto a deck.")
        },
        {
            title: qsTr("Tracks from a rekordbox USB stick"),
            text: qsTr("Plug in a USB stick prepared with rekordbox and use Import USB drive: tracks, beat grids, cues and crates show up in the library. You can also export your library to rekordbox.xml.")
        },
        {
            title: qsTr("Stems on the phone"),
            text: qsTr("Split a track into vocals, drums, bass and other directly on the device, with no internet. With the DDJ-FLX4, SMART CFX controls the stems. The first track takes a little while; the result is saved for next time.")
        },
        {
            title: qsTr("You're ready"),
            text: qsTr("Settings (gear icon) has audio output, screen and cache options, and lets you see this tour again. Have a good set!")
        }
    ]
    readonly property bool last: step === steps.length - 1

    function finish() {
        Mixxx.Config.tutorialDone = true;
        close();
    }

    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape
    modal: true
    padding: 20
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 24 : 480, 520)

    // Any way of closing counts as seen
    onClosed: Mixxx.Config.tutorialDone = true
    onAboutToShow: step = 0

    background: Rectangle {
        border.color: Theme.deckLineColor
        color: Theme.darkGray2
        radius: 14
    }
    Overlay.modal: Rectangle {
        color: "#b0000000"
    }
    contentItem: ColumnLayout {
        spacing: 14

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                color: Theme.textMutedColor
                font.pixelSize: 12
                text: qsTr("%1 / %2").arg(root.step + 1).arg(root.steps.length)
            }
            Button {
                text: qsTr("Skip")
                visible: !root.last
                flat: true
                onClicked: root.finish()
                contentItem: Text {
                    color: Theme.accentColor
                    font.pixelSize: 15
                    horizontalAlignment: Text.AlignRight
                    text: parent.text
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
            }
        }
        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.bold: true
            font.pixelSize: 22
            text: root.steps[root.step].title
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            Layout.minimumHeight: 110
            color: Theme.textMutedColor
            font.pixelSize: 15
            lineHeight: 1.15
            text: root.steps[root.step].text
            verticalAlignment: Text.AlignTop
            wrapMode: Text.WordWrap
        }
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            Repeater {
                model: root.steps.length

                Rectangle {
                    color: index === root.step ? Theme.accentColor : Theme.deckLineColor
                    height: 8
                    radius: 4
                    width: 8
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                enabled: root.step > 0
                opacity: enabled ? 1 : 0.35
                text: qsTr("Back")
                onClicked: root.step--
                contentItem: Text {
                    color: Theme.white
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                    text: parent.text
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    border.color: Theme.deckLineColor
                    color: parent.pressed ? Theme.deckLineColor : "transparent"
                    radius: 10
                }
            }
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                text: root.last ? qsTr("Done") : qsTr("Next")
                onClicked: root.last ? root.finish() : root.step++
                contentItem: Text {
                    color: "white"
                    font.bold: true
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                    text: parent.text
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.pressed ? Theme.accent : Theme.accentColor
                    radius: 10
                }
            }
        }
    }
}
