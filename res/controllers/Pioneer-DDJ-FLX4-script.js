// Pioneer-DDJ-FLX4-script.js
// ****************************************************************************
// * Mixxx mapping script file for the Pioneer DDJ-FLX4.
// * Mostly adapted from the DDJ-400 mapping script
// * Authors: Warker, nschloe, dj3730, jusko, Robert904
// ****************************************************************************
//
//  Implemented (as per manufacturer's manual):
//      * Mixer Section (Faders, EQ, Filter, Gain, Cue)
//      * Browsing and loading + Waveform zoom (shift)
//      * Jogwheels, Scratching, Bending, Loop adjust
//      * Cycle Temporange
//      * Beat Sync
//      * Hot Cue Mode
//      * Beat Loop Mode
//      * Beat Jump Mode
//      * Sampler Mode
//      * Keyshift mode
//
//  Custom (Mixxx specific mappings):
//      * BeatFX: Assigned Effect Unit 1
//                v FX_SELECT Load next effect.
//                SHIFT + v FX_SELECT Load previous effect.
//                < LEFT Cycle effect focus leftward
//                > RIGHT Cycle effect focus rightward
//                ON/OFF toggles focused effect slot
//                SHIFT + ON/OFF disables all three effect slots.
//
//      * 32 beat jump forward & back (Shift + </> CUE/LOOP CALL arrows)
//      * Toggle quantize (Shift + channel cue)
//      * Stems selection using PADs (using controller's Keyboard mode)
//
//      * BROWSE / LOAD (rekordbox / CDJ style navigation, in the XML):
//                BROWSE turn      scroll the list or the sources (the first turn
//                                 shows the library if it is hidden: browseRotate)
//                BROWSE press     move the cursor between the sources and the
//                                 track list / open the source ([Library],
//                                 MoveFocusForward). It never loads a track
//                SHIFT + BROWSE turn   waveform zoom
//                SHIFT + BROWSE press  back ([Library],MoveLeft): from the list to
//                                 the sources, on the sources close the open
//                                 source / go up to the parent
//                SHIFT + LOAD 1   library full screen ([Master],maximize_library)
//                SHIFT + LOAD 2   waveform view / show-hide the library
//                                 ([Skin],show_library; it used to be SHIFT +
//                                 BROWSE press)
//
//  Not implemented (after discussion and trial attempts):
//      * Loop Section:
//        * -4BEAT auto loop (hacky---prefer a clean way to set a 4 beat loop
//                            from a previous position on long press)
//
//        * CUE/LOOP CALL - memory & delete (complex and not useful. Hot cues are sufficient)
//
//      * Secondary pad modes (trial attempts complex and too experimental)
//        * Keyboard mode
//
//      * PAD FX1 / PAD FX2: momentary effects on the deck while a pad is held
//        (effect unit 2 = deck 1, unit 3 = deck 2), see PioneerDDJFLX4.padFx.
//        Pad 8 of each bank is a RELEASE FX that ends the track.
//
//      * SMART FADER: crossfader on/off (LED always off: it would switch on the
//        firmware Smart Fader, which fakes EQ LOW from the fader). Off: decks 1
//        and 2 are not affected by the crossfader, so moving it by mistake does
//        nothing; on: the previous crossfader assignment is restored.
//
//      * SMART CFX held + EQ HIGH/MID/LOW = stem volumes (vocals / melody / drums);
//        double press of SMART CFX = all stems back to full volume
//
//  Not implemented yet (but might be in the future):

var PioneerDDJFLX4 = {};

PioneerDDJFLX4.lights = {
    smartFader: {
        status: 0x96,
        data1: 0x01,
    },
    beatFx: {
        status: 0x94,
        data1: 0x47,
    },
    shiftBeatFx: {
        status: 0x94,
        data1: 0x43,
    },
    deck1: {
        vuMeter: {
            status: 0xB0,
            data1: 0x02,
        },
        playPause: {
            status: 0x90,
            data1: 0x0B,
        },
        shiftPlayPause: {
            status: 0x90,
            data1: 0x47,
        },
        cue: {
            status: 0x90,
            data1: 0x0C,
        },
        shiftCue: {
            status: 0x90,
            data1: 0x48,
        },
        hotcueMode: {
            status: 0x90,
            data1: 0x1B,
        },
        keyboardMode: {
            status: 0x90,
            data1: 0x69,
        },
        padFX1Mode: {
            status: 0x90,
            data1: 0x1E,
        },
        padFX2Mode: {
            status: 0x90,
            data1: 0x6B,
        },
        beatJumpMode: {
            status: 0x90,
            data1: 0x20,
        },
        beatLoopMode: {
            status: 0x90,
            data1: 0x6D,
        },
        samplerMode: {
            status: 0x90,
            data1: 0x22,
        },
        keyShiftMode: {
            status: 0x90,
            data1: 0x6F,
        },
    },
    deck2: {
        vuMeter: {
            status: 0xB1,
            data1: 0x02,
        },
        playPause: {
            status: 0x91,
            data1: 0x0B,
        },
        shiftPlayPause: {
            status: 0x91,
            data1: 0x47,
        },
        cue: {
            status: 0x91,
            data1: 0x0C,
        },
        shiftCue: {
            status: 0x91,
            data1: 0x48,
        },
        hotcueMode: {
            status: 0x91,
            data1: 0x1B,
        },
        keyboardMode: {
            status: 0x91,
            data1: 0x69,
        },
        padFX1Mode: {
            status: 0x91,
            data1: 0x1E,
        },
        padFX2Mode: {
            status: 0x91,
            data1: 0x6B,
        },
        beatJumpMode: {
            status: 0x91,
            data1: 0x20,
        },
        beatLoopMode: {
            status: 0x91,
            data1: 0x6D,
        },
        samplerMode: {
            status: 0x91,
            data1: 0x22,
        },
        keyShiftMode: {
            status: 0x91,
            data1: 0x6F,
        },
    },
};

// Store timer IDs
PioneerDDJFLX4.timers = {};

// Keep alive timer
PioneerDDJFLX4.sendKeepAlive = function() {
    midi.sendSysexMsg([0xF0, 0x00, 0x40, 0x05, 0x00, 0x00, 0x04, 0x05, 0x00, 0x50, 0x02, 0xf7], 12); // This was reverse engineered with Wireshark
};

// Jog wheel constants
PioneerDDJFLX4.vinylMode = true;
PioneerDDJFLX4.alpha = 1.0/8;
PioneerDDJFLX4.beta = PioneerDDJFLX4.alpha/32;

// Multiplier for fast seek through track using SHIFT+JOGWHEEL
PioneerDDJFLX4.fastSeekScale = 150;
// Pitch bend with the side of the jog wheel: half of the stock 0.8, which
// was too coarse for nudging two synced tracks
PioneerDDJFLX4.bendScale = 0.4;

PioneerDDJFLX4.tempoRanges = [0.06, 0.10, 0.16, 0.25];

PioneerDDJFLX4.shiftButtonDown = [false, false];

// Jog wheel loop adjust
PioneerDDJFLX4.loopAdjustIn = [false, false];
PioneerDDJFLX4.loopAdjustOut = [false, false];
PioneerDDJFLX4.loopAdjustMultiply = 50;

// Beatjump pad (beatjump_size values)
PioneerDDJFLX4.beatjumpSizeForPad = {
    0x20: -1, // PAD 1
    0x21: 1,  // PAD 2
    0x22: -2, // PAD 3
    0x23: 2,  // PAD 4
    0x24: -4, // PAD 5
    0x25: 4,  // PAD 6
    0x26: -8, // PAD 7
    0x27: 8   // PAD 8
};

// Stems (KEYBOARD) pads mode status for deck 1 and 2, without or with SHIFT pressed
PioneerDDJFLX4.stemsPadsModesStatus = {
    "[Channel1]": [0x97, 0x98],
    "[Channel2]": [0x99, 0x9a],
};

// Stems (KEYBOARD) pad 1 control (pad control = [this value] + [pad  number] - 1)
PioneerDDJFLX4.stemMutePadsFirstControl = 0x40;

// Stems (KEYBOARD) pad 5 control (pad control = [this value] + [pad  number] - 1)
PioneerDDJFLX4.stemFxPadsFirstControl = 0x44;

