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

    implicitWidth: row.implicitWidth + 20
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

    Row {
        id: row

        anchors.centerIn: parent
        spacing: 8

        Text {
            text: "BEAT FX"
            color: Theme.textMutedColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize - 3
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.effectName
            color: root.on ? "#b9ff9e" : Theme.white
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize
            font.bold: true
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.on ? "ON" : "armed"
            color: root.on ? "#4ee04e" : Theme.textMutedColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize - 2
            font.bold: root.on
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.channels + "  " + Math.round(mixControl.value * 100) + "%"
            color: Theme.textMutedColor
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize - 2
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: enabledControl.value = root.on ? 0 : 1
    }
}
