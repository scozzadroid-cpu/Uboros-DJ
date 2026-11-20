import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Layouts
import "Theme"

// "Update X ready: Install now / Later" (over-the-air update). Not modal.
// Installing stops the audio and closes the app: the button is off while a
// deck plays or in performance mode, and it always needs this tap.
Rectangle {
    id: root

    // The version the user answered "Later" to
    property int dismissedCode: 0
    // English by default, Italian on an Italian system (as the dashboard)
    readonly property bool italian: Qt.locale().name.startsWith("it")
    readonly property bool shown: Mixxx.AppUpdater.supported && Mixxx.AppUpdater.state === "ready" && Mixxx.AppUpdater.availableCode !== dismissedCode
    readonly property string blockedBy: Mixxx.AppUpdater.installBlockedBy

    color: Theme.cardColor
    border.color: Qt.rgba(Theme.accentColor.r, Theme.accentColor.g, Theme.accentColor.b, 0.55)
    border.width: 1
    radius: Theme.radiusLarge
    implicitHeight: row.implicitHeight + 24
    // Fades and slides in and out instead of popping up
    opacity: shown ? 1 : 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation {
            duration: 220
            easing.type: Easing.OutQuad
        }
    }
    transform: Translate {
        y: root.shown ? 0 : -14

        Behavior on y {
            NumberAnimation {
                duration: 220
                easing.type: Easing.OutQuad
            }
        }
    }

    // The color stripe on the left says what kind of message this is
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.top: parent.top
        anchors.topMargin: 10
        color: Theme.accentColor
        radius: 2
        width: 4
    }

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.margins: 12
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                color: Theme.white
                font.bold: true
                font.pixelSize: 15
                text: root.italian ? "Aggiornamento %1 pronto".arg(Mixxx.AppUpdater.availableVersion) : qsTr("Update %1 ready").arg(Mixxx.AppUpdater.availableVersion)
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: Theme.lightGray2
                font.pixelSize: 13
                maximumLineCount: 3
                elide: Text.ElideRight
                text: Mixxx.AppUpdater.notes
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: Theme.yellow
                font.pixelSize: 13
                text: root.blockedBy === "playing" ? (root.italian ? "Un deck sta suonando: l'installazione ferma l'audio e chiude l'app" : qsTr("A deck is playing: installing stops the audio and closes the app")) : (root.italian ? "Modalità prestazioni accesa: spegnila per installare" : qsTr("Performance mode is on: turn it off to install"))
                visible: root.blockedBy !== ""
                wrapMode: Text.WordWrap
            }
        }
        Skin.BannerButton {
            enabled: root.blockedBy === ""
            primary: true
            text: root.italian ? "Installa ora" : qsTr("Install now")
            tint: Theme.accentColor

            onClicked: Mixxx.AppUpdater.install()
        }
        Skin.BannerButton {
            text: root.italian ? "Più tardi" : qsTr("Later")
            tint: Theme.accentColor

            onClicked: root.dismissedCode = Mixxx.AppUpdater.availableCode
        }
    }
}
