import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import "Theme"

ApplicationWindow {
    id: root

    // Phones: draw edge to edge (camera hole side, hidden system bars)
    flags: Qt.platform.os === "android" ? (Qt.Window | Qt.ExpandedClientAreaHint) : Qt.Window
    // Qt 6.9 pads the content by the safe area (84 px band on the camera
    // hole side): use the whole width, the hole is a small dot
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    property alias maximizeLibrary: maximizeLibraryButton.checked
    property alias show4decks: show4DecksButton.checked
    property alias showEffects: showEffectsButton.checked
    property alias showSamplers: showSamplersButton.checked
    property alias showMixer: showMixerButton.checked
    // Phones / small tablets: the desktop layout needs ~1000 px of height.
    readonly property bool compact: height < 700
    readonly property bool isAndroid: Qt.platform.os === "android"
    // Android tablet: the short side of the screen is >= 600 dp (Qt sizes
    // are already in dp). Tablets get the big layout, any orientation, and
    // the 4 decks like a desktop.
    readonly property bool isTablet: isAndroid && Math.min(Screen.width, Screen.height) >= 600
    // Large screens (desktop, 4K, big tablets): everything gets a bit more room
    readonly property bool large: height >= 1200
    // Narrow window (portrait phone): popups must fit the width
    readonly property bool narrow: width < 1000
    // Portrait start screen on phones when no DJ controller is connected.
    // The DJ view (decks, waveforms, library) is hidden meanwhile, so it
    // does not render at all.
    property bool dashboardMode: false
    // The user chose the DJ view without a controller: do not switch back
    property bool dashboardDismissed: false
    // Opened from the toolbar: stays open with a controller connected
    property bool dashboardByUser: false
    readonly property int controllerCount: Mixxx.ControllerManager.knownDevices.length + Mixxx.ControllerManager.unknownDevices.length

    // Phones turn when the DJ view (landscape) and the Dashboard (portrait) swap: the window
    // is resized and laid out several times, which showed a broken frame for 1-2 s. A plain
    // dark cover hides it until the size has been the same for a moment (nothing animated).
    property bool orientationChanging: false
    function beginOrientationChange() {
        if (isAndroid && !isTablet) {
            orientationChanging = true;
            orientationSettle.restart();
        }
    }
    onWidthChanged: if (orientationChanging)
        orientationSettle.restart()
    onHeightChanged: if (orientationChanging)
        orientationSettle.restart()
    Timer {
        id: orientationSettle

        interval: 500

        onTriggered: root.orientationChanging = false
    }

    function enterDjView() {
        beginOrientationChange();
        dashboardMode = false;
        Mixxx.Config.setScreenOrientation(isTablet ? Mixxx.Config.ScreenOrientation.Any : Mixxx.Config.ScreenOrientation.Landscape);
    }
    function enterDashboard() {
        beginOrientationChange();
        dashboardMode = true;
        Mixxx.Config.setScreenOrientation(isTablet ? Mixxx.Config.ScreenOrientation.Any : Mixxx.Config.ScreenOrientation.Portrait);
    }

    onControllerCountChanged: {
        // Controller plugged in while on the dashboard: go to the DJ view
        if (dashboardMode && controllerCount > 0 && !dashboardByUser)
            enterDjView();
        // Controller unplugged: back to the dashboard, unless the user
        // explicitly chose the DJ view without a controller
        else if (isAndroid && !dashboardMode && !dashboardDismissed && controllerCount === 0 && startupCheck.done)
            enterDashboard();
    }

    // Controllers are enumerated (and hot-plugged on Android) shortly after
    // startup: decide only after a short grace period.
    Timer {
        id: startupCheck

        property bool done: false

        interval: 2500
        running: root.isAndroid
        repeat: false

        onTriggered: {
            done = true;
            if (root.controllerCount === 0)
                root.enterDashboard();
        }
    }
    // In compact mode the decks are shown minimized (info bar only) unless
    // the on-screen mixer is requested; a hardware controller does the rest.
    readonly property bool decksMinimized: maximizeLibrary || (compact && !showMixer)
    readonly property int waveformCount: show4decks ? 4 : 2
    // "Waveform view" (compact only): library hidden, the waveforms take all
    // the height left. Toggled by [Skin],show_library (FLX4 SHIFT + BROWSE press or
    // the "Waves" toolbar button).
    readonly property bool waveformView: compact && !showMixer && !maximizeLibrary && showLibraryControl.value === 0
    // Interface profiles Club and Stack ("uboros": always false, the original
    // layout): compact DJ view with the waveforms shown. The overview of each
    // deck moves onto its waveform (WaveformDisplay.qml) and the deck rows
    // change: Club has them without the overview, Stack has none and shows the
    // info of the decks over the waveforms. With the library maximized or the
    // on-screen mixer there are no waveforms, the decks are the original ones.
    readonly property bool profileWaves: compact && !showMixer && !maximizeLibrary && (Theme.isClub || Theme.isStack)
    readonly property int waveOverviewHeight: profileWaves ? Theme.waveOverviewHeight : 0
    readonly property bool deckRowsHidden: profileWaves && Theme.isStack
    readonly property real waveformHeight: {
        if (!compact)
            return large ? Math.min(220, Math.floor(height * 0.12)) : 120;
        // The on-screen mixer needs the whole height on a phone
        if (showMixer)
            return 0;
        if (waveformView) {
            // Everything in the Column except the waveforms themselves
            const others = toolbar.height + (deckRowsHidden ? 0 : decks12.height)
                    + (crossfader.visible ? crossfader.height : 0)
                    + (decks34.visible ? decks34.height : 0)
                    + (samplers.visible ? samplers.height : 0)
                    + (effects.visible ? effects.height : 0);
            return Math.max(32, Math.floor((height - others) / waveformCount));
        }
        // Share ~26% of the window between the visible waveforms, the
        // library needs the rest. Club: the same, plus the overview strip of
        // each waveform (the deck row is 31 dp shorter); Stack: x1.25, up to
        // 150 dp, plus the strip.
        if (profileWaves)
            return Math.max(32, Math.min(Theme.waveHeightMax, Math.floor(height * 0.26 * Theme.waveHeightScale / waveformCount))) + waveOverviewHeight;
        return Math.max(32, Math.min(120, Math.floor(height * 0.26 / waveformCount)));
    }

    // Black while the screen is off: OLED pixels really off
    color: screenAsleep ? "black" : Theme.backgroundColor
    // Do not open bigger than the screen (laptops 1366x768, 1280x800)
    height: Math.min(1080, Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 1080)
    minimumHeight: isAndroid ? 0 : 480
    minimumWidth: isAndroid ? 0 : 640

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
    width: Math.min(1920, Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth : 1920)

    // ------------------------------------------------------------- OLED care
    // Burn-in protection: dimmer whites (Theme) and the whole view moved by
    // up to 2 px every 3 minutes, around a small square. A transform changed
    // once every few minutes costs one scene graph update, nothing per frame.
    readonly property bool burnInProtection: Mixxx.Config.burnInProtection
    readonly property var orbitSteps: [[0, 0], [2, 0], [2, 2], [0, 2], [-2, 2], [-2, 0], [-2, -2], [0, -2], [2, -2]]
    property int orbitIndex: 0
    readonly property int orbitX: burnInProtection ? orbitSteps[orbitIndex][0] : 0
    readonly property int orbitY: burnInProtection ? orbitSteps[orbitIndex][1] : 0
    // Screen off when idle: black and nothing drawn (audio goes on); the
    // DJ controller or a touch wakes it (that touch presses nothing)
    readonly property bool screenAsleep: Mixxx.Config.screenAsleep

    Binding {
        target: Theme
        property: "dimWhites"
        value: root.burnInProtection
    }
    // Interface profile ("uboros", "club", "stack"): palette and waveform
    // colors in Theme follow it at once, no restart
    readonly property string interfaceProfile: Mixxx.Config.interfaceProfile

    Binding {
        target: Theme
        property: "profile"
        value: root.interfaceProfile
    }
    Timer {
        interval: 3 * 60 * 1000
        repeat: true
        running: root.burnInProtection

        onTriggered: root.orbitIndex = (root.orbitIndex + 1) % root.orbitSteps.length
    }
    Timer {
        id: screenOffTimer

        interval: Math.max(1, Mixxx.Config.screenOffMinutes) * 60 * 1000
        repeat: false
        // Performance mode: the screen never goes off for inactivity
        running: Mixxx.Config.screenOffMinutes > 0 && !root.screenAsleep && !(perfModeControl.value > 0)

        onTriggered: Mixxx.Config.screenAsleep = true
    }
    Connections {
        target: Mixxx.Config

        function onUserActivity() {
            Mixxx.Config.screenAsleep = false;
            screenOffTimer.restart();
        }
        function onScreenOffMinutesChanged() {
            screenOffTimer.restart();
        }
    }

    Column {
        id: content
        visible: !root.dashboardMode && !root.screenAsleep
        transform: Translate {
            x: root.orbitX
            y: root.orbitY
        }
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
                    // Two decks on the phone (FLX4): no room for four
                    visible: !root.isAndroid || (root.isTablet && !root.compact)
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
                    // Kept in the program but not shown for now: reserved for later
                    visible: false
                }
                Skin.Button {
                    id: showSamplersButton
                    activeColor: Theme.white
                    checkable: true
                    text: "Sampler"
                    // Kept in the program but not shown for now: reserved for later
                    visible: false
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
                Skin.Button {
                    id: autoMixButton
                    activeColor: "#4ee04e"
                    checkable: true
                    checked: Mixxx.AutoMix.active
                    text: "Auto Mix"

                    onClicked: Mixxx.AutoMix.active = !Mixxx.AutoMix.active
                }
                Skin.Button {
                    id: liveStemsButton
                    // Separate the stems of every track without stems as
                    // soon as it is loaded (not only on SMART CFX)
                    activeColor: "#c77dff"
                    checkable: true
                    checked: liveStemsControl.value > 0
                    text: "Live Stems"

                    onClicked: liveStemsControl.value = liveStemsControl.value > 0 ? 0 : 1

                    Mixxx.ControlProxy {
                        id: liveStemsControl

                        group: "[Stems]"
                        key: "live_stems"
                    }
                }
                Skin.Button {
                    id: perfModeButton
                    // Performance mode: waveforms always at full rate, clocks
                    // kept warm, no background analysis. Always off at start.
                    // No text: a speedometer when off, a flame when on (hand
                    // drawn SVGs, no font/emoji needed). The warm active color
                    // gives the ember-colored button fill, see Button.qml
                    Accessible.description: "Waveforms at full rate, no background analysis"
                    Accessible.name: "Performance mode"
                    ToolTip.delay: 600
                    ToolTip.text: "Performance mode"
                    ToolTip.visible: hovered && !root.isAndroid
                    activeColor: "#ff7a1a"
                    checkable: true
                    checked: perfModeControl.value > 0

                    onClicked: perfModeControl.value = perfModeControl.value > 0 ? 0 : 1

                    Image {
                        // Performance mode off: speedometer
                        anchors.centerIn: parent
                        asynchronous: true
                        height: 18
                        source: "images/perf_gauge.svg"
                        sourceSize: Qt.size(64, 64)
                        visible: !perfModeButton.checked
                        width: 18
                        z: 1
                    }
                    Image {
                        id: perfFlame
                        // Performance mode on: flame. Only scale/opacity are
                        // animated (the window renders every frame in this
                        // mode anyway); the animation stops when the flame is
                        // hidden (mode off, toolbar hidden)
                        anchors.centerIn: parent
                        asynchronous: true
                        height: 18
                        source: "images/perf_flame.svg"
                        sourceSize: Qt.size(64, 64)
                        transformOrigin: Item.Bottom
                        visible: perfModeButton.checked
                        width: 18
                        z: 1

                        SequentialAnimation {
                            loops: Animation.Infinite
                            running: perfFlame.visible

                            ParallelAnimation {
                                NumberAnimation {
                                    duration: 700
                                    easing.type: Easing.InOutSine
                                    from: 0.94
                                    property: "scale"
                                    target: perfFlame
                                    to: 1.08
                                }
                                NumberAnimation {
                                    duration: 700
                                    easing.type: Easing.InOutSine
                                    from: 0.8
                                    property: "opacity"
                                    target: perfFlame
                                    to: 1
                                }
                            }
                            ParallelAnimation {
                                NumberAnimation {
                                    duration: 700
                                    easing.type: Easing.InOutSine
                                    from: 1.08
                                    property: "scale"
                                    target: perfFlame
                                    to: 0.94
                                }
                                NumberAnimation {
                                    duration: 700
                                    easing.type: Easing.InOutSine
                                    from: 1
                                    property: "opacity"
                                    target: perfFlame
                                    to: 0.8
                                }
                            }
                        }
                    }

                    Mixxx.ControlProxy {
                        id: perfModeControl

                        group: "[Skin]"
                        key: "performance_mode"
                    }
                }
                Text {
                    // What Auto Mix is doing: next track, countdown, mixing
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    visible: Mixxx.AutoMix.active
                    text: Mixxx.AutoMix.status
                    color: "#a6e36b"
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textFontPixelSize - 1
                    verticalAlignment: Text.AlignVCenter
                }
                Item {
                    Layout.fillWidth: true
                    visible: !Mixxx.AutoMix.active
                }
                Skin.BeatFxIndicator {
                    id: beatFxIndicator

                    // Shrinks (down to a minimum) when the toolbar is full, so that the
                    // "?" and the gear at the right end never go off screen
                    Layout.fillWidth: true
                    Layout.maximumWidth: implicitWidth
                    Layout.minimumWidth: 120
                    Layout.rightMargin: 6
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
                    id: exportButton
                    activeColor: Theme.white
                    text: "Export"

                    onClicked: exportPopup.open()
                }
                Skin.Button {
                    id: showDashboardButton
                    activeColor: Theme.white
                    text: "Dashboard"
                    visible: root.isAndroid

                    onClicked: {
                        root.dashboardByUser = true;
                        root.enterDashboard();
                    }
                }
                // Help: the guided tour again, always visible
                Skin.Button {
                    id: helpButton
                    activeColor: Theme.white
                    implicitWidth: implicitHeight
                    text: "?"

                    onClicked: tutorial.open()
                }
                Skin.Button {
                    id: showPreferencesButton
                    Layout.rightMargin: 8
                    activeColor: Theme.white
                    // Android: the settings are in the Dashboard (the popup and the desktop dialog do not fit a phone)
                    visible: !root.isAndroid
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
            infoChips: root.profileWaves && Theme.isStack && root.show4decks
            overviewAbove: Theme.isStack
            overviewHeight: root.waveOverviewHeight
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
            infoChips: root.profileWaves && Theme.isStack
            overviewAbove: Theme.isStack
            overviewHeight: root.waveOverviewHeight
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
            infoChips: root.profileWaves && Theme.isStack
            overviewAbove: Theme.isStack
            overviewHeight: root.waveOverviewHeight
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
            infoChips: root.profileWaves && Theme.isStack && root.show4decks
            overviewAbove: Theme.isStack
            overviewHeight: root.waveOverviewHeight
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
            overviewOnWave: root.profileWaves
            rightDeckGroup: "[Channel2]"
            // Stack: the info of the decks is over the waveforms
            visible: !root.deckRowsHidden
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
            overviewOnWave: root.profileWaves
            rightDeckGroup: "[Channel4]"
            visible: root.show4decks && !root.deckRowsHidden
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
    // [Master],maximize_library (a controller, e.g. SHIFT + LOAD 1 of the
    // FLX4) <-> the Library button. The control is saved in the settings but
    // the view always starts normal: at start the button wins. After that each
    // side follows the other; the control's echo is read back from the control
    // so a late echo cannot undo a newer change of the button.
    Mixxx.ControlProxy {
        id: maximizeLibraryControl

        property bool ready: false

        group: "[Master]"
        key: "maximize_library"

        Component.onCompleted: {
            maximizeLibraryControl.ready = true;
            if (maximizeLibraryControl.initialized)
                maximizeLibraryControl.value = maximizeLibraryButton.checked ? 1 : 0;
        }
        onValueChanged: (newValue) => {
            if (!maximizeLibraryControl.ready || !maximizeLibraryControl.initialized)
                return;
            // The control as it is now (a queued echo may be older)
            const on = maximizeLibraryControl.value > 0;
            if (maximizeLibraryButton.checked !== on)
                maximizeLibraryButton.checked = on;
        }
    }
    Connections {
        target: maximizeLibraryButton

        function onCheckedChanged() {
            if (maximizeLibraryControl.ready && maximizeLibraryControl.initialized)
                maximizeLibraryControl.value = maximizeLibraryButton.checked ? 1 : 0;
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
        visible: !root.dashboardMode && !root.screenAsleep && !(root.compact && (root.showMixer || root.waveformView))
        transform: Translate {
            x: root.orbitX
            y: root.orbitY
        }
    }
    // Cover while the phone turns (see beginOrientationChange)
    Rectangle {
        anchors.fill: parent
        color: "#0b0c0f"
        visible: root.orientationChanging
        z: 1000

        Image {
            anchors.centerIn: parent
            fillMode: Image.PreserveAspectFit
            height: Math.min(parent.width, parent.height) * 0.22
            mipmap: true
            source: "images/logo.png"
            width: height
        }
    }
    Skin.PortraitDashboard {
        id: dashboard

        anchors.fill: parent
        openedByUser: root.dashboardByUser
        visible: root.dashboardMode && !root.screenAsleep

        onContinueWithoutController: {
            root.dashboardDismissed = root.controllerCount === 0;
            root.dashboardByUser = false;
            root.enterDjView();
        }
        onShowTutorial: tutorial.open()
    }
    Skin.StemSlowBanner {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 8
        width: Math.min(parent.width - 16, 900)
        z: 100
    }
    Skin.UpdateBanner {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 8
        width: Math.min(parent.width - 16, 900)
        z: 99
    }
    // Tutorial: the interface is prepared per step and put back at the end
    property var tutorialSaved: null
    property string tutorialPendingView: ""
    // Phase 1 (now): the library flag. Phase 2 (200 ms later, after the
    // control proxy echoed it back): the buttons, so its echo cannot undo them.
    function tutorialView(view) {
        if (!tutorialSaved) {
            tutorialSaved = {
                library: showLibraryControl.value,
                mixer: showMixer,
                maximize: maximizeLibrary,
                effects: showEffects,
                samplers: showSamplers
            };
        }
        if (dashboardMode)
            return;
        tutorialPendingView = view;
        const b = tutorialSaved;
        if (view === "library" || view === "mixer")
            showLibraryControl.value = 1;
        else if (view === "waves" || view === "effects")
            showLibraryControl.value = compact ? 0 : 1;
        else
            showLibraryControl.value = b.library;
        tutorialApplyTimer.restart();
    }
    function tutorialApply2() {
        if (!tutorialSaved || dashboardMode)
            return;
        const b = tutorialSaved;
        const view = tutorialPendingView === "restore" ? "none" : tutorialPendingView;
        if (tutorialPendingView === "restore")
            tutorialSaved = null;
        maximizeLibrary = (view === "none") ? b.maximize : false;
        showMixer = (view === "mixer" && compact) ? true : ((view === "none") ? b.mixer : false);
        showEffects = (view === "effects") ? true : b.effects;
        showSamplers = b.samplers;
    }
    Timer {
        id: tutorialApplyTimer

        interval: 200
        onTriggered: root.tutorialApply2()
    }
    function tutorialRestore() {
        if (!tutorialSaved)
            return;
        if (dashboardMode) {
            tutorialSaved = null;
            return;
        }
        tutorialPendingView = "restore";
        showLibraryControl.value = tutorialSaved.library;
        tutorialApplyTimer.restart();
    }
    Skin.Tutorial {
        id: tutorial

        onPrepareView: view => root.tutorialView(view)
        onRestoreView: root.tutorialRestore()
        targets: ({
                toolbar: toolbar,
                show4DecksButton: show4DecksButton,
                maximizeLibraryButton: maximizeLibraryButton,
                showMixerButton: showMixerButton,
                waveformViewButton: waveformViewButton,
                autoMixButton: autoMixButton,
                liveStemsButton: liveStemsButton,
                beatFxIndicator: beatFxIndicator,
                perfModeButton: perfModeButton,
                showDashboardButton: showDashboardButton,
                helpButton: helpButton,
                showPreferencesButton: showPreferencesButton,
                deck1waveform: deck1waveform,
                decks12: decks12,
                crossfader: crossfader,
                library: library
            })
    }
    // First run: after the startup check, once, unless it was skipped/finished.
    // Only in the DJ view (the steps point at the decks and the toolbar): in
    // the Dashboard (no controller) it waits and starts when the DJ view opens
    Timer {
        interval: 3500
        running: !Mixxx.Config.tutorialDone && !root.dashboardMode
        repeat: false
        onTriggered: if (!Mixxx.Config.tutorialDone && !root.dashboardMode && !tutorial.opened) tutorial.open()
    }
    Skin.ExportPopup {
        id: exportPopup
        width: Math.min(parent.width - 16, 520)
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
    }
    Skin.Settings {
        id: settingsPopup
        height: (root.compact || root.narrow) ? parent.height - 8 : Math.max(840, parent.height * 0.7)
        modal: true
        width: (root.compact || root.narrow) ? parent.width - 16 : Math.max(1400, parent.width * 0.8)
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