// Pitch shift (KEY SHIFT) pads mode status for deck 1 and 2, without or with SHIFT pressed
PioneerDDJFLX4.pitchPadsModesStatus = {
    "[Channel1]": [0x97, 0x98],
    "[Channel2]": [0x99, 0x9a],
};

// Pitch shift (KEY SHIFT) pad 1 control (pad control = [this value] + [pad  number] - 1)
PioneerDDJFLX4.pitchPadsFirstControl = 0x70;

PioneerDDJFLX4.quickJumpSize = 32;

// Used for tempo slider
PioneerDDJFLX4.highResMSB = {
    "[Channel1]": {},
    "[Channel2]": {}
};

PioneerDDJFLX4.trackLoadedLED = function(value, group, _control) {
    midi.sendShortMsg(
        0x9F,
        group.match(script.channelRegEx)[1] - 1,
        value > 0 ? 0x7F : 0x00
    );
};

PioneerDDJFLX4.toggleLight = function(midiIn, active) {
    midi.sendShortMsg(midiIn.status, midiIn.data1, active ? 0x7F : 0);
};

//
// Init
//

PioneerDDJFLX4.init = function() {
    PioneerDDJFLX4.crossfaderActive = true;
    // Never light SMART FADER: the LED message switches on the FLX4's own
    // Smart Fader, and the firmware then sends fake EQ LOW (and filter)
    // values that follow the channel fader: below ~63% of the fader the
    // bass goes down to a full cut at ~33% (lower volume, VU following the
    // fader). Off at startup in case it was left on.
    PioneerDDJFLX4.toggleLight(PioneerDDJFLX4.lights.smartFader, false);
    engine.setValue("[EffectRack1_EffectUnit1]", "show_focus", 1);
    PioneerDDJFLX4.initPadFx();

    engine.makeConnection("[Channel1]", "vu_meter", PioneerDDJFLX4.vuMeterUpdate);
    engine.makeConnection("[Channel2]", "vu_meter", PioneerDDJFLX4.vuMeterUpdate);

    PioneerDDJFLX4.toggleLight(PioneerDDJFLX4.lights.deck1.vuMeter, false);
    PioneerDDJFLX4.toggleLight(PioneerDDJFLX4.lights.deck2.vuMeter, false);

    engine.softTakeover("[Channel1]", "rate", true);
    engine.softTakeover("[Channel2]", "rate", true);
    engine.softTakeover("[EffectRack1_EffectUnit1_Effect1]", "meta", true);
    engine.softTakeover("[EffectRack1_EffectUnit1_Effect2]", "meta", true);
    engine.softTakeover("[EffectRack1_EffectUnit1_Effect3]", "meta", true);
    // LEVEL/DEPTH = dry/wet, absolute like the console (no soft takeover:
    // with it, Mixxx kept a saved 100% while the knob sat at 0, and BEAT FX
    // played at full level). The FLX4 does not report the knob at startup,
    // so the level starts at 0 until the knob is moved.
    engine.softTakeover("[EffectRack1_EffectUnit1]", "mix", false);
    engine.setValue("[EffectRack1_EffectUnit1]", "mix", 0);
    for (let deck = 1; deck <= 2; deck++) {
        for (let p = 1; p <= 3; p++) {
            engine.softTakeover("[EqualizerRack1_[Channel" + deck + "]_Effect1]", "parameter" + p, true);
        }
        PioneerDDJFLX4.applyFilterResonance("[QuickEffectRack1_[Channel" + deck + "]]");
        // A new track (re)loads the Quick Effect preset: set the Q again
        engine.makeConnection("[Channel" + deck + "]", "track_loaded", function() {
            PioneerDDJFLX4.applyFilterResonance("[QuickEffectRack1_[Channel" + deck + "]]");
        });
    }
    // Until the CH SELECT switch is moved its position is unknown: BEAT FX
    // on both decks (1&2) rather than on none.
    if (!engine.getValue("[EffectRack1_EffectUnit1]", "group_[Channel1]_enable") &&
            !engine.getValue("[EffectRack1_EffectUnit1]", "group_[Channel2]_enable")) {
        engine.setValue("[EffectRack1_EffectUnit1]", "group_[Channel1]_enable", 1);
        engine.setValue("[EffectRack1_EffectUnit1]", "group_[Channel2]_enable", 1);
    }
    PioneerDDJFLX4.focusedFxGroup();
    for (let slot = 1; slot <= 3; slot++) {
        const slotGroup = "[EffectRack1_EffectUnit1_Effect" + slot + "]";
        PioneerDDJFLX4.setBeatFxMeta(0, slotGroup);
        engine.makeConnection(slotGroup, "loaded_effect", PioneerDDJFLX4.setBeatFxMeta);
    }
    engine.setValue("[EffectRack1_EffectUnit1]", "enabled", 1);

    const samplerCount = 16;
    if (engine.getValue("[App]", "num_samplers") < samplerCount) {
        engine.setValue("[App]", "num_samplers", samplerCount);
    }
    for (let i = 1; i <= samplerCount; ++i) {
        engine.makeConnection("[Sampler" + i + "]", "play", PioneerDDJFLX4.samplerPlayOutputCallbackFunction);
    }

    engine.makeConnection("[Channel1]", "track_loaded", PioneerDDJFLX4.trackLoadedLED);
    engine.makeConnection("[Channel2]", "track_loaded", PioneerDDJFLX4.trackLoadedLED);

    // play the "track loaded" animation on both decks at startup
    midi.sendShortMsg(0x9F, 0x00, 0x7F);
    midi.sendShortMsg(0x9F, 0x01, 0x7F);

    PioneerDDJFLX4.setLoopButtonLights(0x90, 0x7F);
    PioneerDDJFLX4.setLoopButtonLights(0x91, 0x7F);

    engine.makeConnection("[Channel1]", "loop_enabled", PioneerDDJFLX4.loopToggle);
    engine.makeConnection("[Channel2]", "loop_enabled", PioneerDDJFLX4.loopToggle);

    for (i = 1; i <= 3; i++) {
        engine.makeConnection("[EffectRack1_EffectUnit1_Effect" + i +"]", "enabled", PioneerDDJFLX4.toggleFxLight);
    }
    engine.makeConnection("[EffectRack1_EffectUnit1]", "focused_effect", PioneerDDJFLX4.toggleFxLight);

    // Register callbacks for each deck, when a file is loaded and the number of stems is available
    engine.makeConnection("[Channel1]", "stem_count", PioneerDDJFLX4.stemCountChanged);
    engine.makeConnection("[Channel2]", "stem_count", PioneerDDJFLX4.stemCountChanged);

    // Register callbacks for each stems of each decks, to change pad lights when muted/unmuted/FX
    for (let stem=1; stem<=4; stem++) {
        for (let deck=1; deck<=2; deck++) {
            engine.makeConnection(`[Channel${deck}_Stem${stem}]`, "mute", PioneerDDJFLX4.stemMuteChanged);
            engine.makeConnection(`[QuickEffectRack1_[Channel${deck}_Stem${stem}]]`, "enabled", PioneerDDJFLX4.stemFxChanged);
        }
    }

    // Register callbacks for each deck, when a file is loaded to reset pitch shift
    engine.makeConnection("[Channel1]", "track_loaded", PioneerDDJFLX4.pitchAdjusted);
    engine.makeConnection("[Channel2]", "track_loaded", PioneerDDJFLX4.pitchAdjusted);

    // Register callbacks for each deck, when the pitch shift is modified
    engine.makeConnection("[Channel1]", "pitch_adjust", PioneerDDJFLX4.pitchAdjusted);
    engine.makeConnection("[Channel2]", "pitch_adjust", PioneerDDJFLX4.pitchAdjusted);

    PioneerDDJFLX4.keepAliveTimer = engine.beginTimer(200, PioneerDDJFLX4.sendKeepAlive);

    // query the controller for current control positions on startup
    PioneerDDJFLX4.sendKeepAlive(); // the query seems to double as a keep alive message
};

//
// Waveform zoom
//

PioneerDDJFLX4.waveformZoom = function(midichan, control, value, status, group) {
    if (value === 0x7f) {
        script.triggerControl(group, "waveform_zoom_up", 100);
    } else {
        script.triggerControl(group, "waveform_zoom_down", 100);
    }
};

//
// Channel level lights
//

