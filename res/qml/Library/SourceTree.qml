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
                    parent.dragImage.grabToImage((result) => {
                            parent.Drag.imageSource = result.url;
                        }, Qt.size(parent.dragImage.width, parent.dragImage.height));
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
            font.pixelSize: Qt.platform.os === "android" ? 12 : 14
            text: display ?? ""
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            color: cell.textColor
            font.bold: cell.textBold
        }
    }

    // Green: same key or compatible on the Camelot wheel
    component KeyDelegate: DefaultDelegate {
        readonly property int match: Harmony.compatibility(Harmony.fromText(display), root.referenceKey)
        textColor: match == 2 ? "#4ee04e" : (match == 1 ? "#a6e36b" : baseTextColor)
        textBold: match > 0
    }

    // Green: within 3% of the reference BPM (also half/double tempo),
    // yellow: within 6%
    component BpmDelegate: DefaultDelegate {
        readonly property real distance: Harmony.bpmDistancePercent(display, root.referenceBpm)
        textColor: distance <= 3 ? "#4ee04e" : (distance <= 6 ? "#e3c85a" : baseTextColor)
        textBold: distance <= 6
    }

    defaultColumns: [
        Mixxx.TrackListColumn {
            // Smaller cover column on phones, the table needs the room
            preferredWidth: Qt.platform.os === "android" ? 40 : 110

            columnIdx: Mixxx.TrackListColumn.SQLColumns.Album

            delegate: Rectangle {
                color: decoration
                implicitHeight: Qt.platform.os === "android" ? 22 : 30

                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    source: cover_art
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
        }
    ]
    Mixxx.LibraryAllTrackSource {
        label: qsTr("All...")
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
        label: qsTr("Computer")

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
