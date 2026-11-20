import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import "Theme"

// One line that says what the app is doing in the background: reading or
// preparing a USB drive, analyzing tracks, measuring the speed of the stem
// separation, separating stems. Nothing is shown when nothing is going on.
// Not interactive and it takes no room of its own: it fills what the parent
// gives it (a spare space of the toolbar, or the splash screen).
Item {
    id: root

    property bool centered: false
    property int pixelSize: Theme.textFontPixelSize - 1
    property color textColor: Theme.lightGray2

    // [Stems],busy: 0 nothing, 1 separating the track of a deck, 2 speed
    // test, 3 best quality pass with the decks stopped
    readonly property int stemsBusy: Math.round(stemsBusyControl.value)
    readonly property string usbText: Mixxx.Analysis.deviceBusy
    readonly property bool analyzing: Mixxx.Analysis.active && Mixxx.Analysis.totalTracks > 0
    readonly property string statusText: {
        const parts = [];
        if (usbText !== "")
            parts.push(usbText);
        if (stemsBusy === 2)
            parts.push(qsTr("Measuring stem speed…"));
        else if (stemsBusy === 1)
            parts.push(qsTr("Separating stems…"));
        else if (stemsBusy === 3)
            parts.push(qsTr("Improving stems…"));
        if (analyzing)
            parts.push(qsTr("Analyzing %1/%2").arg(Mixxx.Analysis.currentTrack).arg(Mixxx.Analysis.totalTracks));
        return parts.join("  ·  ");
    }

    implicitHeight: label.implicitHeight
    visible: statusText !== ""

    Mixxx.ControlProxy {
        id: stemsBusyControl

        group: "[Stems]"
        key: "busy"
    }
    Row {
        anchors.centerIn: root.centered ? parent : undefined
        anchors.left: root.centered ? undefined : parent.left
        anchors.verticalCenter: root.centered ? undefined : parent.verticalCenter
        spacing: 8

        Skin.Spinner {
            id: dot

            anchors.verticalCenter: parent.verticalCenter
            height: 12
            running: root.visible
            width: 12
        }
        Text {
            id: label

            anchors.verticalCenter: parent.verticalCenter
            color: root.textColor
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: root.pixelSize
            text: root.statusText
            // The spinner and the gap take 20 px; never wider than the space given
            width: Math.min(implicitWidth, Math.max(0, root.width - 20))
        }
    }
}
