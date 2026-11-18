import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme"

// First-run guided tutorial. A full-window overlay dims the whole interface
// except the element the current step points at (spotlight with a softly
// pulsing accent border); a card with title and text sits on the side with
// the most room. Touches on the rest of the interface are blocked meanwhile.
// Any way of closing (Skip, Done, Back/Esc) marks the tutorial as seen.
// Settings > Maintenance can show it again.
//
// API kept from the old popup: open(), close(), opened, Mixxx.Config.tutorialDone.
// `targets` is a map name -> Item supplied by main.qml. If the target of a step
// is missing, hidden or empty, the card is shown in the centre without spotlight.
// prepareView(view) / restoreView() let main.qml set up the interface (e.g.
// show the library or the effects) and put it back when the tutorial ends.
Item {
    id: root

    property int step: 0
    property var targets: ({})
    property bool opened: false
    // "it" or "en"
    // Always starts in English (the IT/EN switch on the card changes it)
    property string lang: "en"
    readonly property bool it: lang === "it"
    // Camera hole on the left of the phone: 84 px, in dp
    readonly property real safeLeft: Math.ceil(84 / Screen.devicePixelRatio) + 4
    property rect spot: Qt.rect(0, 0, 0, 0)
    property bool hasSpot: false

    signal prepareView(string view)
    signal restoreView

    function tr(itText, enText) {
        return it ? itText : enText;
    }

    // Name of the first connected controller (and its mapping), lower case
    readonly property var devices: (Mixxx.ControllerManager.knownDevices || []).concat(Mixxx.ControllerManager.unknownDevices || [])
    readonly property string deviceName: devices.length > 0 ? devices[0].name : ""
    readonly property string deviceKey: devices.length > 0 ? (devices[0].name + " " + (devices[0].activeMappingName || "")).toLowerCase() : ""
    readonly property string brand: {
        const k = deviceKey;
        if (k === "")
            return "";
        if (k.indexOf("flx4") >= 0)
            return "flx4";
        if (k.indexOf("rane") >= 0)
            return "rane";
        if (k.indexOf("pioneer") >= 0 || k.indexOf("alphatheta") >= 0 || k.indexOf("ddj") >= 0)
            return "pioneer";
        if (k.indexOf("hercules") >= 0)
            return "hercules";
        if (k.indexOf("numark") >= 0)
            return "numark";
        if (k.indexOf("reloop") >= 0)
            return "reloop";
        if (k.indexOf("denon") >= 0)
            return "denon";
        if (k.indexOf("roland") >= 0)
            return "roland";
        if (k.indexOf("traktor") >= 0 || k.indexOf("native instruments") >= 0)
            return "ni";
        return "other";
    }
    readonly property string who: deviceName !== "" ? deviceName : tr("il tuo controller", "your controller")
    // The step about the controller itself changes with the brand
    readonly property var controllerStep: {
        switch (brand) {
        case "flx4":
            return {
                title: tr("La tua DDJ-FLX4", "Your DDJ-FLX4"),
                text: tr("BROWSE scorre la libreria, LOAD carica il brano sul deck. BEAT SYNC attiva il sync (SHIFT + BEAT SYNC: key lock). BEAT FX suona un effetto alla volta, SMART CFX comanda gli stem, SMART FADER attiva il crossfader.", "BROWSE scrolls the library, LOAD puts the track on the deck. BEAT SYNC turns sync on (SHIFT + BEAT SYNC is key lock). BEAT FX plays one effect at a time, SMART CFX drives the stems, SMART FADER switches the crossfader on.")
            };
        case "pioneer":
            return {
                title: tr("Il tuo %1", "Your %1").arg(who),
                text: tr("Jog, play, cue, sync, pad, EQ, filtro e BEAT FX seguono il mapping Mixxx per questo modello. BROWSE scorre la libreria, LOAD carica sul deck. Testato finora sulla DDJ-FLX4: altri modelli possono servire ritocchi.", "Jog wheels, play, cue, sync, pads, EQ, filter and BEAT FX follow the Mixxx mapping for this model. BROWSE scrolls the library and LOAD puts the track on a deck. Tested so far on the DDJ-FLX4: other models may need small adjustments.")
            };
        case "rane":
            return {
                title: tr("Il tuo Rane Four", "Your Rane Four"),
                text: tr("Il mixer funziona: fader, EQ, filtro, gain, cuffie, crossfader. Per deck: play, cue, sync, pitch, nudge, key lock, loop e beat jump. Caricamento brani e jog non sono ancora mappati: carica dallo schermo.", "The mixer works: faders, EQ, filter, gain, headphones, crossfader. Per deck: play, cue, sync, pitch, nudge, key lock, loops and beat jump. Loading tracks and jog wheels are not mapped yet: load from the screen.")
            };
        case "hercules":
            return {
                title: tr("Il tuo %1", "Your %1").arg(who),
                text: tr("Mixer, play, cue, sync, loop, hot cue ed effetti seguono il mapping Mixxx; il jog scratcha al tocco. Scegli il brano con i comandi browse e caricalo con LOAD, oppure usa lo schermo.", "Mixer, play, cue, sync, loops, hot cues and effects follow the Mixxx mapping; the jog wheel scratches when touched. Choose a track with the browse controls and load it with LOAD, or use the screen.")
            };
        case "numark":
            return {
                title: tr("Il tuo %1", "Your %1").arg(who),
                text: tr("Trasporto, sync, pitch, loop, EQ ed effetti seguono il mapping Mixxx; il jog scratcha in modalità vinile. Scegli con la manopola browse, carica con LOAD. Molti Numark sono testati solo in parte.", "Transport, sync, pitch, loops, EQ and effects follow the Mixxx mapping; the jog wheel scratches in vinyl mode. Choose with the browse knob and load with LOAD. Many Numark units are only partly tested.")
            };
        case "reloop":
            return {
                title: tr("Il tuo %1", "Your %1").arg(who),
                text: tr("Il mapping Mixxx copre mixer, trasporto, loop, hot cue ed effetti. Naviga e carica come sull'unità, o usa lo schermo per quel che non è ancora mappato.", "The Mixxx mapping covers the mixer, transport, loops, hot cues and effects. Browse and load as on the unit, or use the screen for anything not mapped yet.")
            };
        case "denon":
            return {
                title: tr("Il tuo %1", "Your %1").arg(who),
                text: tr("Mixer, trasporto, pad, loop ed effetti seguono il mapping Mixxx. Le unità Denon grandi mandano anche l'audio via USB: se non senti nulla controlla l'uscita in Impostazioni > Audio.", "Mixer, transport, pads, loops and effects follow the Mixxx mapping. Larger Denon units also send audio over USB: if you hear nothing check the output in Settings > Sound.")
            };
        case "roland":
            return {
                title: tr("Il tuo %1", "Your %1").arg(who),
                text: tr("Il mapping richiede l'unità in modalità MIDI e manda molti messaggi per luci e display: se una luce è sbagliata, scollega e ricollega.", "The mapping needs the unit in its MIDI mode and sends many messages for lights and screens: if a light looks wrong, unplug and plug it back in.")
            };
        case "ni":
            return {
                title: tr("Il tuo Traktor Kontrol", "Your Traktor Kontrol"),
                text: tr("I Traktor che usano HID o USB bulk non funzionano ancora tramite il MIDI di Android. Se non risponde, usa un altro controller o lo schermo.", "Traktor controllers that talk HID or bulk USB can't be used through Android's MIDI interface yet. If it doesn't respond, use another controller or the screen.")
            };
        default:
            return {
                title: deviceName !== "" ? tr("Il tuo %1", "Your %1").arg(who) : tr("Collega il controller", "Plug in your controller"),
                text: deviceName !== "" ? tr("È collegato ma non c'è ancora un mapping: sceglilo in Impostazioni > Controller o mixa dallo schermo.", "It is connected, but there is no mapping for it yet: pick one in Settings > Controllers or mix with the screen.") : tr("Collega il controller alla porta USB con un cavo OTG (un hub se il telefono non lo alimenta). Viene riconosciuto da solo, niente da installare, niente root. La DDJ-FLX4 è il riferimento testato.", "Connect the controller to the USB port with an OTG cable (a hub if the phone can't power it). It is set up by itself: nothing to install, no root. The DDJ-FLX4 is the tested reference.")
            };
        }
    }
    readonly property bool flx: brand === "flx4"
    // targets: names tried in order, the first usable one is highlighted
    readonly property var steps: [
        {
            view: "none",
            targets: [],
            title: tr("Benvenuto in Ubøros DJ", "Welcome to Ubøros DJ"),
            text: tr("Software DJ libero e open source: niente account, abbonamenti o pubblicità. Un breve giro dell'interfaccia; puoi saltarlo e rivederlo in Impostazioni.", "Free and open source DJ software: no account, no subscription, no ads. A short tour of the interface; you can skip it and see it again in Settings.")
        },
        {
            view: "none",
            targets: [],
            title: controllerStep.title,
            text: controllerStep.text
        },
        {
            view: "library",
            targets: ["library", "maximizeLibraryButton"],
            title: tr("Libreria: carica un brano", "Library: load a track"),
            text: flx ? tr("Gira BROWSE per scorrere la libreria e premi LOAD sul deck che vuoi. Su telefono la libreria è nascosta per lasciare spazio alle waveform: il tasto Library la mostra.", "Turn BROWSE to scroll the library and press LOAD on the deck you want. On a phone the library is hidden to leave room for the waveforms: the Library button shows it.") : tr("Tocca un brano e caricalo su un deck, oppure usa browse e LOAD del controller. Su telefono il tasto Library mostra o nasconde la libreria.", "Tap a track and load it on a deck, or use the controller's browse and LOAD. On a phone the Library button shows or hides the library.")
        },
        {
            view: "waves",
            targets: ["deck1waveform", "waveformViewButton"],
            title: tr("Waveform e zoom", "Waveforms and zoom"),
            text: tr("Qui scorre il brano, con la griglia dei battiti. Zoom con la rotella del mouse o con il comando zoom del controller. Il tasto Waves nasconde la libreria e ingrandisce le waveform.", "The track scrolls here, with the beat grid. Zoom with the mouse wheel or the controller's zoom control. The Waves button hides the library and enlarges the waveforms.")
        },
        {
            view: "mixer",
            targets: ["decks12", "showMixerButton"],
            title: tr("Deck: play, cue, sync", "Deck: play, cue, sync"),
            text: tr("Play avvia, Cue segna il punto di partenza, Sync allinea il tempo all'altro deck. Sul telefono il tasto Mixer mostra i comandi completi sullo schermo.", "Play starts, Cue marks the start point, Sync matches the tempo to the other deck. On a phone the Mixer button shows the full on-screen controls.")
        },
        {
            view: "mixer",
            targets: ["crossfader", "showMixerButton"],
            title: tr("Mixer", "Mixer"),
            text: tr("Crossfader per passare da un deck all'altro, EQ per alti, medi e bassi, filtro per tagliare o aprire il suono. Tutto anche sul controller.", "Crossfader to move between decks, EQ for highs, mids and lows, filter to cut or open the sound. All of it also on the controller.")
        },
        {
            view: "none",
            targets: ["beatFxIndicator"],
            title: tr("Effetti", "Effects"),
            text: flx ? tr("Qui vedi l'effetto Beat FX attivo. Sulla FLX4 BEAT FX suona un effetto alla volta a tempo con il brano: scegli l'effetto e regola il livello dal controller.", "Here you see the active Beat FX effect. On the FLX4 BEAT FX plays one effect at a time in time with the track: pick the effect and set the level from the controller.") : tr("Qui vedi l'effetto Beat FX attivo, a tempo con il brano. Scegli e regola gli effetti dal controller.", "Here you see the active Beat FX effect, in time with the track. Pick and adjust effects from the controller.")
        },
        {
            view: "mixer",
            targets: ["decks12", "showMixerButton"],
            title: tr("Hot cue e loop", "Hot cues and loops"),
            text: tr("Gli hot cue salvano punti del brano da richiamare con un tocco. Il loop ripete una porzione a tempo. Sul controller usa i pad prestazione.", "Hot cues save points of the track to jump to with one tap. A loop repeats a section in time. On the controller use the performance pads.")
        },
        {
            view: "none",
            targets: ["liveStemsSwitch", "liveStemsButton"],
            title: tr("Stem", "Stems"),
            text: flx ? tr("Separa voce, batteria, basso e resto direttamente sul telefono, senza internet. Sulla FLX4 comanda gli stem con SMART CFX; accendi prima l'interruttore Live Stems. Il primo brano richiede un po' di tempo, poi il risultato resta in cache.", "Split vocals, drums, bass and other right on the phone, no internet. On the FLX4 SMART CFX drives the stems; turn the Live Stems switch on first. The first track takes a little while, then the result is cached.") : tr("Separa voce, batteria, basso e resto direttamente sul telefono, senza internet. Si comandano dallo schermo. Accendi l'interruttore Live Stems. Il primo brano richiede un po' di tempo, poi il risultato resta in cache.", "Split vocals, drums, bass and other right on the phone, no internet. They are controlled from the screen. Turn the Live Stems switch on. The first track takes a little while, then the result is cached.")
        },
        {
            view: "none",
            targets: ["showDashboardButton"],
            title: tr("Chiavetta rekordbox", "Rekordbox USB stick"),
            text: tr("Collega una chiavetta preparata con rekordbox e usa Import USB drive nella schermata iniziale: brani, griglie, cue e crate compaiono in libreria. Senza root.", "Plug in a USB stick prepared with rekordbox and use Import USB drive on the start screen: tracks, beat grids, cues and crates appear in the library. No root needed.")
        },
        {
            view: "none",
            targets: ["showPreferencesButton"],
            title: tr("Sessione e schermo", "Session and screen"),
            text: tr("Se Mixxx si chiude, riparte dove eri. Lo schermo si spegne dopo un po' di inattività (risparmio, OLED) e si risveglia toccando o muovendo la FLX4. Opzioni in Impostazioni.", "If Mixxx closes, it restarts where you were. The screen turns off after a while of inactivity (saves power, OLED) and wakes up when you touch it or move the FLX4. Options are in Settings.")
        },
        {
            view: "none",
            targets: ["showPreferencesButton"],
            title: tr("Pronto!", "You're ready"),
            text: tr("Dall'ingranaggio trovi audio, schermo e cache, e puoi rivedere questo giro. Buon set!", "The gear icon has audio, screen and cache options, and lets you see this tour again. Have a good set!")
        }
    ]
    readonly property bool last: step === steps.length - 1
    readonly property var cur: steps[Math.min(step, steps.length - 1)]

    function open() {
        step = 0;
        lang = "en";
        opened = true;
        applyStep();
        forceActiveFocus();
    }
    function close() {
        if (!opened)
            return;
        opened = false;
        Mixxx.Config.tutorialDone = true;
        restoreView();
    }
    function finish() {
        close();
    }
    function applyStep() {
        if (!opened)
            return;
        prepareView(cur.view);
        refresh();
    }
    function usable(t) {
        return t && t.visible && t.width > 1 && t.height > 1 && t.opacity > 0.05;
    }
    // Where the target of this step is, in window coordinates
    function refresh() {
        if (!opened)
            return;
        const names = cur.targets || [];
        for (let i = 0; i < names.length; i++) {
            const t = targets ? targets[names[i]] : null;
            if (!usable(t))
                continue;
            const p = t.mapToItem(root, 0, 0);
            const x0 = Math.max(0, p.x), y0 = Math.max(0, p.y);
            const x1 = Math.min(width, p.x + t.width), y1 = Math.min(height, p.y + t.height);
            if (x1 - x0 < 4 || y1 - y0 < 4)
                continue;
            const r = Qt.rect(x0, y0, x1 - x0, y1 - y0);
            if (!hasSpot || r.x !== spot.x || r.y !== spot.y || r.width !== spot.width || r.height !== spot.height)
                spot = r;
            hasSpot = true;
            return;
        }
        hasSpot = false;
    }
    function next() {
        if (last)
            finish();
        else
            step++;
    }

    anchors.fill: parent
    focus: opened
    visible: opened
    z: 1000

    onStepChanged: applyStep()
    onWidthChanged: refresh()
    onHeightChanged: refresh()
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            finish();
            event.accepted = true;
        } else if (event.key === Qt.Key_Right) {
            next();
            event.accepted = true;
        } else if (event.key === Qt.Key_Left && step > 0) {
            step--;
            event.accepted = true;
        }
    }

    // The layout may still be animating after a view change: follow it
    Timer {
        interval: 120
        repeat: true
        running: root.opened
        onTriggered: root.refresh()
    }

    // Blocks every touch on the interface below
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onWheel: wheel => wheel.accepted = true
    }

    // Dimmed everywhere except the spotlight (four bands: no shader needed)
    readonly property color dim: "#cc000000"
    readonly property real pad: 4
    readonly property real sx: hasSpot ? Math.max(0, spot.x - pad) : 0
    readonly property real sy: hasSpot ? Math.max(0, spot.y - pad) : 0
    readonly property real sr: hasSpot ? Math.min(width, spot.x + spot.width + pad) : 0
    readonly property real sb: hasSpot ? Math.min(height, spot.y + spot.height + pad) : 0

    Rectangle {
        color: root.dim
        height: root.hasSpot ? root.sy : root.height
        width: root.width
    }
    Rectangle {
        color: root.dim
        visible: root.hasSpot
        y: root.sb
        height: root.height - root.sb
        width: root.width
    }
    Rectangle {
        color: root.dim
        visible: root.hasSpot
        y: root.sy
        height: root.sb - root.sy
        width: root.sx
    }
    Rectangle {
        color: root.dim
        visible: root.hasSpot
        x: root.sr
        y: root.sy
        height: root.sb - root.sy
        width: root.width - root.sr
    }
    Rectangle {
        id: frame

        border.color: Theme.accentColor
        border.width: 3
        color: "transparent"
        height: root.sb - root.sy
        radius: 6
        visible: root.hasSpot
        width: root.sr - root.sx
        x: root.sx
        y: root.sy

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            running: root.opened && root.hasSpot

            NumberAnimation {
                duration: 900
                easing.type: Easing.InOutSine
                from: 1
                to: 0.35
            }
            NumberAnimation {
                duration: 900
                easing.type: Easing.InOutSine
                from: 0.35
                to: 1
            }
        }
    }

    // ------------------------------------------------------------ the card
    Rectangle {
        id: card

        readonly property real margin: 12
        readonly property real maxW: root.width - root.safeLeft - margin
        readonly property real spaceAbove: root.sy
        readonly property real spaceBelow: root.height - root.sb
        readonly property real spaceLeft: root.sx - root.safeLeft
        readonly property real spaceRight: root.width - root.sr
        // Placement with the most room: 0 centre, 1 below, 2 above, 3 right, 4 left
        readonly property int place: {
            if (!root.hasSpot)
                return 0;
            const need = height + margin * 2;
            const needW = width + margin * 2;
            const c = [[1, spaceBelow, need], [2, spaceAbove, need], [3, spaceRight, needW], [4, spaceLeft, needW]];
            let best = 0, bestSpace = -1;
            for (let i = 0; i < c.length; i++) {
                if (c[i][1] >= c[i][2] && c[i][1] > bestSpace) {
                    best = c[i][0];
                    bestSpace = c[i][1];
                }
            }
            if (best !== 0)
                return best;
            // Nothing fits: the biggest free area, clamped on screen
            const m = Math.max(spaceBelow, spaceAbove);
            return m === spaceBelow ? 1 : 2;
        }

        border.color: Theme.deckLineColor
        color: Theme.darkGray2
        height: col.implicitHeight + 24
        radius: 12
        width: Math.max(220, Math.min(maxW, 400))
        x: {
            let v;
            switch (place) {
            case 3:
                v = root.sr + margin;
                break;
            case 4:
                v = root.sx - margin - width;
                break;
            case 0:
                v = (root.width - width) / 2;
                break;
            default:
                v = root.sx + (root.sr - root.sx - width) / 2;
            }
            return Math.round(Math.max(root.safeLeft, Math.min(root.width - width - margin, v)));
        }
        y: {
            let v;
            switch (place) {
            case 1:
                v = root.sb + margin;
                break;
            case 2:
                v = root.sy - margin - height;
                break;
            case 0:
                v = (root.height - height) / 2;
                break;
            default:
                v = root.sy + (root.sb - root.sy - height) / 2;
            }
            return Math.round(Math.max(margin, Math.min(root.height - height - margin, v)));
        }

        ColumnLayout {
            id: col

            anchors.left: parent.left
            anchors.margins: 14
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 12
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    color: Theme.white
                    elide: Text.ElideRight
                    font.bold: true
                    font.pixelSize: 18
                    text: root.cur.title
                }
                Row {
                    spacing: 0

                    Repeater {
                        model: ["it", "en"]

                        Rectangle {
                            required property string modelData

                            color: root.lang === modelData ? Theme.accentColor : "transparent"
                            border.color: Theme.deckLineColor
                            height: 26
                            width: 34

                            Text {
                                anchors.centerIn: parent
                                color: root.lang === parent.modelData ? "white" : Theme.textMutedColor
                                font.bold: true
                                font.pixelSize: 12
                                text: parent.modelData.toUpperCase()
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: root.lang = parent.modelData
                            }
                        }
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textMutedColor
                font.pixelSize: 14
                lineHeight: 1.1
                text: root.cur.text
                wrapMode: Text.WordWrap
            }
            Row {
                Layout.alignment: Qt.AlignHCenter
                spacing: 6

                Repeater {
                    model: root.steps.length

                    Rectangle {
                        color: index === root.step ? Theme.accentColor : Theme.deckLineColor
                        height: 7
                        radius: 3.5
                        width: 7
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    Layout.preferredHeight: 38
                    text: root.tr("Salta", "Skip")
                    visible: !root.last
                    onClicked: root.finish()
                    contentItem: Text {
                        color: Theme.textMutedColor
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        text: parent.text
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Item {}
                }
                Item {
                    Layout.fillWidth: true
                }
                Button {
                    Layout.preferredHeight: 38
                    Layout.preferredWidth: 90
                    enabled: root.step > 0
                    opacity: enabled ? 1 : 0.35
                    text: root.tr("Indietro", "Back")
                    onClicked: root.step--
                    contentItem: Text {
                        color: Theme.white
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        text: parent.text
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        border.color: Theme.deckLineColor
                        color: parent.pressed ? Theme.deckLineColor : "transparent"
                        radius: 8
                    }
                }
                Button {
                    Layout.preferredHeight: 38
                    Layout.preferredWidth: 90
                    text: root.last ? root.tr("Fine", "Done") : root.tr("Avanti", "Next")
                    onClicked: root.next()
                    contentItem: Text {
                        color: "white"
                        font.bold: true
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        text: parent.text
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.pressed ? Theme.accent : Theme.accentColor
                        radius: 8
                    }
                }
            }
        }
    }
}