PioneerDDJFLX4.vuMeterUpdate = function(value, group) {
    const newVal = value * 127;

    switch (group) {
    case "[Channel1]":
        midi.sendShortMsg(0xB0, 0x02, newVal);
        break;

    case "[Channel2]":
        midi.sendShortMsg(0xB1, 0x02, newVal);
        break;
    }
};

//
// Effects
//

PioneerDDJFLX4.toggleFxLight = function(_value, _group, _control) {
    const enabled = engine.getValue(PioneerDDJFLX4.focusedFxGroup(), "enabled");

    PioneerDDJFLX4.toggleLight(PioneerDDJFLX4.lights.beatFx, enabled);
    PioneerDDJFLX4.toggleLight(PioneerDDJFLX4.lights.shiftBeatFx, enabled);
};

// Mixxx loads Echo, Reverb and Flanger with their meta knob at 0, where
// they are silent (Echo/Reverb send and Flanger mix follow the meta). BEAT
// FX slots get a strong depth at startup and whenever another effect is
// selected; LEVEL/DEPTH (dry/wet) and SHIFT + LEVEL/DEPTH (depth) reduce it.
PioneerDDJFLX4.beatFxDefaultMeta = 0.85;

PioneerDDJFLX4.setBeatFxMeta = function(_value, group) {
    // loaded_effect is an index: the effect itself is not readable from a
    // mapping, so the strong default applies to every effect
    engine.setParameter(group, "meta", PioneerDDJFLX4.beatFxDefaultMeta);
    engine.softTakeoverIgnoreNextValue(group, "meta");
};

PioneerDDJFLX4.focusedFxGroup = function() {
    let focusedFx = engine.getValue("[EffectRack1_EffectUnit1]", "focused_effect");
    // 0 = no focus: BEAT FX ON/OFF and LEVEL/DEPTH would address a slot
    // that does not exist ("_Effect0"), so they did nothing at all.
    if (focusedFx < 1) {
        focusedFx = 1;
        engine.setValue("[EffectRack1_EffectUnit1]", "focused_effect", 1);
    }
    return "[EffectRack1_EffectUnit1_Effect" + focusedFx + "]";
};

PioneerDDJFLX4.beatFxLevelDepthRotate = function(_channel, _control, value) {
    if (PioneerDDJFLX4.shiftButtonDown[0] || PioneerDDJFLX4.shiftButtonDown[1]) {
        engine.setParameter(PioneerDDJFLX4.focusedFxGroup(), "meta", value / 0x7F);
    } else {
        engine.softTakeoverIgnoreNextValue(PioneerDDJFLX4.focusedFxGroup(), "meta");
        engine.setParameter("[EffectRack1_EffectUnit1]", "mix", value / 0x7F);
    }
};

PioneerDDJFLX4.changeFocusedEffectBy = function(numberOfSteps) {
    PioneerDDJFLX4.beatFxAllOff();
    let focusedEffect = engine.getValue("[EffectRack1_EffectUnit1]", "focused_effect");

    // Convert to zero-based index
    focusedEffect -= 1;

    // Standard Euclidean modulo by use of two plain modulos
    const numberOfEffectsPerEffectUnit = 3;
    focusedEffect = (((focusedEffect + numberOfSteps) % numberOfEffectsPerEffectUnit) + numberOfEffectsPerEffectUnit) % numberOfEffectsPerEffectUnit;

    // Convert back to one-based index
    focusedEffect += 1;

    engine.setValue("[EffectRack1_EffectUnit1]", "focused_effect", focusedEffect);
};

// One BEAT FX at a time, like a DJM: switching on the armed effect switches
// the others off, and choosing another effect (BEAT FX SELECT) or slot
// (BEAT ◀▶) switches the running one off.
PioneerDDJFLX4.beatFxAllOff = function() {
    for (let i = 1; i <= 3; i++) {
        engine.setValue("[EffectRack1_EffectUnit1_Effect" + i + "]", "enabled", 0);
    }
};

PioneerDDJFLX4.beatFxSelectPressed = function(_channel, _control, value) {
    if (value === 0) { return; }

    PioneerDDJFLX4.beatFxAllOff();

    engine.setValue(PioneerDDJFLX4.focusedFxGroup(), "next_effect", value);
};

PioneerDDJFLX4.beatFxSelectShiftPressed = function(_channel, _control, value) {
    if (value === 0) { return; }

    PioneerDDJFLX4.beatFxAllOff();

    engine.setValue(PioneerDDJFLX4.focusedFxGroup(), "prev_effect", value);
};

PioneerDDJFLX4.beatFxLeftPressed = function(_channel, _control, value) {
    if (value === 0) { return; }

    PioneerDDJFLX4.changeFocusedEffectBy(-1);
};

PioneerDDJFLX4.beatFxRightPressed = function(_channel, _control, value) {
    if (value === 0) { return; }

    PioneerDDJFLX4.changeFocusedEffectBy(1);
};

PioneerDDJFLX4.beatFxOnOffPressed = function(_channel, _control, value) {
    if (value === 0) { return; }

    const toggleEnabled = !engine.getValue(PioneerDDJFLX4.focusedFxGroup(), "enabled");
    if (toggleEnabled) {
        PioneerDDJFLX4.beatFxAllOff();
    }
    engine.setValue(PioneerDDJFLX4.focusedFxGroup(), "enabled", toggleEnabled);
};

PioneerDDJFLX4.beatFxOnOffShiftPressed = function(_channel, _control, value) {
    if (value === 0) { return; }

    // Only switch the slots off: zeroing "mix" here left every later BEAT FX
    // silent until the LEVEL/DEPTH knob was moved past the soft takeover.
    for (let i = 1; i <= 3; i++) {
        engine.setValue("[EffectRack1_EffectUnit1_Effect" + i + "]", "enabled", 0);
    }
    PioneerDDJFLX4.toggleLight(PioneerDDJFLX4.lights.beatFx, false);
    PioneerDDJFLX4.toggleLight(PioneerDDJFLX4.lights.shiftBeatFx, false);
};

// BEAT FX CH SELECT (CH1 / CH2 / 1&2). The FLX4 reports the position of the
// switch with note-on/off messages: 0x94 0x10 = CH1, 0x95 0x11 = CH2; the 1&2
// position turns both off (or sends its own note). Every candidate message is
// bound here and the routing is derived from the whole state, so any of these
// variants ends in the right routing: CH1 -> deck 1, CH2 -> deck 2, otherwise
// both decks.
PioneerDDJFLX4.beatFxChannelState = {};

PioneerDDJFLX4.applyBeatFxRouting = function() {
    const state = PioneerDDJFLX4.beatFxChannelState;
    const ch1 = state["0x94 0x10"] === true;
    const ch2 = state["0x95 0x11"] === true || state["0x94 0x11"] === true;
    let deck1 = true;
    let deck2 = true;
    if (ch1 && !ch2) {
        deck2 = false;
    } else if (ch2 && !ch1) {
        deck1 = false;
    }
    for (const otherKey in state) {
        if (state[otherKey] === true && otherKey !== "0x94 0x10" &&
                otherKey !== "0x95 0x11" && otherKey !== "0x94 0x11") {
            // A dedicated 1&2 note
            deck1 = true;
            deck2 = true;
        }
    }
    const unit = "[EffectRack1_EffectUnit1]";
    engine.setValue(unit, "group_[Channel1]_enable", deck1 ? 1 : 0);
    engine.setValue(unit, "group_[Channel2]_enable", deck2 ? 1 : 0);
};

PioneerDDJFLX4.beatFxChannelSelect = function(_channel, control, value, status) {
    const key = "0x" + status.toString(16) + " 0x" + (control < 0x10 ? "0" : "") + control.toString(16);
    PioneerDDJFLX4.beatFxChannelState[key] = value > 0;
    // Warning level: the Android log keeps only warnings (switch position, rare)
    console.warn("BEAT FX CH SELECT " + key + " = " + value);
    PioneerDDJFLX4.applyBeatFxRouting();
};

// Old names kept for mappings that still reference them
PioneerDDJFLX4.beatFxChannel1 = PioneerDDJFLX4.beatFxChannelSelect;
PioneerDDJFLX4.beatFxChannel2 = PioneerDDJFLX4.beatFxChannelSelect;

