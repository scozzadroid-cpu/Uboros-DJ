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
    // Name of the first connected controller (and its mapping), lower case
    readonly property var devices: (Mixxx.ControllerManager.knownDevices || []).concat(Mixxx.ControllerManager.unknownDevices || [])
    readonly property string deviceName: devices.length > 0 ? devices[0].name : ""
    readonly property string deviceKey: devices.length > 0 ? (devices[0].name + " " + (devices[0].activeMappingName || "")).toLowerCase() : ""
    readonly property string brand: {
        const k = deviceKey;
        if (k === "")
            return "";
        if (k.indexOf("flx4") >= 0)
            return "flx4";
        if (k.indexOf("rane") >= 0)
            return "rane";
        if (k.indexOf("pioneer") >= 0 || k.indexOf("alphatheta") >= 0 || k.indexOf("ddj") >= 0)
            return "pioneer";
        if (k.indexOf("hercules") >= 0)
            return "hercules";
        if (k.indexOf("numark") >= 0)
            return "numark";
        if (k.indexOf("reloop") >= 0)
            return "reloop";
        if (k.indexOf("denon") >= 0)
            return "denon";
        if (k.indexOf("roland") >= 0)
            return "roland";
        if (k.indexOf("traktor") >= 0 || k.indexOf("native instruments") >= 0)
            return "ni";
        return "other";
    }
    readonly property string who: deviceName !== "" ? deviceName : qsTr("your controller")
    // The step about the controller itself changes with the brand
    readonly property var controllerStep: {
        switch (brand) {
        case "flx4":
            return {
                title: qsTr("Your DDJ-FLX4"),
                text: qsTr("Turn BROWSE to scroll the library, press it to show or hide it, LOAD puts the track on the deck. BEAT SYNC turns sync on and off, SHIFT + BEAT SYNC is key lock. BEAT FX plays one effect at a time. SMART CFX drives the stems (see the next card). SMART FADER switches the crossfader on and off.")
            };
        case "pioneer":
            return {
                title: qsTr("Your %1").arg(who),
                text: qsTr("Jog wheels, play, cue, sync, performance pads, EQ, filter and BEAT FX follow the Mixxx mapping for this model. BROWSE scrolls the library and LOAD puts the track on a deck. Tested so far on the DDJ-FLX4: other Pioneer models may need small adjustments, tell us what feels off.")
            };
        case "rane":
            return {
                title: qsTr("Your Rane Four"),
                text: qsTr("The mixer part works: faders, EQ, filter, gain, headphone cue and mix, crossfader. Per deck: play, cue, sync, pitch, nudge, key lock, loop out and beat jump. Loading tracks and the jog wheels are not mapped yet: load from the touch screen.")
            };
        case "hercules":
            return {
                title: qsTr("Your %1").arg(who),
                text: qsTr("Mixer, play, cue, sync, loops, hot cues and effects follow the Mixxx mapping; the jog wheel scratches when touched. Choose a track with the browse controls and load it with the LOAD buttons, or use the touch screen.")
            };
        case "numark":
            return {
                title: qsTr("Your %1").arg(who),
                text: qsTr("Transport, sync, pitch, loops, EQ and effects follow the Mixxx mapping for this model; the jog wheel scratches in vinyl mode. Choose a track with the browse knob and load it with the LOAD buttons. Many Numark controllers are only partly tested here.")
            };
        case "reloop":
            return {
                title: qsTr("Your %1").arg(who),
                text: qsTr("The Mixxx mapping covers the mixer, transport, loops, hot cues and effects. Browse and load as on the unit, or use the touch screen for anything not mapped yet.")
            };
        case "denon":
            return {
                title: qsTr("Your %1").arg(who),
                text: qsTr("Mixer, transport, pads, loops and effects follow the Mixxx mapping. Larger Denon units also send audio over USB: check the output in Settings > Sound if you hear nothing.")
            };
        case "roland":
            return {
                title: qsTr("Your %1").arg(who),
                text: qsTr("The mapping needs the unit in its MIDI mode and sends many messages for lights and screens: if a light looks wrong, unplug and plug it back in.")
            };
        case "ni":
            return {
                title: qsTr("Your Traktor Kontrol"),
                text: qsTr("Traktor controllers that talk HID or bulk USB can't be used through Android's MIDI interface yet. If it doesn't respond, use another controller or the touch screen.")
            };
        default:
            return {
                title: deviceName !== "" ? qsTr("Your %1").arg(who) : qsTr("Plug in your controller"),
                text: deviceName !== "" ? qsTr("It is connected, but there is no mapping for it yet: pick one in Settings > Controllers or mix with the touch screen.") : qsTr("Connect the controller to the USB port with an OTG cable (a hub if the phone can't power it). It is recognised and set up by itself: nothing to install, no root. The Pioneer DDJ-FLX4 is the tested reference; other models use the Mixxx mappings.")
            };
        }
    }
    readonly property bool hasStemsPad: brand === "flx4"
    readonly property var steps: [
        {
            title: qsTr("Welcome to Ubøros DJ"),
            text: qsTr("Free and open source DJ software: no account, no subscription, no ads. A short tour; you can skip it now and find it again in Settings.")
        },
        controllerStep,
        {
            title: qsTr("Load a track"),
            text: brand === "flx4" ? qsTr("Browse the library with the BROWSE knob and press LOAD on the deck you want. On a phone the library is hidden to leave room for the waveforms: press BROWSE to show it.") : qsTr("Pick a track in the library on the screen and drag it onto a deck, or use your controller's browse and load buttons. On a phone the library can be hidden to leave room for the waveforms: use the Library button.")
        },
        {
            title: qsTr("Tracks from a rekordbox USB stick"),
            text: qsTr("Plug in a USB stick prepared with rekordbox and use Import USB drive: tracks, beat grids, cues and crates show up in the library. You can also export your library to rekordbox.xml.")
        },
        {
            title: qsTr("Stems on the phone"),
            text: hasStemsPad ? qsTr("Split a track into vocals, drums, bass and other directly on the device, with no internet. On the DDJ-FLX4 SMART CFX controls the stems. The first track takes a little while; the result is saved for next time.") : qsTr("Split a track into vocals, drums, bass and other directly on the device, with no internet. Stems are controlled from the screen. The first track takes a little while; the result is saved for next time.")
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
