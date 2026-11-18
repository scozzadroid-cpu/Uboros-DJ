/**
 * common-stems.js - stem (vocals / drums / bass / other) helpers shared by the
 * controller mappings.
 *
 * Include it from a mapping with ONE line in the <scriptfiles> of the .midi.xml,
 * before the mapping's own script (same mechanism as common-controller-scripts.js;
 * this Mixxx has no engine.include):
 *
 *     <file filename="common-stems.js"/>
 *
 * The file has no functionprefix, so it only defines the global `CommonStems`
 * (no init/shutdown is called). A mapping then calls, from its own handlers:
 *
 *     CommonStems.padPress(deck, padNumber)          // pads 1-4 toggle stem 1-4, 5-8 solo
 *     CommonStems.toggleMute(deck, stem)             // mute / unmute one stem
 *     CommonStems.solo(deck, stem)                   // only that stem plays
 *     CommonStems.resetDeck(deck) / resetAll(decks)  // everything back to full
 *     CommonStems.separate(deck)                     // start the separation if no stems yet
 *     CommonStems.eqToStems(deck, "high", knob01)    // EQ knob as stem volume (true = handled)
 *     CommonStems.watch([1, 2], function (deck, stem, lit) {...})  // LED callback
 *     CommonStems.unwatch()                          // in shutdown()
 *
 * `deck` is a deck number (1..4) or a group string ("[Channel1]").
 *
 * Rules every function follows:
 *  - a deck without stems (stem_count = 0) is never touched: toggle/solo/reset/
 *    knob do nothing there (the knob helpers return false so that the caller
 *    keeps doing what it always did, e.g. the EQ);
 *  - the only thing a stem action does on a deck without stems is to ask for
 *    the separation (`[ChannelN],stem_separate`, "instant stems" of this fork);
 *  - with "instant stems" the stems exist (stem_count = 4) before they are
 *    computed: they are used only when `[ChannelN],stems_ready` > 0 (always 1
 *    for real stem files and sidecars).
 *
 * Controls used: [ChannelN] stem_count, stems_ready, stem_separate;
 * [ChannelN_StemK] volume, mute (K = 1..4); [Skin] stem_mode (deck overlay in
 * the QML interface, optional).
 */

var CommonStems = {};

CommonStems.MAX_STEMS = 4;

// Set to false to not touch [Skin],stem_mode (e.g. desktop skins without it)
CommonStems.useSkinStemMode = true;
// Use stems only when [ChannelN],stems_ready > 0 (this fork). Set to false on
// a Mixxx without instant stems (control missing).
CommonStems.requireReady = true;
// Pickup tolerance (0..1 of the knob travel)
CommonStems.pickupTolerance = 0.03;
// Two presses of the "stem mode" button within this time = reset all
CommonStems.doublePressMs = 400;

// Stems of each EQ band: HIGH = vocals (4), MID = bass + other (2, 3),
// LOW = drums (1). Same as the DDJ-FLX4 SMART CFX mode.
CommonStems.eqMap = {
    high: [4],
    mid: [2, 3],
    low: [1],
};

// Clock, replaceable in tests
CommonStems.now = function() {
    return Date.now();
};

// ---------------------------------------------------------------- helpers

CommonStems.deckNumber = function(deck) {
    if (typeof deck === "number") {
        return deck;
    }
    const m = String(deck).match(/Channel(\d+)/);
    return m ? Number(m[1]) : 0;
};

CommonStems.deckGroup = function(deck) {
    return "[Channel" + CommonStems.deckNumber(deck) + "]";
};

CommonStems.stemGroup = function(deck, stem) {
    return "[Channel" + CommonStems.deckNumber(deck) + "_Stem" + stem + "]";
};

/** Number of stems of the deck (0 = none / no track), at most 4 */
CommonStems.stemCount = function(deck) {
    const n = Number(engine.getValue(CommonStems.deckGroup(deck), "stem_count"));
    if (!(n > 0)) {
        return 0;
    }
    return Math.min(n, CommonStems.MAX_STEMS);
};

/** True if the deck has stems that can be used right now */
CommonStems.ready = function(deck) {
    if (CommonStems.stemCount(deck) <= 0) {
        return false;
    }
    if (!CommonStems.requireReady) {
        return true;
    }
    return Number(engine.getValue(CommonStems.deckGroup(deck), "stems_ready")) > 0;
};

