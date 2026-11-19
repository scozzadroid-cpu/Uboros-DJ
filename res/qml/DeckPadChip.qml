import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

// Interface profile Uboros: ONE small chip in the info bar of the deck with the
// active pad mode of the controller and, where the mode has one, its value:
// loop size, range of the jump sizes, semitones. Shown only when the controller
// reports its pad mode ([ChannelN],pad_mode, see DeckPads.qml: -1 = unknown or
// no such controller, then the chip is not there and the info bar is unchanged).
Skin.StatusChip {
    id: root

    required property string group
    // false where the deck shows its pads in a bigger way (DeckPads.qml)
    property bool allowed: true
    readonly property int mode: padModeControl.initialized ? Math.round(padModeControl.value) : -1
    readonly property var tints: ["#ff9a3c", "#4ee04e", "#4ec3ff", "#ff7ad9", "#c38bff", "#ffd24a", "#ffd24a", "#33c8ff"]

    function sizeText(size) {
        return size >= 1 ? String(Math.round(size)) : "1/" + Math.round(1 / size);
    }

    active: allowed && mode >= 0 && mode <= 7
    tint: active ? tints[mode] : "#ffffff"
    text: {
        switch (mode) {
        case 0:
            return "HOT CUE";
        case 1:
            return "LOOP " + sizeText(loopSizeControl.value);
        case 2:
            // The pads jump 1, 2, 4 and 8 times the unit of the script
            return "JUMP " + sizeText(jumpUnitControl.value > 0 ? jumpUnitControl.value : 1) + "-" + sizeText(8 * (jumpUnitControl.value > 0 ? jumpUnitControl.value : 1));
        case 3:
            return "SAMPLER";
        case 4:
            return "KEY " + (Math.round(pitchControl.value) > 0 ? "+" : "") + Math.round(pitchControl.value);
        case 5:
            return "FX 1";
        case 6:
            return "FX 2";
        case 7:
            return "STEMS";
        }
        return "";
    }

    Mixxx.ControlProxy {
        id: padModeControl

        group: root.group
        key: "pad_mode"
    }

    Mixxx.ControlProxy {
        id: loopSizeControl

        group: root.group
        key: "beatloop_size"
    }

    Mixxx.ControlProxy {
        id: jumpUnitControl

        group: "[Skin]"
        key: "pad_beatjump_unit"
    }

    Mixxx.ControlProxy {
        id: pitchControl

        group: root.group
        key: "pitch_adjust"
    }
}
