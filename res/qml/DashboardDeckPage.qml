import QtQuick
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "Theme"

// Decks page of the Android hub: the deck preferences of the desktop dialog (Mixxx.DeckPrefs).
// Saved in the config and applied to the decks at start-up and at once.
ColumnLayout {
    id: root

    readonly property bool italian: Qt.locale().name.startsWith("it")

    function tr(en, it) {
        return root.italian ? it : en;
    }

    spacing: 10

    DashboardChoice {
        current: Mixxx.DeckPrefs.cueMode
        model: [
            {
                "label": "Mixxx",
                "value": 0
            },
            {
                "label": "Pioneer (CDJ)",
                "value": 1
            },
            {
                "label": "Denon",
                "value": 2
            },
            {
                "label": "Numark",
                "value": 3
            },
            {
                "label": root.tr("Mixxx, no blinking", "Mixxx, senza lampeggio"),
                "value": 4
            },
            {
                "label": root.tr("Cue and play", "Cue e play"),
                "value": 5
            }
        ]
        subtitle: root.tr("How the CUE button works on the decks. Pioneer behaves like a CDJ: CUE while stopped sets the cue point, hold it to preview.", "Come funziona il tasto CUE dei deck. Pioneer si comporta come un CDJ: CUE da fermo imposta il punto di cue, tenuto premuto fa l'ascolto.")
        title: root.tr("Cue mode", "Modo CUE")

        onPicked: value => Mixxx.DeckPrefs.cueMode = value
    }
    DashboardChoice {
        current: Mixxx.DeckPrefs.rateRangePercent
        model: [4, 6, 8, 10, 16, 24, 50, 90].map(p => ({
                    "label": p + "%",
                    "value": p
                }))
        subtitle: root.tr("Range of the tempo slider of every deck when the app starts (the deck bar still changes it per deck).", "Intervallo del cursore del tempo di ogni deck all'avvio (la barra del deck lo cambia comunque per deck).")
        title: root.tr("Tempo range", "Intervallo del tempo")

        onPicked: value => Mixxx.DeckPrefs.rateRangePercent = value
    }
    DashboardChoice {
        current: Mixxx.DeckPrefs.keylockMode
        model: [
            {
                "label": root.tr("Original key", "Tonalità originale"),
                "value": 0
            },
            {
                "label": root.tr("Current key", "Tonalità attuale"),
                "value": 1
            }
        ]
        subtitle: root.tr("Master Tempo (key lock): which key is kept when you turn it on while the tempo is changed.", "Master Tempo (key lock): quale tonalità si mantiene quando lo accendi con il tempo già cambiato.")
        title: root.tr("Key lock", "Key lock")

        onPicked: value => Mixxx.DeckPrefs.keylockMode = value
    }
    DashboardChoice {
        current: Mixxx.DeckPrefs.keyunlockMode
        model: [
            {
                "label": root.tr("Reset the key", "Azzera la tonalità"),
                "value": 0
            },
            {
                "label": root.tr("Keep the key", "Mantieni la tonalità"),
                "value": 1
            }
        ]
        subtitle: root.tr("What happens to the key when you turn key lock off.", "Cosa succede alla tonalità quando spegni il key lock.")
        title: root.tr("Key lock off", "Key lock spento")

        onPicked: value => Mixxx.DeckPrefs.keyunlockMode = value
    }
    DashboardChoice {
        current: Mixxx.DeckPrefs.speedAutoReset
        model: [
            {
                "label": root.tr("Keep", "Mantieni"),
                "value": 0
            },
            {
                "label": root.tr("Reset the key", "Azzera la tonalità"),
                "value": 1
            },
            {
                "label": root.tr("Reset the tempo", "Azzera il tempo"),
                "value": 3
            },
            {
                "label": root.tr("Reset both", "Azzera entrambi"),
                "value": 2
            }
        ]
        subtitle: root.tr("Pitch and tempo of a deck when a new track is loaded on it.", "Tonalità e tempo di un deck quando ci carichi un nuovo brano.")
        title: root.tr("On track load", "Al caricamento di un brano")

        onPicked: value => Mixxx.DeckPrefs.speedAutoReset = value
    }
    DashboardChoice {
        current: Mixxx.DeckPrefs.loadPoint
        model: [
            {
                "label": root.tr("Intro start", "Inizio intro"),
                "value": 3
            },
            {
                "label": root.tr("Main cue", "Cue principale"),
                "value": 0
            },
            {
                "label": root.tr("First hot cue", "Primo hot cue"),
                "value": 4
            },
            {
                "label": root.tr("First sound", "Primo suono"),
                "value": 2
            },
            {
                "label": root.tr("Beginning", "Inizio del brano"),
                "value": 1
            }
        ]
        subtitle: root.tr("Where a deck is positioned when a track is loaded.", "Dove si posiziona un deck quando ci carichi un brano.")
        title: root.tr("Load point", "Punto di caricamento")

        onPicked: value => Mixxx.DeckPrefs.loadPoint = value
    }
    Text {
        Layout.fillWidth: true
        Layout.topMargin: 4
        color: Theme.midGray
        font.pixelSize: 12
        text: root.tr("Saved and applied to the decks at every start. Cue mode, tempo range and key lock act at once; the last two apply from the next track you load.", "Salvate e applicate ai deck a ogni avvio. Modo CUE, intervallo del tempo e key lock agiscono subito; le ultime due valgono dal prossimo brano caricato.")
        wrapMode: Text.WordWrap
    }
}
