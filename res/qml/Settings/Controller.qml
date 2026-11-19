import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "../Theme"

Category {
    id: root

    label: "Controllers"

    component DeviceCard: Rectangle {
        id: card

        required property var device
        readonly property var mappings: Mixxx.ControllerManager.mappings(device.type)

        // Compare by name: mapping proxies may be recreated, and devices
        // without a product match have no mapping proxy at all.
        function mappingIndex() {
            const name = card.device.mapping ? card.device.mapping.name : card.device.activeMappingName;
            if (!name)
                return -1;
            for (let i = 0; i < card.mappings.length; i++) {
                if (card.mappings[i].name === name)
                    return i;
            }
            return -1;
        }

        color: Theme.darkGray3
        implicitHeight: content.implicitHeight + 24
        radius: 6
        width: parent ? parent.width : 0

        ColumnLayout {
            id: content

            anchors.left: parent.left
            anchors.margins: 12
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            RowLayout {
                Layout.fillWidth: true

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        color: Theme.white
                        elide: Text.ElideRight
                        font.bold: true
                        font.pixelSize: 16
                        text: card.device.name
                    }
                    Text {
                        Layout.fillWidth: true
                        color: Theme.lightGray
                        elide: Text.ElideRight
                        font.pixelSize: 12
                        text: {
                            const types = ["MIDI", "HID", "USB bulk"];
                            return `${types[card.device.type] ?? ""} · ${card.device.vendor} ${card.device.product}`;
                        }
                    }
                }
                Switch {
                    checked: card.device.enabled

                    onToggled: card.device.enabled = checked
                }
            }
            // The label above the selector (a phone is too narrow for label, selector and button in a row)
            Text {
                Layout.topMargin: 4
                color: Theme.lightGray
                font.pixelSize: 12
                text: "Mapping"
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ComboBox {
                    id: mappingCombo

                    Layout.fillWidth: true
                    Layout.minimumWidth: 120
                    currentIndex: card.mappingIndex()
                    displayText: currentIndex < 0 ? "No mapping" : currentText
                    model: card.mappings
                    textRole: "name"

                    onActivated: index => {
                        card.device.mapping = card.mappings[index];
                    }

                    Connections {
                        function onMappingChanged() {
                            mappingCombo.currentIndex = card.mappingIndex();
                        }

                        target: card.device
                    }
                }
                Button {
                    enabled: card.device.edited
                    text: "Apply"

                    onClicked: {
                        if (!card.device.save(Mixxx.Config))
                            console.warn(`Failed to save controller ${card.device.name}`);
                    }
                }
            }
        }
    }

    Flickable {
        id: flickable

        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentHeight: deviceColumn.implicitHeight + 16

        ScrollBar.vertical: ScrollBar {
        }

        Column {
            id: deviceColumn

            spacing: 10
            width: flickable.width - 12

            Text {
                color: Theme.lightGray
                font.pixelSize: 13
                text: "Controllers are detected when plugged in. A matching mapping is selected automatically the first time a controller is seen."
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Repeater {
                model: Mixxx.ControllerManager.knownDevices

                DeviceCard {
                    required property var modelData

                    device: modelData
                }
            }
            Repeater {
                model: Mixxx.ControllerManager.unknownDevices

                DeviceCard {
                    required property var modelData

                    device: modelData
                }
            }
            Text {
                color: Theme.white
                font.pixelSize: 14
                text: "No controller connected."
                visible: Mixxx.ControllerManager.knownDevices.length + Mixxx.ControllerManager.unknownDevices.length === 0
            }
        }
    }
}