//
// FILTER (CFX knob)
//
// The knob is turned into a cutoff frequency that moves in octaves (like a
// DJM): LPF 20 kHz -> 30 Hz on the left half, HPF 25 Hz -> 9 kHz on the right
// half, with a small dead zone in the middle and a curve that is finer near
// the centre. The Filter's LPF/HPF parameters use Mixxx's "logarithmic"
// scaler, which is a 40 dB taper over the LINEAR range 13..22050 Hz (half the
// knob is already ~2 kHz), so the frequency is converted with the inverse of
// that taper (ControlLogPotmeterBehavior). The Q is raised for a resonant
// sweep.
//

PioneerDDJFLX4.filter = {
    deadZone: 0.03,     // fraction of the knob around the centre with no filter
    curve: 1.3,         // > 1 = finer control near the centre
    lpfStartHz: 20000,
    lpfEndHz: 30,       // fully left: only the sub rumble is left
    hpfStartHz: 25,
    hpfEndHz: 9000,     // fully right: only the hi-hats are left
    minHz: 13,          // range of the Filter parameters
    maxHz: 22050,
    q: 0.86,            // normalized Q: 0.4 * 10^0.86 = ~2.9 (Butterworth 0.707)
    msb: {},
    lsb: {},
};

// Frequency -> normalized LPF/HPF parameter (inverse of the 40 dB taper)
PioneerDDJFLX4.filterParameterForHz = function(hz) {
    const f = PioneerDDJFLX4.filter;
    const lin = Math.min(Math.max((hz - f.minHz) / (f.maxHz - f.minHz), 0), 1);
    return 1 + Math.log10(0.01 + 0.99 * lin) / 2;
};

PioneerDDJFLX4.applyFilterResonance = function(group) {
    const effectGroup = group.replace("]]", "]_Effect1]");
    // Only the built-in Filter (LPF, Q, HPF) has its Q as parameter 2
    if (engine.getValue(effectGroup, "num_parameters") === 3) {
        engine.setParameter(effectGroup, "parameter2", PioneerDDJFLX4.filter.q);
    }
};

// Knob position 0..1 -> super1 of the Quick Effect (0.5 = no filter)
PioneerDDJFLX4.filterSuperForKnob = function(knob) {
    const f = PioneerDDJFLX4.filter;
    const offset = knob - 0.5;
    const distance = Math.max(0, Math.abs(offset) - f.deadZone) / (0.5 - f.deadZone);
    if (distance <= 0) {
        return 0.5;
    }
    const shaped = Math.pow(Math.min(distance, 1), f.curve);
    if (offset < 0) {
        // LinkedLeft: LPF parameter = 2 * super1
        const hz = f.lpfStartHz * Math.pow(f.lpfEndHz / f.lpfStartHz, shaped);
        return Math.min(0.5, PioneerDDJFLX4.filterParameterForHz(hz) / 2);
    }
    // LinkedRight: HPF parameter = 2 * (super1 - 0.5)
    const hz = f.hpfStartHz * Math.pow(f.hpfEndHz / f.hpfStartHz, shaped);
    return Math.max(0.5, 0.5 + PioneerDDJFLX4.filterParameterForHz(hz) / 2);
};

PioneerDDJFLX4.filterKnob = function(_channel, control, value, _status, group) {
    const f = PioneerDDJFLX4.filter;
    const base = control & 0x1F;
    if (control >= 0x20) {
        f.lsb[base] = value;
    } else {
        f.msb[base] = value;
        f.lsb[base] = 0;
    }
    const knob = ((f.msb[base] || 0) * 128 + (f.lsb[base] || 0)) / 16383;
    PioneerDDJFLX4.applyFilterResonance(group);
    engine.setParameter(group, "super1", PioneerDDJFLX4.filterSuperForKnob(knob));
};

//
// EQ knobs and SMART CFX = stems
//
// Holding SMART CFX turns the EQ knobs of each deck into stem volumes: HIGH =
// vocals (stem 4), MID = melody (bass + other, stems 2 and 3), LOW = drums
// (stem 1). Centre and right = full volume, left = down to silence. Releasing
// SMART CFX gives the knobs back to the EQ with soft takeover, so the EQ does
// not jump to where the knob was left. A deck without stems keeps its EQ
// while SMART CFX is held (its deck shows a notice).
//

PioneerDDJFLX4.smartCfxHeld = false;
PioneerDDJFLX4.eqMsb = {};
PioneerDDJFLX4.eqLsb = {};

// MSB control number -> EQ parameter and stems
PioneerDDJFLX4.eqKnobs = {
    0x07: {parameter: "parameter3", stems: [4]},    // HIGH = vocals
    0x0B: {parameter: "parameter2", stems: [2, 3]}, // MID = bass + other
    0x0F: {parameter: "parameter1", stems: [1]},    // LOW = drums
};

PioneerDDJFLX4.stemVolumeFromKnob = function(knob) {
    if (knob >= 0.5) {
        return 1;
    }
    return Math.pow(knob / 0.5, 1.5);
};

PioneerDDJFLX4.eqKnob = function(_channel, control, value, status, group) {
    const base = control & 0x1F;
    const knob = PioneerDDJFLX4.eqKnobs[base];
    if (knob === undefined) {
        return;
    }
    const key = status + ":" + base;
    if (control >= 0x20) {
        PioneerDDJFLX4.eqLsb[key] = value;
    } else {
        PioneerDDJFLX4.eqMsb[key] = value;
        PioneerDDJFLX4.eqLsb[key] = 0;
    }
    const v = ((PioneerDDJFLX4.eqMsb[key] || 0) * 128 + (PioneerDDJFLX4.eqLsb[key] || 0)) / 16383;
    const deck = group.match(/\[EqualizerRack1_\[Channel(\d+)\]_Effect1\]/)[1];

    // SMART CFX held: stems, but a deck without stems keeps its EQ (the deck
    // shows "No stems - EQ")
    // Instant stems: only where they are already computed at the play
    // position (stems_ready; always 1 for real stem files and sidecars)
    if (PioneerDDJFLX4.smartCfxHeld && engine.getValue("[Channel" + deck + "]", "stem_count") > 0 &&
            engine.getValue("[Channel" + deck + "]", "stems_ready") > 0) {
        const volume = PioneerDDJFLX4.stemVolumeFromKnob(v);
        for (const stem of knob.stems) {
            engine.setValue("[Channel" + deck + "_Stem" + stem + "]", "volume", volume);
        }
        return;
    }
    engine.setParameter(group, knob.parameter, v);
};

// Double press of SMART CFX (within this time) = all stems of both decks
// back to full volume and unmuted
PioneerDDJFLX4.smartCfxDoublePressMs = 400;
PioneerDDJFLX4.smartCfxLastPress = 0;

PioneerDDJFLX4.resetStems = function() {
    for (let deck = 1; deck <= 2; deck++) {
        for (let stem = 1; stem <= 4; stem++) {
            const stemGroup = "[Channel" + deck + "_Stem" + stem + "]";
            engine.setValue(stemGroup, "volume", 1);
            engine.setValue(stemGroup, "mute", 0);
        }
    }
    console.warn("SMART CFX double press: stems reset to full volume");
};

PioneerDDJFLX4.smartCfxPressed = function(_channel, _control, value) {
    PioneerDDJFLX4.smartCfxHeld = value > 0;
    // Deck overlay in the QML interface (control missing on the desktop)
    engine.setValue("[Skin]", "stem_mode", value > 0 ? 1 : 0);
    midi.sendShortMsg(0x96, 0x00, value > 0 ? 0x7F : 0x00);
    if (value > 0) {
        // Instant stems: pressing SMART CFX starts separating the tracks of
        // both decks that have no stems yet (from their play position)
        for (let deck = 1; deck <= 2; deck++) {
            engine.setValue("[Channel" + deck + "]", "stem_separate", 1);
        }
        const now = Date.now();
        if (now - PioneerDDJFLX4.smartCfxLastPress <= PioneerDDJFLX4.smartCfxDoublePressMs) {
            PioneerDDJFLX4.resetStems();
            PioneerDDJFLX4.smartCfxLastPress = 0;
        } else {
            PioneerDDJFLX4.smartCfxLastPress = now;
        }
    }
};

