import ".." as Skin
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Shapes 1.12
import Qt5Compat.GraphicalEffects
import "../Theme"

Rectangle {
    id: root

    required property var model
    readonly property var featureSelection: ItemSelectionModel {}

    color: Theme.backgroundColor

    Component.onCompleted: {
        root.showSource(root.model.index(0, 0))
    }

    // Path of the source whose tracks are shown, e.g. "Rekordbox > Playlists
    // > House" (label above the track list, so "back" is predictable)
    property string shownPath: ""

    function pathOf(index) {
        const names = [];
        for (let i = index; i && i.valid; i = i.parent)
            names.unshift(root.model.data(i, Qt.DisplayRole));
        return names.join(" › ");
    }

    // Shows the tracks of a source (every place that activates one)
    function showSource(index) {
        root.shownPath = root.pathOf(index);
        root.model.activate(index);
    }

    // Controller navigation (e.g. the BROWSE encoder of a DJ controller),
    // like a CDJ: when [Library],focused_widget is the sidebar, turning only
    // moves the cursor (nothing is opened on the way). Pressing opens the
    // source under the cursor: a closed source with children (Rekordbox, a USB
    // drive, a folder) is shown and expanded and the cursor stays here; a
    // source without children, or one already open, is shown and the focus
    // goes to the track list. Pressing in the track list comes back here
    // (Control.qml).
    Skin.FocusedWidgetControl {
        id: focusedWidget
    }

    readonly property bool controllerFocus: focusedWidget.value == Skin.FocusedWidgetControl.WidgetKind.Sidebar

    // Top-level entries that only appear when there is something inside
    // (a source with no playlists, no crates, no artists or no folders is
    // just a dead button). The labels are the ones of SourceTree.qml.
    // History is empty until a track has been played (its sessions are listed
    // only when they have tracks).
    readonly property var hideWhenEmpty: [qsTr("Playlists"), qsTr("Crates"), qsTr("Artists"), qsTr("Folders"), qsTr("History")]
    // Only in the Club profile (Matching: tracks that fit the deck, like the
    // related tracks of rekordbox)
    readonly property string clubOnlyLabel: qsTr("Matching")

    function isRowHidden(row) {
        if (row < 0 || row >= featureView.rows)
            return false;
        const index = featureView.index(row, 0);
        if (!index || !index.valid || index.parent.valid)
            return false;
        const label = root.model.data(index, Qt.DisplayRole);
        if (label === root.clubOnlyLabel)
            return !Theme.isClub;
        return root.hideWhenEmpty.indexOf(label) >= 0 && !root.model.hasChildren(index);
    }

    function currentViewRow() {
        const current = featureSelection.currentIndex;
        return current && current.valid ? featureView.rowAtIndex(current) : -1;
    }

    function moveSelection(offset) {
        const rowCount = featureView.rows;
        if (rowCount <= 0 || offset == 0)
            return;
        // The encoder is in use: the keyboard of the search field is not
        // needed (the field keeps its text)
        Qt.inputMethod.hide();
        let row = root.currentViewRow();
        if (row < 0) {
            row = 0;
        } else {
            // Step over the hidden (empty) entries
            const step = offset > 0 ? 1 : -1;
            for (let left = Math.abs(offset); left > 0; --left) {
                let next = row + step;
                while (next >= 0 && next < rowCount && root.isRowHidden(next))
                    next += step;
                if (next < 0 || next >= rowCount)
                    break;
                row = next;
            }
        }
        const index = featureView.index(row, 0);
        featureSelection.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows);
        featureView.positionViewAtRow(row, TableView.Contain);
    }

    function openSelection() {
        const row = root.currentViewRow();
        if (row < 0)
            return;
        Qt.inputMethod.hide();
        const index = featureView.index(row, 0);
        // Read before activating: activating may list the children lazily
        const wasExpanded = featureView.isExpanded(row);
        root.showSource(index);
        if (wasExpanded) {
            focusedWidget.value = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
            return;
        }
        // Some sources (Rekordbox, USB drives, folders) only know their
        // children once activated: decide after they had time to appear.
        root.openedRow = row;
        openedCheck.restart();
    }

    property int openedRow: -1
    // A tap on an open node closes it: no automatic re-expansion until then
    property double noAutoExpandUntil: 0

    Timer {
        id: openedCheck
        interval: 250
        onTriggered: {
            const row = root.openedRow;
            root.openedRow = -1;
            if (row < 0 || row !== root.currentViewRow())
                return;
            if (root.model.hasChildren(featureView.index(row, 0))) {
                if (!featureView.isExpanded(row))
                    featureView.expand(row);
            } else {
                focusedWidget.value = Skin.FocusedWidgetControl.WidgetKind.LibraryView;
            }
        }
    }

    // Back (SHIFT + BROWSE press on the FLX4 = [Library],MoveLeft) while the
    // cursor is on the sources: an open source is closed first (like a tap on
    // it), then the cursor goes up to the parent source. Only the cursor
    // moves; nothing is opened on the way. In the track list the same control
    // brings the cursor here instead (Control.qml).
    function back() {
        const row = root.currentViewRow();
        if (row < 0)
            return;
        // A source the controller has just pressed must not open afterwards
        openedCheck.stop();
        root.openedRow = -1;
        if (featureView.isExpanded(row)) {
            // No automatic re-expansion until the close is done
            root.noAutoExpandUntil = Date.now() + 800;
            featureView.collapse(row);
            return;
        }
        const parentIndex = featureSelection.currentIndex.parent;
        if (!parentIndex || !parentIndex.valid)
            return;
        const parentRow = featureView.rowAtIndex(parentIndex);
        if (parentRow < 0)
            return;
        featureSelection.setCurrentIndex(parentIndex, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows);
        featureView.positionViewAtRow(parentRow, TableView.Contain);
    }

    onControllerFocusChanged: {
        // Start from the source whose tracks are shown
        if (root.controllerFocus && root.currentViewRow() < 0) {
            featureSelection.setCurrentIndex(featureView.index(0, 0), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows);
        }
    }

    // The same press also reaches Control.qml (track list -> sidebar): both
    // read the focus as it was before the press and change it afterwards, so
    // one press never does both.
    function onPress() {
        if (root.controllerFocus)
            Qt.callLater(root.openSelection);
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveFocusForward"
        onValueChanged: (value) => {
            if (value != 0)
                root.onPress();
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveFocusBackward"
        onValueChanged: (value) => {
            if (value != 0)
                root.onPress();
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveFocus"
        onValueChanged: (value) => {
            if (value != 0)
                root.onPress();
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveLeft"
        onValueChanged: (value) => {
            if (value != 0 && root.controllerFocus)
                root.back();
        }
    }

    // The other way round: like pressing, enters the source under the cursor
    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveRight"
        onValueChanged: (value) => {
            if (value != 0 && root.controllerFocus)
                Qt.callLater(root.openSelection);
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveVertical"
        onValueChanged: (value) => {
            if (value != 0 && root.controllerFocus)
                root.moveSelection(value);
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveUp"
        onValueChanged: (value) => {
            if (value != 0 && root.controllerFocus)
                root.moveSelection(-1);
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "MoveDown"
        onValueChanged: (value) => {
            if (value != 0 && root.controllerFocus)
                root.moveSelection(1);
        }
    }

    // Any LOAD button of the controller acts as Enter while the cursor is on
    // the sources (the track list handles them itself, see Control.qml)
    Instantiator {
        model: 8
        delegate: Mixxx.ControlProxy {
            required property int index
            group: "[Channel" + (Math.floor(index / 2) + 1) + "]"
            key: index % 2 == 0 ? "LoadSelectedTrack" : "LoadSelectedTrackAndPlay"
            onValueChanged: (value) => {
                if (value != 0 && root.controllerFocus)
                    Qt.callLater(root.openSelection);
            }
        }
    }

    Mixxx.ControlProxy {
        group: "[Library]"
        key: "GoToItem"
        onValueChanged: (value) => {
            if (value != 0 && root.controllerFocus)
                Qt.callLater(root.openSelection);
        }
    }

    // Shows where the controller cursor is
    Rectangle {
        z: 1
        anchors.fill: parent
        color: "transparent"
        border.width: 2
        border.color: Theme.white
        visible: root.controllerFocus
    }

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: Qt.platform.os === "android" ? 4 : 7
        anchors.leftMargin: Qt.platform.os === "android" ? 4 : 7
        anchors.rightMargin: Qt.platform.os === "android" ? 6 : 25
        anchors.bottomMargin: Qt.platform.os === "android" ? 40 : 7
        radius: 6
        color: Theme.sunkenBackgroundColor

        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            ScrollView {
                Layout.fillHeight: true
                Layout.fillWidth: true

                TreeView {
                    id: featureView
                    Layout.fillWidth: true

                    clip: true

                    model: root.model

                    selectionModel: featureSelection

                    // Empty entries (see hideWhenEmpty) take no room
                    rowHeightProvider: function (row) {
                        return root.isRowHidden(row) ? 0 : -1;
                    }

                    // The profile decides whether Matching is listed
                    Connections {
                        target: Theme
                        function onProfileChanged() {
                            Qt.callLater(featureView.forceLayout);
                        }
                    }

                    // Expanding/collapsing a node shifts the view rows without
                    // any signal from the model: recompute the hidden rows
                    Connections {
                        target: featureView
                        function onExpanded() {
                            Qt.callLater(featureView.forceLayout);
                        }
                        function onCollapsed() {
                            Qt.callLater(featureView.forceLayout);
                        }
                    }

                    // Some sources find their children asynchronously after
                    // being activated (e.g. Rekordbox USB drives): show them
                    // as soon as they are known.
                    Connections {
                        target: root.model
                        function onRowsRemoved() {
                            Qt.callLater(featureView.forceLayout);
                        }
                        function onModelReset() {
                            Qt.callLater(featureView.forceLayout);
                        }
                        function onRowsInserted(parent, first, last) {
                            // An entry may have got its first item
                            Qt.callLater(featureView.forceLayout);
                            // Top-level sources, and the source just opened
                            // with the controller (e.g. a USB drive's playlists)
                            if (!parent.valid)
                                return;
                            const isOpened = root.currentViewRow() >= 0 && featureView.rowAtIndex(parent) === root.currentViewRow();
                            if (parent.parent.valid && !isOpened)
                                return;
                            // Artists, Folders...: they read their groups on their own
                            // when the library changes; stay closed until opened
                            if (!parent.parent.valid && !isOpened && root.hideWhenEmpty.indexOf(root.model.data(parent, Qt.DisplayRole)) >= 0)
                                return;
                            Qt.callLater(() => {
                                const viewRow = featureView.rowAtIndex(parent);
                                if (Date.now() < root.noAutoExpandUntil)
                                    return;
                                if (viewRow >= 0 && !featureView.isExpanded(viewRow))
                                    featureView.expand(viewRow);
                            });
                        }
                    }

                    delegate: FocusScope {
                        required property string label
                        required property var icon

                        readonly property real indentation: 40
                        readonly property real padding: 5

                        // Assigned to by TreeView:
                        required property TreeView treeView
                        required property bool isTreeNode
                        required property bool expanded
                        required property int hasChildren
                        required property int depth
                        required property int row
                        required property int column
                        required property bool current
                        // FIXME The signature of modelIndex() has changed after Qt 6.4.2 (currently shipped on Ubuntu 24.04)
                        // See https://github.com/mixxxdj/mixxx/pull/14514#issuecomment-2770811094 for further details
                        // Since Qt 6.4.3 it is modelIndex(row, column); index(row, column) only exists there.
                        // A function, not a property binding: when rows are
                        // inserted above (a USB drive found under "Rekordbox"),
                        // TreeView reuses the delegate at the same row number
                        // for another item, so a binding on row/column would
                        // not re-evaluate and kept the old item's index (a tap
                        // on the drive activated the "Computer" row below it).
                        function currentModelIndex() {
                            return typeof treeView.index === "function" ? treeView.index(row, column) : treeView.modelIndex(column, row);
                        }

                        implicitWidth: treeView.width
                        implicitHeight: Qt.platform.os === "android" ? (depth == 0 ? 30 : 26) : (depth == 0 ? 42 : 35)

                        // The indicator shows ▶/▼ from the expanded state

                        Rectangle {
                            id: background
                            anchors.fill: parent
                            color: depth == 0 ? Theme.darkGray3 : 'transparent'

                            MouseArea {
                                id: rowMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                // Tap: open/close the node (a source without
                                // children is just shown). Long press: select the
                                // source and show its tracks on the right.
                                onClicked: (event) => {
                                    const viewRow = row;
                                    const index = currentModelIndex();
                                    const isOpen = treeView.isExpanded(viewRow);
                                    treeView.selectionModel.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows);
                                    if (isOpen) {
                                        root.noAutoExpandUntil = Date.now() + 800;
                                        treeView.collapse(viewRow);
                                    } else {
                                        // Lazy sources (folders, drives) only list
                                        // their children once activated
                                        if (!treeView.model.hasChildren(index) || !hasChildren)
                                            root.showSource(index);
                                        if (treeView.model.hasChildren(index))
                                            treeView.expand(viewRow);
                                        else
                                            Qt.callLater(() => {
                                                const r = featureView.rowAtIndex(index);
                                                if (r >= 0 && treeView.model.hasChildren(index) && !treeView.isExpanded(r))
                                                    treeView.expand(r);
                                            });
                                    }
                                    event.accepted = true
                                }
                                onPressAndHold: (event) => {
                                    const index = currentModelIndex();
                                    treeView.selectionModel.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows);
                                    root.showSource(index);
                                }
                            }

                            Rectangle {
                                id: rowHighlight

                                anchors.fill: parent
                                anchors.leftMargin: 3
                                anchors.rightMargin: 3
                                anchors.topMargin: 1
                                anchors.bottomMargin: 1
                                radius: 5
                                color: current ? Qt.rgba(Theme.accentColor.r, Theme.accentColor.g, Theme.accentColor.b, 0.35) : 'transparent'
                                border.width: current ? 1 : 0
                                border.color: Theme.accentColor

                                Row {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 6 + 14 * depth
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 5

                                    Label {
                                        id: indicator
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 10
                                        opacity: isTreeNode && hasChildren ? 1 : 0
                                        color: Theme.textMutedColor
                                        // A chevron: the triangles are color emoji on
                                        // Android or missing from the font
                                        text: "\u203A"
                                        font.bold: true
                                        font.pixelSize: 16
                                        horizontalAlignment: Text.AlignHCenter
                                        rotation: expanded ? 90 : 0
                                    }

                                    Image {
                                        anchors.verticalCenter: parent.verticalCenter
                                        visible: depth == 0 && !!icon
                                        // The model gives the feature icon name (e.g. "rekordbox")
                                        source: depth == 0 && !!icon ? "../../images/library/ic_library_" + icon + ".svg" : ""
                                        height: Qt.platform.os === "android" ? 18 : 22
                                        width: height
                                        sourceSize.width: 44
                                        sourceSize.height: 44
                                    }

                                    Label {
                                        id: labelItem
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: parent.width - x - 6
                                        elide: Text.ElideRight
                                        font.weight: depth == 0 ? Font.Bold : Font.Medium
                                        font.pixelSize: Qt.platform.os === "android" ? 12 : 14
                                        text: label
                                        color: current ? Theme.white : (depth == 0 ? Theme.textColor : Theme.textMutedColor)
                                    }
                                }

                                Item {
                                    visible: rowMouseArea.containsMouse && isTreeNode && hasChildren
                                    id: newItem
                                    height: parent.height
                                    anchors {
                                        verticalCenter: parent.verticalCenter
                                        right: parent.right
                                        rightMargin: 10
                                    }
                                    Rectangle {
                                        width: 30
                                        height: parent.height
                                        anchors.centerIn: parent
                                        gradient: Gradient {
                                            orientation: Gradient.Horizontal

                                            GradientStop {
                                                position: 1
                                                color: Theme.sunkenBackgroundColor
                                            }

                                            GradientStop {
                                                position: 0
                                                color: 'transparent'
                                            }
                                        }
                                    }
                                    Rectangle {
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.rightMargin: 5
                                        width: 20
                                        height: 20
                                        border.width: 2
                                        border.color: Theme.white
                                        radius: 20
                                        color: 'transparent'
                                        Shape {
                                            anchors.fill: parent
                                            anchors.margins: 4
                                            ShapePath {
                                                strokeWidth: 2
                                                fillColor: Theme.white
                                                capStyle: ShapePath.RoundCap

                                                startX: 6
                                                startY: 0
                                                PathLine { x: 6; y: 12 }
                                                PathLine { x: 6; y: 8 }
                                            }
                                            ShapePath {
                                                strokeWidth: 2
                                                fillColor: Theme.white
                                                capStyle: ShapePath.RoundCap

                                                startX: 0
                                                startY: 6
                                                PathLine { y: 6; x: 12 }
                                                PathLine { y: 6; x: 8 }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Android: USB drives that Android does not mount (e.g. FAT32 without a
    // partition table) are imported to the internal storage, without root
    Skin.Button {
        visible: Qt.platform.os === "android"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 4
        height: 32
        activeColor: Theme.white
        normalColor: Theme.white
        text: qsTr("Import USB drive")
        onClicked: Mixxx.Library.importUsbDrives()
    }
}
