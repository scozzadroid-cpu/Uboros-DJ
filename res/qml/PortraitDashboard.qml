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

    signal openSettings
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
        anchors.bottom: parent.bottom
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
            Skin.DashboardAction {
                Layout.fillWidth: true
                subtitle: qsTr("Decks, library and on-screen mixer, in landscape")
                title: root.deviceCount > 0 ? qsTr("Back to the DJ view") : qsTr("Open the DJ view without a controller")

                onClicked: root.continueWithoutController()
            }
            Skin.DashboardMaintenance {
                Layout.bottomMargin: 24
                Layout.fillWidth: true
                // Not built until the dashboard is shown
                visible: root.visible
            }
        }
    }
}
