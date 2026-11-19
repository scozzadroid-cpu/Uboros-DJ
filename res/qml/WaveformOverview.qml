import "." as Skin
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property string group
    readonly property var deckPlayer: Mixxx.PlayerManager.getPlayer(root.group)

    function profileApply() {
        // Run later (Qt.callLater): by then the Club and Stack profiles may
        // have moved this overview from the deck to the waveform
        if (!root.parent)
            return;
        overviewLoader.active = false;
        overviewLoader.active = true;
    }

    states: [
        State {
            when: passthroughControl.value == 0

            PropertyChanges {
                target: waveformContainer
                opacity: 1
                visible: true
            }

            PropertyChanges {
                target: passthroughContainer
                opacity: 0
                visible: false
            }

        },
        State {
            when: passthroughControl.value != 0

            PropertyChanges {
                target: waveformContainer
                opacity: 0
                visible: false
            }

            PropertyChanges {
                target: passthroughContainer
                opacity: 1
                visible: true
            }
        }
    ]
    transitions: [
        Transition {
            enabled: passthroughContainer.visible

            SequentialAnimation {
                PropertyAction {
                    target: waveformContainer
                    property: "visible"
                }

                ParallelAnimation {
                    NumberAnimation {
                        target: waveformContainer
                        property: "opacity"
                        duration: 150
                    }

                    NumberAnimation {
                        target: passthroughContainer
                        property: "opacity"
                        duration: 150
                    }
                }

                PropertyAction {
                    target: passthroughContainer
                    property: "visible"
                }
            }

        },
        Transition {
            enabled: waveformContainer.visible

            SequentialAnimation {
                PropertyAction {
                    target: passthroughContainer
                    property: "visible"
                }

                ParallelAnimation {
                    NumberAnimation {
                        target: waveformContainer
                        property: "opacity"
                        duration: 150
                    }

                    NumberAnimation {
                        target: passthroughContainer
                        property: "opacity"
                        duration: 150
                    }
                }

                PropertyAction {
                    target: waveformContainer
                    property: "visible"
                }
            }
        }
    ]

    Mixxx.ControlProxy {
        id: passthroughControl

        group: root.group
        key: "passthrough"
    }

    // Hot cues (and, on the Club profile, the phrases of the track) to draw
    // over the overview: built once when the track is loaded or its cues
    // change. The phrases are read from the drive only for Club.
    Mixxx.TrackMarks {
        id: trackMarks

        track: root.deckPlayer.currentTrack
        readPhrases: Theme.isClub
    }

    Mixxx.ControlProxy {
        id: trackSamplesControl

        group: root.group
        key: "track_samples"
    }

    Mixxx.ControlProxy {
        id: durationControl

        group: root.group
        key: "duration"
    }

    Rectangle {
        id: waveformContainer

        anchors.fill: parent
        color: Theme.deckBackgroundColor

        // Club: the faint bands of the phrases, behind the overview
        Loader {
            anchors.fill: parent
            active: Theme.isClub

            sourceComponent: Skin.PhraseBar {
                phrases: trackMarks.phrases
                duration: durationControl.value
                showTop: false
            }
        }

        // The overview paints with its colors when it is made or the track
        // changes, not when a color changes: it is made again when the interface
        // profile changes (see WaveformDisplay.qml)
        Loader {
            id: overviewLoader

            anchors.fill: parent

            Connections {
                function onProfileChanged() {
                    Qt.callLater(root.profileApply);
                }

                target: Theme
            }

            sourceComponent: Component {
                MixxxControls.WaveformOverview {
                    anchors.fill: parent
                    channels: Mixxx.WaveformOverview.Channels.LeftChannel
                    renderer: Mixxx.WaveformOverview.Renderer.Filtered
                    // Same three bands as the scrolling waveform
                    colorHigh: Theme.waveHigh
                    colorMid: Theme.waveMid
                    colorLow: Theme.waveLow
                    group: root.group
                }
            }
        }

        // Club: the strip and the codes of the phrases, over the overview
        Loader {
            anchors.fill: parent
            active: Theme.isClub

            sourceComponent: Skin.PhraseBar {
                phrases: trackMarks.phrases
                duration: durationControl.value
                showTint: false
            }
        }

        // The hot cues, over everything (no touch handling: the overview
        // below takes the touches)
        Skin.OverviewCues {
            anchors.fill: parent
            cues: trackMarks.cues
            trackSamples: trackSamplesControl.value
        }
    }

    Rectangle {
        id: passthroughContainer

        anchors.fill: parent
        color: "transparent"

        Skin.SectionText {
            anchors.centerIn: parent
            text: "Passthrough Enabled"
        }
    }
}
