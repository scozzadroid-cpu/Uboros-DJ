import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import "Theme"

ApplicationWindow {
    id: root

    property alias maximizeLibrary: maximizeLibraryButton.checked
    property alias show4decks: show4DecksButton.checked
    property alias showEffects: showEffectsButton.checked
    property alias showSamplers: showSamplersButton.checked
    property alias showMixer: showMixerButton.checked
    // Phones / small tablets: the desktop layout needs ~1000 px of height.
    readonly property bool compact: height < 700
    readonly property bool isAndroid: Qt.platform.os === "android"
    // In compact mode the decks are shown minimized (info bar only) unless
    // the on-screen mixer is requested; a hardware controller does the rest.
    readonly property bool decksMinimized: maximizeLibrary || (compact && !showMixer)
    readonly property int waveformCount: show4decks ? 4 : 2
    // "Waveform view" (compact only): library hidden, the waveforms take all
    // the height left. Toggled by [Skin],show_library (FLX4 SHIFT + BROWSE press or
    // the "Waves" toolbar button).
    readonly property bool waveformView: compact && !showMixer && !maximizeLibrary && showLibraryControl.value === 0
    readonly property real waveformHeight: {
        if (!compact)
            return 120;
        // The on-screen mixer needs the whole height on a phone
        if (showMixer)
            return 0;
        if (waveformView) {
            // Everything in the Column except the waveforms themselves
            const others = toolbar.height + decks12.height
                    + (crossfader.visible ? crossfader.height : 0)
                    + (decks34.visible ? decks34.height : 0)
                    + (samplers.visible ? samplers.height : 0)
                    + (effects.visible ? effects.height : 0);
            return Math.max(32, Math.floor((height - others) / waveformCount));
        }
        // Share ~26% of the window between the visible waveforms, the
        // library needs the rest.
        return Math.max(32, Math.min(120, Math.floor(height * 0.26 / waveformCount)));
    }

    color: Theme.backgroundColor
    height: 1080

    Mixxx.ControlProxy {
        id: showLibraryControl

        group: "[Skin]"
        key: "show_library"

        // Library and Mixer buttons switch the waveform view off
        onValueChanged: (value) => {
            if (value === 0 && root.compact) {
                root.showMixer = false;
                root.maximizeLibrary = false;
            }
        }
    }
    visible: true
    width: 1920

    Column {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        move: Transition {
            NumberAnimation {
                duration: 150
                properties: "x,y"
            }
        }

        Rectangle {
            id: toolbar
            color: Theme.toolbarBackgroundColor
            height: root.compact ? 40 : 36
            radius: 1
            width: parent.width

            RowLayout {
                anchors.fill: parent

                Skin.Button {
                    id: show4DecksButton
                    activeColor: Theme.white
                    checkable: true
                    text: "4 Decks"
                }
                Skin.Button {
                    id: maximizeLibraryButton
                    activeColor: Theme.white
                    checkable: true
                    text: "Library"

                    // In compact mode Library and Mixer share the same space
                    onCheckedChanged: {
                        if (checked && root.compact) {
                            root.showMixer = false;
                            showLibraryControl.value = 1;
                        }
                    }
                }
                Skin.Button {
                    id: showEffectsButton
                    activeColor: Theme.white
                    checkable: true
                    text: "Effects"
                }
                Skin.Button {
                    id: showSamplersButton
                    activeColor: Theme.white
                    checkable: true
                    text: "Sampler"
                }
                Skin.Button {
                    id: showMixerButton
                    activeColor: Theme.white
                    checkable: true
                    text: "Mixer"
                    visible: root.compact

                    onCheckedChanged: {
                        if (checked) {
                            root.maximizeLibrary = false;
                            showLibraryControl.value = 1;
                        }
                    }
                }
                Skin.Button {
                    id: waveformViewButton
                    activeColor: Theme.white
                    checked: root.waveformView
                    text: "Waves"
                    visible: root.compact

                    onClicked: showLibraryControl.value = showLibraryControl.value === 0 ? 1 : 0
                }
                Item {
                    Layout.fillWidth: true
                }
                Skin.Button {
                    id: showDevToolsButton
                    activeColor: Theme.white
                    checkable: true
                    checked: devToolsWindow.visible
                    text: "Develop"
                    visible: !root.isAndroid

                    onClicked: {
                        if (devToolsWindow.visible)
                            devToolsWindow.close();
                        else
                            devToolsWindow.show();
                    }

                    DeveloperToolsWindow {
                        id: devToolsWindow
                        height: 480
                        width: 640
                    }
                }
                Skin.Button {
                    id: showPreferencesButton
                    activeColor: Theme.white
                    checked: settingsPopup.opened
                    icon.height: 16
                    icon.source: "images/gear.svg"
                    icon.width: 16
                    implicitWidth: implicitHeight

                    onClicked: {
                        if (!settingsPopup.opened) {
                            settingsPopup.open();
                        }
                    }
                    onPressAndHold: {
                        Mixxx.PreferencesDialog.show();
                    }
                }
            }
        }
        Skin.WaveformDisplay {
            id: deck3waveform
            group: "[Channel3]"
            height: root.waveformHeight
            visible: root.show4decks && !root.maximizeLibrary && root.waveformHeight > 0
            width: root.width

            FadeBehavior on visible  {
                fadeTarget: deck3waveform
            }
        }
        Skin.WaveformDisplay {
            id: deck1waveform
            group: "[Channel1]"
            height: root.waveformHeight
            visible: !root.maximizeLibrary && root.waveformHeight > 0
            width: root.width

            FadeBehavior on visible  {
                fadeTarget: deck1waveform
            }
        }
        Skin.WaveformDisplay {
            id: deck2waveform
            group: "[Channel2]"
            height: root.waveformHeight
            visible: !root.maximizeLibrary && root.waveformHeight > 0
            width: root.width

            FadeBehavior on visible  {
                fadeTarget: deck2waveform
            }
        }
        Skin.WaveformDisplay {
            id: deck4waveform
            group: "[Channel4]"
            height: root.waveformHeight
            visible: root.show4decks && !root.maximizeLibrary && root.waveformHeight > 0
            width: root.width

            FadeBehavior on visible  {
                fadeTarget: deck4waveform
            }
        }
        Skin.DeckRow {
            id: decks12
            leftDeckGroup: "[Channel1]"
            minimized: root.decksMinimized
            rightDeckGroup: "[Channel2]"
            width: parent.width
        }
        Skin.CrossfaderRow {
            id: crossfader
            crossfaderWidth: decks12.mixer.width
            visible: !root.decksMinimized
            width: parent.width

            Skin.FadeBehavior on visible  {
                fadeTarget: crossfader
            }
        }
        Skin.DeckRow {
            id: decks34
            leftDeckGroup: "[Channel3]"
            minimized: root.decksMinimized || root.compact
            rightDeckGroup: "[Channel4]"
            visible: root.show4decks
            width: parent.width

            Skin.FadeBehavior on visible  {
                fadeTarget: decks34
            }
        }
        Skin.SamplerRow {
            id: samplers
            visible: root.showSamplers
            width: parent.width

            Skin.FadeBehavior on visible  {
                fadeTarget: samplers
            }
        }
        Skin.EffectRow {
            id: effects
            visible: root.showEffects
            width: parent.width

            Skin.FadeBehavior on visible  {
                fadeTarget: effects
            }
        }
    }
    // The library fills the space below the Column. It must not be a child
    // of the Column: its height would depend on the y the Column assigns,
    // and with the animated "move" transition that y got stuck off screen.
    Skin.Library {
        id: library

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: content.bottom
        compact: root.compact
        visible: !(root.compact && (root.showMixer || root.waveformView))
    }
    Skin.Settings {
        id: settingsPopup
        height: root.compact ? parent.height - 8 : Math.max(840, parent.height * 0.7)
        modal: true
        width: root.compact ? parent.width - 16 : Math.max(1400, parent.width * 0.8)
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)

        Overlay.modal: Rectangle {
            id: overlayModal
            property real radius: 12

            readonly property bool hasHardwareAcceleration: Mixxx.Config.useAcceleration()

            anchors.fill: parent
            color: Qt.alpha('#00000010', hasHardwareAcceleration ? 1.0 : 0.6)

            Repeater {
                model: hasHardwareAcceleration ? 1 : 0
                GaussianBlur {
                    anchors.fill: overlayModal
                    deviation: 4
                    radius: Math.max(0, overlayModal.radius)
                    samples: 16
                    source: content
                }
            }
        }
    }
}
