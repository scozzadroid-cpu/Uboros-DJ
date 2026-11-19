import "." as Skin
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property string group

    function profileApply() {
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

    Rectangle {
        id: waveformContainer

        anchors.fill: parent
        color: Theme.deckBackgroundColor

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
