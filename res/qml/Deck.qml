import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

Item {
    id: root

    required property string group
    property bool minimized: false
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
        rightColumnWidth: rateSlider.width
    }

    // Minimized (e.g. on a phone): a thin overview of the whole track under
    // the info bar, with the play position, the cues and the loops, like on
    // a CDJ. The full deck has its own overview below.
    Rectangle {
        id: miniOverview

        visible: root.minimized
        anchors.top: infoBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        anchors.topMargin: 3
        height: 28
        radius: 4
        color: Theme.deckBackgroundColor

        Skin.WaveformOverview {
            anchors.fill: parent
            group: root.group
        }

        // Deck state badges, like the CDJ display: MASTER (sync leader),
        // SYNC, KEY LOCK and the active loop with its length in beats
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
                visible: leaderControl.value > 0
                text: "MASTER"
                tint: "#ff9a3c"
            }

            Badge {
                visible: syncControl.value > 0
                text: "SYNC"
                tint: "#4ec3ff"
            }

            Badge {
                visible: keylockControl.value > 0
                text: "KEY LOCK"
                tint: "#c38bff"
            }

            Badge {
                visible: loopControl.value > 0
                text: "LOOP " + (loopSizeControl.value >= 1 ? Math.round(loopSizeControl.value) : "1/" + Math.round(1 / loopSizeControl.value))
                tint: "#4ee04e"
            }
        }

        Mixxx.ControlProxy {
            id: leaderControl

            group: root.group
            key: "sync_leader"
        }

        Mixxx.ControlProxy {
            id: syncControl

            group: root.group
            key: "sync_enabled"
        }

        Mixxx.ControlProxy {
            id: keylockControl

            group: root.group
            key: "keylock"
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
