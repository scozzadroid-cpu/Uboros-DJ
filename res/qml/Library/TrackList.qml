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
    // Rows of tracks on a drive that is not connected are dimmed (Library.isDriveMissing,
    // cached per drive); the counter makes the visible rows ask again when the list is on
    // screen (a drive mounted later lights its tracks up within a few seconds)
    property int driveEpoch: 0
    // Path of the source whose tracks are shown (set by Library.qml from
    // Browser.qml), e.g. "Rekordbox > Playlists > House"
    property string pathText: ""

    // Search field (SearchField.qml), on the line above the column headers
    // together with the path: Uboros a magnifier at the end of the line that
    // opens into a field; Club the field at the end of the line (the library
    // header); Stack a wide bar at the start of the line, always there, the
    // path after it. (Uboros: the line is no taller than it was before.)
    readonly property int searchRowHeight: Theme.isClub || Theme.isStack ? 20 : Math.max(16, Math.ceil(pathLabel.implicitHeight) + 2)
    readonly property bool searchOnLeft: Theme.isStack

    // What the list has been asked to search for (after the pause of the field)
    property string appliedSearch: ""

    // Every list starts without a filter: the field is emptied
    onModelChanged: {
        root.appliedSearch = "";
        searchField.reset();
    }

    // The keyboard of the phone is closed and the cursor goes back to the
    // list (typing is over: Enter/Done, a tap on a row, the BROWSE encoder).
    // Delayed: the key press that ends the typing is still being handled.
    function leaveSearch() {
        Qt.inputMethod.hide();
        Qt.callLater(() => {
            view.forceActiveFocus();
            libraryControl.focusWidget = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
        });
    }

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

    // The path line, as high as the search field that shares it
    Item {
        id: pathBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        anchors.topMargin: 3
        height: root.searchRowHeight

        Text {
            id: pathLabel

            anchors.left: parent.left
            anchors.right: parent.right
            // Room for the search field: at the end of the line, in Stack
            // at the start
            anchors.rightMargin: root.searchOnLeft ? 0 : searchField.width + 8
            anchors.leftMargin: root.searchOnLeft ? searchField.width + 10 : 10
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

    LibraryComponent.SearchField {
        id: searchField

        z: 2
        iconMode: !(Theme.isClub || Theme.isStack)
        available: root.model ? root.model.searchAvailable : false
        height: root.searchRowHeight
        expandedWidth: Theme.isStack ? Math.max(140, root.width * 0.5) : Math.min(190, Math.max(120, root.width * 0.38))
        // Uboros and Club: at the end of the line (the icon grows to the
        // left); Stack: at the start
        x: root.searchOnLeft ? 5 : root.width - width - 5
        y: 3

        onSearchRequested: (text) => {
            root.appliedSearch = text.trim();
            if (root.model)
                root.model.searchText = text;
        }
        onFinished: root.leaveSearch()
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
                // Same start as the text of the cells (SourceTree.qml), room
                // for the sort arrow at the end
                anchors.leftMargin: Qt.platform.os === "android" ? 6 : 15
                anchors.rightMargin: sortIndicator.visible ? 14 : 2
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                font.family: Theme.fontFamily
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.5
                font.pixelSize: 11
                font.weight: Font.Bold
                // The sorted column is brighter
                color: horizontalHeader.sortingColumn == index ? Theme.white : Theme.textMutedColor
            }

            Item {
                anchors {
                    right: parent.right
                    rightMargin: 9
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
                    // The sorted column: accent color (red read as an error)
                    color: Theme.accentColor
                }
            }
            // Line under the headers
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                color: Theme.buttonBorderColor
                height: 1
            }
            Rectangle {
                id: columnResizer
                color: Theme.dividerColor
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

        // A plugin may get the selected track in the background: told once
        // the selection rests (the browse knob passes many rows)
        Timer {
            id: selectionRest

            interval: 700
            onTriggered: {
                const urls = view.selectionModel ? view.selectionModel.selectedTrackUrls() : [];
                if (urls.length > 0)
                    Mixxx.Plugins.trackSelected(urls[0]);
            }
        }

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
                // Title, artist, BPM, key, duration (and the stems badge). No
                // cover in the list (it made scrolling stutter): decision 19/11
                if (kind === "duration")
                    return 52;
                if (kind === "cover" || kind === "album" || kind === "year" || kind === "filetype" || kind === "bitrate")
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
            onSelectionChanged: selectionRest.restart()

            function selectRow(row) {
                const rowCount = this.model.rowCount();
                if (rowCount == 0) {
                    this.clear();
                    return ;
                }
                const newRow = Mixxx.MathUtils.positiveModulo(row, rowCount);
                // A row chosen (touch or controller) ends the typing: the
                // keyboard closes, the controller keeps working on the list
                if (searchField.inputFocus)
                    root.leaveSearch();
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

        // What the cells of every column need, read for each cell
        // (required properties). The roles only some columns use (track,
        // cover_art, has_stems, decoration) are read through cellData by the
        // cells that show them: a required property is fetched from the model
        // for every cell, and the track role loads the whole track.
        readonly property var trackCapabilities: model ? model.getCapabilities() : Mixxx.LibraryTrackListModel.Capability.None

        delegate: Item {
            id: item
            required property bool selected
            required property var display
            required property string file_url
            required property int row
            required property bool played
            // The model item: its roles are read from the model when a
            // binding uses them (cellData.track, cellData.cover_art...)
            required property var model
            // On a removable drive that is not mounted now: the row says so, loading it fails
            readonly property bool driveMissing: Mixxx.Library.isDriveMissing(file_url, root.driveEpoch)

            implicitHeight: Qt.platform.os === "android" ? 22 : 30

            Loader {
                id: loader
                anchors.fill: parent
                property bool selected: item.selected
                property var display: item.display
                property url file_url: item.file_url
                property int row: item.row
                property bool played: item.played
                property var cellData: item.model
                property var tableView: view
                property var capabilities: view.trackCapabilities
                sourceComponent: delegate
                focus: true

                onLoaded: {
                // Workaround needed for WaveformOverview column to load the data
                //     if (track)
                //         Mixxx.Library.analyze(track)
                }
            }
            // A drag image made for the previous row is dropped (Cell.qml)
            TableView.onPooled: {
                if (loader.item && loader.item.dragImageWanted)
                    loader.item.dragImageWanted = false;
            }
            // Workaround needed for WaveformOverview column to load the data
            // TableView.onReused: {
            //     if (track)
            //         Mixxx.Library.analyze(track)
            // }

            // The drive of this track is not connected: veil and a mark at the start of the row
            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(Theme.darkGray.r, Theme.darkGray.g, Theme.darkGray.b, 0.62)
                visible: item.driveMissing
                z: 1
            }
            Rectangle {
                color: Theme.yellow
                height: parent.height
                visible: item.driveMissing
                width: 3
                z: 2
            }
        }
    }

    Timer {
        interval: 5000
        repeat: true
        running: root.visible
        onTriggered: root.driveEpoch++
    }

    // A list with nothing in it says why and what to do, instead of staying blank:
    // a search (or Matching) that finds nothing, an empty playlist, a drive being read
    Skin.EmptyState {
        id: emptyState

        readonly property bool empty: view.rows === 0
        readonly property bool matching: root.pathText.indexOf(qsTr("Matching")) === 0
        readonly property bool searching: root.appliedSearch !== ""
        // Shown once the list has stayed empty for a moment: a playlist that is
        // loading is empty for a few frames and must not flash this message
        property bool settled: false

        anchors.fill: view
        anchors.topMargin: 24
        loading: Mixxx.Analysis.deviceBusy !== ""
        subtitle: loading ? "" : (searching ? qsTr("Try a shorter word, or clear the search.") : (matching ? "" : qsTr("Choose a playlist or a drive in the sources on the left.")))
        symbol: searching || matching ? "?" : "♪"
        tint: loading ? Theme.accentColor : Theme.textMutedColor
        title: loading ? Mixxx.Analysis.deviceBusy : (searching ? qsTr("No tracks match \"%1\"").arg(root.appliedSearch) : (matching ? qsTr("No tracks match the track on the deck") : qsTr("No tracks here")))
        visible: empty && (settled || loading)

        onEmptyChanged: {
            if (!empty)
                settled = false;
        }

        Timer {
            interval: 450
            running: emptyState.empty && !emptyState.settled
            onTriggered: emptyState.settled = true
        }
    }

    // USB drive being read / prepared (Mixxx.Analysis.deviceBusy)
    Rectangle {
        id: deviceBusyBanner

        readonly property string busyText: Mixxx.Analysis.deviceBusy
        // With an empty list the empty state already says it
        visible: busyText !== "" && !(emptyState.visible && emptyState.loading)
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

        Skin.Spinner {
            id: busyDot

            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            height: 16
            running: deviceBusyBanner.visible
            width: 16
        }

        Text {
            id: busyLabel

            anchors.left: busyDot.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: deviceBusyBanner.busyText
            color: Theme.white
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize
            font.bold: true
        }
    }
}
