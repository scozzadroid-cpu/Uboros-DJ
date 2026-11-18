import "." as Skin
import Mixxx 1.0 as Mixxx
import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme"

// Library export for rekordbox (XML collection + optional audio copy).
// Pioneer players read the USB stick that rekordbox writes from this XML.
Popup {
    id: root

    function defaultFolder() {
        if (Qt.platform.os === "android")
            return "/storage/emulated/0/Download/UborosExport";
        const docs = StandardPaths.writableLocation(StandardPaths.DocumentsLocation).toString();
        return docs.replace(/^file:\/\/(\/(?=[A-Za-z]:))?/, "") + "/UborosExport";
    }

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    modal: true
    padding: 16

    onAboutToShow: {
        setChooser.model = ["(whole library)"].concat(Mixxx.RekordboxExport.setNames());
        if (folderField.text === "")
            folderField.text = defaultFolder();
    }

    background: Rectangle {
        border.color: Theme.deckLineColor
        color: Theme.darkGray2
        radius: 10
    }

    contentItem: ColumnLayout {
        spacing: 10

        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.bold: true
            font.pixelSize: 18
            text: "Export library (rekordbox)"
        }
        Text {
            Layout.fillWidth: true
            color: Theme.textMutedColor
            font.pixelSize: 12
            text: "Writes rekordbox.xml with tracks, beat grid, hot cues, loops, crates and playlists. In rekordbox: File > Library > Import, then export to the USB stick for CDJ/XDJ."
            wrapMode: Text.WordWrap
        }
        Text {
            color: Theme.white
            text: "What to export"
        }
        Skin.ComboBox {
            id: setChooser
            Layout.fillWidth: true
            model: ["(whole library)"]
        }
        Text {
            color: Theme.white
            text: "Folder (a USB stick path works too)"
        }
        TextField {
            id: folderField
            Layout.fillWidth: true
            color: Theme.white
            selectByMouse: true
            background: Skin.EmbeddedBackground {
            }
        }
        Skin.Button {
            id: copyBox
            activeColor: Theme.green
            checkable: true
            checked: true
            text: "Copy the audio files too (Contents folder)"
        }
        Text {
            Layout.fillWidth: true
            color: "#a6e36b"
            text: Mixxx.RekordboxExport.status
            visible: text !== ""
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }
            Skin.Button {
                activeColor: Theme.white
                text: "Close"

                onClicked: root.close()
            }
            Skin.Button {
                activeColor: Theme.green
                enabled: !Mixxx.RekordboxExport.busy
                text: Mixxx.RekordboxExport.busy ? "Exporting..." : "Export"

                onClicked: Mixxx.RekordboxExport.exportXml(folderField.text,
                        copyBox.checked,
                        setChooser.currentIndex > 0 ? setChooser.currentText : "")
            }
        }
    }
}