//
// PAD FX1 / PAD FX2
//
// Momentary effects on the deck's own channel while a pad is held, like the
// PAD FX of rekordbox. Effect unit 2 serves deck 1 and unit 3 deck 2; Mixxx
// loads Echo, Reverb, Flanger and Bitcrusher in them when they are empty.
// Pad 8 of each bank is a RELEASE FX: it ends the track (vinyl brake, echo
// out). SHIFT + pad does the same as the pad.
//

PioneerDDJFLX4.padFx = {
    // PAD FX1
    0x10: {slots: [1]},          // Echo
    0x11: {slots: [2]},          // Reverb
    0x12: {slots: [3]},          // Flanger
    0x13: {slots: [4]},          // Bitcrusher
    0x14: {roll: "0.5"},         // Roll 1/2
    0x15: {roll: "0.25"},        // Roll 1/4
    0x16: {roll: "0.125"},       // Roll 1/8
    0x17: {release: "brake"},    // RELEASE FX: vinyl brake
    // PAD FX2
    0x50: {slots: [1, 2]},       // Echo + Reverb
    0x51: {slots: [3, 2]},       // Flanger + Reverb
    0x52: {slots: [4, 1]},       // Bitcrusher + Echo
    0x53: {roll: "1"},           // Roll 1
    0x54: {roll: "0.0625"},      // Roll 1/16
    0x55: {roll: "2"},           // Roll 2
    0x56: {release: "spinback"}, // RELEASE FX: backspin
    0x57: {release: "echoOut"},  // RELEASE FX: echo out (stops, the echo rings while held)
};

// Meta knob of slots 1-4 of the PAD FX units (Echo, Reverb, Flanger, Bitcrusher)
PioneerDDJFLX4.padFxMeta = [0.5, 0.7, 0.8, 0.75];

PioneerDDJFLX4.padFxUnit = function(group) {
    return group === "[Channel1]" ? "[EffectRack1_EffectUnit2]" : "[EffectRack1_EffectUnit3]";
};

PioneerDDJFLX4.initPadFx = function() {
    const decks = ["[Channel1]", "[Channel2]"];
    for (const deck of decks) {
        const unit = PioneerDDJFLX4.padFxUnit(deck);
        for (let ch = 1; ch <= 4; ch++) {
            engine.setValue(unit, "group_[Channel" + ch + "]_enable", "[Channel" + ch + "]" === deck ? 1 : 0);
        }
        engine.setValue(unit, "mix", 0.8);
        engine.setValue(unit, "super1", 0.5);
        engine.setValue(unit, "enabled", 1);
        for (let slot = 1; slot <= 4; slot++) {
            const slotGroup = unit.replace("]", "_Effect" + slot + "]");
            engine.setValue(slotGroup, "enabled", 0);
            // With the manifest defaults Reverb (send 0) and Bitcrusher
            // (16 bit, no downsampling) are silent: give each pad a clear
            // amount of effect through the slot's meta knob.
            engine.setParameter(slotGroup, "meta", PioneerDDJFLX4.padFxMeta[slot - 1]);
        }
    }
};

PioneerDDJFLX4.padFxPressed = function(_channel, control, value, status, group) {
    const fx = PioneerDDJFLX4.padFx[control];
    if (fx === undefined) {
        return;
    }
    const pressed = value > 0;
    const unit = PioneerDDJFLX4.padFxUnit(group);
    const deckNumber = group === "[Channel1]" ? 1 : 2;

    // Pad light while held
    midi.sendShortMsg(status, control, pressed ? 0x7F : 0x00);

    if (fx.slots !== undefined) {
        for (const slot of fx.slots) {
            engine.setValue(unit.replace("]", "_Effect" + slot + "]"), "enabled", pressed ? 1 : 0);
        }
    } else if (fx.roll !== undefined) {
        engine.setValue(group, "beatlooproll_" + fx.roll + "_activate", pressed ? 1 : 0);
    } else if (fx.release === "brake") {
        if (pressed) {
            engine.brake(deckNumber, true);
        }
    } else if (fx.release === "spinback") {
        if (pressed) {
            engine.spinback(deckNumber, true);
        }
    } else if (fx.release === "echoOut") {
        // The echo keeps ringing after the stop while the pad is held
        engine.setValue(unit.replace("]", "_Effect1]"), "enabled", pressed ? 1 : 0);
        if (pressed) {
            engine.setValue(group, "play", 0);
        }
    }
};

//
// Loop IN/OUT ADJUST
//

PioneerDDJFLX4.toggleLoopAdjustIn = function(channel, _control, value, _status, group) {
    if (value === 0 || engine.getValue(group, "loop_enabled") === 0) {
        return;
    }
    PioneerDDJFLX4.loopAdjustIn[channel] = !PioneerDDJFLX4.loopAdjustIn[channel];
    PioneerDDJFLX4.loopAdjustOut[channel] = false;
};

PioneerDDJFLX4.toggleLoopAdjustOut = function(channel, _control, value, _status, group) {
    if (value === 0 || engine.getValue(group, "loop_enabled") === 0) {
        return;
    }
    PioneerDDJFLX4.loopAdjustOut[channel] = !PioneerDDJFLX4.loopAdjustOut[channel];
    PioneerDDJFLX4.loopAdjustIn[channel] = false;
};

// Two signals are sent here so that the light stays lit/unlit in its shift state too
PioneerDDJFLX4.setReloopLight = function(status, value) {
    midi.sendShortMsg(status, 0x4D, value);
    midi.sendShortMsg(status, 0x50, value);
};


PioneerDDJFLX4.setLoopButtonLights = function(status, value) {
    [0x10, 0x11, 0x4E, 0x4C].forEach(function(control) {
        midi.sendShortMsg(status, control, value);
    });
};

PioneerDDJFLX4.startLoopLightsBlink = function(channel, control, status, group) {
    let blink = 0x7F;

    PioneerDDJFLX4.stopLoopLightsBlink(group, control, status);

    PioneerDDJFLX4.timers[group][control] = engine.beginTimer(500, () => {
        blink = 0x7F - blink;

        // When adjusting the loop out position, turn the loop in light off
        if (PioneerDDJFLX4.loopAdjustOut[channel]) {
            midi.sendShortMsg(status, 0x10, 0x00);
            midi.sendShortMsg(status, 0x4C, 0x00);
        } else {
            midi.sendShortMsg(status, 0x10, blink);
            midi.sendShortMsg(status, 0x4C, blink);
        }

        // When adjusting the loop in position, turn the loop out light off
        if (PioneerDDJFLX4.loopAdjustIn[channel]) {
            midi.sendShortMsg(status, 0x11, 0x00);
            midi.sendShortMsg(status, 0x4E, 0x00);
        } else {
            midi.sendShortMsg(status, 0x11, blink);
            midi.sendShortMsg(status, 0x4E, blink);
        }
    });

};

PioneerDDJFLX4.stopLoopLightsBlink = function(group, control, status) {
    PioneerDDJFLX4.timers[group] = PioneerDDJFLX4.timers[group] || {};

    if (PioneerDDJFLX4.timers[group][control] !== undefined) {
        engine.stopTimer(PioneerDDJFLX4.timers[group][control]);
    }
    PioneerDDJFLX4.timers[group][control] = undefined;
    PioneerDDJFLX4.setLoopButtonLights(status, 0x7F);
};

PioneerDDJFLX4.loopToggle = function(value, group, control) {
    const status = group === "[Channel1]" ? 0x90 : 0x91,
        channel = group === "[Channel1]" ? 0 : 1;

    PioneerDDJFLX4.setReloopLight(status, value ? 0x7F : 0x00);

    if (value) {
        PioneerDDJFLX4.startLoopLightsBlink(channel, control, status, group);
    } else {
        PioneerDDJFLX4.stopLoopLightsBlink(group, control, status);
        PioneerDDJFLX4.loopAdjustIn[channel] = false;
        PioneerDDJFLX4.loopAdjustOut[channel] = false;
    }
};

//
// CUE/LOOP CALL
//

PioneerDDJFLX4.cueLoopCallLeft = function(_channel, _control, value, _status, group) {
    if (value) {
        engine.setValue(group, "loop_scale", 0.5);
    }
};

PioneerDDJFLX4.cueLoopCallRight = function(_channel, _control, value, _status, group) {
    if (value) {
        engine.setValue(group, "loop_scale", 2.0);
    }
};

