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

    // The Android hub: a home with one tile per category; each tile opens a page that
    // is built only when it is open (the dashboard used to build ~60 items at once).
    // Android back key / back gesture: one page back, then the DJ view.
    focus: visible
    Keys.onBackPressed: event => {
        event.accepted = true;
        root.goBack();
    }
    Keys.onEscapePressed: event => {
        event.accepted = true;
        root.goBack();
    }
    onVisibleChanged: {
        if (visible) {
            stack.pop(null);
            forceActiveFocus();
        }
    }

    function goBack() {
        if (stack.depth > 1)
            stack.pop();
        else
            root.continueWithoutController();
    }
    function openMaintenance(title, page) {
        stack.push(maintenancePage, {
            "pageTitle": title,
            "page": page
        });
    }
    function openSettingsSection(title, section) {
        stack.push(settingsPage, {
            "pageTitle": title,
            "section": section
        });
    }
    function tr(en, it) {
        return root.italian ? it : en;
    }

    StackView {
        id: stack

        anchors.bottom: backBar.visible ? backBar.top : parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        initialItem: homePage
        // No transitions: instant, nothing animated
        popEnter: null
        popExit: null
        pushEnter: null
        pushExit: null
        replaceEnter: null
        replaceExit: null
    }

    Component {
        id: homePage

        Flickable {
            id: homeFlick

            // Tablets / big screens: a centered column, not stretched edge to edge
            readonly property real contentMaxWidth: 760

            boundsBehavior: Flickable.StopAtBounds
            clip: true
            contentHeight: column.implicitHeight + 32
            contentWidth: width

            ColumnLayout {
                id: column

                spacing: 14
                width: Math.min(homeFlick.width - 32, homeFlick.contentMaxWidth)
                x: (homeFlick.width - width) / 2
                y: 16

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
                    text: root.tr("Settings", "Impostazioni")
                }

                GridLayout {
                    Layout.fillWidth: true
                    columnSpacing: 10
                    columns: 2
                    rowSpacing: 10

                    Skin.DashboardTile {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        subtitle: root.tr("Output device, latency, routing", "Uscita audio, latenza, instradamento")
                        title: root.tr("Audio", "Audio")

                        onClicked: root.openSettingsSection(title, "SoundHardware")
                    }
                    Skin.DashboardTile {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        subtitle: root.deviceCount > 0 ? root.tr("%1 connected", "%1 collegato").arg(root.deviceCount) : root.tr("None connected", "Nessuno collegato")
                        title: root.tr("Controllers", "Controller")

                        onClicked: root.openSettingsSection(title, "Controller")
                    }
                    Skin.DashboardTile {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        subtitle: Theme.profileName(Mixxx.Config.interfaceProfile) + " · " + root.tr("profiles, waveform colors", "profili, colori delle onde")
                        title: root.tr("Interface", "Interfaccia")

                        onClicked: root.openSettingsSection(title, "Interface")
                    }
                    Skin.DashboardTile {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        subtitle: root.tr("Import a USB drive, USB analyses, caches", "Importa una chiavetta, analisi USB, cache")
                        title: root.tr("Library & USB", "Libreria e USB")
                        visible: true

                        onClicked: root.openMaintenance(title, "library")
                    }
                    Skin.DashboardTile {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        subtitle: root.tr("BPM and key detection, analyze tracks", "Rilevamento di BPM e tonalità, analisi dei brani")
                        title: root.tr("Analysis", "Analisi")

                        onClicked: root.openMaintenance(title, "analysis")
                    }
                    Skin.DashboardTile {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        subtitle: root.tr("Waveforms, screen off, burn-in protection", "Onde, spegnimento schermo, protezione burn-in")
                        title: root.tr("Display & power", "Schermo e consumi")

                        onClicked: root.openMaintenance(title, "display")
                    }
                    Skin.DashboardTile {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        subtitle: root.tr("Live stems, quality, preparing tracks", "Stem dal vivo, qualità, preparazione dei brani")
                        title: root.tr("Stems", "Stem")

                        onClicked: root.openMaintenance(title, "stems")
                    }
                    Skin.DashboardTile {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        subtitle: root.tr("Tutorial, automatic updates", "Tutorial, aggiornamenti automatici")
                        title: root.tr("Updates & help", "Aggiornamenti e aiuto")

                        onClicked: root.openMaintenance(title, "updates")
                    }
                }

                // Sections of the plugins (src/plugins; none without UBOROS_PLUGINS_DIR): one QML file each
                Repeater {
                    model: Mixxx.Plugins.dashboardSections

                    delegate: Loader {
                        required property string modelData

                        Layout.fillWidth: true
                        source: modelData
                    }
                }
                Item {
                    Layout.preferredHeight: 8
                }
            }
        }
    }

    // A page of maintenance options (DashboardMaintenance.qml, one section only)
    Component {
        id: maintenancePage

        Item {
            id: pageRoot

            property string page
            property string pageTitle

            Skin.DashboardPageHeader {
                id: pageHeader

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                title: pageRoot.pageTitle

                onBack: root.goBack()
            }
            Flickable {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: pageHeader.bottom
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                contentHeight: maintenance.implicitHeight + 32
                contentWidth: width

                Skin.DashboardMaintenance {
                    id: maintenance

                    page: pageRoot.page
                    width: Math.min(parent.width - 32, 760)
                    x: (parent.width - width) / 2
                    y: 8

                    onShowTutorial: root.showTutorial()
                }
            }
        }
    }

    // A page of the settings (Settings/*.qml)
    Component {
        id: settingsPage

        Item {
            id: pageRoot

            property string pageTitle
            property string section

            Skin.DashboardPageHeader {
                id: pageHeader

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                title: pageRoot.pageTitle

                onBack: root.goBack()
            }
            Skin.DashboardSettingsPage {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.margins: 12
                anchors.top: pageHeader.bottom
                section: pageRoot.section
                width: Math.min(parent.width - 24, 900)
            }
        }
    }

    // Fixed bar at the bottom: the way back to the DJ view never scrolls away
    // (and stays within the thumb's reach). Not in the scrolling list.
    Rectangle {
        id: backBar

        // Only on the home page: the pages have their own back arrow
        visible: stack.depth === 1
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
