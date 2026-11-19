import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property string group
    property bool minimized: false
    // Interface profiles Club / Stack: the overview of this deck is on its
    // waveform (WaveformDisplay.qml), so it is not made here too
    property bool overviewOnWave: false
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)

    Drag.active: dragArea.drag.active
    Drag.dragType: Drag.Automatic
    Drag.supportedActions: Qt.CopyAction
    Drag.mimeData: {
        let data = {
            "mixxx/player": group
        };
        const trackLocationUrl = deckPlayer.trackLocationUrl;
        if (trackLocationUrl)
            data["text/uri-list"] = trackLocationUrl;

        return data;
    }

    MouseArea {
        id: dragArea

        anchors.fill: root
        drag.target: root
    }

    Skin.SectionBackground {
        anchors.fill: parent
    }

    Skin.DeckInfoBar {
        id: infoBar

        anchors.leftMargin: 5
        anchors.topMargin: 5
        anchors.rightMargin: 5
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        group: root.group
        // Minimized decks have room to spare: the BPM column also shows the
        // difference from the original BPM
        rightColumnWidth: root.minimized ? 96 : rateSlider.width
        // Club: the BPM is the big number of the minimized deck
        bpmFontSize: root.minimized && Theme.isClub ? 20 : 17
        // Club: the pad mode is on the pads strip below, not a chip here
        padChip: !padsStrip.active
    }

    // A plugin is getting the track that was loaded into this deck: a panel over the
    // info bar (the buttons stay usable: the previous track goes on until the new one is there)
    Skin.DeckLoadingOverlay {
        anchors.fill: infoBar
        group: root.group
        z: 5
    }

    // Club (minimized deck, overview on the waveform): what the pads of the
    // controller do now, on a strip under the info bar. Nothing is created and
    // no height is taken until the controller reports its pad mode.
    Loader {
        id: padsStrip

        // Height of the strip once the controller reports its pad mode (DeckRow
        // makes the row that much taller)
        readonly property real wanted: item && item.shown ? item.stripHeight : 0

        active: root.minimized && root.overviewOnWave && Theme.isClub
        anchors.top: infoBar.bottom
        anchors.topMargin: 2
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.right: parent.right
        anchors.rightMargin: 5
        height: wanted

        sourceComponent: Skin.DeckPads {
            group: root.group
        }
    }
    readonly property real padsHeight: padsStrip.wanted

    // Minimized (e.g. on a phone): a thin overview of the whole track under
    // the info bar, with the play position, the cues and the loops, like on
    // a CDJ. The full deck has its own overview below.
    Rectangle {
        id: miniOverview

        visible: root.minimized && !root.overviewOnWave
        anchors.top: infoBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        anchors.topMargin: 3
        height: 28
        radius: 4
        color: Theme.deckBackgroundColor

        // One overview per deck: here, or on the waveform (Club, Stack)
        Loader {
            anchors.fill: parent
            active: !root.overviewOnWave

            sourceComponent: Skin.WaveformOverview {
                group: root.group
            }
        }

        // Active loop with its length in beats (MASTER, SYNC and KEY LOCK are
        // in the info bar)
        Row {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 2
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
            anchors.top: parent.top
            anchors.margins: 3
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

    Skin.ControlSlider {
        id: rateSlider

        visible: !root.minimized
        anchors.topMargin: 5
        anchors.rightMargin: 5
        anchors.bottomMargin: 5
        anchors.top: infoBar.bottom
        anchors.right: parent.right
        anchors.bottom: buttonBar.top
        width: syncButton.width
        group: root.group
        key: "rate"
        barStart: 0.5
        barColor: Theme.bpmSliderBarColor
        bg: Theme.imgBpmSliderBackground

        FadeBehavior on visible {
            fadeTarget: rateSlider
        }
    }

    Rectangle {
        id: overview

        visible: !root.minimized
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        anchors.bottomMargin: 5
        anchors.top: rateSlider.top
        anchors.bottom: buttonBar.top
        anchors.left: parent.left
        anchors.right: rateSlider.left
        radius: 5
        color: Theme.deckBackgroundColor
        height: 56

        Skin.WaveformOverview {
            group: root.group
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.height - 26
        }

        Item {
            id: waveformBar

            height: 26
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            Rectangle {
                id: waveformBarVSeparator

                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.rightMargin: 5
                anchors.leftMargin: 5
                height: 2
                color: infoBar.lineColor
            }

            InfoBarButton {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: 5
                width: rateSlider.width
                group: "[EffectRack1_EffectUnit1]"
                key: "group_" + root.group + "_enable"
                activeColor: Theme.deckActiveColor

                foreground: Skin.EmbeddedText {
                    anchors.centerIn: parent
                    text: "FX 1"
                }
            }

            Rectangle {
                id: waveformBarHSeparator1

                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: waveformBarVSeparator.left
                anchors.leftMargin: rateSlider.width
                width: 2
                color: infoBar.lineColor
            }

            InfoBarButton {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: waveformBarHSeparator1.left
                width: rateSlider.width
                group: "[EffectRack1_EffectUnit2]"
                key: "group_" + root.group + "_enable"
                activeColor: Theme.deckActiveColor

                foreground: Skin.EmbeddedText {
                    anchors.centerIn: parent
                    text: "FX 2"
                }
            }

            Rectangle {
                id: waveformBarHSeparator2

                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: waveformBarHSeparator1.right
                anchors.leftMargin: rateSlider.width
                width: 2
                color: infoBar.lineColor
            }

            Skin.EmbeddedText {
                id: waveformBarPosition

                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: waveformBarHSeparator2.right
                anchors.leftMargin: 5
                text: {
                    const positionSeconds = samplesControl.value / 2 / sampleRateControl.value * playPositionControl.value;
                    if (isNaN(positionSeconds))
                        return "";

                    let minutes = Math.floor(positionSeconds / 60);
                    let seconds = positionSeconds - (minutes * 60);
                    const deciseconds = Math.trunc((seconds - Math.trunc(seconds)) * 10);
                    seconds = Math.trunc(seconds);
                    if (minutes < 10)
                        minutes = "0" + minutes;

                    if (seconds < 10)
                        seconds = "0" + seconds;

                    return minutes + ':' + seconds + "." + deciseconds;
                }

                Mixxx.ControlProxy {
                    id: playPositionControl

                    group: root.group
                    key: "playposition"
                }

                Mixxx.ControlProxy {
                    id: sampleRateControl

                    group: root.group
                    key: "track_samplerate"
                }

                Mixxx.ControlProxy {
                    id: samplesControl

                    group: root.group
                    key: "track_samples"
                }
            }

            Item {
                id: waveformBarRightSpace

                anchors.top: waveformBar.top
                anchors.bottom: waveformBar.bottom
                anchors.right: waveformBar.right
                width: rateSlider.width
            }

            Rectangle {
                id: waveformBarHSeparator

                anchors.top: waveformBar.top
                anchors.bottom: waveformBar.bottom
                anchors.right: waveformBarRightSpace.left
                anchors.bottomMargin: 5
                width: 2
                color: infoBar.lineColor
            }

            InfoBarButton {
                anchors.top: waveformBarVSeparator.bottom
                anchors.bottom: waveformBar.bottom
                anchors.left: waveformBarRightSpace.left
                anchors.right: waveformBarRightSpace.right
                group: root.group
                key: "quantize"
                activeColor: Theme.deckActiveColor

                foreground: Image {
                    anchors.centerIn: parent
                    source: "images/icon_quantize.svg"
                }
            }

            Item {
                id: waveformBarLeftSpace

                anchors.top: waveformBar.top
                anchors.bottom: waveformBar.bottom
                anchors.right: waveformBarHSeparator.left
                width: rateSlider.width
            }

            Rectangle {
                id: waveformBarHSeparator3

                anchors.top: waveformBar.top
                anchors.bottom: waveformBar.bottom
                anchors.right: waveformBarLeftSpace.left
                anchors.bottomMargin: 5
                width: 2
                color: infoBar.lineColor
            }

            InfoBarButton {
                anchors.top: waveformBarVSeparator.bottom
                anchors.bottom: waveformBar.bottom
                anchors.left: waveformBarLeftSpace.left
                anchors.right: waveformBarLeftSpace.right
                group: root.group
                key: "passthrough"
                activeColor: Theme.deckActiveColor

                foreground: Image {
                    anchors.centerIn: parent
                    source: "images/icon_passthrough.svg"
                }
            }
        }

        FadeBehavior on visible {
            fadeTarget: overview
        }
    }

    Item {
        id: buttonBar

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        anchors.bottomMargin: 5
        height: 56
        visible: !root.minimized

        Skin.ControlButton {
            id: cueButton

            anchors.left: parent.left
            anchors.bottom: playButton.top
            anchors.bottomMargin: 5
            group: root.group
            key: "cue_default"
            text: "Cue"
            activeColor: Theme.deckActiveColor
        }

        Skin.ControlButton {
            id: playButton

            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.topMargin: 5
            group: root.group
            key: "play"
            text: "Play"
            toggleable: true
            activeColor: Theme.deckActiveColor
        }

        Row {
            anchors.left: playButton.right
            anchors.leftMargin: 10
            anchors.bottom: playButton.bottom
            anchors.topMargin: 5
            spacing: -1

            Skin.IntroOutroButton {
                keyPrefix: "intro_start"
                group: root.group

                text: "Intro\nStart"

                width: playButton.height * 2 - 1
                height: playButton.height
            }

            Skin.IntroOutroButton {
                keyPrefix: "intro_end"
                group: root.group

                text: "Intro\nEnd"

                width: playButton.height * 2 - 1
                height: playButton.height
            }

            Skin.IntroOutroButton {
                keyPrefix: "outro_start"
                group: root.group

                text: "Outro\nStart"

                width: playButton.height * 2 - 1
                height: playButton.height
            }

            Skin.IntroOutroButton {
                keyPrefix: "outro_end"
                group: root.group

                text: "Outro\nEnd"

                width: playButton.height * 2 - 1
                height: playButton.height
            }
        }

        Row {
            anchors.left: cueButton.right
            anchors.top: parent.top
            anchors.leftMargin: 10
            spacing: -1

            Repeater {
                model: 8

                Skin.HotcueButton {
                    required property int index

                    hotcueNumber: this.index + 1
                    group: root.group
                    width: playButton.height
                    height: playButton.height
                }
            }
        }

        Skin.SyncButton {
            id: syncButton

            anchors.right: parent.right
            anchors.top: parent.top
            group: root.group
        }

        FadeBehavior on visible {
            fadeTarget: buttonBar
        }
    }

    Mixxx.PlayerDropArea {
        anchors.fill: parent
        group: root.group
    }
}