//
// BEAT SYNC
//
// Note that the controller sends different signals for a short press and a long
// press of the same button.
//

// Like rekordbox: a press turns sync on (the deck follows the tempo and the
// beat of the master deck and the button stays lit) or off. A long press makes
// the deck the sync master.
PioneerDDJFLX4.syncPressed = function(channel, control, value, status, group) {
    if (value === 0) { return; } // ignore release
    script.toggleControl(group, "sync_enabled");
};

PioneerDDJFLX4.syncLongPressed = function(channel, control, value, status, group) {
    if (value) {
        engine.setValue(group, "sync_enabled", 1);
        engine.setValue(group, "sync_leader", 1);
    }
};

// SHIFT + BEAT SYNC: keylock on/off ("Master Tempo" on the CDJ). Keylock is
// on by default (engine default). The tempo range is changed from the deck bar.
PioneerDDJFLX4.toggleKeylock = function(_channel, _control, value, _status, group) {
    if (value === 0) { return; } // ignore release
    script.toggleControl(group, "keylock");
};

PioneerDDJFLX4.cycleTempoRange = function(_channel, _control, value, _status, group) {
    if (value === 0) { return; } // ignore release

    const currRange = engine.getValue(group, "rateRange");
    let idx = 0;

    for (let i = 0; i < this.tempoRanges.length; i++) {
        if (currRange === this.tempoRanges[i]) {
            // idx get the index of the value in tempoRanges following the currently configured one
            // or cycle back to 0 if the current is the last value of the list.
            idx = (i + 1) % this.tempoRanges.length;
            break;
        }
    }
    engine.setValue(group, "rateRange", this.tempoRanges[idx]);
};

//
// Jog wheels
//

PioneerDDJFLX4.jogTurn = function(channel, _control, value, _status, group) {
    const deckNum = channel + 1;
    // wheel center at 64; <64 rew >64 fwd
    let newVal = value - 64;

    // loop_in / out adjust
    const loopEnabled = engine.getValue(group, "loop_enabled");
    if (loopEnabled > 0) {
        if (PioneerDDJFLX4.loopAdjustIn[channel]) {
            newVal = newVal * PioneerDDJFLX4.loopAdjustMultiply + engine.getValue(group, "loop_start_position");
            engine.setValue(group, "loop_start_position", newVal);
            return;
        }
        if (PioneerDDJFLX4.loopAdjustOut[channel]) {
            newVal = newVal * PioneerDDJFLX4.loopAdjustMultiply + engine.getValue(group, "loop_end_position");
            engine.setValue(group, "loop_end_position", newVal);
            return;
        }
    }

    if (engine.isScratching(deckNum)) {
        engine.scratchTick(deckNum, newVal);
    } else { // fallback
        engine.setValue(group, "jog", newVal * this.bendScale);
    }
};


PioneerDDJFLX4.jogSearch = function(_channel, _control, value, _status, group) {
    const newVal = (value - 64) * PioneerDDJFLX4.fastSeekScale;
    engine.setValue(group, "jog", newVal);
};

PioneerDDJFLX4.jogTouch = function(channel, _control, value) {
    const deckNum = channel + 1;

    // skip while adjusting the loop points
    if (PioneerDDJFLX4.loopAdjustIn[channel] || PioneerDDJFLX4.loopAdjustOut[channel]) {
        return;
    }

    if (value !== 0 && this.vinylMode) {
        engine.scratchEnable(deckNum, 720, 33+1/3, this.alpha, this.beta);
    } else {
        engine.scratchDisable(deckNum);
    }
};

//
// Shift button
//

PioneerDDJFLX4.shiftPressed = function(channel, _control, value, _status, _group) {
    PioneerDDJFLX4.shiftButtonDown[channel] = value === 0x7F;
};


//
// BROWSE rotate: scrolls the track list or the sources. When the list is
// hidden (waveform view, SHIFT + LOAD 2), the first turn shows it again
// without moving the selection.
//

PioneerDDJFLX4.browseRotate = function(_channel, _control, value) {
    if (engine.getValue("[Skin]", "show_library") === 0) {
        engine.setValue("[Skin]", "show_library", 1);
        return;
    }
    // Relative encoder: 1..63 = clockwise steps, 65..127 = counter-clockwise
    const delta = value < 64 ? value : value - 128;
    engine.setValue("[Library]", "MoveVertical", delta);
};

//
// Tempo sliders
//
// The tempo option in Mixxx's deck preferences determine whether down/up
// increases/decreases the rate. Therefore it must be inverted here so that the
// UI and the control sliders always move in the same direction.
//

PioneerDDJFLX4.tempoSliderMSB = function(channel, control, value, status, group) {
    PioneerDDJFLX4.highResMSB[group].tempoSlider = value;
};

PioneerDDJFLX4.tempoSliderLSB = function(channel, control, value, status, group) {
    const fullValue = (PioneerDDJFLX4.highResMSB[group].tempoSlider << 7) + value;
    let rate = 1 - (fullValue / 0x2000);

    // Snap the resulting tempo to 0.05 BPM steps (xxx.x0 / xxx.x5), so the
    // two-decimal BPM on the deck lands on round values: Mixxx's speed is
    // 1 + rate_dir * rateRange * rate, times the file BPM.
    const fileBpm = engine.getValue(group, "file_bpm");
    const range = engine.getValue(group, "rateRange");
    const dir = engine.getValue(group, "rate_dir") || 1;
    if (fileBpm > 0 && range > 0) {
        const bpm = fileBpm * (1 + dir * range * rate);
        // Nearest 0.05 step, else (at the ends of the range) the next step
        // towards the centre
        for (const snapped of [Math.round(bpm * 20) / 20,
            Math.floor(bpm * 20) / 20, Math.ceil(bpm * 20) / 20]) {
            const snappedRate = (snapped / fileBpm - 1) / (dir * range);
            if (snappedRate >= -1 && snappedRate <= 1) {
                rate = snappedRate;
                break;
            }
        }
    }
    engine.setValue(group, "rate", rate);
};

//
// Beat Jump mode
//
// Note that when we increase/decrease the sizes on the pad buttons, we use the
// value of the first pad (0x21) as an upper/lower limit beyond which we don't
// allow further increasing/decreasing of all the values.
//

PioneerDDJFLX4.beatjumpPadPressed = function(_channel, control, value, _status, group) {
    if (value === 0) {
        return;
    }
    engine.setValue(group, "beatjump_size", Math.abs(PioneerDDJFLX4.beatjumpSizeForPad[control]));
    engine.setValue(group, "beatjump", PioneerDDJFLX4.beatjumpSizeForPad[control]);
};

PioneerDDJFLX4.increaseBeatjumpSizes = function(_channel, control, value, _status, group) {
    if (value === 0 || PioneerDDJFLX4.beatjumpSizeForPad[0x21] * 16 > 16) {
        return;
    }
    Object.keys(PioneerDDJFLX4.beatjumpSizeForPad).forEach(function(pad) {
        PioneerDDJFLX4.beatjumpSizeForPad[pad] = PioneerDDJFLX4.beatjumpSizeForPad[pad] * 16;
    });
    engine.setValue(group, "beatjump_size", PioneerDDJFLX4.beatjumpSizeForPad[0x21]);
};

PioneerDDJFLX4.decreaseBeatjumpSizes = function(_channel, control, value, _status, group) {
    if (value === 0 || PioneerDDJFLX4.beatjumpSizeForPad[0x21] / 16 < 1/16) {
        return;
    }
    Object.keys(PioneerDDJFLX4.beatjumpSizeForPad).forEach(function(pad) {
        PioneerDDJFLX4.beatjumpSizeForPad[pad] = PioneerDDJFLX4.beatjumpSizeForPad[pad] / 16;
    });
    engine.setValue(group, "beatjump_size", PioneerDDJFLX4.beatjumpSizeForPad[0x21]);
};

//
// Sampler mode
//

