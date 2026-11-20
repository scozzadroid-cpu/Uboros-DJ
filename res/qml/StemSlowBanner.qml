import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Layouts
import "Theme"

// Shown when the stem separation does not keep up on this phone (or makes
// the audio drop out): proposes the light quality. Not modal: the DJ can go
// on playing and answer later.
Rectangle {
    id: root

    readonly property bool shown: slowControl.value > 0

    color: Theme.cardColor
    border.color: Qt.rgba(Theme.yellow.r, Theme.yellow.g, Theme.yellow.b, 0.55)
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

    // The color stripe on the left says what kind of message this is (a warning)
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.top: parent.top
        anchors.topMargin: 10
        color: Theme.yellow
        radius: 2
        width: 4
    }

    Mixxx.ControlProxy {
        id: slowControl

        group: "[Stems]"
        key: "slow_detected"
    }
    // Not modal and reported once per session: if nobody answers it goes
    // away by itself, also when the screen turns off or the app is paused.
    Timer {
        interval: 15000
        running: root.shown
        onTriggered: slowControl.value = 0
    }
    Connections {
        function onStateChanged() {
            if (Qt.application.state !== Qt.ApplicationActive && root.shown)
                slowControl.value = 0;
        }

        target: Qt.application
    }
    Mixxx.ControlProxy {
        id: qualityControl

        group: "[Stems]"
        key: "quality"
    }

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.margins: 12
        spacing: 12

        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.pixelSize: 15
            text: qsTr("This phone is too slow for the stem quality in use: the stems arrive late or the audio drops out. Use the light quality?")
            wrapMode: Text.WordWrap
        }
        Skin.BannerButton {
            primary: true
            text: qsTr("Light")
            tint: Theme.yellow

            onClicked: {
                qualityControl.value = 1;
                slowControl.value = 0;
            }
        }
        Skin.BannerButton {
            text: qsTr("Not now")
            tint: Theme.yellow

            onClicked: slowControl.value = 0
        }
    }
}
