// Pioneer-DDJ-FLX4-pioneer-layout.js
// ****************************************************************************
// * Handlers of the "Pioneer DDJ-FLX4 (original layout)" mapping.
// *
// * Pioneer-DDJ-FLX4-pioneer.midi.xml is GENERATED from Pioneer-DDJ-FLX4.midi.xml
// * by tools/gen_flx4_pioneer_xml.py and loads, in this order:
// *     Pioneer-DDJ-FLX4-script.js (prefix PioneerDDJFLX4), this file (no prefix).
// * Both files share one JS engine: this file only ADDS handlers to the same
// * PioneerDDJFLX4 object (init / shutdown and everything else are the ones of
// * the base script). It changes nothing of the mapping "Pioneer DDJ-FLX4" (the
// * default one, auto-detected).
// *
// * The layout follows the official Pioneer function table (DDJ-FLX4 Hardware
// * Diagram for rekordbox, v1.0, page 2) where Mixxx 2.7 has a control for it:
// *
// *   4 BEAT/EXIT          no loop: 4 beat loop; loop on: exit
// *   SHIFT + 4 BEAT/EXIT  active loop on/off (reloop_toggle, XML)
// *   CUE/LOOP CALL < >    loop on: halve / double; loop off: call the last loop
// *                        (reloop). Memory cues do not exist in Mixxx
// *   SHIFT + BEAT SYNC    Tempo Range (6 / 10 / 16 / 25 %); keylock is not on
// *                        the controller in this layout
// *   SHIFT + CUE          jump to the track start and stop (start_stop, XML)
// *   SHIFT + channel CUE  Tap BPM (bpm_tap, XML)
// *   LOAD pressed twice   Instant Doubles
// *   BROWSE press         move the cursor between sources and track list (XML)
// *   SHIFT + BROWSE press same as the press (the table has no SHIFT function)
// *   SHIFT + LOAD 1 / 2   library full screen / show-hide the library (XML)
// *
// * Kept as in the default mapping (user decision): the stems. SMART CFX held +
// * EQ knobs = stem volumes (double press = all stems back to full) and the
// * KEYBOARD pads = stem mute / solo / FX, i.e. the official Quick Effect on/off
// * of SMART CFX and the pitch play of the KEYBOARD pads are NOT implemented.
// *
// * NOT implemented (no control in Mixxx 2.7, the function of the default
// * mapping is kept where there is one): memory cue call / save / delete
// * (SHIFT + CUE/LOOP CALL stays +-32 beats), Release FX and BPM auto / tap of
// * Beat FX (SHIFT + BEAT FX), channel fader start, SHIFT + LOAD 1 "related
// * tracks".
// ****************************************************************************

// Push-button pulse (1 then 0): a control that is already at 1 does not see
// a second "1"
PioneerDDJFLX4.pulse = function(group, key) {
    engine.setValue(group, key, 1);
    engine.setValue(group, key, 0);
};

//
// 4 BEAT/EXIT and CUE/LOOP CALL
//

PioneerDDJFLX4.fourBeatExitPressed = function(_channel, _control, value, _status, group) {
    if (value === 0) { return; } // release
    if (engine.getValue(group, "loop_enabled") > 0) {
        PioneerDDJFLX4.pulse(group, "reloop_exit");
    } else {
        PioneerDDJFLX4.pulse(group, "beatloop_4_activate");
    }
};

// Loop on: scale the loop. Loop off: "call" = switch the last loop on again
// (jumps to it if the play position is already past it)
PioneerDDJFLX4.loopCall = function(group, scale) {
    if (engine.getValue(group, "loop_enabled") > 0) {
        engine.setValue(group, "loop_scale", scale);
    } else {
        PioneerDDJFLX4.pulse(group, "reloop_toggle");
    }
};

PioneerDDJFLX4.loopCallLeftPressed = function(_channel, _control, value, _status, group) {
    if (value === 0) { return; }
    PioneerDDJFLX4.loopCall(group, 0.5);
};

PioneerDDJFLX4.loopCallRightPressed = function(_channel, _control, value, _status, group) {
    if (value === 0) { return; }
    PioneerDDJFLX4.loopCall(group, 2.0);
};

//
// LOAD: a second press within instantDoublesMs = Instant Doubles
//
// The first press loads the selected track at once, as the plain mapping
// does (no delay on every LOAD). The second press, if it comes in time,
// copies the track (and position, cues, loops, play state) of the other deck
// with CloneFromDeck. It is refused (the press is then a normal LOAD) when the
// library cursor is on the sources (LOAD opens the source there), the deck
// that receives the copy is playing, or the other deck has no track.
//

PioneerDDJFLX4.instantDoublesMs = 400;
PioneerDDJFLX4.lastLoadPress = {};
PioneerDDJFLX4.loadIsDouble = {};

// [Library],focused_widget: 2 = sources, 3 = track list
PioneerDDJFLX4.focusedWidgetSources = 2;

PioneerDDJFLX4.instantDoubles = function(group) {
    const m = group.match(/^\[Channel(\d+)\]$/);
    const deck = m ? Number(m[1]) : 0;
    const source = deck === 1 ? 2 : (deck === 2 ? 1 : 0);
    if (source === 0) {
        return false;
    }
    if (engine.getValue("[Library]", "focused_widget") === PioneerDDJFLX4.focusedWidgetSources) {
        return false;
    }
    if (engine.getValue(group, "play") > 0) {
        return false;
    }
    if (!(engine.getValue("[Channel" + source + "]", "track_loaded") > 0)) {
        return false;
    }
    engine.setValue(group, "CloneFromDeck", source);
    return true;
};

PioneerDDJFLX4.loadPressed = function(_channel, _control, value, _status, group) {
    if (value === 0) {
        // Release: forwarded like the plain binding, unless this press was
        // taken by Instant Doubles
        if (!PioneerDDJFLX4.loadIsDouble[group]) {
            engine.setValue(group, "LoadSelectedTrack", 0);
        }
        PioneerDDJFLX4.loadIsDouble[group] = false;
        return;
    }
    const now = Date.now();
    const last = PioneerDDJFLX4.lastLoadPress[group] || 0;
    if (now - last <= PioneerDDJFLX4.instantDoublesMs && PioneerDDJFLX4.instantDoubles(group)) {
        PioneerDDJFLX4.lastLoadPress[group] = 0;
        PioneerDDJFLX4.loadIsDouble[group] = true;
        return;
    }
    PioneerDDJFLX4.lastLoadPress[group] = now;
    PioneerDDJFLX4.loadIsDouble[group] = false;
    engine.setValue(group, "LoadSelectedTrack", 1);
};