CommonStems.isMuted = function(deck, stem) {
    return Number(engine.getValue(CommonStems.stemGroup(deck, stem), "mute")) > 0;
};

// ------------------------------------------------------------- separation

/**
 * Start the separation of the track of a deck that has no usable stems yet.
 * Returns true if it was requested. The control is pulsed (1 then 0) so that
 * it can be requested again for the next track.
 */
CommonStems.separate = function(deck) {
    const group = CommonStems.deckGroup(deck);
    if (CommonStems.ready(deck)) {
        return false;
    }
    engine.setValue(group, "stem_separate", 1);
    engine.setValue(group, "stem_separate", 0);
    return true;
};

/** separate() on every deck of the list that has no usable stems */
CommonStems.separateDecks = function(decks) {
    let n = 0;
    for (const deck of decks) {
        if (CommonStems.separate(deck)) {
            n++;
        }
    }
    return n;
};

// --------------------------------------------------------- mute / solo / reset

/**
 * Toggle the mute of a stem. On a deck without usable stems it only asks for
 * the separation. Returns "toggled", "separating" or "none".
 */
CommonStems.toggleMute = function(deck, stem) {
    if (!CommonStems.ready(deck)) {
        return CommonStems.separate(deck) ? "separating" : "none";
    }
    if (stem < 1 || stem > CommonStems.stemCount(deck)) {
        return "none";
    }
    const group = CommonStems.stemGroup(deck, stem);
    engine.setValue(group, "mute", CommonStems.isMuted(deck, stem) ? 0 : 1);
    return "toggled";
};

/**
 * Only `stem` plays, the others are muted. With toggle = true, if `stem` is
 * already the only one playing everything is unmuted again.
 * Returns "solo", "unmuted", "separating" or "none".
 */
CommonStems.solo = function(deck, stem, toggle) {
    if (!CommonStems.ready(deck)) {
        return CommonStems.separate(deck) ? "separating" : "none";
    }
    const count = CommonStems.stemCount(deck);
    if (stem < 1 || stem > count) {
        return "none";
    }
    let alreadySolo = !CommonStems.isMuted(deck, stem);
    for (let i = 1; i <= count && alreadySolo; i++) {
        if (i !== stem && !CommonStems.isMuted(deck, i)) {
            alreadySolo = false;
        }
    }
    if (toggle && alreadySolo) {
        for (let i = 1; i <= count; i++) {
            engine.setValue(CommonStems.stemGroup(deck, i), "mute", 0);
        }
        return "unmuted";
    }
    for (let i = 1; i <= count; i++) {
        engine.setValue(CommonStems.stemGroup(deck, i), "mute", i === stem ? 0 : 1);
    }
    return "solo";
};

/** Unmute all the stems and bring the volumes back to 1 (deck with stems only) */
CommonStems.resetDeck = function(deck) {
    const count = CommonStems.stemCount(deck);
    if (count <= 0) {
        return false;
    }
    for (let i = 1; i <= CommonStems.MAX_STEMS; i++) {
        const group = CommonStems.stemGroup(deck, i);
        engine.setValue(group, "volume", 1);
        engine.setValue(group, "mute", 0);
    }
    CommonStems.resetPickup(deck);
    return true;
};

/** resetDeck() if the deck has usable stems, otherwise ask for the separation */
CommonStems.resetOrSeparate = function(deck) {
    if (CommonStems.ready(deck)) {
        return CommonStems.resetDeck(deck) ? "reset" : "none";
    }
    return CommonStems.separate(deck) ? "separating" : "none";
};

CommonStems.resetAll = function(decks) {
    for (const deck of decks) {
        CommonStems.resetDeck(deck);
    }
};

/**
 * Pad layout used by most mappings (8 pads): pads 1-4 toggle the mute of stem
 * 1-4, pads 5-8 = only stem 1-4 (pressing it again unmutes everything).
 * `padNumber` is 1..8. Returns the result of toggleMute / solo.
 */
CommonStems.padPress = function(deck, padNumber) {
    if (padNumber >= 1 && padNumber <= 4) {
        return CommonStems.toggleMute(deck, padNumber);
    }
    if (padNumber >= 5 && padNumber <= 8) {
        return CommonStems.solo(deck, padNumber - 4, true);
    }
    return "none";
};

// ------------------------------------------------------------ stem volumes

