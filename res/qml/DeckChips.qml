import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"
import "Library/Harmony.js" as Harmony

// Interface profile Stack: the info of a deck as semi-transparent chips in the
// corners of its waveform instead of the deck row (title top left, BPM, key
// and time left top right, loop and beat counter at the bottom). A reduced
// DeckInfoBar: the same data and formats, no cover, no marquee; MASTER, SYNC,
// MT and the stem mode of the SMART CFX are kept because the deck row, where
// they are shown, is not in this view. Nothing here takes mouse events.
Item {
    id: root

    required property string group
    readonly property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)
    readonly property var currentTrack: deckPlayer ? deckPlayer.currentTrack : null
    readonly property color deckColor: Theme.deckColor(group)
    readonly property string deckNumber: {
        const match = /(\d+)\]$/.exec(group);
        return match ? match[1] : "";
    }
    readonly property bool loaded: trackLoadedControl.value != 0
    readonly property color chipColor: Qt.rgba(0, 0, 0, 0.55)
    // A track that could not be opened: said on the chip for a while
    property string failure: ""
    // The title stays out of the middle of the waveform (the play marker) and
    // out of the info chip
    readonly property real titleMax: Math.max(40, Math.min(root.width * 0.46 - 36, root.width - (infoChip.visible ? infoChip.width : 0) - 46))

    Timer {
        id: failureTimer

        interval: 8000
        onTriggered: root.failure = ""
    }

    Connections {
        target: root.deckPlayer
        ignoreUnknownSignals: true

        function onLoadFailed(reason) {
            root.failure = reason;
            failureTimer.restart();
        }

        function onTrackLoading() {
            root.failure = "";
        }
    }

    // Deck number and track title (or what is going on with an empty deck)
    Rectangle {
        id: titleChip

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 4
        anchors.topMargin: 4
        height: 18
        radius: 9
        width: 2 + 14 + 5 + titleText.width + 9
        color: root.chipColor

        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            width: 14
            height: 14
            radius: 7
            color: root.deckColor

            Text {
                anchors.centerIn: parent
                text: root.deckNumber
                font.pixelSize: 10
                font.bold: true
                color: "#0b0c0f"
            }
        }

        Text {
            id: titleText

            anchors.left: parent.left
            anchors.leftMargin: 2 + 14 + 5
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, root.titleMax)
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: 11
            font.bold: root.loaded
            font.italic: !root.loaded && root.failure === ""
            color: root.loaded ? Theme.white : (root.failure !== "" ? Theme.errorColor : Theme.textMutedColor)
            text: {
                if (root.loaded)
                    return root.currentTrack ? root.currentTrack.title : "";
                if (root.deckPlayer && root.deckPlayer.isLoading)
                    return qsTr("Loading…");
                if (root.failure !== "")
                    return root.failure.indexOf("File not found") === 0 ? root.failure : qsTr("Couldn't load track: %1").arg(root.failure);
                return qsTr("Load a track");
            }
        }
    }

    // BPM, key (pill in the color of the Camelot wheel), remaining time and
    // the deck state (MASTER, SYNC, MT: shown only when on)
    Rectangle {
        id: infoChip

        visible: root.loaded
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 4
        anchors.topMargin: 4
        height: 18
        radius: 9
        width: infoRow.implicitWidth + 14
        color: root.chipColor

        Row {
            id: infoRow

            anchors.centerIn: parent
            spacing: 6

            Skin.StatusChip {
                anchors.verticalCenter: parent.verticalCenter
                text: "MASTER"
                tint: "#ff9a3c"
                active: syncLeaderControl.value > 0
            }

            Skin.StatusChip {
                anchors.verticalCenter: parent.verticalCenter
                text: "SYNC"
                tint: "#4ec3ff"
                active: syncEnabledControl.value > 0
            }

            Skin.StatusChip {
                anchors.verticalCenter: parent.verticalCenter
                // Master Tempo, as the CDJ calls keylock
                text: "MT"
                tint: "#c38bff"
                active: keylockStateControl.value > 0
            }

            Text {
                id: bpmText

                // Current BPM like on the deck bar: the track's BPM times
                // the tempo fader; with SYNC on, the tempo that sync matches
                readonly property real displayBpm: fileBpmControl.value > 0 && !syncEnabledControl.value ? fileBpmControl.value * rateRatioControl.value : bpmControl.value
                readonly property bool unsure: root.currentTrack ? root.currentTrack.beatsUnsure : false

                anchors.verticalCenter: parent.verticalCenter
                text: displayBpm > 0 ? displayBpm.toFixed(2) + (unsure ? "?" : "") : "-"
                font.family: Theme.fontFamily
                font.pixelSize: 13
                font.bold: true
                color: unsure ? Theme.yellow : Theme.white
            }

            Item {
                id: keyPill

                readonly property var camelot: Harmony.fromChromaticKey(keyControl.value)
                // Key shift in semitones: the pill then shows the shifted key ("9A +2")
                readonly property int shiftSteps: Math.round(pitchAdjustControl.value)
                readonly property bool shifted: shiftSteps !== 0 && camelot !== null

                anchors.verticalCenter: parent.verticalCenter
                visible: keyText.text !== ""
                width: keyText.implicitWidth + 10
                height: 14

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: keyPill.camelot ? Qt.darker(Theme.camelotColor(keyPill.camelot.n), 1.5) : Theme.darkGray2
                    // Key Sync on: a white ring
                    border.width: keySyncControl.value > 0 ? 2 : 1
                    border.color: keySyncControl.value > 0 ? Theme.white : (keyPill.camelot ? Theme.camelotColor(keyPill.camelot.n) : Theme.deckLineColor)
                }

                Text {
                    id: keyText

                    anchors.centerIn: parent
                    text: keyPill.shifted ? Harmony.camelotText(keyControl.value) + " " + (keyPill.shiftSteps > 0 ? "+" : "") + keyPill.shiftSteps : (root.currentTrack ? root.currentTrack.keyText : "")
                    font.pixelSize: 10
                    font.bold: true
                    color: Theme.white
                }

                // KEY SYNC on/off with a tap, off and shift taken away with a long press (like the info bar)
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    enabled: !!root.currentTrack
                    onClicked: keySyncControl.value = keySyncControl.value > 0 ? 0 : 1
                    onPressAndHold: {
                        keySyncControl.value = 0;
                        resetKeyControl.value = 1;
                        resetKeyControl.value = 0;
                    }
                }
            }

            Text {
                id: timeText

                // Remaining time like on a CDJ; red in the last 30 s of a
                // playing track
                readonly property real remaining: Math.max(0, durationControl.value * (1 - playPositionControl.value))
                readonly property bool ending: playControl.value > 0 && durationControl.value > 0 && remaining < 30

                anchors.verticalCenter: parent.verticalCenter
                visible: durationControl.value > 0
                text: {
                    const seconds = Math.floor(remaining);
                    const minutes = Math.floor(seconds / 60);
                    const rest = seconds % 60;
                    return "-" + minutes + ":" + (rest < 10 ? "0" : "") + rest;
                }
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.bold: true
                color: ending ? "#ff3030" : Theme.white
            }
        }
    }

    // Loop and beat counter, over the lower corners of the waveform
    Skin.DeckOverviewMarks {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 3
        height: 16
        group: root.group
    }

    // Pad mode of the controller at the bottom, after the loop badge: hot cue
    // A-H (colors) and the beat loop sizes; the other modes show their name
    // only. Nothing until the controller reports its pad mode.
    Skin.DeckPads {
        anchors.left: parent.left
        anchors.leftMargin: 64
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        group: root.group
        compact: true
    }

    // DDJ-FLX4 SMART CFX held: the EQ knobs drive the stems (the same message
    // as the deck bar, on the top of the waveform)
    Rectangle {
        id: stemModeOverlay

        // Real stems, or instant stems already computed at the play position
        readonly property bool hasStems: stemCountControl.value > 0 && stemsReadyControl.value > 0
        readonly property bool separating: stemProgressControl.value >= 0 && stemProgressControl.value < 1

        visible: stemModeControl.value > 0 && root.loaded
        z: 20
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.topMargin: 4
        height: 18
        radius: 9
        color: hasStems ? "#e6102a3a" : "#e63a2a08"
        border.width: 1
        border.color: hasStems ? "#33c8ff" : "#ffb000"

        Text {
            anchors.centerIn: parent
            text: stemModeOverlay.hasStems ? qsTr("STEMS   HIGH vocals · MID melody · LOW drums") : (stemModeOverlay.separating ? qsTr("Separating stems %1%: knobs = EQ until ready").arg(Math.floor(Math.max(0, stemProgressControl.value) * 100)) : qsTr("No stems on this track: knobs = EQ"))
            font.family: Theme.fontFamily
            font.pixelSize: 11
            font.bold: true
            color: stemModeOverlay.hasStems ? "#9fe6ff" : "#ffd27a"
        }
    }

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }

    Mixxx.ControlProxy {
        id: bpmControl

        group: root.group
        key: "bpm"
    }

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

    Mixxx.ControlProxy {
        id: keyControl

        group: root.group
        key: "key"
    }

    Mixxx.ControlProxy {
        id: pitchAdjustControl

        group: root.group
        key: "pitch_adjust"
    }

    Mixxx.ControlProxy {
        id: keySyncControl

        group: root.group
        key: "key_sync_enabled"
    }

    Mixxx.ControlProxy {
        id: resetKeyControl

        group: root.group
        key: "reset_key"
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
        // Android: playposition changes on every audio callback; feeds only the remaining-time text
        updateInterval: Qt.platform.os === "android" ? 100 : 0
    }

    Mixxx.ControlProxy {
        id: playControl

        group: root.group
        key: "play"
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

    Mixxx.ControlProxy {
        id: stemsReadyControl

        group: root.group
        key: "stems_ready"
    }

    Mixxx.ControlProxy {
        id: stemProgressControl

        group: root.group
        key: "stem_progress"
    }
}
