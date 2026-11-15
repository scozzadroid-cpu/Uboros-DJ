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
        root.model.activate(root.model.index(0, 0))
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

    function currentViewRow() {
        const current = featureSelection.currentIndex;
        return current && current.valid ? featureView.rowAtIndex(current) : -1;
    }

    function moveSelection(offset) {
        const rowCount = featureView.rows;
        if (rowCount <= 0 || offset == 0)
            return;
        let row = root.currentViewRow();
        row = row < 0 ? 0 : Math.max(0, Math.min(rowCount - 1, row + offset));
        const index = featureView.index(row, 0);
        featureSelection.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows);
        featureView.positionViewAtRow(row, TableView.Contain);
    }

    function openSelection() {
        const row = root.currentViewRow();
        if (row < 0)
            return;
        const index = featureView.index(row, 0);
        // Read before activating: activating may list the children lazily
        const wasExpanded = featureView.isExpanded(row);
        root.model.activate(index);
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

                    // Some sources find their children asynchronously after
                    // being activated (e.g. Rekordbox USB drives): show them
                    // as soon as they are known.
                    Connections {
                        target: root.model
                        function onRowsInserted(parent, first, last) {
                            // Top-level sources, and the source just opened
                            // with the controller (e.g. a USB drive's playlists)
                            if (!parent.valid)
                                return;
                            const isOpened = root.currentViewRow() >= 0 && featureView.rowAtIndex(parent) === root.currentViewRow();
                            if (parent.parent.valid && !isOpened)
                                return;
                            Qt.callLater(() => {
                                const viewRow = featureView.rowAtIndex(parent);
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
                        readonly property var index: typeof treeView.index === "function" ? treeView.index(row, column) : treeView.modelIndex(column, row)

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
                                onClicked: (event) => {
                                    // Read the state before activating: the activation may
                                    // populate the children (folders are listed lazily)
                                    const wasExpanded = expanded;
                                    const viewRow = row;
                                    treeView.selectionModel.setCurrentIndex(index, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows);
                                    treeView.model.activate(index)
                                    if (wasExpanded) {
                                        treeView.collapse(viewRow)
                                    } else {
                                        treeView.expand(viewRow)
                                    }
                                    event.accepted = true
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