/** Knob position (0..1) -> stem volume: right half = full, left half = down to silence */
CommonStems.volumeFromKnob = function(knob) {
    if (knob >= 0.5) {
        return 1;
    }
    return Math.pow(Math.max(knob, 0) / 0.5, 1.5);
};

/** Inverse of volumeFromKnob (the knob position of the left half) */
CommonStems.knobFromVolume = function(volume) {
    if (volume >= 0.999) {
        return 0.5;
    }
    return 0.5 * Math.pow(Math.max(volume, 0), 1 / 1.5);
};

CommonStems.pickups = {};

/** Forget the pickup state (all, or of one deck): call it when a mode starts */
CommonStems.resetPickup = function(deck) {
    if (deck === undefined) {
        CommonStems.pickups = {};
        return;
    }
    const prefix = CommonStems.deckNumber(deck) + ":";
    for (const key of Object.keys(CommonStems.pickups)) {
        if (key.indexOf(prefix) === 0) {
            delete CommonStems.pickups[key];
        }
    }
};

/**
 * Set the volume of `stems` (array of stem numbers) from a knob (0..1).
 * Returns true if it applied the volume, false if the deck has no usable
 * stems or the knob is still waiting to reach the current volume (pickup).
 * options.pickup (default true): the knob is ignored until it crosses the
 * current stem volume, so the stem does not jump. Without pickup the volume
 * follows the knob immediately.
 */
CommonStems.setVolumeFromKnob = function(deck, stems, knob, options) {
    if (!CommonStems.ready(deck)) {
        return false;
    }
    const count = CommonStems.stemCount(deck);
    const valid = stems.filter(function(s) {
        return s >= 1 && s <= count;
    });
    if (valid.length === 0) {
        return false;
    }
    const usePickup = !options || options.pickup !== false;
    if (usePickup) {
        const key = CommonStems.deckNumber(deck) + ":" + valid.join(",");
        let state = CommonStems.pickups[key];
        if (state === undefined) {
            state = {picked: false, last: undefined};
            CommonStems.pickups[key] = state;
        }
        if (!state.picked) {
            const volume = Number(engine.getValue(CommonStems.stemGroup(deck, valid[0]), "volume"));
            const target = CommonStems.knobFromVolume(volume);
            const tol = CommonStems.pickupTolerance;
            let reached;
            if (volume >= 0.999) {
                reached = knob >= 0.5 - tol;
            } else {
                reached = Math.abs(knob - target) <= tol ||
                    (state.last !== undefined && (state.last - target) * (knob - target) <= 0);
            }
            state.last = knob;
            if (!reached) {
                return false;
            }
            state.picked = true;
        }
    }
    const volume = CommonStems.volumeFromKnob(knob);
    for (const s of valid) {
        engine.setValue(CommonStems.stemGroup(deck, s), "volume", volume);
    }
    return true;
};

/**
 * EQ knob as stem volume. band = "high" | "mid" | "low", knob = 0..1.
 * Returns true if the stems took the knob (the caller must then NOT move the
 * EQ), false if the deck has no usable stems or the pickup is waiting: in the
 * first case the caller keeps the EQ behaviour. Typical use:
 *
 *     if (!(CommonStems.modeActive && CommonStems.eqToStems(deck, "high", v))) {
 *         // normal EQ
 *     }
 */
CommonStems.eqToStems = function(deck, band, knob, options) {
    const stems = CommonStems.eqMap[band];
    if (stems === undefined) {
        return false;
    }
    return CommonStems.setVolumeFromKnob(deck, stems, knob, options);
};

/**
 * Same as eqToStems but tells apart "waiting for pickup" from "no stems":
 * returns "applied", "waiting" or "nostems". Use it to keep the EQ still
 * (and not move it) while the pickup is waiting on a deck that has stems.
 */
CommonStems.eqToStemsState = function(deck, band, knob, options) {
    if (!CommonStems.ready(deck)) {
        return "nostems";
    }
    return CommonStems.eqToStems(deck, band, knob, options) ? "applied" : "waiting";
};

// --------------------------------------------------------- "stem mode" button

CommonStems.modeActive = false;
CommonStems.lastModePress = 0;

