import QtQuick
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Skin
import "Theme"

// A page of the settings (Settings/<section>.qml: SoundHardware, Controller, Interface) inside the
// Android hub: the same pages as the Settings popup of the desktop interface, without the popup
// (no category list that does not fit, no way back). Built only when the page is opened.
Item {
    id: root

    // File name in Settings/ without .qml
    property string section: "SoundHardware"

    readonly property var pageItem: loader.item
    readonly property var tabs: pageItem && pageItem.tabs ? pageItem.tabs : []
    readonly property int selectedIndex: pageItem && pageItem.selectedIndex !== undefined ? pageItem.selectedIndex : 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        // Tabs of the page (e.g. engine / delays), as buttons that fit a phone
        Flow {
            Layout.fillWidth: true
            spacing: 6
            visible: root.tabs.length > 0

            Repeater {
                model: root.tabs

                Skin.Button {
                    required property int index
                    required property string modelData

                    activeColor: Theme.white
                    checked: root.selectedIndex === index
                    implicitHeight: 40
                    implicitWidth: Math.max(96, label.implicitWidth + 28)
                    text: modelData

                    onPressed: root.pageItem.selectedIndex = index

                    Text {
                        id: label
                        visible: false
                        text: parent.text
                        font.pixelSize: 14
                    }
                }
            }
        }
        Mixxx.SettingParameterManager {
            Layout.fillHeight: true
            Layout.fillWidth: true

            Loader {
                id: loader

                anchors.fill: parent
                source: "Settings/" + root.section + ".qml"
            }
        }
    }
}
