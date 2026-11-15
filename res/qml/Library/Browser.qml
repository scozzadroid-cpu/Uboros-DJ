import ".." as Skin
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
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

    // Controller navigation (e.g. the BROWSE encoder of a DJ controller):
    // when [Library],focused_widget is the sidebar, the library move controls
    // move the selection here. The source under the cursor is shown shortly
    // after the cursor stops, and when the focus goes back to the track list.
    Skin.FocusedWidgetControl {
        id: focusedWidget
    }

    readonly property bool controllerFocus: focusedWidget.value == Skin.FocusedWidgetControl.WidgetKind.Sidebar
    property bool activationPending: false

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
        root.activationPending = true;
        activationTimer.restart();
    }

    function activateSelection() {
        activationTimer.stop();
        if (!root.activationPending)
            return;
        root.activationPending = false;
        const row = root.currentViewRow();
        if (row < 0)
            return;
        root.model.activate(featureView.index(row, 0));
        // Show the children (sub-folders, playlists, USB drives...)
        if (!featureView.isExpanded(row))
            featureView.expand(row);
    }

    Timer {
        id: activationTimer
        interval: 500
        onTriggered: root.activateSelection()
    }

    onControllerFocusChanged: {
        if (root.controllerFocus) {
            // Start from the source whose tracks are shown
            if (root.currentViewRow() < 0) {
                featureSelection.setCurrentIndex(featureView.index(0, 0), ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows);
            }
        } else {
            root.activateSelection();
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
            if (value != 0 && root.controllerFocus) {
                root.activationPending = true;
                root.activateSelection();
            }
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
        anchors.topMargin: 7
        anchors.leftMargin: 7
        anchors.rightMargin: 25
        anchors.bottomMargin: 40
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
                            if (!parent.valid || parent.parent.valid)
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
                        implicitHeight: depth == 0 ? 42 : 35

                        // Rotate indicator when expanded by the user
                        // (requires TreeView to have a selectionModel)
                        property Animation indicatorAnimation: NumberAnimation {
                            target: indicator
                            property: "rotation"
                            from: expanded ? 0 : 90
                            to: expanded ? 90 : 0
                            duration: 100
                            easing.type: Easing.OutQuart
                        }
                        TableView.onPooled: indicatorAnimation.complete()
                        TableView.onReused: if (current) indicatorAnimation.start()
                        onExpandedChanged: indicator.rotation = expanded ? 90 : 0

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
                                width: 25
                                anchors.left: parent.left
                                anchors.leftMargin: 10 + 15 * depth
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.right: parent.right
                                color: current ? Theme.midGray : 'transparent'

                                Repeater {
                                    id: lineIcon
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    // The model gives the feature icon name (e.g. "rekordbox")
                                    model: depth == 0 && !!icon ? 1 : 0
                                    Image {
                                        source: "../../images/library/ic_library_" + icon + ".svg"
                                        height: 25
                                        width: 25
                                    }
                                }

                                Label {
                                    id: indicator
                                    Layout.preferredWidth: indicator.implicitWidth
                                    visible: isTreeNode && hasChildren
                                    color: Theme.textColor
                                    text: "▶"

                                    anchors {
                                        left: parent.left
                                        verticalCenter: lineIcon.bottom
                                    }
                                }

                                Label {
                                    id: labelItem
                                    anchors.left: parent.left
                                    anchors.leftMargin: depth == 0 && row == 0 ? 10 : 34
                                    anchors.verticalCenter: parent.verticalCenter
                                    clip: true
                                    font.weight: depth == 0 ? Font.Bold : Font.Medium
                                    font.pixelSize: 14
                                    text: label
                                    color: Theme.textColor
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
    Button {
        visible: Qt.platform.os === "android"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 6
        height: 30
        text: qsTr("Import USB drive")
        onClicked: Mixxx.Library.importUsbDrives()
    }
}