CommonStems.setMode = function(on) {
    on = !!on;
    if (on && !CommonStems.modeActive) {
        CommonStems.resetPickup();
    }
    CommonStems.modeActive = on;
    if (CommonStems.useSkinStemMode) {
        engine.setValue("[Skin]", "stem_mode", on ? 1 : 0);
    }
};

/**
 * Handler for a "hold = stem mode" button (like SMART CFX on the DDJ-FLX4):
 * press = mode on, starts the separation on `decks` without stems, a double
 * press resets all the stems; release = mode off.
 * value: MIDI value (> 0 pressed). Returns true on the press.
 */
CommonStems.modePress = function(value, decks) {
    const pressed = value > 0;
    CommonStems.setMode(pressed);
    if (!pressed) {
        return false;
    }
    CommonStems.separateDecks(decks);
    const now = CommonStems.now();
    if (now - CommonStems.lastModePress <= CommonStems.doublePressMs) {
        CommonStems.resetAll(decks);
        CommonStems.lastModePress = 0;
    } else {
        CommonStems.lastModePress = now;
    }
    return true;
};

// ------------------------------------------------------------------- LEDs

CommonStems.connections = [];
CommonStems.ledCallback = null;
CommonStems.watchedDecks = [];

CommonStems.stemLit = function(deck, stem) {
    return stem <= CommonStems.stemCount(deck) && !CommonStems.isMuted(deck, stem);
};

/** True if `stem` is the only one playing on the deck (solo) */
CommonStems.isSolo = function(deck, stem) {
    const count = CommonStems.stemCount(deck);
    if (stem < 1 || stem > count || CommonStems.isMuted(deck, stem)) {
        return false;
    }
    for (let i = 1; i <= count; i++) {
        if (i !== stem && !CommonStems.isMuted(deck, i)) {
            return false;
        }
    }
    return true;
};

/** Call the LED callback for the 4 stems of a deck */
CommonStems.refreshDeck = function(deck) {
    if (typeof CommonStems.ledCallback !== "function") {
        return;
    }
    const n = CommonStems.deckNumber(deck);
    for (let stem = 1; stem <= CommonStems.MAX_STEMS; stem++) {
        CommonStems.ledCallback(n, stem, CommonStems.stemLit(n, stem));
    }
};

CommonStems.refreshAll = function() {
    for (const deck of CommonStems.watchedDecks) {
        CommonStems.refreshDeck(deck);
    }
};

CommonStems.connect_ = function(group, control, fn) {
    try {
        const conn = engine.makeConnection(group, control, fn);
        if (conn) {
            CommonStems.connections.push(conn);
        }
    } catch (e) {
        console.warn("CommonStems: no connection to " + group + "," + control);
    }
};

/**
 * Optional LED support. callback(deck, stem, lit): lit = the stem exists and
 * is not muted. It is called when stem_count / stems_ready / mute change and
 * once for every stem at the start. Call unwatch() in the mapping shutdown.
 */
CommonStems.watch = function(decks, callback) {
    CommonStems.unwatch();
    CommonStems.ledCallback = callback;
    // only the decks that exist (no connection warnings on a 2-deck setup)
    const numDecks = Number(engine.getValue("[App]", "num_decks")) || 0;
    CommonStems.watchedDecks = decks.map(CommonStems.deckNumber).filter(function(d) {
        return numDecks <= 0 || d <= numDecks;
    });
    for (const deck of CommonStems.watchedDecks) {
        const refresh = function() {
            CommonStems.refreshDeck(deck);
        };
        CommonStems.connect_("[Channel" + deck + "]", "stem_count", refresh);
        CommonStems.connect_("[Channel" + deck + "]", "stems_ready", refresh);
        for (let stem = 1; stem <= CommonStems.MAX_STEMS; stem++) {
            CommonStems.connect_(CommonStems.stemGroup(deck, stem), "mute", refresh);
        }
        CommonStems.refreshDeck(deck);
    }
};

CommonStems.unwatch = function() {
    for (const conn of CommonStems.connections) {
        try {
            conn.disconnect();
        } catch (e) {
            // already gone
        }
    }
    CommonStems.connections = [];
    CommonStems.ledCallback = null;
    CommonStems.watchedDecks = [];
};

/** Leave the stems and the stem mode as found (call from the mapping shutdown) */
CommonStems.shutdown = function() {
    CommonStems.unwatch();
    if (CommonStems.modeActive) {
        CommonStems.setMode(false);
    }
    CommonStems.pickups = {};
};
