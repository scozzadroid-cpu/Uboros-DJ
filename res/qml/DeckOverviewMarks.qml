import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

// Loop badge (left) and beat counter (right) of a deck, drawn over its
// overview. The same as the ones of the minimized deck in Deck.qml (uboros),
// for the overview on the waveform (interface profiles Club and Stack, see
// WaveformDisplay.qml): the size of this item is the one of the strip they
// are drawn on, they are centered vertically.
Item {
    id: root

    required property string group

    // Active loop with its length in beats (MASTER, SYNC and KEY LOCK are
    // in the info bar)
    Row {
        anchors.left: parent.left
        anchors.leftMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3

        component Badge: Rectangle {
            property alias text: badgeText.text
            property color tint: Theme.white

            height: 13
            width: badgeText.implicitWidth + 6
            radius: 2
            color: Qt.rgba(0, 0, 0, 0.65)
            border.width: 1
            border.color: tint

            Text {
                id: badgeText

                anchors.centerIn: parent
                font.pixelSize: 9
                font.bold: true
                color: parent.tint
            }
        }

        Badge {
            visible: loopControl.value > 0
            text: "LOOP " + (loopSizeControl.value >= 1 ? Math.round(loopSizeControl.value) : "1/" + Math.round(1 / loopSizeControl.value))
            tint: "#4ee04e"
        }
    }

    // Beat counter, like the CDJ: four boxes (beat in the bar) and
    // "bar.beat" from the first beat of the grid, which Mixxx and
    // Rekordbox put on a downbeat. Assumes 4/4.
    Row {
        id: beatCounter

        readonly property var beats: root.deckPlayer && root.deckPlayer.currentTrack ? root.deckPlayer.currentTrack.beatsModel : null
        readonly property real firstBeatFrame: {
            if (!beats || beats.rowCount() === 0)
                return NaN;
            const first = beats.get(0);
            return first ? first.framePosition : NaN;
        }
        // beat_prev/beat_next are engine sample positions (2 per frame),
        // -1 when there is no beat before/after the play position
        readonly property real framesPerBeat: {
            if (prevBeatControl.value >= 0 && nextBeatControl.value > prevBeatControl.value)
                return (nextBeatControl.value - prevBeatControl.value) / 2;
            if (beatFileBpmControl.value > 0 && beatSampleRateControl.value > 0)
                return 60 * beatSampleRateControl.value / beatFileBpmControl.value;
            return 0;
        }
        readonly property int beatIndex: {
            if (isNaN(firstBeatFrame) || !(framesPerBeat > 0))
                return -1;
            if (prevBeatControl.value >= 0)
                return Math.max(0, Math.round((prevBeatControl.value / 2 - firstBeatFrame) / framesPerBeat));
            // Before the first beat: show the first one
            if (nextBeatControl.value >= 0)
                return 0;
            return -1;
        }
        readonly property int beatInBar: beatIndex >= 0 ? beatIndex % 4 : -1

        visible: beatIndex >= 0
        anchors.right: parent.right
        anchors.rightMargin: 3
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Text {
            anchors.verticalCenter: parent.verticalCenter
            rightPadding: 3
            text: (Math.floor(beatCounter.beatIndex / 4) + 1) + "." + (beatCounter.beatInBar + 1)
            font.pixelSize: 9
            font.bold: true
            color: Theme.white
            style: Text.Outline
            styleColor: "#000000"
        }

        Repeater {
            model: 4

            Rectangle {
                required property int index

                width: 8
                height: 8
                radius: 1.5
                color: index === beatCounter.beatInBar ? Theme.deckColor(root.group) : Qt.rgba(0, 0, 0, 0.6)
                border.width: 1
                border.color: index === beatCounter.beatInBar ? Qt.lighter(Theme.deckColor(root.group), 1.3) : Theme.textMutedColor
            }
        }
    }

    readonly property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)

    Mixxx.ControlProxy {
        id: prevBeatControl

        group: root.group
        key: "beat_prev"
    }

    Mixxx.ControlProxy {
        id: nextBeatControl

        group: root.group
        key: "beat_next"
    }

    Mixxx.ControlProxy {
        id: beatFileBpmControl

        group: root.group
        key: "file_bpm"
    }

    Mixxx.ControlProxy {
        id: beatSampleRateControl

        group: root.group
        key: "track_samplerate"
    }

    Mixxx.ControlProxy {
        id: loopControl

        group: root.group
        key: "loop_enabled"
    }

    Mixxx.ControlProxy {
        id: loopSizeControl

        group: root.group
        key: "beatloop_size"
    }
}
