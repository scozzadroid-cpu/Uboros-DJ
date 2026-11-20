import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import Qt5Compat.GraphicalEffects
import "Theme"

// Start screen over the interface while the app starts: what it is waiting
// for (interface, audio engine, DJ controller) with a progress bar, and the
// work that goes on in the background (USB drive, analysis, stems).
//
// It stays up until everything the app loads at the start is loaded: the
// interface, the audio engine, the search for the controller and the library
// (the files of the tracks are checked and every USB drive that is plugged in
// is read and prepared: doing that with the interface in use made it stutter).
// It goes away with a short fade, not before minVisibleMs (no flash), at once
// when a deck starts playing (the hardware controller works under it all the
// time) and at the latest after maxVisibleMs; after skipAfterMs a tap closes it.
Item {
    id: root

    // Set by main.qml: DJ controllers found, and whether the grace period in
    // which they are enumerated is over
    property int controllerCount: 0
    property bool controllerSearchDone: true
    property int minVisibleMs: 700
    // A drive with thousands of tracks takes a while: this is only the net under
    // a start that never finishes
    property int maxVisibleMs: 120000
    // From then on a tap closes the screen (before that a stray tap must not)
    property int skipAfterMs: 20000

    // The interface under the splash exists and has drawn some frames
    property bool completed: false
    property int frames: 0
    readonly property bool interfaceReady: completed && (frames >= 3 || elapsedMs >= 1500)
    // [App],samplerate is set when the audio device is open
    readonly property bool audioReady: sampleRateControl.value > 0
    readonly property bool controllerReady: controllerCount > 0 || controllerSearchDone
    // The files of the library are checked and the drives are read and prepared
    readonly property bool libraryReady: !Mixxx.Analysis.startupBusy
    readonly property string driveText: Mixxx.Analysis.deviceBusy
    // How far the library is: "Preparing USB drive… 34/156" gives the fraction
    readonly property real libraryFraction: {
        if (libraryReady)
            return 1.0;
        const m = /(\d+)\/(\d+)\s*$/.exec(driveText);
        if (m && parseInt(m[2]) > 0)
            return 0.3 + 0.65 * Math.min(1, parseInt(m[1]) / parseInt(m[2]));
        return driveText !== "" ? 0.15 : 0.05;
    }
    // A deck plays (started from the controller): the screen must not hide the decks
    property bool anyPlaying: false
    property bool minElapsed: false
    property bool dismissed: false
    property real elapsedMs: 0

    readonly property real progress: dismissed ? 1.0 : Math.min(1.0, 0.05
            + (completed ? 0.05 : 0)
            + (interfaceReady ? 0.15 : 0)
            + (audioReady ? 0.12 : 0)
            + (controllerReady ? 0.13 : 0)
            + 0.5 * libraryFraction)
    readonly property string phaseText: {
        if (!interfaceReady)
            return qsTr("Loading the interface…");
        if (!audioReady)
            return qsTr("Starting the audio engine…");
        if (!controllerReady)
            return qsTr("Looking for your DJ controller…");
        if (!libraryReady)
            return driveText !== "" ? driveText : qsTr("Checking the library and the USB drives…");
        return controllerCount > 0 ? qsTr("DJ controller connected") : qsTr("Ready");
    }
    readonly property string version: Mixxx.AppUpdater.installedVersion

    function dismiss() {
        dismissed = true;
    }

    anchors.fill: parent
    opacity: dismissed ? 0 : 1
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation {
            duration: 350
            easing.type: Easing.OutQuad
        }
    }

    Component.onCompleted: {
        completed = true;
        startedAt = Date.now();
        tick.start();
    }
    onInterfaceReadyChanged: root.closeIfReady()
    onAudioReadyChanged: root.closeIfReady()
    onControllerReadyChanged: root.closeIfReady()
    onLibraryReadyChanged: root.closeIfReady()
    onAnyPlayingChanged: {
        if (anyPlaying)
            root.dismiss();
    }
    onMinElapsedChanged: root.closeIfReady()

    property real startedAt: 0

    function closeIfReady() {
        if (interfaceReady && audioReady && controllerReady && libraryReady && minElapsed)
            dismiss();
    }

    Mixxx.ControlProxy {
        id: sampleRateControl

        group: "[App]"
        key: "samplerate"
    }
    // The decks: playing one of them closes the screen
    Mixxx.ControlProxy {
        id: numDecksControl

        group: "[App]"
        key: "num_decks"
    }
    Repeater {
        id: playProxies

        model: Math.max(2, Math.min(4, Math.round(numDecksControl.value)))

        delegate: Item {
            id: deckProxy

            required property int index

            visible: false

            Mixxx.ControlProxy {
                id: playControl

                group: "[Channel" + (deckProxy.index + 1) + "]"
                key: "play"

                onValueChanged: {
                    let playing = false;
                    for (let i = 0; i < playProxies.count; i++) {
                        const item = playProxies.itemAt(i);
                        if (item && item.playing)
                            playing = true;
                    }
                    root.anyPlaying = playing;
                }
            }
            readonly property bool playing: playControl.value > 0
        }
    }
    // Frames drawn by the window under the splash (null without a window: the
    // 1.5 s fallback of interfaceReady applies)
    Connections {
        function onFrameSwapped() {
            if (root.frames < 3)
                root.frames++;
        }

        ignoreUnknownSignals: true
        target: root.Window.window
    }
    Timer {
        id: tick

        interval: 100
        repeat: true

        onTriggered: {
            root.elapsedMs = Date.now() - root.startedAt;
            if (root.elapsedMs >= root.minVisibleMs)
                root.minElapsed = true;
            if (root.elapsedMs >= root.maxVisibleMs)
                root.dismiss();
            if (root.dismissed)
                stop();
        }
    }

    // The same dark cover as the one used while the phone turns
    Rectangle {
        anchors.fill: parent
        color: "#0b0c0f"
    }
    Column {
        anchors.centerIn: parent
        spacing: Math.round(root.height * 0.025)
        width: Math.min(root.width - 48, 420)

        // The logo on a soft glow of the accent color; it breathes slowly
        Item {
            id: logoBox

            anchors.horizontalCenter: parent.horizontalCenter
            height: Math.min(root.width, root.height) * 0.22
            width: height

            RadialGradient {
                anchors.centerIn: parent
                height: parent.height * 2.6
                width: height

                gradient: Gradient {
                    GradientStop {
                        color: Qt.rgba(Theme.accentColor.r, Theme.accentColor.g, Theme.accentColor.b, 0.22)
                        position: 0.0
                    }
                    GradientStop {
                        color: Qt.rgba(Theme.accentColor.r, Theme.accentColor.g, Theme.accentColor.b, 0.07)
                        position: 0.45
                    }
                    GradientStop {
                        color: "transparent"
                        position: 1.0
                    }
                }
            }
            Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                mipmap: true
                source: "images/logo.png"

                SequentialAnimation on scale {
                    loops: Animation.Infinite
                    running: root.visible

                    NumberAnimation {
                        duration: 1800
                        easing.type: Easing.InOutSine
                        from: 1.0
                        to: 1.04
                    }
                    NumberAnimation {
                        duration: 1800
                        easing.type: Easing.InOutSine
                        from: 1.04
                        to: 1.0
                    }
                }
            }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.white
            font.bold: true
            font.family: Theme.fontFamily
            font.letterSpacing: 1
            font.pixelSize: 30
            text: "Ubøros DJ"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.textMutedColor
            font.family: Theme.fontFamily
            font.pixelSize: 12
            text: root.version !== "" ? "v" + root.version : ""
            visible: text !== ""
        }
        // Progress
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#23262c"
            height: 4
            radius: 2
            width: parent.width

            Rectangle {
                height: parent.height
                radius: parent.radius
                width: parent.width * root.progress

                gradient: Gradient {
                    orientation: Gradient.Horizontal

                    GradientStop {
                        color: Qt.darker(Theme.accentColor, 1.6)
                        position: 0.0
                    }
                    GradientStop {
                        color: Theme.accentColor
                        position: 1.0
                    }
                }

                Behavior on width {
                    NumberAnimation {
                        duration: 250
                        easing.type: Easing.OutQuad
                    }
                }
            }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.white
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize
            text: root.phaseText
        }
        // What is done and what is waited for
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 18

            Repeater {
                model: [
                    {
                        "label": qsTr("Interface"),
                        "done": root.interfaceReady
                    },
                    {
                        "label": qsTr("Audio"),
                        "done": root.audioReady
                    },
                    {
                        "label": root.controllerCount > 0 || !root.controllerReady ? qsTr("Controller") : qsTr("No controller"),
                        "done": root.controllerReady
                    },
                    {
                        "label": qsTr("Library"),
                        "done": root.libraryReady
                    }
                ]

                Row {
                    id: chip

                    required property var modelData

                    spacing: 6

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        color: chip.modelData.done ? Theme.green : "#3a3f47"
                        height: 8
                        radius: 4
                        width: 8

                        Behavior on color {
                            ColorAnimation {
                                duration: 250
                            }
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: chip.modelData.done ? Theme.white : Theme.textMutedColor
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        text: chip.modelData.label
                    }
                }
            }
        }
        // USB drive, analysis, stems: goes on after the splash (toolbar)
        Skin.BackgroundStatus {
            anchors.horizontalCenter: parent.horizontalCenter
            centered: true
            pixelSize: 12
            // The drive has its own line above
            showUsb: false
            width: parent.width
        }
    }
    // After skipAfterMs a tap closes the screen; before that it only takes the
    // touch (nothing under the splash must get it)
    MouseArea {
        anchors.fill: parent

        onClicked: {
            if (root.elapsedMs >= root.skipAfterMs)
                root.dismiss();
        }
    }
    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        anchors.horizontalCenter: parent.horizontalCenter
        color: Theme.textMutedColor
        font.family: Theme.fontFamily
        font.pixelSize: 12
        text: qsTr("Tap to skip")
        visible: root.elapsedMs >= root.skipAfterMs
    }
}
