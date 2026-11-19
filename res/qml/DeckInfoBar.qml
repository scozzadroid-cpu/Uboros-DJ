import "." as Skin
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import "Theme"
import "Library/Harmony.js" as Harmony

Rectangle {
    id: root

    required property string group
    required property int rightColumnWidth
    // Size of the BPM number (Deck.qml: bigger on the Club minimized deck)
    property int bpmFontSize: 17
    // The small chip with the pad mode of the controller (DeckPadChip.qml);
    // off where the deck shows the pads on a strip of its own (Club)
    property bool padChip: true
    // Key / remaining time column: narrower than the BPM column (its
    // content is short), the room goes to title and artist
    readonly property int keyColumnWidth: Math.min(rightColumnWidth, 74)
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)
    property var currentTrack: deckPlayer ? deckPlayer.currentTrack : null
    property color lineColor: Theme.deckLineColor
    readonly property color deckColor: Theme.deckColor(group)
    readonly property string deckNumber: {
        const match = /(\d+)\]$/.exec(group);
        return match ? match[1] : "";
    }

    border.width: 1
    border.color: Theme.deckLineColor
    radius: 6
    height: 56

    // Deck color stripe on the left edge
    Rectangle {
        z: 1
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        width: 3
        radius: 1.5
        color: root.deckColor
    }

    Image {
        id: coverArt

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 5
        width: height
        source: root.currentTrack ? root.currentTrack.coverArtUrl : ""
        sourceSize.width: 160
        sourceSize.height: 160
        visible: false
        asynchronous: true
    }

    Rectangle {
        id: coverArtCircle

        anchors.fill: coverArt
        radius: height / 2
        visible: false
    }

    OpacityMask {
        anchors.fill: coverArt
        source: coverArt
        maskSource: coverArtCircle
    }

    Rectangle {
        id: spinnyCircle

        anchors.fill: coverArt
        radius: height / 2
        border.width: 2
        border.color: Qt.darker(root.deckColor, 1.6)
        color: "transparent"
    }

    // Deck number, in the deck color
    Rectangle {
        z: 2
        anchors.right: coverArt.right
        anchors.bottom: coverArt.bottom
        anchors.rightMargin: -3
        anchors.bottomMargin: -3
        width: 17
        height: 17
        radius: 8.5
        color: root.deckColor
        border.width: 2
        border.color: Theme.deckBackgroundColor

        Text {
            anchors.centerIn: parent
            text: root.deckNumber
            font.pixelSize: 10
            font.bold: true
            color: "#0b0c0f"
        }
    }

    Item {
        id: spinny

        anchors.fill: coverArt

        // The Spinnies are automatically hidden if the track
        // is stopped. This is not really useful, but is nice for
        // demo'ing transitions.
        Mixxx.ControlProxy {
            group: root.group
            key: "play"
            onValueChanged: (value) => {
                spinnyIndicator.indicatorVisible = (value > 0);
            }
        }

        MixxxControls.Spinny {
            id: spinnyIndicator

            anchors.fill: parent
            group: root.group
            indicatorVisible: false

            indicator: Item {
                width: spinnyIndicator.width
                height: spinnyIndicator.height

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 2
                    height: parent.height / 2
                    color: Theme.deckTextColor
                }
            }
        }
    }

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }

    // Empty deck, or track being opened: playback of the other deck goes on
    Text {
        id: emptyDeckText

        property string failure: ""

        visible: trackLoadedControl.value == 0
        anchors.left: coverArt.right
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: root.deckPlayer && root.deckPlayer.isLoading ? qsTr("Loading…") : (failure !== "" ? (failure.indexOf("File not found") === 0 ? failure : qsTr("Couldn't load track: %1").arg(failure)) : qsTr("Load a track"))
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textFontPixelSize
        font.italic: failure === ""
        color: failure !== "" ? Theme.red : Theme.textMutedColor
        elide: Text.ElideRight
        maximumLineCount: 2
        wrapMode: Text.Wrap

        Timer {
            id: failureTimer

            interval: 8000
            onTriggered: emptyDeckText.failure = ""
        }

        Connections {
            target: root.deckPlayer
            ignoreUnknownSignals: true

            function onLoadFailed(reason) {
                emptyDeckText.failure = reason;
                failureTimer.restart();
            }

            function onTrackLoading() {
                emptyDeckText.failure = "";
            }
        }
    }

    // Track title: scrolls (marquee) when it does not fit, like a CDJ
    Item {
        id: infoBarTitle

        anchors.top: infoBarHSeparator1.top
        anchors.left: infoBarVSeparator.left
        anchors.right: infoBarHSeparator1.left
        anchors.bottom: infoBarVSeparator.bottom
        clip: true

        readonly property real overflow: Math.max(0, titleText.implicitWidth - width)

        Skin.EmbeddedText {
            id: titleText

            text: root.currentTrack ? root.currentTrack.title : ""
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.max(implicitWidth, parent.width)
            horizontalAlignment: Text.AlignLeft
            font.bold: true
            font.pixelSize: Theme.textFontPixelSize
            elide: Text.ElideNone
            color: Theme.white
            x: 0

            onTextChanged: {
                titleScroll.stop();
                x = 0;
                if (infoBarTitle.overflow > 0)
                    titleScroll.restart();
            }
        }

        onOverflowChanged: {
            titleScroll.stop();
            titleText.x = 0;
            if (overflow > 0)
                titleScroll.restart();
        }

        SequentialAnimation {
            id: titleScroll

            loops: Animation.Infinite

            PauseAnimation {
                duration: 2000
            }
            NumberAnimation {
                target: titleText
                property: "x"
                from: 0
                to: -infoBarTitle.overflow
                // ~30 px per second, readable while mixing
                duration: Math.max(1000, infoBarTitle.overflow * 33)
                easing.type: Easing.Linear
            }
            PauseAnimation {
                duration: 1500
            }
            NumberAnimation {
                target: titleText
                property: "x"
                to: 0
                duration: 400
                easing.type: Easing.InOutQuad
            }
        }
    }

    Rectangle {
        id: infoBarVSeparator

        // Hidden on an empty deck: it would cross the "Load a track" hint
        opacity: trackLoadedControl.value != 0 ? 1 : 0
        anchors.left: coverArt.right
        anchors.right: root.right
        anchors.verticalCenter: root.verticalCenter
        anchors.margins: 5
        height: 2
        color: root.lineColor
    }

    // Analysis of the loaded track (waveform, beats, key) in progress
    Rectangle {
        id: analysisBadge

        readonly property real progress: root.deckPlayer ? root.deckPlayer.analysisProgress : -1

        visible: progress >= 0 && trackLoadedControl.value != 0
        z: 1
        anchors.right: infoBarHSeparator1.left
        anchors.rightMargin: 6
        anchors.verticalCenter: infoBarArtist.verticalCenter
        height: analysisText.implicitHeight + 4
        width: analysisText.implicitWidth + 12
        radius: height / 2
        color: Theme.darkGray2
        border.width: 1
        border.color: Theme.accentColor

        Text {
            id: analysisText

            anchors.centerIn: parent
            text: qsTr("Analyzing %1%").arg(Math.round(analysisBadge.progress * 100))
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize - 3
            color: Theme.white
        }
    }

    Skin.EmbeddedText {
        id: infoBarArtist

        text: root.currentTrack ? root.currentTrack.artist : ""
        anchors.top: infoBarVSeparator.bottom
        anchors.left: infoBarVSeparator.left
        anchors.right: infoBarHSeparator1.left
        anchors.bottom: infoBarHSeparator1.bottom
        anchors.rightMargin: statusChips.visible ? statusChips.width + 10 : 0
        horizontalAlignment: Text.AlignLeft
        font.bold: false
        font.pixelSize: Theme.textFontPixelSize - 1
        color: Theme.textMutedColor
        elide: Text.ElideRight
    }

    // Deck state like the CDJ display, in the free space of the artist row:
    // shown only when on (MASTER, SYNC, MT, Q), so the artist keeps the room
    Row {
        id: statusChips

        visible: trackLoadedControl.value != 0 && !analysisBadge.visible
        anchors.right: infoBarHSeparator1.left
        anchors.rightMargin: 6
        anchors.verticalCenter: infoBarArtist.verticalCenter
        spacing: 3

        // Pad mode of the controller (only when it reports it)
        Skin.DeckPadChip {
            group: root.group
            allowed: root.padChip
        }

        // Instant stems: progress, green when ready. Tap: separate the stems
        // of this track now (as SMART CFX)
        Skin.StatusChip {
            id: instantStemBadge

            readonly property real progress: stemProgressControl.value

            text: progress >= 1
                ? "STEMS"
                : (progress > 0 ? "STEMS " + Math.floor(progress * 100) + "%" : "STEMS \u25B6")
            tint: progress >= 1 ? "#40ff60" : "#33c8ff"
            active: progress >= 0

            TapHandler {
                onTapped: stemSeparateControl.value = 1
            }
        }

        Skin.StatusChip {
            text: "MASTER"
            tint: "#ff9a3c"
            active: syncLeaderControl.value > 0
        }

        Skin.StatusChip {
            text: "SYNC"
            tint: "#4ec3ff"
            active: syncEnabledControl.value > 0
        }

        Skin.StatusChip {
            // Master Tempo, as the CDJ calls keylock
            text: "MT"
            tint: "#c38bff"
            active: keylockStateControl.value > 0
        }

        Skin.StatusChip {
            // Quantize (FLX4: SHIFT + channel CUE)
            text: "Q"
            tint: "#7be0a0"
            active: quantizeControl.value > 0
        }

        Mixxx.ControlProxy {
            id: quantizeControl

            group: root.group
            key: "quantize"
        }

        Mixxx.ControlProxy {
            id: syncLeaderControl

            group: root.group
            key: "sync_leader"
        }

        Mixxx.ControlProxy {
            id: syncEnabledControl

            group: root.group
            key: "sync_enabled"
        }

        Mixxx.ControlProxy {
            id: keylockStateControl

            group: root.group
            key: "keylock"
        }
    }

    Rectangle {
        id: infoBarHSeparator1

        anchors.top: root.top
        anchors.bottom: root.bottom
        anchors.right: infoBarKey.left
        anchors.topMargin: 5
        anchors.bottomMargin: 5
        width: 2
        color: root.lineColor
    }

    // Key in a pill colored like the Camelot wheel
    Item {
        id: infoBarKey

        readonly property var camelot: Harmony.fromChromaticKey(keyControl.value)

        anchors.top: infoBarHSeparator1.top
        anchors.bottom: infoBarVSeparator.top
        anchors.right: infoBarHSeparator2.left
        width: root.keyColumnWidth

        Rectangle {
            anchors.centerIn: parent
            visible: keyText.text !== ""
            width: Math.min(parent.width - 6, keyText.implicitWidth + 10)
            height: Math.min(parent.height - 4, 18)
            radius: height / 2
            color: infoBarKey.camelot ? Qt.darker(Theme.camelotColor(infoBarKey.camelot.n), 1.5) : Theme.darkGray2
            border.width: 1
            border.color: infoBarKey.camelot ? Theme.camelotColor(infoBarKey.camelot.n) : Theme.deckLineColor
        }

        Text {
            id: keyText

            anchors.centerIn: parent
            width: parent.width - 8
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: root.currentTrack ? root.currentTrack.keyText : ""
            font.pixelSize: 11
            font.bold: true
            color: Theme.white
        }

        Mixxx.ControlProxy {
            id: keyControl

            group: root.group
            key: "key"
        }
    }

    Rectangle {
        id: infoBarHSeparator2

        anchors.top: root.top
        anchors.bottom: root.bottom
        anchors.right: infoBarRateRatio.left
        anchors.topMargin: 5
        anchors.bottomMargin: 5
        width: 2
        color: root.lineColor
    }

    Skin.EmbeddedText {
        id: infoBarRate

        anchors.top: infoBarHSeparator2.top
        anchors.bottom: infoBarVSeparator.top
        anchors.right: root.right
        anchors.rightMargin: 5
        width: root.rightColumnWidth
        // Current BPM like on a CDJ: the track's BPM times the tempo fader.
        // Not the local "bpm" of the beat grid, which wanders by a few
        // hundredths on rekordbox grids (one tempo per beat). With SYNC on,
        // the tempo that sync matches (the local "bpm"), so that synced decks
        // show the same BPM even when a grid is not constant.
        readonly property real displayBpm: fileBpmControl.value > 0 && !syncEnabledControl.value
            ? fileBpmControl.value * rateRatioControl.value
            : bpmControl.value
        // A rekordbox grid that could not be checked against the audio (its
        // two halves disagree, usually a wrong BPM): show it, like a warning
        readonly property bool unsure: root.currentTrack ? root.currentTrack.beatsUnsure : false
        text: displayBpm > 0 ? displayBpm.toFixed(2) + (unsure ? "?" : "") : "-"
        font.bold: true
        font.pixelSize: root.bpmFontSize
        color: unsure ? Theme.yellow : Theme.white

        Mixxx.ControlProxy {
            id: bpmControl

            group: root.group
            key: "bpm"
        }
    }

    Skin.EmbeddedText {
        id: infoBarRateRatio

        property real ratio: ((rateRatioControl.value - 1) * 100).toPrecision(2)

        anchors.top: infoBarVSeparator.bottom
        anchors.bottom: infoBarHSeparator1.bottom
        width: root.rightColumnWidth
        anchors.right: root.right
        anchors.rightMargin: 5
        // Difference from the original BPM of the track and tempo change,
        // like the CDJ display: "+2.0 (+1.6%)"
        readonly property real bpmDelta: fileBpmControl.value > 0 ? fileBpmControl.value * (rateRatioControl.value - 1) : 0
        text: {
            const percent = ((ratio > 0) ? "+" : "") + ratio.toFixed(1) + "%";
            const range = " \u00b1" + Math.round(rateRangeControl.value * 100);
            if (fileBpmControl.value <= 0)
                return percent + range;
            return ((bpmDelta > 0) ? "+" : "") + bpmDelta.toFixed(2) + " (" + percent + ")" + range;
        }

        // Tap to cycle the tempo range, shown like the CDJ "±10" (SHIFT + BEAT
        // SYNC on the FLX4 is keylock now)
        TapHandler {
            onTapped: {
                const ranges = [0.06, 0.10, 0.16, 0.25];
                const current = ranges.findIndex(r => Math.abs(r - rateRangeControl.value) < 0.001);
                rateRangeControl.value = ranges[(current + 1) % ranges.length];
            }
        }

        Mixxx.ControlProxy {
            id: rateRangeControl

            group: root.group
            key: "rateRange"
        }
        font.pixelSize: 11
        color: Math.abs(ratio) < 0.05 ? Theme.textMutedColor : (ratio > 0 ? "#ff9a3c" : "#4ec3ff")

        Mixxx.ControlProxy {
            id: fileBpmControl

            group: root.group
            key: "file_bpm"
        }

        Mixxx.ControlProxy {
            id: rateRatioControl

            group: root.group
            key: "rate_ratio"
        }
    }

    // Remaining time, like on a CDJ: red and blinking during the last 30 s of
    // a playing track
    Skin.EmbeddedText {
        id: infoBarTime

        readonly property real remaining: Math.max(0, durationControl.value * (1 - playPositionControl.value))
        readonly property bool ending: playControl.value > 0 && durationControl.value > 0 && remaining < 30
        property bool blink: false

        anchors.top: infoBarVSeparator.bottom
        anchors.bottom: infoBarHSeparator1.bottom
        anchors.right: infoBarHSeparator2.left
        width: root.keyColumnWidth
        text: {
            if (durationControl.value <= 0)
                return "";
            const seconds = Math.floor(remaining);
            const minutes = Math.floor(seconds / 60);
            const rest = seconds % 60;
            return "-" + minutes + ":" + (rest < 10 ? "0" : "") + rest;
        }
        color: ending ? (blink ? Theme.white : "#ff3030") : Theme.deckTextColor
        font.bold: true
        font.pixelSize: 14

        Timer {
            interval: 500
            repeat: true
            running: infoBarTime.ending
            onTriggered: infoBarTime.blink = !infoBarTime.blink
            onRunningChanged: infoBarTime.blink = false
        }

        Mixxx.ControlProxy {
            id: durationControl

            group: root.group
            key: "duration"
        }

        Mixxx.ControlProxy {
            id: playPositionControl

            group: root.group
            key: "playposition"
        }

        Mixxx.ControlProxy {
            id: playControl

            group: root.group
            key: "play"
        }
    }

    gradient: Gradient {
        orientation: Gradient.Horizontal

        GradientStop {
            position: 0
            color: {
                const trackColor = root.currentTrack ? root.currentTrack.color : undefined;
                if (!trackColor || !trackColor.valid)
                    return Theme.deckBackgroundColor;

                return Qt.darker(trackColor, 2);
            }
        }

        GradientStop {
            position: 1
            color: Theme.deckBackgroundColor
        }
    }

    // DDJ-FLX4 SMART CFX held: the EQ knobs drive the stems. A deck whose
    // track has no stems keeps its EQ, and says so.
    Mixxx.ControlProxy {
        id: stemModeControl

        group: "[Skin]"
        key: "stem_mode"
    }

    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.group
        key: "stem_count"
    }

    // Instant stems (StemSeparationService): -1 none, 0..1 separated share
    Mixxx.ControlProxy {
        id: stemProgressControl

        group: root.group
        key: "stem_progress"
    }

    Mixxx.ControlProxy {
        id: stemsReadyControl

        group: root.group
        key: "stems_ready"
    }

    Mixxx.ControlProxy {
        id: stemSeparateControl

        group: root.group
        key: "stem_separate"
    }

    Rectangle {
        id: stemModeOverlay

        // Real stems, or instant stems already computed at the play position
        readonly property bool hasStems: stemCountControl.value > 0 && stemsReadyControl.value > 0
        readonly property bool separating: stemProgressControl.value >= 0 && stemProgressControl.value < 1

        visible: stemModeControl.value > 0 && trackLoadedControl.value != 0
        z: 20
        anchors.fill: parent
        radius: root.radius
        color: hasStems ? "#e6102a3a" : "#e63a2a08"
        border.width: 2
        border.color: hasStems ? "#33c8ff" : "#ffb000"

        Text {
            anchors.centerIn: parent
            text: stemModeOverlay.hasStems
                ? qsTr("STEMS   HIGH vocals · MID melody · LOW drums")
                : (stemModeOverlay.separating
                    ? qsTr("Separating stems %1%: knobs = EQ until ready").arg(Math.floor(Math.max(0, stemProgressControl.value) * 100))
                    : qsTr("No stems on this track: knobs = EQ"))
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize + 1
            font.bold: true
            color: stemModeOverlay.hasStems ? "#9fe6ff" : "#ffd27a"
        }
    }
}
