import "." as Skin
import QtQuick
import "Theme"

// What a list or a panel shows when there is nothing in it (or something went
// wrong): a round badge with a symbol, a title and one line that says what to
// do. `loading` swaps the symbol for a spinner. Nothing is drawn when hidden.
Item {
    id: root

    property bool loading: false
    // The symbol in the badge: "♪" nothing here, "?" no match, "!" a problem
    property string symbol: "♪"
    property string subtitle: ""
    property color tint: Theme.textMutedColor
    property string title: ""

    implicitHeight: column.implicitHeight
    implicitWidth: Math.min(360, column.implicitWidth)

    Column {
        id: column

        anchors.centerIn: parent
        spacing: 8
        width: Math.min(root.width - 24, 340)

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            border.color: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.45)
            border.width: 2
            color: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.1)
            height: 44
            radius: 22
            width: 44

            Text {
                anchors.centerIn: parent
                color: root.tint
                font.bold: true
                font.family: Theme.fontFamily
                font.pixelSize: 22
                text: root.symbol
                visible: !root.loading
            }
            Skin.Spinner {
                anchors.centerIn: parent
                height: 24
                running: root.loading
                tint: root.tint
                visible: root.loading
                width: 24
            }
        }
        Text {
            color: Theme.white
            elide: Text.ElideRight
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize + 1
            horizontalAlignment: Text.AlignHCenter
            text: root.title
            visible: text !== ""
            width: parent.width
        }
        Text {
            color: Theme.textMutedColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize - 1
            horizontalAlignment: Text.AlignHCenter
            maximumLineCount: 3
            elide: Text.ElideRight
            text: root.subtitle
            visible: text !== ""
            width: parent.width
            wrapMode: Text.WordWrap
        }
    }
}
