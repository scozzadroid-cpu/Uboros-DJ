import ".." as Skin
import "." as LibraryComponent
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls.Basic
import "../Theme"

Rectangle {
    id: root

    color: Theme.darkGray

    required property var model
    // Path of the source whose tracks are shown (set by Library.qml from
    // Browser.qml), e.g. "Rekordbox > Playlists > House"
    property string pathText: ""

    LibraryComponent.Control {
        id: libraryControl

        onMoveVertical: (offset) => {
            view.selectionModel.moveSelectionVertical(offset);
        }
        onLoadSelectedTrack: (group, play) => {
            view.loadSelectedTrack(group, play);
        }
        onLoadSelectedTrackIntoNextAvailableDeck: (play) => {
            view.loadSelectedTrackIntoNextAvailableDeck(play);
        }
        onFocusWidgetChanged: {
            switch (focusWidget) {
                case Skin.FocusedWidgetControl.WidgetKind.LibraryView:
                    view.forceActiveFocus();
                    break;
            }
        }
    }

    // Takes no room while there is no path
    Item {
        id: pathBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        anchors.topMargin: root.pathText !== "" ? 3 : 0
        height: root.pathText !== "" ? pathLabel.implicitHeight + 2 : 0
        visible: root.pathText !== ""

        Text {
            id: pathLabel

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            // The end of a long path (the open folder) stays visible
            elide: Text.ElideLeft
            text: root.pathText
            color: Theme.textMutedColor
            font.family: Theme.fontFamily
            font.pixelSize: Qt.platform.os === "android" ? 10 : 12
            font.weight: Font.Medium
        }
    }

    HorizontalHeaderView {
        id: horizontalHeader

        anchors.top: pathBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 5
        syncView: view

        property int sortingColumn: -1
        property var sortingOrder: Qt.Descending

        delegate: Item {
            id: column
            required property string display
            required property int index

            implicitHeight: columnName.contentHeight + 5
            implicitWidth: columnName.contentWidth + 5

            MouseArea {
                id: columnMouseHandler
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onClicked: {
                    if (horizontalHeader.sortingColumn == index) {
                        horizontalHeader.sortingOrder = horizontalHeader.sortingOrder == Qt.DescendingOrder ? Qt.AscendingOrder : Qt.DescendingOrder
                    } else {
                        horizontalHeader.sortingColumn = index
                        horizontalHeader.sortingOrder = Qt.AscendingOrder
                    }
                    view.model.sort(horizontalHeader.sortingColumn, horizontalHeader.sortingOrder);
                }
            }

            Text {
                id: columnName

                text: display
                anchors.fill: parent
                anchors.leftMargin: 15
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                font.family: Theme.fontFamily
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.5
                font.pixelSize: 11
                font.weight: Font.Bold
                color: Theme.textMutedColor
            }

            Item {
                anchors {
                    left: parent.left
                    leftMargin: 5
                    top: parent.top
                    bottom: parent.bottom
                }
                Label {
                    id: sortIndicator

                    visible: horizontalHeader.sortingColumn == index

                    text: "▶"
                    rotation: horizontalHeader.sortingOrder == Qt.AscendingOrder ? 90 : -90

                    anchors.centerIn: parent
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                    font.family: Theme.fontFamily
                    font.capitalization: Font.AllUppercase
                    font.bold: true
                    font.pixelSize: Theme.buttonFontPixelSize
                    color: "red"
                }
            }
            Rectangle {
                id: columnResizer
                color: Theme.darkGray2
                width: 1
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    right: parent.right
                }
                MouseArea {
                    id: columnResizeHandler

                    property int sizeOffset: 0

                    anchors.fill: parent
                    preventStealing: true
                    drag {
                        target: parent
                        axis: Drag.XAxis
                        threshold: 2
                        onActiveChanged: {
                            if (!drag.active && columnResizeHandler.sizeOffset !== 0) {
                                view.model.columns[index].preferredWidth = column.width
                                columnResizeHandler.sizeOffset = 0
                                view.updateColumnSize()
                                view.forceLayout()
                            }
                        }
                    }
                    cursorShape: Qt.SizeHorCursor
                    onMouseXChanged: {
                        if (drag.active) {
                            column.width += mouseX
                            sizeOffset += mouseX
                        }
                    }
                }
            }
        }
    }

    TableView {
        id: view

        function loadSelectedTrackIntoNextAvailableDeck(play) {
            const urls = this.selectionModel.selectedTrackUrls();
            if (urls.length == 0)
                return ;

            Mixxx.PlayerManager.loadLocationUrlIntoNextAvailableDeck(urls[0], play);
        }

        function loadSelectedTrack(group, play) {
            const urls = this.selectionModel.selectedTrackUrls();
            if (urls.length == 0)
                return ;

            const player = Mixxx.PlayerManager.getPlayer(group);
            if (!player) {
                console.warn(`No player for group ${group}`);
                return;
            }
            player.loadTrackFromLocationUrl(urls[0], play);
        }

        // The controller cursor (e.g. the BROWSE encoder of a DJ controller) is
        // on the track list and not on the sources (see Control.qml): the
        // cells (Cell.qml) draw the selected row stronger, or dim
        readonly property bool controllerFocus: libraryControl.focusWidget == Skin.FocusedWidgetControl.WidgetKind.LibraryView

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AlwaysOn
        }

        anchors.top: horizontalHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 5
        clip: true
        reuseItems: true
        // Touch: scroll vertically only, and give the flick a moment to win
        // over the cells' mouse areas
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Qt.platform.os === "android" ? Flickable.VerticalFlick : Flickable.AutoFlickDirection
        pressDelay: Qt.platform.os === "android" ? 80 : 0
        Keys.onUpPressed: this.selectionModel.moveSelectionVertical(-1)
        Keys.onDownPressed: this.selectionModel.moveSelectionVertical(1)
        Keys.onEnterPressed: this.loadSelectedTrackIntoNextAvailableDeck(false)
        Keys.onReturnPressed: this.loadSelectedTrackIntoNextAvailableDeck(false)
        model: root.model
        // Columns of the interface profiles. The models keep their own copy of
        // the columns of SourceTree.qml, so the widths of the profile are
        // applied here, by position in that list (the Browse source, with
        // other columns, is left as it is). "uboros": every width is the one
        // of the source.
        readonly property var libraryColumnKinds: ["cover", "title", "artist", "album", "year", "bpm", "key", "stems", "filetype", "bitrate", "duration"]

        function preferredWidthOf(c) {
            const own = model.columns[c].preferredWidth;
            if (!(Theme.isClub || Theme.isStack) || model.columns.length !== libraryColumnKinds.length)
                return own;
            const kind = libraryColumnKinds[c];
            if (Theme.isClub) {
                // Cover, title, artist, BPM, key, duration (and the stems badge)
                if (kind === "cover")
                    return 24;
                if (kind === "duration")
                    return 52;
                if (kind === "album" || kind === "year" || kind === "filetype" || kind === "bitrate")
                    return 0;
            } else if (kind === "cover") {
                // Stack: dense rows, no room for a cover
                return 0;
            }
            return own;
        }

        function updateColumnSize() {
            usedWidth = 0;
            dynamicColumnCount = 0;
            if (model == null) {
                return;
            }
            for (let c = 0; c < model.columns.length; c++) {
                const preferredWidth = preferredWidthOf(c);
                if (model.columns[c].hidden || preferredWidth === 0) {
                    continue
                } else if (preferredWidth > 0) {
                    usedWidth += preferredWidth;
                } else {
                    dynamicColumnCount += model.columns[c].fillSpan || 1
                }
            }
        }
        Component.onCompleted: this.updateColumnSize()
        onModelChanged: this.updateColumnSize()

        // The interface profile changed: the widths of the columns and the
        // height of the rows are read again
        Connections {
            target: Theme

            function onProfileChanged() {
                view.updateColumnSize();
                view.forceLayout();
            }
        }

        property int usedWidth: 0
        property int dynamicColumnCount: 0

        // Club and Stack have their own row height ("uboros": the height of
        // the cells, as before)
        rowHeightProvider: function (row) {
            return (Theme.isClub || Theme.isStack) ? Theme.trackRowHeight : -1;
        }

        columnWidthProvider: function(column) {
            const columnDef = view.model.columns[column]
            if (columnDef.hidden) {
                return 0;
            }
            const preferredWidth = view.preferredWidthOf(column);
            if (preferredWidth >= 0) {
                return preferredWidth;
            }
            const span = columnDef.fillSpan || 1;
            return span * (view.width - view.usedWidth) / view.dynamicColumnCount;
        }

        selectionModel: ItemSelectionModel {
            function selectRow(row) {
                const rowCount = this.model.rowCount();
                if (rowCount == 0) {
                    this.clear();
                    return ;
                }
                const newRow = Mixxx.MathUtils.positiveModulo(row, rowCount);
                // Choosing a row (touch, or the controller already here) puts
                // the controller cursor on the list, not on the sources
                if (libraryControl.focusWidget == Skin.FocusedWidgetControl.WidgetKind.Sidebar)
                    libraryControl.focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
                this.select(this.model.index(newRow, 0), ItemSelectionModel.Rows | ItemSelectionModel.Select | ItemSelectionModel.Clear | ItemSelectionModel.Current);
                // Keep the selected track visible, e.g. when browsing with
                // the controller's browse knob
                view.positionViewAtRow(newRow, TableView.Contain);
            }

            function moveSelectionVertical(value) {
                if (value == 0)
                    return ;

                const selected = this.selectedIndexes;
                const oldRow = (selected.length == 0) ? 0 : selected[0].row;
                this.selectRow(oldRow + value);
            }

            function selectedTrackUrls() {
                return this.selectedIndexes.map((index) => {
                        return this.model.getUrl(index.row);
                });
            }
            model: view.model
        }

        delegate: Item {
            id: item
            required property bool selected
            required property color decoration
            required property var display
            required property var track
            required property string file_url
            required property url cover_art
            required property int row
            required property bool played
            required property bool has_stems

            implicitHeight: Qt.platform.os === "android" ? 22 : 30

            Loader {
                id: loader
                anchors.fill: parent
                property bool selected: item.selected
                property color decoration: item.decoration
                property var display: item.display
                property var track: item.track
                property url file_url: item.file_url
                property url cover_art: item.cover_art
                property int row: item.row
                property bool played: item.played
                property bool has_stems: item.has_stems
                property var tableView: view
                property var capabilities: root.model ? root.model.getCapabilities() : Mixxx.LibraryTrackListModel.Capability.None
                sourceComponent: delegate
                focus: true

                onLoaded: {
                // Workaround needed for WaveformOverview column to load the data
                //     if (track)
                //         Mixxx.Library.analyze(track)
                }
            }
            // Workaround needed for WaveformOverview column to load the data
            // TableView.onReused: {
            //     if (track)
            //         Mixxx.Library.analyze(track)
            // }
        }
    }

    // USB drive being read / prepared (Mixxx.Analysis.deviceBusy)
    Rectangle {
        id: deviceBusyBanner

        readonly property string busyText: Mixxx.Analysis.deviceBusy
        visible: busyText !== ""
        z: 50
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 36
        width: busyLabel.implicitWidth + 48
        height: busyLabel.implicitHeight + 18
        radius: height / 2
        color: "#e6141c24"
        border.width: 1
        border.color: Theme.accentColor

        Rectangle {
            id: busyDot

            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            width: 10
            height: 10
            radius: 5
            color: Theme.accentColor

            SequentialAnimation on opacity {
                running: deviceBusyBanner.visible
                loops: Animation.Infinite
                NumberAnimation { to: 0.2; duration: 500 }
                NumberAnimation { to: 1.0; duration: 500 }
            }
        }

        Text {
            id: busyLabel

            anchors.left: busyDot.right
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: deviceBusyBanner.busyText
            color: Theme.white
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize
            font.bold: true
        }
    }
}
