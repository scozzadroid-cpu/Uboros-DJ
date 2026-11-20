import QtQuick
import Mixxx 1.0 as Mixxx
import "." as LibraryComponent
import "../Theme"
import "Harmony.js" as Harmony

Mixxx.LibrarySourceTree {
    id: root

    // Harmonic hints, better than a CDJ: the Key and Bpm columns are
    // highlighted when the track can be mixed with the reference deck, i.e.
    // the sync leader if it plays, otherwise the first deck that plays.
    component DeckState: QtObject {
        id: deckState

        required property string group
        readonly property Mixxx.ControlProxy play: Mixxx.ControlProxy {
            group: deckState.group
            key: "play"
        }
        readonly property Mixxx.ControlProxy leader: Mixxx.ControlProxy {
            group: deckState.group
            key: "sync_leader"
        }
        readonly property Mixxx.ControlProxy key: Mixxx.ControlProxy {
            group: deckState.group
            key: "key"
        }
        readonly property Mixxx.ControlProxy bpm: Mixxx.ControlProxy {
            group: deckState.group
            key: "bpm"
        }
        // BPM and key of the loaded track itself (they change once, when a
        // track is loaded, not with the pitch fader): what Matching is built on
        readonly property Mixxx.ControlProxy fileBpm: Mixxx.ControlProxy {
            group: deckState.group
            key: "file_bpm"
            onValueChanged: root.deckTrackChanged(deckState)
        }
        readonly property Mixxx.ControlProxy fileKey: Mixxx.ControlProxy {
            group: deckState.group
            key: "file_key"
            onValueChanged: root.deckTrackChanged(deckState)
        }
    }

    readonly property list<QtObject> decks: [
        DeckState { group: "[Channel1]" },
        DeckState { group: "[Channel2]" },
        DeckState { group: "[Channel3]" },
        DeckState { group: "[Channel4]" }
    ]
    readonly property var referenceDeck: {
        for (const deck of decks) {
            if (deck.play.value > 0 && deck.leader.value > 0)
                return deck;
        }
        for (const deck of decks) {
            if (deck.play.value > 0)
                return deck;
        }
        return null;
    }
    readonly property var referenceKey: referenceDeck ? Harmony.fromChromaticKey(referenceDeck.key.value) : null
    readonly property color playedTextColor: "#6f8fb0"
    readonly property real referenceBpm: referenceDeck ? referenceDeck.bpm.value : 0

    // Matching (Club): the tracks that fit the deck that plays (the sync
    // leader first), otherwise the deck that loaded a track last. The list
    // is built once per track: the search is computed by Harmony.js when the
    // BPM or the key of the file of that deck change (a short pause groups
    // the two changes of one load).
    property var lastLoadedDeck: null
    readonly property var matchDeck: {
        if (referenceDeck && referenceDeck.fileBpm.value > 0)
            return referenceDeck;
        if (lastLoadedDeck && lastLoadedDeck.fileBpm.value > 0)
            return lastLoadedDeck;
        for (const deck of decks) {
            if (deck.fileBpm.value > 0)
                return deck;
        }
        return null;
    }
    readonly property real matchBpm: matchDeck ? matchDeck.fileBpm.value : 0
    readonly property real matchKey: matchDeck ? matchDeck.fileKey.value : 0
    // What Matching is matched to, for the label above the list
    readonly property string matchingInfo: {
        if (!matchDeck)
            return qsTr("no track loaded");
        const key = Harmony.camelotText(matchKey);
        return matchDeck.group.replace(/^\[Channel(\d+)\]$/, "Deck $1") + "  " + matchBpm.toFixed(1) + " BPM" + (key !== "" ? "  " + key : "");
    }

    function deckTrackChanged(deck) {
        if (deck.fileBpm.value > 0)
            lastLoadedDeck = deck;
        matchTimer.restart();
    }

    onMatchBpmChanged: matchTimer.restart()
    onMatchKeyChanged: matchTimer.restart()

    // (properties, not children: the default property of this item takes
    // library sources only)
    readonly property Timer matchTimer: Timer {
        interval: 300
        // Only the Club profile has the Matching list
        onTriggered: {
            if (Theme.isClub)
                matchingSource.setQuery(Harmony.matchingQuery(root.matchBpm, root.matchKey));
        }
    }
    readonly property Connections themeWatcher: Connections {
        target: Theme

        function onProfileChanged() {
            root.matchTimer.restart();
        }
    }

    Component.onCompleted: matchTimer.restart()

    component DefaultDelegate: LibraryComponent.Cell {
        id: cell
        readonly property var caps: capabilities
        // Tracks already played in this session: dimmer text, like the
        // history colour of a CDJ browser
        readonly property color baseTextColor: played ? root.playedTextColor : Theme.textColor
        property color textColor: baseTextColor
        property bool textBold: false
        // FIXME: https://bugreports.qt.io/browse/QTBUG-111789
        Binding on Drag.active {
            value: dragArea.drag.active
            // This delays the update until the even queue is cleared
            // preventing any potential oscillations causing a loop
            delayed: true
        }

        LibraryComponent.Track {
            id: dragArea
            anchors.fill: parent
            capabilities: cell.caps

            onPressed: {
                if (pressedButtons == Qt.LeftButton) {
                    tableView.selectionModel.selectRow(row);
                    // No drag and drop on touch screens: skip the costly grab
                    if (Qt.platform.os === "android")
                        return;
                    cell.grabDragImage();
                }
            }
            onDoubleClicked: {
                tableView.selectionModel.selectRow(row);
                tableView.loadSelectedTrackIntoNextAvailableDeck(false);
            }
        }

        Text {
            id: value
            anchors.fill: parent
            anchors.leftMargin: Qt.platform.os === "android" ? 6 : 15
            font.pixelSize: Theme.trackFontSize
            text: display ?? ""
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: cell.textColor
            font.bold: cell.textBold
        }
    }

    // Green: same key or compatible on the Camelot wheel
    // Stack: the key in the color of the Camelot wheel (a bar on the left
    // marks a compatible key, bold text a match with the playing deck)
    component KeyDelegate: DefaultDelegate {
        readonly property var camelot: Harmony.fromText(display)
        readonly property int match: Harmony.compatibility(camelot, root.referenceKey)
        textColor: Theme.isStack && camelot ? (played ? Qt.darker(Theme.camelotColor(camelot.n), 1.5) : Theme.camelotColor(camelot.n)) : (match == 2 ? "#4ee04e" : (match == 1 ? "#a6e36b" : baseTextColor))
        textBold: match > 0 || (Theme.isStack && camelot !== null)

        Rectangle {
            visible: Theme.isStack && match > 0
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: parent.height - 6
            radius: 1.5
            color: match == 2 ? "#4ee04e" : "#a6e36b"
        }
    }

    // Green: within 3% of the reference BPM (also half/double tempo),
    // yellow: within 6%
    component BpmDelegate: DefaultDelegate {
        readonly property real distance: Harmony.bpmDistancePercent(display, root.referenceBpm)
        textColor: distance <= 3 ? "#4ee04e" : (distance <= 6 ? "#e3c85a" : baseTextColor)
        textBold: distance <= 6
    }

    // Stems playable for the track (stem file or MIXXX/STEMS on the drive)
    component StemsDelegate: LibraryComponent.Cell {
        id: stemsCell
        readonly property var caps: capabilities

        LibraryComponent.Track {
            anchors.fill: parent
            capabilities: stemsCell.caps
            onPressed: {
                if (pressedButtons == Qt.LeftButton)
                    tableView.selectionModel.selectRow(row);
            }
            onDoubleClicked: {
                tableView.selectionModel.selectRow(row);
                tableView.loadSelectedTrackIntoNextAvailableDeck(false);
            }
        }
        Rectangle {
            // (read for the cells of this column only)
            visible: cellData.has_stems
            anchors.centerIn: parent
            width: stemsLabel.implicitWidth + 10
            height: stemsLabel.implicitHeight + 2
            radius: 3
            color: "#1f4a2a"
            border.width: 1
            border.color: "#4ee04e"

            Text {
                id: stemsLabel

                anchors.centerIn: parent
                text: "STEMS"
                color: "#9dff9d"
                font.family: Theme.fontFamily
                font.pixelSize: Qt.platform.os === "android" ? 9 : 11
                font.bold: true
            }
        }
    }

    defaultColumns: [
        Mixxx.TrackListColumn {
            // No cover column on phones (width 0: TableView creates no cells
            // for it): covers are shown on the decks, and loading one per row
            // made scrolling stutter
            preferredWidth: Qt.platform.os === "android" ? 0 : 110

            columnIdx: Mixxx.TrackListColumn.SQLColumns.Album

            delegate: Rectangle {
                color: cellData.decoration
                implicitHeight: Qt.platform.os === "android" ? 22 : 30

                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    source: cellData.cover_art
                    // Decode a thumbnail, not the full size embedded cover
                    sourceSize.width: 96
                    sourceSize.height: 96
                    clip: true
                    asynchronous: true
                }
            }
        },
        // FIXME: WaveformOverview is currently disabled due to performance limitation. Like for the legacy UI, a cache likely needs to be implemented to help
        // Mixxx.TrackListColumn {
        //     label: qsTr("Preview")
        //     fillSpan: 3
        //     preferredWidth: 300
        //     columnIdx: Mixxx.TrackListColumn.SQLColumns.Title

        //     delegate: LibraryCell {
        //         // implicitHeight: 30
        //         anchors.fill: parent

        //         readonly property var trackProxy: track

        //         Drag.active: dragArea.drag.active
        //         Drag.dragType: Drag.Automatic
        //         Drag.supportedActions: Qt.CopyAction
        //         Drag.mimeData: {
        //             "text/uri-list": file_url,
        //             "text/plain": file_url
        //         }

        //         LibraryComponent.Track {
        //             id: dragArea
        //             anchors.fill: parent
        //             capabilities: parent.capabilities

        //             onPressed: {
        //                 if (pressedButtons == Qt.LeftButton) {
        //                     tableView.selectionModel.selectRow(row);
        //                     parent.dragImage.grabToImage((result) => {
        //                             parent.Drag.imageSource = result.url;
        //                     });
        //                 } else {
        //                 }
        //             }
        //             onDoubleClicked: {
        //                 tableView.selectionModel.selectRow(row);
        //                 tableView.loadSelectedTrackIntoNextAvailableDeck(false);
        //             }
        //         }

        //         Mixxx.WaveformOverview {
        //             anchors.fill: parent
        //             channels: Mixxx.WaveformOverview.Channels.LeftChannel
        //             renderer: Mixxx.WaveformOverview.Renderer.Filtered
        //             colorHigh: Theme.white
        //             colorMid: Theme.blue
        //             colorLow: Theme.green
        //             track: trackProxy
        //         }
        //         Rectangle {
        //             id: border
        //             color: Theme.darkGray2
        //             width: 1
        //             anchors {
        //                 top: parent.top
        //                 bottom: parent.bottom
        //                 right: parent.right
        //             }
        //         }
        //     }

        // },
        Mixxx.TrackListColumn {
            label: qsTr("Title")
            fillSpan: 3
            columnIdx: Mixxx.TrackListColumn.SQLColumns.Title

            delegate: DefaultDelegate { }
        },
        Mixxx.TrackListColumn {
            label: qsTr("Artist")
            fillSpan: 2

            columnIdx: Mixxx.TrackListColumn.SQLColumns.Artist
            delegate: DefaultDelegate { }
        },
        Mixxx.TrackListColumn {
            label: qsTr("Album")
            fillSpan: 1
            // Hidden on phones (0 width): title, artist, BPM and key matter most
            preferredWidth: Qt.platform.os === "android" ? 0 : -1

            columnIdx: Mixxx.TrackListColumn.SQLColumns.Album
            delegate: DefaultDelegate { }
        },
        Mixxx.TrackListColumn {
            label: qsTr("Year")
            preferredWidth: Qt.platform.os === "android" ? 0 : 80

            columnIdx: Mixxx.TrackListColumn.SQLColumns.Year
            delegate: DefaultDelegate { }
        },
        Mixxx.TrackListColumn {
            label: qsTr("Bpm")
            preferredWidth: 60

            columnIdx: Mixxx.TrackListColumn.SQLColumns.Bpm
            delegate: BpmDelegate { }
        },
        Mixxx.TrackListColumn {
            label: qsTr("Key")
            preferredWidth: 70

            columnIdx: Mixxx.TrackListColumn.SQLColumns.Key
            delegate: KeyDelegate { }
        },
        Mixxx.TrackListColumn {
            label: qsTr("Stems")
            preferredWidth: 64

            columnIdx: Mixxx.TrackListColumn.SQLColumns.FileType
            delegate: StemsDelegate { }
        },
        Mixxx.TrackListColumn {
            label: qsTr("File Type")
            preferredWidth: Qt.platform.os === "android" ? 0 : 70

            columnIdx: Mixxx.TrackListColumn.SQLColumns.FileType
            delegate: DefaultDelegate { }
        },
        Mixxx.TrackListColumn {
            label: qsTr("Bitrate")
            preferredWidth: Qt.platform.os === "android" ? 0 : 70

            columnIdx: Mixxx.TrackListColumn.SQLColumns.Bitrate
            delegate: DefaultDelegate { }
        },
        // Track length (ColumnCache COLUMN_LIBRARYTABLE_DURATION = 12, not in
        // the SQLColumns enum). Hidden (0 width) except in the Club profile,
        // see TrackList.qml (preferredWidthOf). Keep it last, and the order of
        // these columns in step with libraryColumnKinds there.
        Mixxx.TrackListColumn {
            label: qsTr("Time")
            preferredWidth: 0

            columnIdx: 12
            delegate: DefaultDelegate { }
        }
    ]
    Mixxx.LibraryAllTrackSource {
        label: qsTr("All...")
        columns: root.defaultColumns
    }
    // Club only (Browser.qml hides the entry in the other profiles): the
    // tracks that fit a deck, see matchDeck above. The list is a search of
    // its own, the text field of the track list is off for it.
    Mixxx.LibraryMatchingSource {
        id: matchingSource

        label: qsTr("Matching")
        columns: root.defaultColumns
    }
    // Ways to split the library: playlists and crates (Mixxx), artists and
    // folders (groups of the library tracks)
    Mixxx.LibraryPlaylistSource {
        label: qsTr("Playlists")
        columns: root.defaultColumns
    }
    Mixxx.LibraryCrateSource {
        label: qsTr("Crates")
        columns: root.defaultColumns
    }
    // The sessions played, newest first; hidden until something has been
    // played (Browser.qml)
    Mixxx.LibraryHistorySource {
        label: qsTr("History")
        columns: root.defaultColumns
    }
    Mixxx.LibraryArtistSource {
        label: qsTr("Artists")
        columns: root.defaultColumns
    }
    Mixxx.LibraryFolderSource {
        label: qsTr("Folders")
        columns: root.defaultColumns
    }
    // USB drives exported with Rekordbox. The devices are searched when the
    // entry is selected.
    Mixxx.LibraryRekordboxSource {
        label: qsTr("Rekordbox")
        columns: root.defaultColumns
    }
    // Folders: quick links, removable devices (USB drives) and the local
    // storage. The columns are the ones of the file browser model, not the
    // library ones.
    Mixxx.LibraryBrowseSource {
        label: qsTr("Locale")

        Mixxx.TrackListColumn {
            label: qsTr("Filename")
            fillSpan: 3
            columnIdx: 1
            delegate: DefaultDelegate { }
        }
        Mixxx.TrackListColumn {
            label: qsTr("Title")
            fillSpan: 2
            columnIdx: 3
            delegate: DefaultDelegate { }
        }
        Mixxx.TrackListColumn {
            label: qsTr("Artist")
            fillSpan: 2
            columnIdx: 2
            delegate: DefaultDelegate { }
        }
        Mixxx.TrackListColumn {
            label: qsTr("Bpm")
            preferredWidth: 60
            columnIdx: 11
            delegate: BpmDelegate { }
        }
        Mixxx.TrackListColumn {
            label: qsTr("Key")
            preferredWidth: 70
            columnIdx: 12
            delegate: KeyDelegate { }
        }
    }
}
