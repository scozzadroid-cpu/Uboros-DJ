import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Layouts
import "Theme"

// Over the info bar of a deck while a plugin (SoundCloud) gets the track that was loaded into it:
// the title and the artist, what is happening (connecting, downloading with the percentage) and a
// progress bar along the bottom edge. A failure stays on for a few seconds, with its reason.
// Nothing is drawn, and no animation runs, when the deck waits for nothing.
Rectangle {
    id: root

    required property string group
    readonly property var info: Mixxx.Plugins.loadingInfo[group] || null
    readonly property bool failed: info !== null && info.state === "error"
    readonly property real progress: info !== null && info.progress !== undefined ? info.progress : -1
    readonly property color tint: failed ? Theme.yellow : "#ff5500"

    border.color: Qt.rgba(tint.r, tint.g, tint.b, 0.55)
    border.width: 1
    clip: true
    color: Qt.rgba(0.07, 0.07, 0.08, 1.0)
    radius: 6
    visible: info !== null

    // Takes the touches: a tap on the panel must not reach the deck below
    MouseArea {
        anchors.fill: parent
    }

    RowLayout {
        anchors.bottomMargin: 6
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 3
        spacing: 8

        // The spinner: a dot goes round a ring (a "!" when it failed)
        Item {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: Math.min(30, root.height - 12)
            Layout.preferredWidth: Layout.preferredHeight

            Rectangle {
                anchors.fill: parent
                border.color: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.3)
                border.width: 3
                color: "transparent"
                radius: width / 2
            }
            Item {
                id: orbit

                anchors.fill: parent
                visible: !root.failed

                RotationAnimator on rotation {
                    duration: 1100
                    from: 0
                    loops: Animation.Infinite
                    running: root.visible && !root.failed
                    to: 360
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: root.tint
                    height: 7
                    radius: 3.5
                    width: 7
                    y: -1
                }
            }
            Text {
                anchors.centerIn: parent
                color: root.tint
                font.bold: true
                font.pixelSize: 16
                text: "!"
                visible: root.failed
            }
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            spacing: 0

            Text {
                Layout.fillWidth: true
                color: Theme.white
                elide: Text.ElideRight
                font.bold: true
                font.pixelSize: 14
                // The list marks tracks that cannot be downloaded with "[DRM] ": the panel says why in its own words
                text: root.info ? String(root.info.title).replace(/^\[DRM\]\s*/, "") : ""
            }
            Text {
                Layout.fillWidth: true
                color: Theme.midGray
                elide: Text.ElideRight
                font.pixelSize: 11
                text: root.info ? root.info.artist : ""
                visible: text.length > 0 && root.height >= 52
            }
            Text {
                Layout.fillWidth: true
                color: root.tint
                elide: Text.ElideRight
                font.bold: true
                font.pixelSize: 11
                text: root.failed ? (root.info.message || "") : (root.info && root.info.status ? root.info.status : (root.progress < 0 ? qsTr("Connecting to SoundCloud…") : qsTr("Downloading from SoundCloud…")))
            }
        }

        Text {
            Layout.alignment: Qt.AlignVCenter
            color: Theme.white
            font.bold: true
            font.pixelSize: 20
            text: Math.round(root.progress * 100) + "%"
            visible: !root.failed && root.progress >= 0 && !(root.info && root.info.status)
        }
    }

    // The bar: the part that is done, or a segment going to and fro while it is not known yet
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, 0.2)
        height: 4
        visible: !root.failed

        // Known: the part that is done
        Rectangle {
            color: root.tint
            height: parent.height
            visible: root.progress >= 0
            width: parent.width * Math.min(1, Math.max(0, root.progress))

            Behavior on width {
                NumberAnimation {
                    duration: 180
                }
            }
        }
        // Not known yet: a segment going to and fro. It exists (and moves) only while it is shown.
        Loader {
            active: root.visible && !root.failed && root.progress < 0
            anchors.fill: parent

            sourceComponent: Rectangle {
                id: segment

                color: root.tint
                height: parent.height
                width: parent.width * 0.3

                SequentialAnimation on x {
                    loops: Animation.Infinite
                    running: true

                    NumberAnimation {
                        duration: 900
                        easing.type: Easing.InOutQuad
                        from: 0
                        to: segment.parent.width * 0.7
                    }
                    NumberAnimation {
                        duration: 900
                        easing.type: Easing.InOutQuad
                        from: segment.parent.width * 0.7
                        to: 0
                    }
                }
            }
        }
    }
}
