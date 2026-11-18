import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects
import "Theme"

Popup {
    id: root

    // Android back button closes the settings
    focus: true
    // Popup is not an Item, use the size of the window content it is shown in
    readonly property bool compact: parent ? parent.height < 700 || parent.width < 1000 : false
    readonly property bool narrow: parent ? parent.width < 600 : false
    // Portrait: categories on top, content below (side by side does not fit)
    readonly property bool portrait: parent ? parent.height > parent.width : false

    property int activeCategoryIndex: 0
    // Only the categories that are implemented in QML. The others are still
    // placeholders upstream; the full widget preferences dialog covers them.
    property list<string> sections: ["SoundHardware", "Controller"]

    readonly property var manager: managerItem

    background: Rectangle {
        anchors.fill: parent
        color: Theme.darkGray2
        opacity: parent.radius < 0 ? Math.max(0.1, 1 + parent.radius / 8) : 1
        radius: 8
    }
    contentItem: Item {
        anchors.centerIn: parent
        height: parent.height - 40
        width: parent.width - 40

        GridLayout {
            anchors.fill: parent
            columnSpacing: 0
            columns: root.portrait ? 1 : 2
            rowSpacing: 0

            Rectangle {
                Layout.fillHeight: !root.portrait
                Layout.fillWidth: root.portrait
                Layout.preferredHeight: root.portrait ? 120 : -1
                Layout.preferredWidth: root.portrait ? -1 : root.narrow ? 120 : root.compact ? 190 : 280
                border.color: Theme.darkGray3
                border.width: 6
                color: Theme.darkGray

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 0

                    Rectangle {
                        id: searchSetting

                        property bool active: false
                        property alias input: searchInput

                        Layout.fillWidth: true
                        color: Theme.midGray
                        height: 30

                        Text {
                            id: searchInputPlaceholder
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.white
                            text: 'Search...'
                            visible: !parent.active
                        }
                        TextInput {
                            id: searchInput
                            anchors.verticalCenter: parent.verticalCenter
                            visible: parent.active
                            width: parent.width

                            onActiveFocusChanged: {
                                parent.active = activeFocus;
                            }
                            onTextEdited: {
                                root.manager.search(text);
                            }
                        }
                        TapHandler {
                            onTapped: {
                                parent.active = true;
                                searchInput.forceActiveFocus();
                            }
                        }
                    }
                    ListView {
                        id: categoryList
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        clip: true
                        focus: true
                        model: sectionProperties
                        visible: !searchSetting.active

                        delegate: Rectangle {
                            required property int index
                            required property var label

                            color: ListView.isCurrentItem ? Theme.darkGray3 : Theme.darkGray2
                            height: 38
                            width: ListView.view.width

                            Image {
                                id: handleImage
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                fillMode: Image.PreserveAspectFit
                                height: 24
                                source: "images/gear.svg"
                                visible: false
                            }
                            ColorOverlay {
                                anchors.fill: handleImage
                                antialiasing: true
                                color: parent.ListView.isCurrentItem ? Theme.accentColor : Theme.midGray
                                source: handleImage
                            }
                            Text {
                                anchors.left: handleImage.right
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.white
                                font.bold: parent.ListView.isCurrentItem
                                text: label
                            }
                            TapHandler {
                                onTapped: {
                                    categoryList.currentIndex = index;
                                }
                            }
                        }
                    }
                    ListView {
                        id: settingResultList
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        clip: true
                        focus: true
                        model: root.manager.model
                        visible: searchSetting.active

                        delegate: Rectangle {
                            required property var display
                            required property int index
                            required property var toolTip
                            required property var whatsThis

                            color: Theme.darkGray2
                            height: 40
                            width: ListView.view.width

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 4

                                Text {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: implicitHeight
                                    color: Theme.white
                                    text: searchSetting.input.text ? display.replace(searchSetting.input.text, `<b>${searchSetting.input.text}</b>`) : display
                                    textFormat: Text.RichText
                                }
                                Text {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: implicitHeight
                                    color: Theme.midGray
                                    font.pixelSize: 10
                                    text: searchSetting.input.text ? whatsThis.replace(searchSetting.input.text, `<b>${searchSetting.input.text}</b>`) : whatsThis
                                    textFormat: Text.RichText
                                }
                            }
                            TapHandler {
                                onTapped: {
                                    for (let setting of toolTip) {
                                        setting.activated();
                                    }
                                    parent.forceActiveFocus();
                                }
                            }
                        }
                    }
                }
            }
            ColumnLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40

                    Text {
                        anchors.centerIn: parent
                        color: Theme.white
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        text: "Settings"
                    }
                    Skin.Button {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        activeColor: Theme.white
                        implicitHeight: 36
                        implicitWidth: 110
                        text: "All settings"

                        onClicked: {
                            root.close();
                            Mixxx.PreferencesDialog.show();
                        }
                    }
                    Skin.Button {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        activeColor: Theme.white
                        implicitHeight: 36
                        implicitWidth: 48
                        text: "Close"

                        onClicked: root.close()
                    }
                }
                Rectangle {
                    id: tabBar

                    readonly property var categoryItem: categoriesLoader.itemAt(categoryList.currentIndex) ? categoriesLoader.itemAt(categoryList.currentIndex).item : null
                    readonly property int selectedIndex: categoryItem && categoryItem.selectedIndex !== undefined ? categoryItem.selectedIndex : 0
                    readonly property var tabs: categoryItem ? categoryItem.tabs : []

                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    color: Theme.darkGray3
                    visible: tabs?.length > 0

                    RowLayout {
                        anchors.fill: parent

                        Repeater {
                            model: tabBar.tabs

                            Skin.Button {
                                required property int index
                                required property string modelData

                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredHeight: 22
                                Layout.preferredWidth: parent.width / (tabBar.tabs.length + 2)
                                activeColor: Theme.white
                                checked: tabBar.selectedIndex == index
                                text: modelData

                                onPressed: {
                                    categoriesLoader.itemAt(categoryList.currentIndex).item.selectedIndex = index;
                                }
                            }
                        }
                    }
                }

                Mixxx.SettingParameterManager {
                    id: managerItem
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.leftMargin: root.portrait ? 4 : 20

                    Repeater {
                        id: categoriesLoader
                        model: root.sections

                        Loader {
                            id: category

                            required property int index
                            required property var modelData

                            anchors.fill: parent
                            source: `Settings/${modelData}.qml`
                            visible: categoryList.currentIndex == index

                            // asynchronous: true // Unsupported
                            onLoaded: {
                                for (let i = sectionProperties.count; i < index; i++)
                                    sectionProperties.append({});
                                sectionProperties.set(index, {
                                        "label": category.item.label
                                });
                            }

                            Connections {
                                function onActivated() {
                                    categoryList.currentIndex = index;
                                }

                                target: category.item
                            }
                        }
                    }
                }
            }
        }
    }

    ListModel {
        id: sectionProperties
    }
}
