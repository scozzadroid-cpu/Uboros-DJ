import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "Theme"

// BEAT FX of the DDJ-FLX4 (effect unit 1) in the toolbar: the effect armed
// by BEAT FX SELECT / BEAT ◀▶, whether it is on, the channels of the CH
// SELECT switch and the LEVEL/DEPTH. A tap switches the effect on/off like
// the BEAT FX ON/OFF button.
Rectangle {
    id: root

    readonly property string unitGroup: "[EffectRack1_EffectUnit1]"
    readonly property int focused: Math.max(1, focusedControl.value)
    readonly property string slotGroup: "[EffectRack1_EffectUnit1_Effect" + focused + "]"
    readonly property var effectSlot: Mixxx.EffectsManager.getEffectSlot(1, focused)
    readonly property bool on: enabledControl.value > 0
    readonly property string effectName: {
        const id = effectSlot ? effectSlot.effectId : "";
        if (!id)
            return qsTr("no effect");
        const model = Mixxx.EffectsManager.visibleEffectsModel;
        for (let i = 0; model && i < model.rowCount(); i++) {
            const entry = model.get(i);
            if (entry && entry.effectId === id)
                return entry.display;
        }
        const last = id.split(/[.#\/]/).pop();
        return last.charAt(0).toUpperCase() + last.slice(1);
    }
    readonly property string channels: {
        const ch1 = ch1Control.value > 0;
        const ch2 = ch2Control.value > 0;
        if (ch1 && ch2)
            return "1&2";
        if (ch1)
            return "CH1";
        if (ch2)
            return "CH2";
        return "—";
    }

    // Natural width (all the parts); the toolbar may give less: then the label
    // "BEAT FX" is dropped and the effect name is shortened (nothing here depends
    // on root.width, so no binding loop)
    // ("ON" only shows while the effect is on: off is the grey frame, no word needed)
    // Rounded up with 2 px to spare: an int that drops a fraction of a pixel
    // made the name a hair too narrow and "Echo" became "Ec…"
    readonly property int fullWidth: Math.ceil(labelText.implicitWidth + Math.ceil(nameMetrics.advanceWidth)
            + (on ? stateText.implicitWidth + row.spacing : 0) + channelsText.implicitWidth + 2 * row.spacing + 20) + 2

    implicitWidth: fullWidth
    implicitHeight: 30
    radius: 6
    color: on ? "#2f6b1f" : Theme.darkGray2
    border.width: 1
    border.color: on ? "#4ee04e" : "#6b6b6b"

    Mixxx.ControlProxy {
        id: focusedControl

        group: root.unitGroup
        key: "focused_effect"
    }

    Mixxx.ControlProxy {
        id: enabledControl

        group: root.slotGroup
        key: "enabled"
    }

    Mixxx.ControlProxy {
        id: ch1Control

        group: root.unitGroup
        key: "group_[Channel1]_enable"
    }

    Mixxx.ControlProxy {
        id: ch2Control

        group: root.unitGroup
        key: "group_[Channel2]_enable"
    }

    Mixxx.ControlProxy {
        id: mixControl

        group: root.unitGroup
        key: "mix"
    }

    Mixxx.ControlProxy {
        id: slot1Enabled

        group: "[EffectRack1_EffectUnit1_Effect1]"
        key: "enabled"
    }

    Mixxx.ControlProxy {
        id: slot2Enabled

        group: "[EffectRack1_EffectUnit1_Effect2]"
        key: "enabled"
    }

    Mixxx.ControlProxy {
        id: slot3Enabled

        group: "[EffectRack1_EffectUnit1_Effect3]"
        key: "enabled"
    }

    // Natural width of the effect name, measured apart: the Text below has an
    // explicit width and elides, so its implicitWidth can follow the shortened text
    TextMetrics {
        id: nameMetrics

        font: nameText.font
        text: root.effectName
    }

    Row {
        id: row

        anchors.centerIn: parent
        spacing: 8

        Text {
            id: labelText

            visible: root.width >= root.fullWidth
            text: "BEAT FX"
            color: Theme.textMutedColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize - 3
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            id: nameText

            // Shortened (with ...) only when the toolbar is too narrow
            width: Math.max(24, Math.min(Math.ceil(nameMetrics.advanceWidth), root.width - 20
                    - (labelText.visible ? labelText.implicitWidth + row.spacing : 0)
                    - (root.on ? stateText.implicitWidth + row.spacing : 0)
                    - channelsText.implicitWidth - row.spacing))
            elide: Text.ElideRight
            text: root.effectName
            color: root.on ? "#b9ff9e" : Theme.white
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            id: stateText

            visible: root.on
            text: "ON"
            color: "#4ee04e"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize - 2
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            id: channelsText

            text: root.channels + "  " + Math.round(mixControl.value * 100) + "%"
            color: Theme.textMutedColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize - 2
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        // One BEAT FX at a time (as on the FLX4)
        onClicked: {
            const turnOn = !root.on;
            if (turnOn) {
                slot1Enabled.value = 0;
                slot2Enabled.value = 0;
                slot3Enabled.value = 0;
            }
            enabledControl.value = turnOn ? 1 : 0;
        }
    }
}