PioneerDDJFLX4.samplerPlayOutputCallbackFunction = function(value, group, _control) {
    if (value === 1) {
        const curPad = group.match(script.samplerRegEx)[1];
        let deckIndex = 0;
        let padIndex = 0;

        if (curPad >=1 && curPad <= 4) {
            deckIndex = 0;
            padIndex = curPad - 1;
        } else if (curPad >=5 && curPad <= 8) {
            deckIndex = 2;
            padIndex = curPad - 5;
        } else if (curPad >=9 && curPad <= 12) {
            deckIndex = 0;
            padIndex = curPad - 5;
        } else if (curPad >=13 && curPad <= 16) {
            deckIndex = 2;
            padIndex = curPad - 9;
        }

        PioneerDDJFLX4.startSamplerBlink(
            0x97 + deckIndex,
            0x30 + padIndex,
            group);
    }
};

PioneerDDJFLX4.padModeKeyPressed = function(_channel, _control, value, _status, _group) {
    const deck = (_status === 0x90 ? PioneerDDJFLX4.lights.deck1 : PioneerDDJFLX4.lights.deck2);

    if (_control === 0x1B) {
        PioneerDDJFLX4.toggleLight(deck.hotcueMode, true);
    } else if (_control === 0x69) {
        PioneerDDJFLX4.toggleLight(deck.keyboardMode, true);
    } else if (_control === 0x1E) {
        PioneerDDJFLX4.toggleLight(deck.padFX1Mode, true);
    } else if (_control === 0x6B) {
        PioneerDDJFLX4.toggleLight(deck.padFX2Mode, true);
    } else if (_control === 0x20) {
        PioneerDDJFLX4.toggleLight(deck.beatJumpMode, true);
    } else if (_control === 0x6D) {
        PioneerDDJFLX4.toggleLight(deck.beatLoopMode, true);
    } else if (_control === 0x22) {
        PioneerDDJFLX4.toggleLight(deck.samplerMode, true);
    } else if (_control === 0x6F) {
        PioneerDDJFLX4.toggleLight(deck.keyShiftMode, true);
    }
};

PioneerDDJFLX4.samplerPadPressed = function(_channel, _control, value, _status, group) {
    if (engine.getValue(group, "track_loaded")) {
        engine.setValue(group, "cue_gotoandplay", value);
    } else {
        engine.setValue(group, "LoadSelectedTrack", value);
    }
};

PioneerDDJFLX4.samplerPadShiftPressed = function(_channel, _control, value, _status, group) {
    if (engine.getValue(group, "play")) {
        engine.setValue(group, "cue_gotoandstop", value);
    } else if (engine.getValue(group, "track_loaded")) {
        engine.setValue(group, "eject", value);
    }
};

PioneerDDJFLX4.startSamplerBlink = function(channel, control, group) {
    let val = 0x7f;

    PioneerDDJFLX4.stopSamplerBlink(channel, control);
    PioneerDDJFLX4.timers[channel][control] = engine.beginTimer(250, () => {
        val = 0x7f - val;

        // blink the appropriate pad
        midi.sendShortMsg(channel, control, val);
        // also blink the pad while SHIFT is pressed
        midi.sendShortMsg((channel+1), control, val);

        const isPlaying = engine.getValue(group, "play") === 1;

        if (!isPlaying) {
            // kill timer
            PioneerDDJFLX4.stopSamplerBlink(channel, control);
            // set the pad LED to ON
            midi.sendShortMsg(channel, control, 0x7f);
            // set the pad LED to ON while SHIFT is pressed
            midi.sendShortMsg((channel+1), control, 0x7f);
        }
    });
};

PioneerDDJFLX4.stopSamplerBlink = function(channel, control) {
    PioneerDDJFLX4.timers[channel] = PioneerDDJFLX4.timers[channel] || {};

    if (PioneerDDJFLX4.timers[channel][control] !== undefined) {
        engine.stopTimer(PioneerDDJFLX4.timers[channel][control]);
        PioneerDDJFLX4.timers[channel][control] = undefined;
    }
};


PioneerDDJFLX4.toggleQuantize = function(_channel, _control, value, _status, group) {
    if (value) {
        script.toggleControl(group, "quantize");
    }
};

PioneerDDJFLX4.quickJumpForward = function(_channel, _control, value, _status, group) {
    if (value) {
        engine.setValue(group, "beatjump", PioneerDDJFLX4.quickJumpSize);
    }
};

PioneerDDJFLX4.quickJumpBack = function(_channel, _control, value, _status, group) {
    if (value) {
        engine.setValue(group, "beatjump", -PioneerDDJFLX4.quickJumpSize);
    }
};

//
// Stems mode
//

PioneerDDJFLX4.stemMutePadPressed = function(_channel, control, value, _status, group) {
    if (value !== 0x7f) {
        return;
    }

    const stemCount = Math.min(engine.getValue(group, "stem_count"), 4);

    if (control - PioneerDDJFLX4.stemMutePadsFirstControl + 1 > stemCount) {
        return;
    }

    const stemGroup = `[${group.substring(1, group.length-1)}_Stem${control - PioneerDDJFLX4.stemMutePadsFirstControl + 1}]`;

    if (engine.getValue(stemGroup, "mute")) {
        engine.setValue(stemGroup, "mute", 0);
    } else {
        engine.setValue(stemGroup, "mute", 1);
    }
};

PioneerDDJFLX4.stemMutePadShiftPressed = function(_channel, control, value, _status, group) {
    if (value !== 0x7f) {
        return;
    }

    const stemCount = Math.min(engine.getValue(group, "stem_count"), 4);

    if (control - PioneerDDJFLX4.stemMutePadsFirstControl + 1 > stemCount) {
        return;
    }

    for (let stemIdx=1; stemIdx<=stemCount; stemIdx++) {
        const stemGroup = `[${group.substring(1, group.length-1)}_Stem${stemIdx}]`;

        if (stemIdx + PioneerDDJFLX4.stemMutePadsFirstControl - 1 === control) {
            engine.setValue(stemGroup, "mute", 0);
        } else {
            engine.setValue(stemGroup, "mute", 1);
        }
    }
};

PioneerDDJFLX4.stemFxPadPressed = function(_channel, control, value, _status, group) {
    if (value !== 0x7f) {
        return;
    }

    if (control - PioneerDDJFLX4.stemFxPadsFirstControl + 1 > 4) {
        return;
    }

    const stemGroup = `[QuickEffectRack1_[${group.substring(1, group.length-1)}_Stem${control - PioneerDDJFLX4.stemFxPadsFirstControl + 1}]]`;

    if (engine.getValue(stemGroup, "enabled")) {
        engine.setValue(stemGroup, "enabled", 0);
    } else {
        engine.setValue(stemGroup, "enabled", 1);
    }
};

PioneerDDJFLX4.stemFxPadShiftPressed = function(_channel, control, value, _status, group) {
    if (value !== 0x7f) {
        return;
    }

    if (control - PioneerDDJFLX4.stemFxPadsFirstControl + 1 > 4) {
        return;
    }

    const stemGroup = `[QuickEffectRack1_[${group.substring(1, group.length-1)}_Stem${control - PioneerDDJFLX4.stemFxPadsFirstControl + 1}]]`;

    engine.setValue(stemGroup, "next_chain_preset", 1);
};

PioneerDDJFLX4.stemCountChanged = function(_value, group, _control) {

    for (let stem=1; stem<=4; stem++) {
        // Stem mute pads
        PioneerDDJFLX4.stemMuteChanged(
            engine.getValue(`[${group.substring(1, group.length-1)}_Stem${stem}]`, "mute"),
            `[${group.substring(1, group.length-1)}_Stem${stem}]`,
            _control,
        );

        // Stem FX pads
        PioneerDDJFLX4.stemFxChanged(
            engine.getValue(`[QuickEffectRack1_[${group.substring(1, group.length-1)}_Stem${stem}]]`, "enabled"),
            `[QuickEffectRack1_[${group.substring(1, group.length-1)}_Stem${stem}]]`,
            _control,
        );
    }
};

PioneerDDJFLX4.stemMuteChanged = function(value, group, _control) {
    const channelStem = group.match(/\[Channel(\d+)_Stem(\d+)\]/);
    const deck = Number(channelStem[1]);
    const stem = Number(channelStem[2]);
    const channel = `[Channel${deck}]`;

    if (stem > 4) {
        return;
    }

    const stemCount = engine.getValue(channel, "stem_count");

    let code = 0x00;
    if (stem <= stemCount && value <= 0.5) {
        code = 0x7f;
    }

    for (let i=0; i<PioneerDDJFLX4.stemsPadsModesStatus[channel].length; i++) {
        midi.sendShortMsg(
            PioneerDDJFLX4.stemsPadsModesStatus[channel][i],
            PioneerDDJFLX4.stemMutePadsFirstControl + stem -1,
            code,
        );
    }
};

