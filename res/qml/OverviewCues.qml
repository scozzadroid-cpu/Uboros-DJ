import QtQuick 2.12
import "Theme"

// The hot cues of a track over the overview of a deck: a small colored tab at
// the top for each cue, at its position, in the color of the cue (the one
// imported from the drive or set on the controller); a saved loop also gets a
// thin band along the bottom. By interface profile the tab is
//   Club:   a flag with the letter of the pad (A-H), like on a CDJ screen
//   Stack:  a plain colored square
//   Uboros: a plain colored notch
// Hot cues 9 and up (the memory cues of a rekordbox drive) have no letter,
// they are a notch with a thin line in every profile.
// Static: one set of rectangles made when the cues change, nothing per frame.
Item {
    id: root

    // [{index, pos, endPos, color, label, isLoop}] from Mixxx.TrackMarks;
    // positions in engine samples (as the hotcue_N_position controls)
    property var cues: []
    // Length of the track in engine samples (the track_samples control)
    property real trackSamples: 0

    function fraction(samples) {
        return trackSamples > 0 ? Math.max(0, Math.min(samples / trackSamples, 1)) : 0;
    }

    // Dark text on a light color, white on a dark one
    function textOn(color) {
        return (color.r * 0.299 + color.g * 0.587 + color.b * 0.114) > 0.6 ? "#101010" : "#ffffff";
    }

    Repeater {
        model: root.trackSamples > 0 ? root.cues : []

        Item {
            id: cue

            required property var modelData
            readonly property bool padCue: modelData.index < 8
            readonly property real cueX: Math.round(root.width * root.fraction(modelData.pos))
            readonly property real loopEndX: Math.round(root.width * root.fraction(modelData.endPos))

            anchors.fill: parent

            // Saved loop: band along the bottom, from start to end
            Rectangle {
                visible: cue.modelData.isLoop && cue.modelData.endPos > cue.modelData.pos
                x: cue.cueX
                width: Math.max(cue.loopEndX - cue.cueX, 1)
                anchors.bottom: parent.bottom
                height: 3
                color: cue.modelData.color
                opacity: 0.7
            }

            // Thin line for the cues that have no line of their own (the
            // overview itself draws one for hot cues 1-8)
            Rectangle {
                visible: !cue.padCue
                x: cue.cueX
                width: 1
                height: parent.height
                color: cue.modelData.color
                opacity: 0.8
            }

            // The tab: starts at the cue and hangs from the top edge
            Rectangle {
                id: tab

                readonly property bool lettered: Theme.isClub && cue.padCue

                x: Theme.isClub ? cue.cueX : cue.cueX - width / 2
                y: 0
                width: lettered ? Math.max(letter.implicitWidth + 4, 9) : (cue.padCue ? 5 : 3)
                height: Math.min(root.height, lettered ? 11 : (Theme.isStack ? 6 : (cue.padCue ? 7 : 5)))
                radius: 1
                color: cue.modelData.color
                border.width: 1
                border.color: Qt.rgba(0, 0, 0, 0.55)

                Text {
                    id: letter

                    visible: tab.lettered
                    anchors.centerIn: parent
                    text: String.fromCharCode(65 + cue.modelData.index)
                    font.pixelSize: 9
                    font.bold: true
                    color: root.textOn(cue.modelData.color)
                }
            }
        }
    }
}
