import "." as Skin
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property string group
    property bool splitStemTracks: false
    // Interface profiles Club / Stack (set by main.qml; 0 = none, uboros): the
    // overview of the deck is on its waveform, a strip of this height under it
    // (Club) or above it (Stack, overviewAbove); Stack also draws the info of
    // the deck as chips over the waveform (infoChips)
    property int overviewHeight: 0
    property bool overviewAbove: false
    property bool infoChips: false

    readonly property string zoomGroup: Mixxx.Config.waveformZoomSynchronization() ? "[Channel1]" : group

    enum MouseStatus {
        Normal,
        Bending,
        Scratching
    }

    // Default zoom: the configured one plus the offset of the interface
    // profile (none for "uboros" and "club")
    function defaultZoom() {
        return Math.min(10, Mixxx.Config.waveformDefaultZoom() + Theme.waveZoomOffset);
    }
    // The interface profile changed: the renderers read their colors when they
    // are made, so the display is made again; and the profile's default zoom
    function profileApply() {
        displayLoader.active = false;
        displayLoader.active = true;
        if (zoomControl.group == root.group)
            zoomControl.value = root.defaultZoom();
    }

    // The renderers are made when the display is created: it is created again
    // when the GPU waveform is switched on or off in the dashboard, and when the
    // interface profile changes
    Loader {
        id: displayLoader

        anchors.fill: parent
        anchors.topMargin: root.overviewAbove ? root.overviewHeight : 0
        anchors.bottomMargin: root.overviewAbove ? 0 : root.overviewHeight

        Connections {
            function onWaveformGpuChanged() {
                displayLoader.active = false;
                displayLoader.active = true;
            }

            target: Mixxx.Config
        }
        // Theme.profile follows Mixxx.Config.interfaceProfile (main.qml) and
        // also changes once at start for a profile other than the default.
        // Done later: Theme's palette must have settled
        Connections {
            function onProfileChanged() {
                Qt.callLater(root.profileApply);
            }

            target: Theme
        }

        sourceComponent: Component {
            MixxxControls.WaveformDisplay {
                anchors.fill: parent
                group: root.group
                zoom: zoomControl.value
                backgroundColor: Theme.waveBackground

                Behavior on zoom {
                    SmoothedAnimation {
                        duration: 500
                        velocity: -1
                    }
                }

                Mixxx.WaveformRendererEndOfTrack {
                    color: Theme.waveEndOfTrack
                    endOfTrackWarningTime: 30
                }

                // Before the start / after the end of the track: discreet, the
                // salmon color is kept for the end of track warning only
                Mixxx.WaveformRendererPreroll {
                    color: Theme.wavePreroll
                }

                Mixxx.WaveformRendererMarkRange {
                    // Loop
                    Mixxx.WaveformMarkRange {
                        startControl: "loop_start_position"
                        endControl: "loop_end_position"
                        enabledControl: "loop_enabled"
                        color: '#00b400'
                        opacity: 0.7
                        disabledColor: '#FFFFFF'
                        disabledOpacity: 0.6
                    }
                    // Intro
                    Mixxx.WaveformMarkRange {
                        startControl: "intro_start_position"
                        endControl: "intro_end_position"
                        color: '#2c5c9a'
                        opacity: 0.6
                        durationTextColor: '#ffffff'
                        durationTextLocation: 'after'
                    }
                    // Outro
                    Mixxx.WaveformMarkRange {
                        startControl: "outro_start_position"
                        endControl: "outro_end_position"
                        color: '#2c5c9a'
                        opacity: 0.6
                        durationTextColor: '#ffffff'
                        durationTextLocation: 'before'
                    }
                }

                Mixxx.WaveformRendererFiltered {
                    axesColor: Theme.waveAxes
                    // Three bands by interface profile (uboros: blue lows, amber
                    // mids, near-white highs; stack: red, green, light blue)
                    lowColor: Theme.waveLow
                    midColor: Theme.waveMid
                    highColor: Theme.waveHigh

                    gainAll: Theme.waveGain
                    gainLow: 1.0
                    gainMid: 1.0
                    gainHigh: 1.0
                }

                Mixxx.WaveformRendererStem {
                    gainAll: root.splitStemTracks ? 2.0 : 1.0
                    splitStemTracks: root.splitStemTracks
                }

                Mixxx.WaveformRendererBeat {
                    color: Theme.waveBeat
                }

                Mixxx.WaveformRendererMark {
                    playMarkerColor: Theme.playMarkerColor
                    playMarkerBackground: Theme.playMarkerColor
                    defaultMark: Mixxx.WaveformMark {
                        align: "bottom|right"
                        color: "#00d9ff"
                        textColor: "#1a1a1a"
                        text: " %1 "
                        endIcon: Qt.resolvedUrl("images/jump_%1.svg")
                    }

                    untilMark.showTime: true
                    untilMark.showBeats: true
                    untilMark.align: Qt.AlignBottom
                    untilMark.textSize: 11

                    Mixxx.WaveformMark {
                        control: "cue_point"
                        text: 'CUE'
                        align: 'top|right'
                        color: 'red'
                        textColor: '#1a1a1a'
                    }
                    Mixxx.WaveformMark {
                        control: "loop_start_position"
                        text: '↻'
                        align: 'top|left'
                        color: 'green'
                        textColor: '#FFFFFF'
                    }
                    Mixxx.WaveformMark {
                        control: "loop_end_position"
                        align: 'bottom|right'
                        color: 'green'
                        textColor: '#FFFFFF'
                    }
                    Mixxx.WaveformMark {
                        control: "intro_start_position"
                        text: '◢'
                        align: 'top|right'
                        color: 'blue'
                        textColor: '#FFFFFF'
                    }
                    Mixxx.WaveformMark {
                        control: "intro_end_position"
                        text: '◢'
                        align: 'top|left'
                        color: 'blue'
                        textColor: '#FFFFFF'
                    }
                    Mixxx.WaveformMark {
                        control: "outro_start_position"
                        text: '◣'
                        align: 'top|right'
                        color: 'blue'
                        textColor: '#FFFFFF'
                    }
                    Mixxx.WaveformMark {
                        control: "outro_end_position"
                        text: '◣'
                        align: 'top|left'
                        color: 'blue'
                        textColor: '#FFFFFF'
                    }
                }
            }
        }
    }

    // The overview of the deck (the only one: Deck.qml makes it only when this
    // is not used) and, on Club, its loop badge and beat counter
    Item {
        id: overviewStrip

        // y, not anchors: switching the anchored edge at run time (the
        // profile changed) leaves the height the anchors made
        anchors.left: parent.left
        anchors.right: parent.right
        y: root.overviewAbove ? 0 : root.height - root.overviewHeight
        height: root.overviewHeight
        visible: root.overviewHeight > 0

        Loader {
            anchors.fill: parent
            active: root.overviewHeight > 0

            sourceComponent: Skin.WaveformOverview {
                group: root.group
            }
        }

        Loader {
            anchors.fill: parent
            active: root.overviewHeight > 0 && !root.infoChips

            sourceComponent: Skin.DeckOverviewMarks {
                group: root.group
            }
        }
    }

    Mixxx.ControlProxy {
        id: scratchPositionEnableControl

        group: root.group
        key: "scratch_position_enable"
    }

    Mixxx.ControlProxy {
        id: scratchPositionControl

        group: root.group
        key: "scratch_position"
    }

    Mixxx.ControlProxy {
        id: wheelControl

        group: root.group
        key: "wheel"
    }

    Mixxx.ControlProxy {
        id: rateRatioControl

        group: root.group
        key: "rate_ratio"
    }

    Mixxx.ControlProxy {
        id: zoomControl

        group: root.zoomGroup
        key: "waveform_zoom"

        Component.onCompleted: {
            if (group == root.group) {
                value = root.defaultZoom()
            }
        }
    }

    MouseArea {
        property int mouseStatus: WaveformDisplay.MouseStatus.Normal
        property point mouseAnchor: Qt.point(0, 0)

        anchors.fill: parent
        // Not over the overview strip (it has its own mouse area)
        anchors.topMargin: root.overviewAbove ? root.overviewHeight : 0
        anchors.bottomMargin: root.overviewAbove ? 0 : root.overviewHeight
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onDoubleClicked: {
            if (mouse.button == Qt.RightButton) {
                root.splitStemTracks = !root.splitStemTracks;
            }
        }
        onPressed: {
            mouseAnchor = Qt.point(mouse.x, mouse.y);
            if (mouse.button == Qt.LeftButton) {
                if (mouseStatus == WaveformDisplay.MouseStatus.Bending)
                    wheelControl.parameter = 0.5;

                mouseStatus = WaveformDisplay.MouseStatus.Scratching;
                scratchPositionControl.value = 0;
                scratchPositionEnableControl.value = 1;
            } else {
                if (mouseStatus == WaveformDisplay.MouseStatus.Scratching)
                    scratchPositionEnableControl.value = 0;

                wheelControl.parameter = 0.5;
                mouseStatus = WaveformDisplay.MouseStatus.Bending;
            }
        }
        onPositionChanged: {
            const diff = mouse.x - mouseAnchor.x;
            switch (mouseStatus) {
                case WaveformDisplay.MouseStatus.Bending: {
                    // Start at the middle of [0.0, 1.0], and emit values based on how far
                    // the mouse has traveled horizontally. Note, for legacy (MIDI) reasons,
                    // this is tuned to 127.
                    const v = 0.5 + (diff / root.width);
                    // clamp to [0.0, 1.0]
                    wheelControl.parameter = Math.max(Math.min(v, 1), 0);
                    break;
                };
                case WaveformDisplay.MouseStatus.Scratching:
                // TODO: Calculate position properly
                    scratchPositionControl.value = -diff * zoomControl.value * 200;
                    break;
            }
        }
        onReleased: {
            switch (mouseStatus) {
                case WaveformDisplay.MouseStatus.Bending:
                    wheelControl.parameter = 0.5;
                    break;
                case WaveformDisplay.MouseStatus.Scratching:
                    scratchPositionEnableControl.value = 0;
                    scratchPositionControl.value = 0;
                    break;
            }
            mouseStatus = WaveformDisplay.MouseStatus.Normal;
        }

        onWheel: (mouse) => {
            if (mouse.angleDelta.y < 0 && zoomControl.value > 1) {
                zoomControl.value -= 1;
            } else if (mouse.angleDelta.y > 0 && zoomControl.value < 10.0) {
                zoomControl.value += 1;
            }
        }
    }

    // Stack: the info of the deck over the waveform (no deck row in that view)
    Loader {
        anchors.fill: parent
        anchors.topMargin: root.overviewAbove ? root.overviewHeight : 0
        anchors.bottomMargin: root.overviewAbove ? 0 : root.overviewHeight
        active: root.infoChips && root.overviewHeight > 0

        sourceComponent: Skin.DeckChips {
            group: root.group
        }
    }
}
