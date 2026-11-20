import Qt5Compat.GraphicalEffects
import QtQuick
import QtQuick.Layouts
import "../Theme"

Rectangle {
    id: root

    readonly property var dragImage: dragImageLoader.item ? dragImageLoader.item.effect : null
    // The drag image is made when the cell is pressed (desktop only, see
    // grabDragImage), not for every cell of the table; TrackList.qml resets
    // it when the cell is put back in the pool.
    property bool dragImageWanted: false

    // Makes the drag image of the track (cover, title, artist) and gives it
    // to the drag. Taken again when the cover arrives (it is loaded in the
    // background).
    function grabDragImage() {
        if (Qt.platform.os === "android")
            return;
        root.dragImageWanted = true;
        const image = root.dragImage;
        if (!image)
            return;
        image.grabToImage((result) => {
            root.Drag.imageSource = result.url;
        }, Qt.size(image.width, image.height));
    }

    anchors.fill: parent

    // The controller cursor (see TrackList.qml): the selected row is bright,
    // with a line above and below, while the cursor is on the track list, and
    // dim while it is on the sources
    readonly property bool cursorOnList: tableView.controllerFocus
    readonly property color rowColor: row % 2 == 0 ? Theme.sunkenBackgroundColor : Theme.backgroundColor

    color: selected ? (cursorOnList ? Theme.accent : Qt.tint(rowColor, Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5))) : rowColor

    Drag.dragType: Drag.Automatic
    Drag.supportedActions: Qt.CopyAction
    Drag.mimeData: {
        "text/uri-list": file_url.toString(),
        "text/plain": file_url.toString(),
    }
    // Drag image (cover + title + shadow): desktop drag and drop only, made
    // on demand (one per cell in the track table was a large cost, and it
    // read the track and the cover of every row).
    Loader {
        id: dragImageLoader
        active: Qt.platform.os !== "android" && root.dragImageWanted
        sourceComponent: Item {
            readonly property alias effect: dragImageEffect
            width: dragImageSource.width
            height: dragImageSource.height

            Item {
                id: dragImageSource
                width: 190
                height: 85
                visible: false
                Rectangle {
                    color: Theme.sunkenBackgroundColor
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom
                        margins: 5
                    }
                    radius: 12
                    RowLayout {
                        anchors.fill: parent
                        Image {
                            id: cover
                            Layout.fillHeight: true
                            Layout.preferredWidth: cellData.cover_art ? 75 : 0
                            fillMode: Image.PreserveAspectFit
                            source: cellData.cover_art
                            sourceSize.width: 150
                            sourceSize.height: 150
                            clip: true
                            asynchronous: true
                            onStatusChanged: {
                                if (status === Image.Ready)
                                    root.grabDragImage();
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            // (the track is read when the drag image is made)
                            readonly property var track: cellData.track
                            Text {
                                text: parent.track ? parent.track.title : 'Unknown title'
                                color: Theme.textColor
                            }
                            Text {
                                text: parent.track ? parent.track.artist : 'Unknown artist'
                                color: Theme.midGray
                            }
                        }
                    }
                    Rectangle {
                        width: 20
                        anchors {
                            top: parent.top
                            right: parent.right
                            bottom:parent.bottom
                        }
                        gradient: Gradient {
                            orientation: Gradient.Horizontal

                            GradientStop {
                                position: 1
                                color: Theme.darkGray
                            }

                            GradientStop {
                                position: 0
                                color: 'transparent'
                            }
                        }
                    }
                }
            }
            DropShadow {
                id: dragImageEffect
                visible: false
                anchors.fill: dragImageSource
                source: dragImageSource
                horizontalOffset: 0
                verticalOffset: 0
                radius: 10.0
                color: "#80000000"
            }
        }
    }

    // Hairline between the columns
    Rectangle {
        id: border
        color: Theme.dividerColor
        width: 1
        anchors {
            top: parent.top
            bottom: parent.bottom
            right: parent.right
        }
    }

    // Lines above and below the row under the controller cursor (created
    // only for its cells)
    Loader {
        anchors.fill: parent
        active: selected && root.cursorOnList
        sourceComponent: Item {
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.white
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.white
            }
        }
    }
}
