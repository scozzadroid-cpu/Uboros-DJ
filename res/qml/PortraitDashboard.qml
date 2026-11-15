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

    signal openSettings
    signal continueWithoutController

    color: Theme.backgroundColor

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
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

                Text {
                    Layout.fillWidth: true
                    color: Theme.white
                    font.bold: true
                    font.pixelSize: 26
                    text: "Mixxx"
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
                border.color: Theme.deckLineColor
                border.width: 1
                color: Theme.darkGray2
                implicitHeight: statusColumn.implicitHeight + 28
                radius: 12

                ColumnLayout {
                    id: statusColumn

                    anchors.left: parent.left
                    anchors.margins: 14
                    anchors.right: parent.right
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
                        text: root.deviceCount > 0 ? qsTr("Switching to the DJ view…") : qsTr("Connect a DJ controller over USB (OTG adapter or a hub with power delivery). The DJ view opens automatically.")
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
                            text: "• " + modelData.name + "  " + qsTr("(no mapping)")
                        }
                    }
                }
            }

            Text {
                Layout.topMargin: 6
                color: Theme.midGray
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
                title: qsTr("Open the DJ view without a controller")

                onClicked: root.continueWithoutController()
            }
        }
    }
}
