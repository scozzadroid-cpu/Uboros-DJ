import QtQuick 2.12
import "Theme"

// The phrases of a track (its song structure: intro, verses, bridge, chorus,
// outro, and up/down in high-energy tracks) over the overview of a deck, one
// colored segment for each with a short code. Only the Club profile shows it
// (WaveformOverview.qml). Everything here is static: a few rectangles and
// texts made when the phrases arrive (once per track), nothing changes with
// the play position.
//
// The overview is drawn in between two instances of this item: the faint
// bands behind it (showTop: false), the solid strip and the codes over it
// (showTint: false).
Item {
    id: root

    // [{kind, number, mood, start, end}] in seconds, from Mixxx.TrackMarks;
    // end 0 means "until the end of the track"
    property var phrases: []
    // Length of the track in seconds
    property real duration: 0
    property bool showTint: true
    property bool showTop: true

    // One color for each kind of phrase (generic palette, not a copy of any
    // product's)
    function kindColor(phrase) {
        switch (phrase.kind) {
        case "intro":
            return "#4a8fe0";
        case "verse":
            return phrase.number % 2 === 0 ? "#2fa88a" : "#3cc08f";
        case "bridge":
            return "#e0a232";
        case "chorus":
            return "#e0507c";
        case "outro":
            return "#8a64d6";
        case "up":
            return "#f07c32";
        case "down":
            return "#30b4cc";
        }
        return "#808080";
    }

    // Short code of a phrase
    function kindCode(phrase) {
        switch (phrase.kind) {
        case "intro":
            return "IN";
        case "verse":
            return phrase.number > 0 ? "V" + phrase.number : "V";
        case "bridge":
            return "BR";
        case "chorus":
            return "CH";
        case "outro":
            return "OUT";
        case "up":
            return "UP";
        case "down":
            return "DN";
        }
        return "";
    }

    // The phrase ends where the next one starts (the end of the last one is
    // the "end beat" of the structure); an unknown end is the end of the track
    function endOf(phrase) {
        return phrase.end > phrase.start ? Math.min(phrase.end, root.duration) : root.duration;
    }

    Repeater {
        model: root.duration > 0 ? root.phrases : []

        Item {
            id: segment

            required property var modelData
            readonly property real fromX: root.width * Math.min(modelData.start / root.duration, 1)
            readonly property real toX: root.width * Math.min(root.endOf(modelData) / root.duration, 1)
            readonly property color tint: root.kindColor(modelData)

            x: fromX
            width: Math.max(toX - fromX, 1)
            height: root.height

            // Faint band over the whole height (behind the waveform) and a
            // solid strip along the top (over it), with a gap before the next
            // phrase
            Rectangle {
                visible: root.showTint
                anchors.fill: parent
                anchors.rightMargin: parent.width > 3 ? 1 : 0
                color: segment.tint
                opacity: 0.3
            }

            Rectangle {
                visible: root.showTop
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.rightMargin: parent.width > 3 ? 1 : 0
                height: 3
                color: segment.tint
            }

            // The code, only when the phrase is wide enough to hold it. At
            // the bottom and in the middle: the flags of the hot cues hang
            // from the top at the start of a phrase, where they are usually set
            Text {
                id: code

                visible: root.showTop && implicitWidth + 14 <= segment.width
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                text: root.kindCode(segment.modelData)
                font.pixelSize: 9
                font.bold: true
                color: "#ffffff"
                style: Text.Outline
                styleColor: "#000000"
            }
        }
    }
}
