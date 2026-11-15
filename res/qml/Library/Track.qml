import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls.Basic
import "../Theme"

MouseArea {
    id: dragArea

    required property var capabilities

    readonly property var library: Mixxx.Library

    // On touch screens dragging must scroll the list, not move the cell
    drag.target: Qt.platform.os === "android" ? null : value
    acceptedButtons: Qt.LeftButton | Qt.RightButton
    onClicked: (mouse) => {
        if (mouse.button === Qt.RightButton)
            openContextMenu()
    }
    onPressAndHold: (mouse) => {
        // Long press opens the menu on touch screens too ("Load to" deck)
        if (mouse.source === Qt.MouseEventNotSynthesized || Qt.platform.os === "android")
            openContextMenu()
    }

    // The menu tree is created on demand: one per cell (hundreds in the
    // track table, recreated while scrolling) was a large hidden cost.
    function openContextMenu() {
        contextMenuLoader.active = true;
        contextMenuLoader.item.popup();
    }

    Loader {
        id: contextMenuLoader
        active: false
        sourceComponent: contextMenuComponent
    }

    function hasCapabilities(caps) {
        return (dragArea.capabilities & caps) == caps;
    }

    Component {
        id: contextMenuComponent

        Menu {
            id: contextMenu
            title: qsTr("File")

            Menu {
                title: qsTr("Load to")
                enabled: {
                    hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToDeck) ||
                    hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToSampler) ||
                    hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToPreviewDeck)
                }

                Menu {
                    id: loadToDeckMenu
                    title: qsTr("Deck")
                    enabled: hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToDeck)
                    Instantiator {
                        model: 4
                        delegate: MenuItem {
                            text: qsTr("Deck %1").arg(modelData+1)
                            onTriggered: Mixxx.PlayerManager.getPlayer(`[Channel${modelData+1}]`).loadTrack(track ?? tableView.model.getTrack(row))
                        }

                        onObjectAdded: (index, object) => loadToDeckMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => loadToDeckMenu.removeItem(object)
                    }
                }

                Menu {
                    title: qsTr("Sampler")
                    enabled: hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToSampler)
                }

                // Instantiator {
                //     id: recentFilesInstantiator
                //     model: settings.recentFiles
                //     delegate: MenuItem {
                //         text: settings.displayableFilePath(modelData)
                //         onTriggered: loadFile(modelData)
                //     }

                //     onObjectAdded: (index, object) => recentFilesMenu.insertItem(index, object)
                //     onObjectRemoved: (index, object) => recentFilesMenu.removeItem(object)
                // }
            }

            Menu {
                id: addToPlaylistMenu
                title: qsTr("Add to playlists")
                enabled: {
                    hasCapabilities(Mixxx.LibraryTrackListModel.Capability.AddToTrackSet)
                }

                MenuSeparator {}

                MenuItem {
                    enabled: false // TODO implement
                    text: qsTr("Create New Playlist")
                }
            }

            Menu {
                id: addToCrateMenu
                title: qsTr("Crates")
                enabled: {
                    hasCapabilities(Mixxx.LibraryTrackListModel.Capability.AddToTrackSet)
                }

                MenuSeparator {}

                MenuItem {
                    enabled: false // TODO implement
                    text: qsTr("Create New Crate")
                }
            }

            Menu {
                id: analyzeMenu
                title: qsTr("Analyze")
                enabled: {
                    hasCapabilities(Mixxx.LibraryTrackListModel.Capability.EditMetadata)||
                    hasCapabilities(Mixxx.LibraryTrackListModel.Capability.Analyze)
                }
                MenuItem {
                    text: qsTr("Analyze")
                    onTriggered: {
                        library.analyze(track)
                    }
                }
                MenuItem {
                    enabled: false // TODO implement
                    text: qsTr("Reanalyze")
                }
                MenuItem {
                    enabled: false // TODO implement
                    text: qsTr("Reanalyze (constant BPM)")
                }
                MenuItem {
                    enabled: false // TODO implement
                    text: qsTr("Reanalyze (variable BPM)")
                }
            }
        }
    }
}
