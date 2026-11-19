import "." as Skin
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Shapes 1.6
import "Theme"
import "Library" as LibraryComponent

Item {
    id: root

    property var sidebar: librarySources.sidebar()
    property bool compact: false


    LibraryComponent.SourceTree {
        id: librarySources
    }

    // Every track needs its BPM (and key) in the list: Mixxx only analyzes a
    // track when it is loaded, so analyze the remaining ones in the
    // background, a while after startup. The batch analysis pauses while a
    // deck analyzes the track just loaded.
    Timer {
        interval: 20000
        running: true
        onTriggered: Mixxx.Library.analyzeTracksWithoutBpm()
    }

    SplitView {
        id: librarySplitView
        orientation: Qt.Horizontal
        anchors.fill: parent

        handle: Rectangle {
            id: handleDelegate
            implicitWidth: 8
            implicitHeight: 8
            color: Theme.panelSplitterBackground
            clip: true
            property color handleColor: SplitHandle.pressed || SplitHandle.hovered ? Theme.panelSplitterHandleActive : Theme.panelSplitterHandle
            property int handleSize: SplitHandle.pressed || SplitHandle.hovered ? 6 : 5

            ColumnLayout {
                anchors.centerIn: parent
                Repeater {
                    model: 3
                    Rectangle {
                        width: handleSize
                        height: handleSize
                        radius: handleSize
                        color: handleColor
                    }
                }
            }

            containmentMask: Item {
                x: (handleDelegate.width - width) / 2
                width: 8
                height: librarySplitView.height
            }
        }

        SplitView {
            id: sideBarSplitView
            SplitView.minimumWidth: 100
            // Interface profiles: Club 22% of the width, Stack 18%; uboros
            // (librarySidebarShare 0) keeps the original width
            SplitView.preferredWidth: Theme.librarySidebarShare > 0 ? Math.round(root.width * Theme.librarySidebarShare) : (root.compact ? Math.min(170, root.width * 0.2) : 415)
            SplitView.maximumWidth: root.compact ? root.width * 0.5 : 600

            orientation: Qt.Vertical

            handle: Rectangle {
                id: handleDelegate
                implicitWidth: 8
                implicitHeight: 8
                color: Theme.panelSplitterBackground
                clip: true
                property color handleColor: SplitHandle.pressed || SplitHandle.hovered ? Theme.panelSplitterHandleActive : Theme.panelSplitterHandle
                property int handleSize: SplitHandle.pressed || SplitHandle.hovered ? 6 : 5

                RowLayout {
                    anchors.centerIn: parent
                    Repeater {
                        model: 3
                        Rectangle {
                            width: handleSize
                            height: handleSize
                            radius: handleSize
                            color: handleColor
                        }
                    }
                }

                containmentMask: Item {
                    x: (handleDelegate.width - width) / 2
                    height: 8
                    width: sideBarSplitView.width
                }
            }
            LibraryComponent.Browser {
                id: browser
                SplitView.minimumHeight: root.compact ? 80 : 200
                SplitView.preferredHeight: 500
                SplitView.fillHeight: true

                model: root.sidebar
            }

            Skin.PreviewDeck {
                visible: !root.compact
                SplitView.minimumHeight: 100
                SplitView.preferredHeight: 100
                SplitView.maximumHeight: 200
            }
        }
        LibraryComponent.TrackList {
            id: trackList
            SplitView.fillHeight: true

            // FIXME: this is necessary to prevent the header label to render outside of the table when horizontally scrolling: https://github.com/mixxxdj/mixxx/pull/14514#issuecomment-3311914346
            clip: true

            model: root.sidebar.tracklist
            // The Matching list says which track it is matched to
            pathText: browser.shownPath + (browser.shownPath === qsTr("Matching") && librarySources.matchingInfo !== "" ? "   (" + librarySources.matchingInfo + ")" : "")
        }
    }
}