PioneerDDJFLX4.stemFxChanged = function(value, group, _control) {
    const channelStem = group.match(/\[QuickEffectRack1_\[Channel(\d+)_Stem(\d+)\]\]/);
    const deck = Number(channelStem[1]);
    const stem = Number(channelStem[2]);
    const channel = `[Channel${deck}]`;

    if (stem > 4) {
        return;
    }

    for (let i=0; i<PioneerDDJFLX4.stemsPadsModesStatus[channel].length; i++) {
        midi.sendShortMsg(
            PioneerDDJFLX4.stemsPadsModesStatus[channel][i],
            PioneerDDJFLX4.stemFxPadsFirstControl + stem -1,
            value <= 0.5 ? 0x00 : 0x7f,
        );
    }
};

//
// Pitch Shift mode
//

PioneerDDJFLX4.pitchAdjusted = function(_value, group, _control) {
    const pitchAdjust = Math.round(engine.getValue(group, "pitch_adjust"));
    let lights = 0b00000000;

    if (pitchAdjust === 0) {
        lights = 0b10000001;
    } else if (pitchAdjust === 1) {
        lights = 0b01000000;
    } else if (pitchAdjust === 2) {
        lights = 0b00100000;
    } else if (pitchAdjust === 3) {
        lights = 0b00010000;
    } else if (pitchAdjust === 4) {
        lights = 0b10010000;
    } else if (pitchAdjust === 5) {
        lights = 0b01010000;
    } else if (pitchAdjust === 6) {
        lights = 0b00110000;
    } else if (pitchAdjust === 7) {
        lights = 0b10110000;
    } else if (pitchAdjust === 8) {
        lights = 0b01110000;
    } else if (pitchAdjust > 8) {
        lights = 0b11110000;
    } else if (pitchAdjust === -1) {
        lights = 0b00000010;
    } else if (pitchAdjust === -2) {
        lights = 0b00000100;
    } else if (pitchAdjust === -3) {
        lights = 0b00001000;
    } else if (pitchAdjust === -4) {
        lights = 0b00001001;
    } else if (pitchAdjust === -5) {
        lights = 0b00001010;
    } else if (pitchAdjust === -6) {
        lights = 0b00001100;
    } else if (pitchAdjust === -7) {
        lights = 0b00001101;
    } else if (pitchAdjust === -8) {
        lights = 0b00001110;
    } else if (pitchAdjust < -8) {
        lights = 0b00001111;
    } else {
        lights = 0b11111111;
    }

    for (let i=0; i<8; i++) {
        let code = 0x00;
        const pad = 0b10000000 >>> i;

        if (lights & pad) {
            code = 0x7f;
        } else {
            code = 0x00;
        }

        PioneerDDJFLX4.pitchPadsModesStatus[group].forEach(
            (padMode) => midi.sendShortMsg(
                padMode,
                PioneerDDJFLX4.pitchPadsFirstControl + i,
                code,
            )
        );
    }
};

PioneerDDJFLX4.pitchPadPressed = function(_channel, control, value, _status, group) {
    if (value !== 0x7f) {
        return;
    }

    const pad = control - this.pitchPadsFirstControl;
    let pitch = 0;

    if (pad === 0) {
        pitch = 0;
    } else if (pad === 1) {
        pitch = 1;
    } else if (pad === 2) {
        pitch = 2;
    } else if (pad === 3) {
        pitch = 3;
    } else if (pad === 4) {
        pitch = -3;
    } else if (pad === 5) {
        pitch = -2;
    } else if (pad === 6) {
        pitch = -1;
    } else if (pad === 7) {
        pitch = 0;
    }

    engine.setValue(group, "pitch_adjust", pitch);
};

PioneerDDJFLX4.pitchPadShiftPressed = function(_channel, control, value, _status, group) {
    if (value !== 0x7f) {
        return;
    }

    const pad = control - this.pitchPadsFirstControl;

    let currentPitch = engine.getValue(group, "pitch_adjust");

    if (pad === 0) {
        currentPitch += 1;
    } else if (pad === 1) {
        currentPitch += 2;
    } else if (pad === 2) {
        currentPitch += 3;
    } else if (pad === 3) {
        currentPitch += 4;
    } else if (pad === 4) {
        currentPitch += -4;
    } else if (pad === 5) {
        currentPitch += -3;
    } else if (pad === 6) {
        currentPitch += -2;
    } else if (pad === 7) {
        currentPitch += -1;
    }

    engine.setValue(group, "pitch_adjust", currentPitch);
};


//
// SMART FADER: crossfader on/off
//

// [ChannelN],orientation: 0 = left, 1 = center (not affected), 2 = right
PioneerDDJFLX4.crossfaderActive = true;
PioneerDDJFLX4.savedOrientation = {"[Channel1]": 0, "[Channel2]": 2};

PioneerDDJFLX4.setCrossfaderActive = function(active) {
    ["[Channel1]", "[Channel2]"].forEach(function(group) {
        if (active) {
            engine.setValue(group, "orientation", PioneerDDJFLX4.savedOrientation[group]);
        } else {
            PioneerDDJFLX4.savedOrientation[group] = engine.getValue(group, "orientation");
            engine.setValue(group, "orientation", 1);
        }
    });
    PioneerDDJFLX4.crossfaderActive = active;
    // No LED: lighting it enables the firmware Smart Fader (see init)
};

PioneerDDJFLX4.smartFaderPress = function(_channel, _control, value) {
    if (value === 0) {
        return; // release
    }
    PioneerDDJFLX4.setCrossfaderActive(!PioneerDDJFLX4.crossfaderActive);
};

//
// Shutdown
//

PioneerDDJFLX4.shutdown = function() {
    // leave the crossfader working for whoever uses Mixxx next
    if (!PioneerDDJFLX4.crossfaderActive) {
        PioneerDDJFLX4.setCrossfaderActive(true);
    }
    PioneerDDJFLX4.toggleLight(PioneerDDJFLX4.lights.smartFader, false);
    // reset vumeter
    PioneerDDJFLX4.toggleLight(PioneerDDJFLX4.lights.deck1.vuMeter, false);
    PioneerDDJFLX4.toggleLight(PioneerDDJFLX4.lights.deck2.vuMeter, false);

    // housekeeping
    // turn off all Sampler LEDs
    for (var i = 0; i <= 7; ++i) {
        midi.sendShortMsg(0x97, 0x30 + i, 0x00);    // Deck 1 pads
        midi.sendShortMsg(0x98, 0x30 + i, 0x00);    // Deck 1 pads with SHIFT
        midi.sendShortMsg(0x99, 0x30 + i, 0x00);    // Deck 2 pads
        midi.sendShortMsg(0x9A, 0x30 + i, 0x00);    // Deck 2 pads with SHIFT
    }
    // turn off all Hotcue LEDs
    for (i = 0; i <= 7; ++i) {
        midi.sendShortMsg(0x97, 0x00 + i, 0x00);    // Deck 1 pads
        midi.sendShortMsg(0x98, 0x00 + i, 0x00);    // Deck 1 pads with SHIFT
        midi.sendShortMsg(0x99, 0x00 + i, 0x00);    // Deck 2 pads
        midi.sendShortMsg(0x9A, 0x00 + i, 0x00);    // Deck 2 pads with SHIFT
    }

    // turn off loop in and out lights
    PioneerDDJFLX4.setLoopButtonLights(0x90, 0x00);
    PioneerDDJFLX4.setLoopButtonLights(0x91, 0x00);

    // turn off reloop lights
    PioneerDDJFLX4.setReloopLight(0x90, 0x00);
    PioneerDDJFLX4.setReloopLight(0x91, 0x00);

    // stop any flashing lights
    PioneerDDJFLX4.toggleLight(PioneerDDJFLX4.lights.beatFx, false);
    PioneerDDJFLX4.toggleLight(PioneerDDJFLX4.lights.shiftBeatFx, false);

    // stop the keepalive timer
    engine.stopTimer(PioneerDDJFLX4.keepAliveTimer);
};
