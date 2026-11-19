import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "../Theme"

// Interface profile: three looks of the same interface. The choice is saved
// (Mixxx.Config.interfaceProfile) and applies at once, no restart.
Category {
    id: root

    label: "Interface"

    // Mini screen of a profile, made of plain rectangles with the colors of
    // that profile (not of the current one): waveforms, deck colors, grid
    component Preview: Item {
        id: preview

        required property string profile
        readonly property var pal: Theme.paletteOf(profile)
        // Heights of the bars of the little waveforms (fractions of the strip)
        readonly property var bars: [0.45, 0.8, 0.6, 1.0, 0.7, 0.9, 0.4, 0.85, 0.55, 0.95, 0.5, 0.75]

        implicitHeight: 76
        implicitWidth: 132

        Rectangle {
            anchors.fill: parent
            border.color: preview.pal.deckLineColor
            border.width: 1
            color: preview.pal.backgroundColor
            radius: 5

            // Toolbar
            Rectangle {
                id: toolbar

                color: preview.pal.toolbarBackgroundColor
                height: 9
                radius: 4
                width: parent.width - 2
                x: 1
                y: 1

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 5
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3

                    Rectangle {
                        color: preview.pal.accentColor
                        height: 4
                        radius: 2
                        width: 12
                    }
                    Rectangle {
                        color: preview.pal.buttonBackgroundColor
                        height: 4
                        radius: 2
                        width: 12
                    }
                    Rectangle {
                        color: preview.pal.buttonBackgroundColor
                        height: 4
                        radius: 2
                        width: 12
                    }
                }
            }

            // Two waveforms: bars in the three band colors, beat lines, play marker
            Repeater {
                model: 2

                Rectangle {
                    id: strip

                    required property int index
                    readonly property real barWidth: Math.floor((width - 4) / preview.bars.length)

                    border.color: preview.pal.deckLineColor
                    border.width: 1
                    color: preview.pal.darkGray
                    height: 17
                    width: preview.width - 10
                    x: 5
                    y: 13 + index * 20

                    Repeater {
                        model: preview.bars

                        Item {
                            id: bar

                            required property int index
                            required property real modelData

                            height: strip.height - 2
                            width: strip.barWidth
                            x: 2 + index * strip.barWidth
                            y: 1

                            Rectangle {
                                anchors.verticalCenter: bar.verticalCenter
                                color: preview.pal.waveLow
                                height: Math.max(2, (bar.height - 1) * Math.min(1, bar.modelData * preview.pal.waveGain))
                                width: Math.max(1, bar.width - 1)
                            }
                            Rectangle {
                                anchors.verticalCenter: bar.verticalCenter
                                color: preview.pal.waveMid
                                height: Math.max(2, (bar.height - 1) * Math.min(1, bar.modelData * 0.6 * preview.pal.waveGain))
                                width: Math.max(1, bar.width - 1)
                            }
                            Rectangle {
                                anchors.verticalCenter: bar.verticalCenter
                                color: preview.pal.waveHigh
                                height: Math.max(2, (bar.height - 1) * Math.min(1, bar.modelData * 0.3 * preview.pal.waveGain))
                                width: Math.max(1, bar.width - 1)
                            }
                        }
                    }
                    // Beat lines: thin, with the start of the bar in red
                    Rectangle {
                        color: preview.pal.waveBeat
                        height: parent.height - 2
                        width: 1
                        x: 2 + strip.barWidth * 3
                        y: 1
                    }
                    Rectangle {
                        color: "#ff3b30"
                        height: parent.height - 2
                        width: 2
                        x: 2 + strip.barWidth * 6
                        y: 1
                    }
                    Rectangle {
                        color: preview.pal.waveBeat
                        height: parent.height - 2
                        width: 1
                        x: 2 + strip.barWidth * 9
                        y: 1
                    }
                    // Play marker
                    Rectangle {
                        color: "#e4e7eb"
                        height: parent.height
                        width: 1
                        x: Math.round(parent.width / 2)
                    }
                    // Color of the deck
                    Rectangle {
                        color: preview.pal.deck[strip.index]
                        height: parent.height
                        width: 3
                    }
                }
            }

            // Library: a few rows
            Repeater {
                model: 3

                Rectangle {
                    required property int index

                    color: index === 0 ? preview.pal.accent : preview.pal.darkGray2
                    height: 4
                    radius: 1
                    width: preview.width - 10 - index * 14
                    x: 5
                    y: 54 + index * 6
                }
            }
        }
    }

    // One card per profile: mini screen, name, one line; tap to choose it
    component ProfileCard: Rectangle {
        id: card

        required property string profile
        readonly property bool active: Mixxx.Config.interfaceProfile === profile
        readonly property bool italian: Qt.locale().name.startsWith("it")
        // Three cards side by side when there is room, else one under the other
        property bool wide: false

        border.color: active ? Theme.accentColor : Theme.deckLineColor
        border.width: active ? 2 : 1
        color: Theme.darkGray3
        implicitHeight: layout.implicitHeight + 20
        radius: 10

        GridLayout {
            id: layout

            anchors.left: parent.left
            anchors.margins: 10
            anchors.right: parent.right
            anchors.top: parent.top
            columnSpacing: 12
            columns: card.wide ? 1 : 2
            rowSpacing: 8

            Preview {
                Layout.alignment: card.wide ? Qt.AlignHCenter : Qt.AlignTop
                profile: card.profile
            }
            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        color: Theme.white
                        font.bold: true
                        font.pixelSize: 17
                        text: Theme.profileName(card.profile)
                    }
                    Rectangle {
                        color: Theme.accent
                        implicitHeight: activeLabel.implicitHeight + 4
                        implicitWidth: activeLabel.implicitWidth + 12
                        radius: 8
                        visible: card.active

                        Text {
                            id: activeLabel

                            anchors.centerIn: parent
                            color: Theme.white
                            font.bold: true
                            font.letterSpacing: 0.8
                            font.pixelSize: 10
                            text: card.italian ? "IN USO" : "IN USE"
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
                Text {
                    Layout.fillWidth: true
                    color: Theme.midGray
                    font.pixelSize: 13
                    text: Theme.profileDescription(card.profile, card.italian)
                    wrapMode: Text.WordWrap
                }
            }
        }
        TapHandler {
            onTapped: Mixxx.Config.interfaceProfile = card.profile
        }
    }

    Flickable {
        id: flickable

        readonly property bool italian: Qt.locale().name.startsWith("it")
        // Three cards side by side when there is room
        readonly property bool wideLayout: width >= 520

        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentHeight: pageColumn.implicitHeight + 16

        ScrollBar.vertical: ScrollBar {
        }

        Column {
            id: pageColumn

            spacing: 10
            width: flickable.width - 12

            Text {
                color: Theme.lightGray
                font.pixelSize: 13
                text: flickable.italian ? "Scegli l'aspetto dell'interfaccia: colori e onde cambiano subito, senza riavviare. La scelta resta salvata." : "Choose how the interface looks: colors and waveforms change at once, no restart. The choice is kept."
                width: parent.width
                wrapMode: Text.WordWrap
            }
            GridLayout {
                columnSpacing: 10
                columns: flickable.wideLayout ? 3 : 1
                rowSpacing: 10
                width: parent.width

                Repeater {
                    model: Theme.profileIds

                    ProfileCard {
                        required property string modelData

                        Layout.alignment: Qt.AlignTop
                        Layout.fillWidth: true
                        Layout.preferredWidth: 100
                        profile: modelData
                        wide: flickable.wideLayout
                    }
                }
            }
        }
    }
}
