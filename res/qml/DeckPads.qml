import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

// What the pads of the controller do right now, for one deck (interface
// profiles Club and Stack; Uboros has only the small DeckPadChip). The pad
// mode comes from the controller script ([ChannelN],pad_mode: 0 hot cue,
// 1 beat loop, 2 beat jump, 3 sampler, 4 key shift, 5 pad fx 1, 6 pad fx 2,
// 7 keyboard/stems, -1 = unknown or no such controller: nothing is shown and
// nothing here is created); the state of the eight pads is read from the usual
// Mixxx controls (hot cue status and color, loop, samplers, pitch_adjust...).
// Only text and rectangles that change when a control changes: no timers, no
// animations. The controls of a mode exist only while that mode is shown.
//
// Club: the mode name and eight cells in every mode, as wide as the item.
// Stack (compact): small cells over the waveform, only for HOT CUE and BEAT
// LOOP; the other modes show their name only.
Item {
    id: root

    required property string group
    property bool compact: false

    readonly property int stripHeight: 14
    readonly property int cellGap: compact ? 2 : 3
    readonly property int labelWidth: compact ? modeLabel.implicitWidth : 58
    readonly property real cellWidth: compact ? 18 : Math.max(10, (width - labelWidth - 8 * cellGap) / 8)
    readonly property int mode: padModeControl.initialized ? Math.round(padModeControl.value) : -1
    readonly property bool shown: mode >= 0 && mode <= 7
    readonly property bool cellsShown: shown && (!compact || mode <= 1)
    readonly property var modeNames: ["HOT CUE", "BEAT LOOP", "BEAT JUMP", "SAMPLER", "KEY SHIFT", "PAD FX 1", "PAD FX 2", "STEMS"]
    readonly property var modeTints: ["#ff9a3c", "#4ee04e", "#4ec3ff", "#ff7ad9", "#c38bff", "#ffd24a", "#ffd24a", "#33c8ff"]
    readonly property color modeTint: shown ? modeTints[mode] : "#ffffff"
    // Chromatic transposition shown by the KEY SHIFT label (the pads reach +-3)
    readonly property int pitchSteps: Math.round(pitchControl.value)
    // Pad FX: label of each pad, and the control that says it is on: the
    // effect slot of the pad unit (slot number, 0 = none) or the roll size
    readonly property var fxLabels: [["ECHO", "REVERB", "FLANGE", "CRUSH", "ROLL 1/2", "ROLL 1/4", "ROLL 1/8", "BRAKE"], ["ECHO+REV", "FLG+REV", "CRS+ECHO", "ROLL 1", "ROLL 1/16", "ROLL 2", "SPINBACK", "ECHO OUT"]]
    readonly property var fxSlots: [[1, 2, 3, 4, 0, 0, 0, 0], [1, 3, 4, 0, 0, 0, 0, 0]]
    readonly property var fxRolls: [["", "", "", "", "0.5", "0.25", "0.125", ""], ["", "", "", "1", "0.0625", "2", "", ""]]
    // Effect unit of the pad FX of this deck (the script uses unit 2 for deck 1 and unit 3 for deck 2)
    readonly property string fxUnit: group === "[Channel1]" ? "[EffectRack1_EffectUnit2" : "[EffectRack1_EffectUnit3"
    // [ChannelN] without the brackets, for the stem groups
    readonly property string channel: group.substring(1, group.length - 1)

    function sizeText(size) {
        return size >= 1 ? String(Math.round(size)) : "1/" + Math.round(1 / size);
    }

    // Pad of this deck (0..7) -> sampler number: deck 1 has samplers 1-4 and
    // 9-12, deck 2 has 5-8 and 13-16 (the DDJ-FLX4 mapping)
    function samplerNumber(pad) {
        return (pad < 4 ? 1 : 9) + (group === "[Channel2]" ? 4 : 0) + pad % 4;
    }

    function cellsComponent(m) {
        switch (m) {
        case 0:
            return hotcueCells;
        case 1:
            return loopCells;
        case 2:
            return jumpCells;
        case 3:
            return samplerCells;
        case 4:
            return keyCells;
        case 5:
        case 6:
            return fxCells;
        case 7:
            return stemCells;
        }
        return null;
    }

    visible: shown
    implicitHeight: stripHeight
    implicitWidth: compact ? labelWidth + (cellsShown ? cellGap + 8 * cellWidth + 7 * cellGap : 0) : 0

    Mixxx.ControlProxy {
        id: padModeControl

        group: root.group
        key: "pad_mode"
    }

    Mixxx.ControlProxy {
        id: pitchControl

        group: root.group
        key: "pitch_adjust"
    }

    // A pad: 0 = nothing there (grey), 1 = there but not on, 2 = on (filled
    // with its color); ring = a white outline (hot cue active, e.g. a saved
    // loop that is looping)
    component Cell: Rectangle {
        id: cell

        property string label: ""
        property int level: 0
        property color tint: "#ffffff"
        property bool ring: false
        readonly property color ink: (0.299 * tint.r + 0.587 * tint.g + 0.114 * tint.b) > 0.6 ? "#0b0c0f" : "#ffffff"

        radius: 3
        color: level === 2 ? tint : (level === 1 ? Qt.darker(tint, 3.4) : Qt.rgba(1, 1, 1, 0.05))
        border.width: 1
        border.color: ring ? "#ffffff" : (level === 2 ? Qt.lighter(tint, 1.25) : (level === 1 ? Qt.darker(tint, 1.9) : Qt.rgba(1, 1, 1, 0.16)))

        Text {
            anchors.centerIn: parent
            width: parent.width - 2
            horizontalAlignment: Text.AlignHCenter
            text: cell.label
            font.family: Theme.fontFamily
            font.pixelSize: 9
            font.bold: true
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 6
            color: cell.level === 2 ? cell.ink : (cell.level === 1 ? Qt.lighter(cell.tint, 1.15) : Qt.rgba(1, 1, 1, 0.28))
        }
    }

    // The mode name
    Rectangle {
        id: modeLabel

        visible: root.shown
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: root.compact ? modeText.implicitWidth + 10 : root.labelWidth
        implicitWidth: modeText.implicitWidth + 10
        height: root.stripHeight
        radius: 3
        color: root.compact ? Qt.rgba(0, 0, 0, 0.6) : Qt.darker(root.modeTint, 3.2)
        border.width: 1
        border.color: root.modeTint

        Text {
            id: modeText

            anchors.centerIn: parent
            text: root.mode === 4 ? "KEY " + (root.pitchSteps > 0 ? "+" : "") + root.pitchSteps : (root.shown ? root.modeNames[root.mode] : "")
            font.family: Theme.fontFamily
            font.pixelSize: 9
            font.bold: true
            color: root.modeTint
        }
    }

    // Over the waveform (Stack) the empty cells need a dark ground to be seen
    Rectangle {
        visible: root.compact && root.cellsShown
        x: cells.x - 2
        anchors.verticalCenter: parent.verticalCenter
        width: cells.width + 4
        height: root.stripHeight
        radius: 4
        color: Qt.rgba(0, 0, 0, 0.55)
    }

    Loader {
        id: cells

        active: root.cellsShown
        anchors.left: modeLabel.right
        anchors.leftMargin: root.cellGap
        anchors.right: root.compact ? undefined : parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: root.stripHeight
        sourceComponent: root.cellsComponent(root.mode)
    }

    // HOT CUE: A-H, the color of the cue; grey = empty
    Component {
        id: hotcueCells

        Row {
            spacing: root.cellGap

            Repeater {
                model: 8

                Cell {
                    id: cue

                    required property int index

                    width: root.cellWidth
                    height: root.stripHeight
                    label: String.fromCharCode(65 + index)
                    tint: cueColor.value < 0 ? Theme.deckActiveColor : "#" + cueColor.value.toString(16).padStart(6, "0")
                    level: cueStatus.value > 0 ? 2 : 0
                    ring: cueStatus.value > 1

                    Mixxx.ControlProxy {
                        id: cueStatus

                        group: root.group
                        key: "hotcue_" + (cue.index + 1) + "_status"
                    }

                    Mixxx.ControlProxy {
                        id: cueColor

                        group: root.group
                        key: "hotcue_" + (cue.index + 1) + "_color"
                    }
                }
            }
        }
    }

    // BEAT LOOP: the eight sizes of the pads, the active loop lit
    Component {
        id: loopCells

        Row {
            spacing: root.cellGap

            Mixxx.ControlProxy {
                id: loopEnabled

                group: root.group
                key: "loop_enabled"
            }

            Mixxx.ControlProxy {
                id: loopSize

                group: root.group
                key: "beatloop_size"
            }

            Repeater {
                model: [0.25, 0.5, 1, 2, 4, 8, 16, 32]

                Cell {
                    id: loopCell

                    required property real modelData

                    width: root.cellWidth
                    height: root.stripHeight
                    label: root.sizeText(modelData)
                    tint: "#4ee04e"
                    level: loopEnabled.value > 0 && Math.abs(loopSize.value - modelData) < 0.0001 ? 2 : 1
                }
            }
        }
    }

    // BEAT JUMP: -1 +1 -2 +2 -4 +4 -8 +8 times the unit the script keeps
    Component {
        id: jumpCells

        Row {
            spacing: root.cellGap

            Mixxx.ControlProxy {
                id: jumpUnit

                group: "[Skin]"
                key: "pad_beatjump_unit"
            }

            Repeater {
                model: 8

                Cell {
                    id: jumpCell

                    required property int index

                    width: root.cellWidth
                    height: root.stripHeight
                    label: (index % 2 === 0 ? "-" : "+") + root.sizeText((jumpUnit.value > 0 ? jumpUnit.value : 1) * [1, 1, 2, 2, 4, 4, 8, 8][index])
                    tint: "#4ec3ff"
                    level: 1
                }
            }
        }
    }

    // SAMPLER: lit while the sample plays, dim when it holds a track
    Component {
        id: samplerCells

        Row {
            spacing: root.cellGap

            Repeater {
                model: 8

                Cell {
                    id: sampleCell

                    required property int index

                    width: root.cellWidth
                    height: root.stripHeight
                    label: String(index + 1)
                    tint: "#ff7ad9"
                    level: samplePlay.value > 0 ? 2 : (sampleLoaded.value > 0 ? 1 : 0)

                    Mixxx.ControlProxy {
                        id: samplePlay

                        group: "[Sampler" + root.samplerNumber(sampleCell.index) + "]"
                        key: "play"
                    }

                    Mixxx.ControlProxy {
                        id: sampleLoaded

                        group: "[Sampler" + root.samplerNumber(sampleCell.index) + "]"
                        key: "track_loaded"
                    }
                }
            }
        }
    }

    // KEY SHIFT: the semitones of the pads (0 +1 +2 +3 -3 -2 -1 0), the one in use lit
    Component {
        id: keyCells

        Row {
            spacing: root.cellGap

            Repeater {
                model: [0, 1, 2, 3, -3, -2, -1, 0]

                Cell {
                    id: keyCell

                    required property real modelData

                    width: root.cellWidth
                    height: root.stripHeight
                    label: (modelData > 0 ? "+" : "") + modelData
                    tint: "#c38bff"
                    level: root.pitchSteps === modelData ? 2 : 1
                }
            }
        }
    }

    // PAD FX 1 / 2: the effect or roll of each pad, lit while it is on
    Component {
        id: fxCells

        Row {
            spacing: root.cellGap

            Repeater {
                model: 8

                Cell {
                    id: fxCell

                    required property int index
                    readonly property int bank: Math.max(0, Math.min(1, root.mode - 5))
                    readonly property int slot: root.fxSlots[bank][index]
                    readonly property string roll: root.fxRolls[bank][index]
                    readonly property bool hasState: slot > 0 || roll !== ""

                    width: root.cellWidth
                    height: root.stripHeight
                    label: root.fxLabels[bank][index]
                    tint: "#ffd24a"
                    level: hasState && fxState.value > 0 ? 2 : 1

                    // Pads without a state (RELEASE FX) read a control that
                    // exists anyway and ignore it
                    Mixxx.ControlProxy {
                        id: fxState

                        group: fxCell.slot > 0 ? root.fxUnit + "_Effect" + fxCell.slot + "]" : root.group
                        key: fxCell.slot > 0 ? "enabled" : (fxCell.roll !== "" ? "beatlooproll_" + fxCell.roll + "_activate" : "pad_mode")
                    }
                }
            }
        }
    }

    // KEYBOARD: pads 1-4 the stems (lit = playing, dim = muted), pads 5-8 their effects
    Component {
        id: stemCells

        Row {
            spacing: root.cellGap

            Mixxx.ControlProxy {
                id: stemCount

                group: root.group
                key: "stem_count"
            }

            Repeater {
                model: 8

                Cell {
                    id: stemCell

                    required property int index
                    readonly property int stem: index % 4 + 1
                    readonly property bool effect: index >= 4

                    width: root.cellWidth
                    height: root.stripHeight
                    label: (effect ? "FX" : "S") + stem
                    tint: "#33c8ff"
                    level: stemCount.value < stem ? 0 : ((stemState.value > 0) === effect ? 2 : 1)

                    Mixxx.ControlProxy {
                        id: stemState

                        group: stemCell.effect ? "[QuickEffectRack1_[" + root.channel + "_Stem" + stemCell.stem + "]]" : "[" + root.channel + "_Stem" + stemCell.stem + "]"
                        key: stemCell.effect ? "enabled" : "mute"
                    }
                }
            }
        }
    }
}
