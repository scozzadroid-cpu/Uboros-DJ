import QtQuick 2.12

Item {
    id: root

    required property string leftDeckGroup
    required property string rightDeckGroup
    property alias mixer: mixer
    property bool minimized: false
    // Interface profiles Club / Stack: the overview of the decks is on the
    // waveforms (WaveformDisplay.qml), the minimized row is the info bar only
    property bool overviewOnWave: false

    implicitHeight: mixer.height
    states: [
        State {
            when: root.minimized
            name: "minimized"

            PropertyChanges {
                target: mixer
                visible: false
            }

            PropertyChanges {
                target: root
                // Info bar + the thin track overview (see Deck.qml); Club:
                // the info bar only (the overview is under the waveform)
                implicitHeight: root.overviewOnWave ? 66 : 97
            }

            AnchorChanges {
                target: leftDeck
                anchors.right: mixer.horizontalCenter
            }

            AnchorChanges {
                target: rightDeck
                anchors.left: mixer.horizontalCenter
            }

        },
        State {
            // This State can't be deduplicated by making the first one
            // reversible, because for decks 3/4 the mixer may already be
            // hidden (since the whole deck row is already hidden). In that
            // case, disabling the minimized state would not show the mixer
            // again.
            when: !root.minimized
            name: "maximized"

            PropertyChanges {
                target: mixer
                visible: true
            }

            PropertyChanges {
                target: root
                implicitHeight: mixer.height
            }

            AnchorChanges {
                target: leftDeck
                anchors.right: mixer.left
            }

            AnchorChanges {
                target: rightDeck
                anchors.left: mixer.right
            }
        }
    ]

    Deck {
        id: leftDeck

        minimized: root.minimized
        overviewOnWave: root.overviewOnWave
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        group: root.leftDeckGroup
    }

    Mixer {
        id: mixer

        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        leftDeckGroup: root.leftDeckGroup
        rightDeckGroup: root.rightDeckGroup

        FadeBehavior on visible {
            fadeTarget: mixer
        }
    }

    Deck {
        id: rightDeck

        minimized: root.minimized
        overviewOnWave: root.overviewOnWave
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        group: root.rightDeckGroup
    }

    transitions: Transition {
        to: "minimized"
        reversible: true

        SequentialAnimation {
            AnchorAnimation {
                targets: [leftDeck, rightDeck]
                duration: 150
            }

            PropertyAnimation {
                target: root
                property: "implicitHeight"
                duration: 150
            }
        }
    }
}
