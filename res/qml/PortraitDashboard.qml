import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme"

// Portrait start screen, shown when no DJ controller is connected (phones).
// It has no continuous animation, so the window does not redraw while idle.
Rectangle {
    id: root

    // Devices seen by the controller manager (known and unknown mappings)
    readonly property var knownDevices: Mixxx.ControllerManager.knownDevices
    readonly property var unknownDevices: Mixxx.ControllerManager.unknownDevices
    readonly property int deviceCount: knownDevices.length + unknownDevices.length
    // Opened from the DJ view (toolbar button): it stays open with a
    // controller connected
    property bool openedByUser: false
    // English by default, Italian on an Italian system (as the dashboard sections)
    readonly property bool italian: Qt.locale().name.startsWith("it")

    signal openSettings
    signal showTutorial
    signal continueWithoutController

    gradient: Gradient {
        GradientStop {
            position: 0.0
            color: "#1a1233"
        }
        GradientStop {
            position: 0.35
            color: Theme.backgroundColor
        }
        GradientStop {
            position: 1.0
            color: "#0b0c0f"
        }
    }

    Flickable {
        // Tablets / big screens: a centered column, not stretched edge to edge
        readonly property real contentMaxWidth: 760
        // The scrolling list ends where the fixed bar starts
        anchors.bottom: backBar.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: 16
        anchors.top: parent.top
        width: Math.min(parent.width - 32, contentMaxWidth)
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentHeight: column.implicitHeight

        ColumnLayout {
            id: column

            spacing: 14
            width: parent.width

            // Header
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                spacing: 12

                Image {
                    Layout.preferredHeight: 52
                    Layout.preferredWidth: 52
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                    source: "images/logo.png"
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Text {
                        color: Theme.white
                        font.bold: true
                        font.letterSpacing: 1
                        font.pixelSize: 26
                        text: "Ubøros DJ"
                    }
                    Text {
                        color: Theme.accentColor
                        font.letterSpacing: 1.5
                        font.pixelSize: 11
                        text: qsTr("FREE · OPEN SOURCE")
                    }
                }
                Skin.Button {
                    activeColor: Theme.white
                    implicitHeight: 40
                    implicitWidth: 40
                    text: "?"

                    onClicked: root.showTutorial()
                }
                Skin.Button {
                    activeColor: Theme.white
                    icon.height: 18
                    icon.source: "images/gear.svg"
                    icon.width: 18
                    implicitHeight: 40
                    implicitWidth: 40

                    onClicked: root.openSettings()
                }
            }

            // Controller status card
            Rectangle {
                Layout.fillWidth: true
                border.color: root.deviceCount > 0 ? Qt.alpha(Theme.green, 0.6) : Theme.deckLineColor
                border.width: 1
                color: Theme.darkGray2
                implicitHeight: statusColumn.implicitHeight + 28
                radius: 14

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 14
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.topMargin: 14
                    color: root.deviceCount > 0 ? Theme.green : Theme.red
                    radius: 2
                    width: 4
                }

                ColumnLayout {
                    id: statusColumn

                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    RowLayout {
                        spacing: 10

                        Rectangle {
                            color: root.deviceCount > 0 ? Theme.green : Theme.red
                            implicitHeight: 12
                            implicitWidth: 12
                            radius: 6
                        }
                        Text {
                            Layout.fillWidth: true
                            color: Theme.white
                            font.bold: true
                            font.pixelSize: 18
                            text: root.deviceCount > 0 ? qsTr("Controller connected") : qsTr("No controller connected")
                            wrapMode: Text.WordWrap
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        color: Theme.midGray
                        font.pixelSize: 14
                        text: root.deviceCount > 0 ? (root.openedByUser ? qsTr("Ready.") : qsTr("Switching to the DJ view…")) : qsTr("Connect a DJ controller over USB (OTG adapter or a hub with power delivery). The DJ view opens automatically.")
                        wrapMode: Text.WordWrap
                    }
                    Repeater {
                        model: root.knownDevices

                        delegate: Text {
                            required property var modelData

                            Layout.fillWidth: true
                            color: Theme.lightGray2
                            elide: Text.ElideRight
                            font.pixelSize: 14
                            text: "• " + modelData.name + (modelData.activeMappingName ? "  (" + modelData.activeMappingName + ")" : "")
                        }
                    }
                    Repeater {
                        model: root.unknownDevices

                        delegate: Text {
                            required property var modelData

                            Layout.fillWidth: true
                            color: Theme.lightGray2
                            elide: Text.ElideRight
                            font.pixelSize: 14
                            // On Android a MIDI device has no USB product info, so a
                            // device with a working mapping is listed here too
                            text: "• " + modelData.name + "  " + (modelData.activeMappingName ? "(" + modelData.activeMappingName + ")" : qsTr("(no mapping)"))
                        }
                    }
                }
            }

            Text {
                Layout.topMargin: 6
                color: Theme.accentColor
                font.bold: true
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1.2
                font.pixelSize: 12
                text: qsTr("Quick actions")
            }

            Skin.DashboardAction {
                Layout.fillWidth: true
                subtitle: qsTr("Audio output, controllers, library, waveforms")
                title: qsTr("Settings")

                onClicked: root.openSettings()
            }
            Skin.DashboardAction {
                Layout.fillWidth: true
                subtitle: qsTr("Copy a rekordbox USB drive to the phone, no root needed")
                title: qsTr("Import USB drive")
                visible: Qt.platform.os === "android"

                onClicked: Mixxx.Library.importUsbDrives()
            }
            Skin.DashboardMaintenance {
                onShowTutorial: root.showTutorial()
                Layout.bottomMargin: 24
                Layout.fillWidth: true
                // Not built until the dashboard is shown
                visible: root.visible
            }
        }
    }

    // Fixed bar at the bottom: the way back to the DJ view never scrolls away
    // (and stays within the thumb's reach). Not in the scrolling list.
    Rectangle {
        id: backBar

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: "#0b0c0f"
        implicitHeight: backButton.implicitHeight + 16
        z: 5

        // Top edge line
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            color: Theme.deckLineColor
            height: 1
        }
        AbstractButton {
            id: backButton

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            // Large touch target (at least 56 px high), same column width as the list
            implicitHeight: Math.max(56, backLabels.implicitHeight + 20)
            width: Math.min(parent.width - 32, 760)

            background: Rectangle {
                border.color: Qt.lighter(Theme.accentColor, 1.3)
                border.width: 1
                color: backButton.pressed ? Qt.darker(Theme.accentColor, 1.4) : Theme.accentColor
                radius: 14
            }
            contentItem: RowLayout {
                spacing: 12

                Text {
                    Layout.leftMargin: 14
                    color: Theme.white
                    font.bold: true
                    font.pixelSize: 30
                    text: "‹"
                }
                ColumnLayout {
                    id: backLabels

                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        color: Theme.white
                        font.bold: true
                        font.pixelSize: 16
                        text: root.deviceCount > 0 ? (root.italian ? "Torna alla vista DJ" : qsTr("Back to the DJ view")) : (root.italian ? "Apri la vista DJ senza controller" : qsTr("Open the DJ view without a controller"))
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        Layout.fillWidth: true
                        color: Qt.alpha(Theme.white, 0.75)
                        font.pixelSize: 12
                        text: root.italian ? "Deck, libreria e mixer a schermo, in orizzontale" : qsTr("Decks, library and on-screen mixer, in landscape")
                        wrapMode: Text.WordWrap
                    }
                }
            }

            onClicked: root.continueWithoutController()
        }
    }
}
