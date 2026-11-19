import QtQuick
import "../Theme"

// Search field of the track list. Only rectangles and text (no images, no
// effects, nothing that animates): a box with a magnifier made of two
// rectangles, a TextInput and a clear button.
//
// iconMode (Uboros): only the magnifier until it is tapped, then it grows
// into the field and goes back to the icon when it is left empty.
//
// The text is handed to the list after a short pause (searchRequested), once
// per change and not at every key. It never takes part in the controller
// navigation: the controller keeps working while the keyboard is open (see
// TrackList.qml, which closes the keyboard when the list is used).
Item {
    id: root

    // Only the magnifier until tapped
    property bool iconMode: false
    // The list can be filtered by text (Mixxx.LibraryTrackListModel.searchAvailable)
    property bool available: true
    // Width of the field when it is open; the owner sets the position
    property real expandedWidth: 160
    property real collapsedWidth: 28
    // Pause after the last key before the search is applied
    property int debounceMs: 250
    // What is typed in the field (read only; clear() and reset() change it)
    readonly property string text: input.text
    readonly property bool expanded: !iconMode || openedByTap || input.text.length > 0
    readonly property bool inputFocus: input.activeFocus
    // iconMode: the icon has been tapped and the field is open
    property bool openedByTap: false

    // The text to search for (after the pause, or at once with Enter)
    signal searchRequested(string text)
    // Enter / Done on the keyboard (or Escape): the list takes the focus back
    signal finished

    implicitHeight: 20
    width: expanded ? expandedWidth : collapsedWidth
    height: implicitHeight
    opacity: available ? 1 : 0.45

    function open() {
        if (!root.available)
            return;
        root.openedByTap = true;
        input.forceActiveFocus();
    }

    // Empty the field and show the whole list again
    function clear() {
        input.text = "";
        debounce.stop();
        root.searchRequested("");
        if (root.iconMode) {
            root.openedByTap = false;
            input.focus = false;
        }
    }

    // Another list has been opened (it starts without a filter): empty the
    // field without asking for a search
    function reset() {
        input.text = "";
        debounce.stop();
        root.openedByTap = false;
        input.focus = false;
    }

    Timer {
        id: debounce

        interval: root.debounceMs
        onTriggered: root.searchRequested(input.text)
    }

    // The icon alone (Uboros)
    MouseArea {
        anchors.fill: parent
        visible: !root.expanded
        enabled: !root.expanded
        onClicked: root.open()
    }

    Rectangle {
        id: box

        anchors.fill: parent
        radius: 3
        visible: root.expanded
        color: Theme.sunkenBackgroundColor
        border.width: 1
        border.color: input.activeFocus ? Theme.accentColor : Theme.buttonBorderColor

        // A tap anywhere on the box (also on the magnifier) opens the keyboard
        MouseArea {
            anchors.fill: parent
            enabled: root.available
            onClicked: input.forceActiveFocus()
        }
    }

    // Magnifier: a ring and a short handle
    Item {
        id: magnifier

        readonly property color tint: input.activeFocus ? Theme.textColor : Theme.textMutedColor

        x: root.expanded ? 6 : (root.width - width) / 2
        anchors.verticalCenter: parent.verticalCenter
        width: 14
        height: 14

        Rectangle {
            x: 1
            y: 1
            width: 9
            height: 9
            radius: 4.5
            color: "transparent"
            border.width: 1.5
            border.color: magnifier.tint
        }
        Rectangle {
            x: 9.5
            y: 8
            width: 2
            height: 5
            radius: 1
            rotation: -45
            color: magnifier.tint
        }
    }

    TextInput {
        id: input

        anchors.left: magnifier.right
        anchors.leftMargin: 4
        anchors.right: clearButton.left
        anchors.rightMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        visible: root.expanded
        enabled: root.available
        clip: true
        color: Theme.textColor
        selectionColor: Theme.accentColor
        selectedTextColor: Theme.white
        font.family: Theme.fontFamily
        font.pixelSize: root.height < 18 ? 11 : 12
        selectByMouse: true
        // No suggestion strip on top of the keyboard; the Enter key says "search"
        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        EnterKey.type: Qt.EnterKeySearch

        onTextChanged: debounce.restart()
        onAccepted: {
            debounce.stop();
            root.searchRequested(input.text);
            root.finished();
        }
        // Left empty: an icon field goes back to the icon
        onActiveFocusChanged: {
            if (!activeFocus && text.length === 0)
                root.openedByTap = false;
        }
        Keys.onEscapePressed: {
            root.clear();
            root.finished();
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: input.text.length === 0
            text: root.available ? qsTr("Search") : qsTr("Search n/a")
            color: Theme.textMutedColor
            font.family: Theme.fontFamily
            font.pixelSize: 12
        }
    }

    // Clear button: a cross
    Item {
        id: clearButton

        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        width: input.text.length > 0 && root.expanded ? 22 : 0
        height: parent.height
        visible: width > 0

        Text {
            anchors.centerIn: parent
            text: "×"
            color: Theme.textColor
            font.family: Theme.fontFamily
            font.pixelSize: 16
            font.bold: true
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.clear()
        }
    }
}
