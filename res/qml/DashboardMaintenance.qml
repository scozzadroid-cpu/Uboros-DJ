import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme"

// Dashboard sections: track analysis, caches and stems
ColumnLayout {
    id: root

    property var info: ({})
    readonly property var options: Mixxx.Analysis.options
    // Destructive actions ask for a second tap within a few seconds
    property string armedAction: ""

    function formatBytes(bytes) {
        if (!bytes || bytes <= 0)
            return "0 MB";
        if (bytes >= 1024 * 1024 * 1024)
            return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB";
        return Math.max(0.1, bytes / (1024 * 1024)).toFixed(1) + " MB";
    }
    function refresh() {
        root.info = Mixxx.Analysis.cacheInfo();
    }
    // First tap arms the action, a second tap within 4 s runs it
    function confirm(action) {
        if (root.armedAction === action) {
            root.armedAction = "";
            return true;
        }
        root.armedAction = action;
        disarmTimer.restart();
        return false;
    }

    spacing: 10

    Component.onCompleted: refresh()
    onVisibleChanged: {
        if (visible)
            refresh();
    }

    Connections {
        function onCacheChanged() {
            root.refresh();
        }

        target: Mixxx.Analysis
    }
    Timer {
        id: disarmTimer

        interval: 4000

        onTriggered: root.armedAction = ""
    }
    // Keep the counters fresh while a batch runs
    Timer {
        interval: 5000
        repeat: true
        running: Mixxx.Analysis.active && root.visible

        onTriggered: root.refresh()
    }

    component SectionTitle: Text {
        Layout.topMargin: 10
        color: Theme.midGray
        font.bold: true
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1.2
        font.pixelSize: 12
    }
    component Card: Rectangle {
        default property alias content: cardColumn.data

        Layout.fillWidth: true
        border.color: Theme.deckLineColor
        border.width: 1
        color: Theme.darkGray2
        implicitHeight: cardColumn.implicitHeight + 24
        radius: 12

        ColumnLayout {
            id: cardColumn

            anchors.left: parent.left
            anchors.margins: 14
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
        }
    }
    component InfoLine: Text {
        Layout.fillWidth: true
        color: Theme.lightGray2
        font.pixelSize: 14
        wrapMode: Text.WordWrap
    }
    component Option: Skin.DashboardToggle {
        required property string option

        Layout.fillWidth: true
        on: !!Mixxx.Analysis.options[option]

        onSwitched: value => Mixxx.Analysis.setOption(option, value)
    }

    // ---------------------------------------------------------------- Analysis
    SectionTitle {
        text: qsTr("Track analysis")
    }
    Card {
        InfoLine {
            color: Theme.white
            font.bold: true
            font.pixelSize: 16
            text: Mixxx.Analysis.active ? (Mixxx.Analysis.totalTracks > 0 ? qsTr("Analyzing track %1 of %2").arg(Mixxx.Analysis.currentTrack).arg(Mixxx.Analysis.totalTracks) : qsTr("Analyzing…")) : qsTr("No analysis running")
        }
        ProgressBar {
            id: analysisProgress

            Layout.fillWidth: true
            from: 0
            implicitHeight: 8
            to: 1
            value: Mixxx.Analysis.totalTracks > 0 ? (Mixxx.Analysis.currentTrack - 1 + Mixxx.Analysis.trackProgress) / Mixxx.Analysis.totalTracks : 0
            visible: Mixxx.Analysis.active

            background: Rectangle {
                color: Theme.darkGray3
                radius: 4
            }
            contentItem: Item {
                Rectangle {
                    color: Theme.accent
                    height: parent.height
                    radius: 4
                    width: analysisProgress.visualPosition * parent.width
                }
            }
        }
        InfoLine {
            text: qsTr("%1 tracks in the library · %2 without BPM · %3 without waveform").arg(root.info.libraryTracks || 0).arg(root.info.tracksWithoutBpm || 0).arg(root.info.tracksWithoutWaveform || 0)
        }
        InfoLine {
            color: Theme.midGray
            font.pixelSize: 12
            text: qsTr("The analysis pauses while a deck is playing and resumes afterwards.")
        }
    }
    Skin.DashboardAction {
        Layout.fillWidth: true
        subtitle: qsTr("Stop the running analysis; finished tracks are kept")
        title: qsTr("Stop analysis")
        visible: Mixxx.Analysis.active

        onClicked: Mixxx.Analysis.stop()
    }
    Skin.DashboardAction {
        Layout.fillWidth: true
        subtitle: qsTr("Only tracks without BPM, key or waveform")
        title: qsTr("Analyze missing tracks")

        onClicked: Mixxx.Analysis.analyze("missing")
    }
    Skin.DashboardAction {
        Layout.fillWidth: true
        subtitle: qsTr("Tracks of the connected USB drives and SD cards")
        title: qsTr("Analyze USB drive tracks")

        onClicked: Mixxx.Analysis.analyze("drives")
    }
    Skin.DashboardAction {
        Layout.fillWidth: true
        subtitle: qsTr("Every track; parts already analyzed are skipped")
        title: qsTr("Analyze the whole library")

        onClicked: Mixxx.Analysis.analyze("all")
    }
    Option {
        option: "bpmDetection"
        subtitle: qsTr("Detect BPM and beat grid of tracks that have none")
        title: qsTr("BPM detection")
    }
    Option {
        enabled: !!root.options.bpmDetection
        option: "bpmFixedTempo"
        subtitle: qsTr("Straight grid for electronic music; off for live drums")
        title: qsTr("Assume constant tempo")
    }
    Option {
        enabled: !!root.options.bpmDetection
        option: "bpmFast"
        subtitle: qsTr("Faster, less accurate: analyzes only part of the track")
        title: qsTr("Fast BPM analysis")
    }
    Option {
        enabled: !!root.options.bpmDetection
        option: "bpmReanalyzeOnChange"
        subtitle: qsTr("Analyze the beat grid again when these settings change")
        title: qsTr("Re-detect BPM after changes")
    }
    Option {
        enabled: !!root.options.bpmDetection
        option: "bpmReanalyzeImported"
        subtitle: qsTr("Replaces the rekordbox grids with Mixxx's own: usually leave off")
        title: qsTr("Re-detect BPM of imported tracks")
    }
    Option {
        option: "keyDetection"
        subtitle: qsTr("Detect the key (shown as 8A (Am))")
        title: qsTr("Key detection")
    }
    Option {
        enabled: !!root.options.keyDetection
        option: "keyFast"
        subtitle: qsTr("Faster, less accurate")
        title: qsTr("Fast key analysis")
    }
    Option {
        enabled: !!root.options.keyDetection
        option: "keyReanalyzeOnChange"
        subtitle: qsTr("Analyze the key again when these settings change")
        title: qsTr("Re-detect key after changes")
    }
    Option {
        option: "devicePreanalysis"
        subtitle: qsTr("Analyze all the tracks of a USB drive in the background when it is opened")
        title: qsTr("Pre-analyze USB drives")
    }
    Option {
        option: "deviceStore"
        subtitle: qsTr("Keep the analyses on the drive (MIXXX folder, next to PIONEER): the drive works on another phone without analyzing again")
        title: qsTr("Store analyses on the USB drive")
    }

    // ------------------------------------------------------------------- Cache
    SectionTitle {
        text: qsTr("Cache")
    }
    Card {
        InfoLine {
            text: qsTr("Waveforms on the phone: %1 (overviews %2)").arg(root.formatBytes(root.info.waveformBytes)).arg(root.formatBytes(root.info.summaryBytes))
        }
        Repeater {
            model: root.info.drives || []

            delegate: InfoLine {
                required property var modelData

                text: qsTr("%1: %2 analyses on the drive, %3 · %4 free%5").arg(modelData.label).arg(modelData.sidecars).arg(root.formatBytes(modelData.bytes)).arg(root.formatBytes(modelData.freeBytes)).arg(modelData.writable ? "" : qsTr(" · read-only"))
            }
        }
    }
    Skin.DashboardAction {
        Layout.fillWidth: true
        subtitle: root.armedAction === "waveforms" ? qsTr("Tap again to confirm") : qsTr("Rebuilt when a track is loaded or analyzed; USB drive tracks reload theirs from the drive")
        title: qsTr("Clear waveform cache")

        onClicked: {
            if (root.confirm("waveforms"))
                Mixxx.Analysis.clearWaveformCache();
        }
    }
    Repeater {
        model: (root.info.drives || []).filter(drive => drive.sidecars > 0)

        delegate: Skin.DashboardAction {
            required property var modelData

            Layout.fillWidth: true
            subtitle: root.armedAction === modelData.path ? qsTr("Tap again to confirm") : qsTr("Deletes only the MIXXX/ANLZ folder of %1; rekordbox data and music are not touched. Not while a deck is playing.").arg(modelData.label)
            title: qsTr("Delete analyses on %1").arg(modelData.label)

            onClicked: {
                if (root.confirm(modelData.path))
                    Mixxx.Analysis.deleteDriveAnalyses(modelData.path);
            }
        }
    }

    // ------------------------------------------------------------------- Stems
    SectionTitle {
        text: qsTr("Stems")
    }
    Card {
        InfoLine {
            text: qsTr("%1 stem files in the library (%2)").arg(root.info.stemTracks || 0).arg(root.formatBytes(root.info.stemBytes))
        }
        InfoLine {
            color: Theme.midGray
            font.pixelSize: 12
            text: qsTr("Stem files (.stem.mp4) play with 4 separate parts: vocals, drums, bass, melody.")
        }
    }
    Skin.DashboardAction {
        Layout.fillWidth: true
        enabled: false
        opacity: 0.5
        subtitle: qsTr("Separating tracks into stems on the phone is in development")
        title: qsTr("Prepare stems for tracks")
    }
    Option {
        option: "stemAutoReset"
        subtitle: qsTr("Volume and mute of each stem return to normal when a track is loaded")
        title: qsTr("Reset stem controls on load")
    }
    Option {
        option: "bpmFromDrums"
        subtitle: qsTr("Detect the BPM of stem files from the drums stem only")
        title: qsTr("BPM from the drums stem")
    }
    Option {
        option: "keyWithoutDrums"
        subtitle: qsTr("Detect the key of stem files without the drums stem")
        title: qsTr("Key without the drums stem")
    }
}
