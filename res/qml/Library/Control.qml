import ".." as Skin
import "." as LibraryComponent
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

Item {
    id: root

    property alias focusWidget: focusedWidgetControl.value

    signal moveVertical(int offset)
    signal loadSelectedTrack(string group, bool play)
    signal loadSelectedTrackIntoNextAvailableDeck(bool play)

    Skin.FocusedWidgetControl {
        id: focusedWidgetControl

        Component.onCompleted: this.value = Skin.FocusedWidgetControl.WidgetKind.LibraryView
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "GoToItem"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.loadSelectedTrackIntoNextAvailableDeck(false);
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "LoadSelectedIntoFirstStopped"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.loadSelectedTrackIntoNextAvailableDeck(false);
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectTrackKnob"
        onValueChanged: (value) => {
            if (value != 0) {
                root.focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
                root.moveVertical(value);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectPrevTrack"
        onValueChanged: (value) => {
            if (value != 0) {
                root.focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
                root.moveVertical(-1);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Playlist]"
        key: "SelectNextTrack"
        onValueChanged: (value) => {
            if (value != 0) {
                root.focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
                root.moveVertical(1);
            }
        }
    }

    // In the QML UI the focus between the sidebar (sources) and the track list
    // is handled here and in Browser.qml: the legacy LibraryControl only
    // handles widgets. A press in the track list moves the cursor to the
    // sources; a press there opens the source (Browser.qml decides whether
    // the focus comes back). The focus is read now and changed afterwards,
    // so the same press is not handled twice.
    function toggleFocus() {
        if (root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.Sidebar)
            return;
        Qt.callLater(() => {
            root.focusWidget = Skin.FocusedWidgetControl.WidgetKind.Sidebar;
        });
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveFocusForward"
        onValueChanged: (value) => {
            if (value != 0)
                root.toggleFocus();
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveFocusBackward"
        onValueChanged: (value) => {
            if (value != 0)
                root.toggleFocus();
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveFocus"
        onValueChanged: (value) => {
            if (value != 0)
                root.toggleFocus();
        }
    }

    // Back (SHIFT + BROWSE press on the FLX4): from the track list the cursor
    // goes to the sources, the same as the plain press does (toggleFocus).
    // From the sources Browser.qml closes the open source or goes up to its
    // parent; the focus is read before it is changed, so a press never does
    // both.
    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveLeft"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.toggleFocus();
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveVertical"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.moveVertical(value);
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveUp"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.moveVertical(-1);
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveDown"
        onValueChanged: (value) => {
            if (value != 0 && root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView)
                root.moveVertical(1);
        }
    }

    Mixxx.ControlProxy {
        id: numDecksControl

        group: "[App]"
        key: "num_decks"
    }

    Instantiator {
        model: numDecksControl.value

        delegate: LibraryComponent.ControlLoadSelectedTrackHandler {
            required property int index

            group: "[Channel" + (index + 1) + "]"
            enabled: root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView
            onLoadTrackRequested: (play) => {
                root.loadSelectedTrack(this.group, play);
            }
        }
    }

    Mixxx.ControlProxy {
        id: numPreviewDecksControl

        group: "[App]"
        key: "num_preview_decks"
    }

    Instantiator {
        model: numPreviewDecksControl.value

        delegate: LibraryComponent.ControlLoadSelectedTrackHandler {
            required property int index

            group: "[PreviewDeck" + (index + 1) + "]"
            enabled: root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView
            onLoadTrackRequested: (play) => {
                root.loadSelectedTrack(this.group, play);
            }
        }
    }

    Mixxx.ControlProxy {
        id: numSamplersControl

        group: "[App]"
        key: "num_samplers"
    }

    Instantiator {
        model: numSamplersControl.value

        delegate: LibraryComponent.ControlLoadSelectedTrackHandler {
            required property int index

            group: "[Sampler" + (index + 1) + "]"
            enabled: root.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView
            onLoadTrackRequested: (play) => {
                root.loadSelectedTrack(this.group, play);
            }
        }
    }
}
