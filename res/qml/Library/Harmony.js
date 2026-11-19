.pragma library

// Harmonic mixing helpers for the track list: compare the key and BPM of a
// track with the deck that is playing (Camelot wheel).
//
// A Camelot key is {n: 1..12, minor: bool}: 8A = A minor, 8B = C major.

// Pitch class (C = 0) of the note names, with the common enharmonics
const kPitchClass = {
    "C": 0, "B#": 0,
    "C#": 1, "Db": 1,
    "D": 2,
    "D#": 3, "Eb": 3,
    "E": 4, "Fb": 4,
    "F": 5, "E#": 5,
    "F#": 6, "Gb": 6,
    "G": 7,
    "G#": 8, "Ab": 8,
    "A": 9,
    "A#": 10, "Bb": 10,
    "B": 11, "Cb": 11,
};

function camelotOfMajor(pitchClass) {
    // C major = 8B, each fifth up adds 1
    return ((pitchClass * 7) % 12 + 7) % 12 + 1;
}

function fromPitchClass(pitchClass, minor) {
    // A minor (pc 9) shares the number of its relative major C (pc 0)
    const n = camelotOfMajor(minor ? (pitchClass + 3) % 12 : pitchClass);
    return {n: n, minor: minor};
}

/// Mixxx ChromaticKey value of the [ChannelN],key control:
/// 1..12 = C..B major, 13..24 = C..B minor, 0 = invalid.
function fromChromaticKey(value) {
    const key = Math.round(value);
    if (key < 1 || key > 24)
        return null;
    const minor = key > 12;
    return fromPitchClass((key - 1) % 12, minor);
}

/// Parses the key text shown in the library: traditional ("Cm", "Bbm",
/// "D♭", "F# minor"), Camelot/Lancelot ("8A", "8A (Am)") or OpenKey ("1m",
/// "1d (C)").
function fromText(text) {
    if (!text)
        return null;
    let t = String(text).trim().replace(/♭/g, "b").replace(/♯/g, "#");
    // Camelot/Lancelot, alone or followed by the traditional name: "8A (Am)"
    let match = /^(\d{1,2})\s*([ABab])(?:\s*\(.*\))?$/.exec(t);
    if (match) {
        const n = parseInt(match[1]);
        if (n < 1 || n > 12)
            return null;
        return {n: n, minor: match[2].toUpperCase() === "A"};
    }
    match = /^(\d{1,2})\s*([dmDM])(?:\s*\(.*\))?$/.exec(t);
    if (match) {
        // OpenKey 1d = C major = 8B, 1m = A minor = 8A
        const n = parseInt(match[1]);
        if (n < 1 || n > 12)
            return null;
        return {n: (n + 6) % 12 + 1, minor: match[2].toLowerCase() === "m"};
    }
    match = /^([A-Ga-g])([#b]?)\s*(m|min|minor|maj|major)?$/.exec(t);
    if (match) {
        const name = match[1].toUpperCase() + match[2];
        const pitchClass = kPitchClass[name];
        if (pitchClass === undefined)
            return null;
        const minor = match[3] !== undefined && match[3].startsWith("m") && !match[3].startsWith("ma");
        return fromPitchClass(pitchClass, minor);
    }
    return null;
}

/// 2 = same key, 1 = compatible (±1 on the wheel or relative major/minor),
/// 0 = not compatible or unknown.
function compatibility(a, b) {
    if (!a || !b)
        return 0;
    if (a.n === b.n && a.minor === b.minor)
        return 2;
    if (a.n === b.n)
        return 1;
    if (a.minor === b.minor) {
        const d = Math.abs(a.n - b.n);
        if (d === 1 || d === 11)
            return 1;
    }
    return 0;
}

/// Relative BPM difference in percent, also comparing half/double tempo.
/// Returns Infinity when one of them is unknown.
function bpmDistancePercent(bpm, referenceBpm) {
    const value = parseFloat(String(bpm).replace(",", "."));
    if (!(value > 0) || !(referenceBpm > 0))
        return Infinity;
    let best = Infinity;
    for (const factor of [0.5, 1, 2]) {
        const d = Math.abs(value * factor - referenceBpm) / referenceBpm * 100;
        best = Math.min(best, d);
    }
    return best;
}

// ---- Matching (the Club "Matching" list: tracks that fit a deck) ----

/// Same window as the BPM highlight of the track list (BpmDelegate: yellow up to 6%)
const kMatchingBpmPercent = 6;

/// OpenKey text ("1d".."12d" major, "1m".."12m" minor) of a Mixxx ChromaticKey
/// (1..12 = C..B major, 13..24 = C..B minor). The search of the library
/// understands it ("key:1m"). Empty when the key is unknown.
function openKeyText(chromaticKey) {
    const key = Math.round(chromaticKey);
    if (key < 1 || key > 24)
        return "";
    const minor = key > 12;
    const pitchClass = (key - 1) % 12;
    // Position on the circle of fifths of the (relative) major key, C = 1
    const majorPitchClass = minor ? (pitchClass + 3) % 12 : pitchClass;
    return ((majorPitchClass * 7) % 12 + 1) + (minor ? "m" : "d");
}

/// Camelot name ("8A", "11B") of a ChromaticKey, "" when unknown
function camelotText(chromaticKey) {
    const camelot = fromChromaticKey(chromaticKey);
    return camelot ? camelot.n + (camelot.minor ? "A" : "B") : "";
}

/// The keys that go with a key: the ones that compatibility() accepts
/// (same key, relative major/minor, one step on the wheel).
function compatibleChromaticKeys(chromaticKey) {
    const reference = fromChromaticKey(chromaticKey);
    const keys = [];
    if (!reference)
        return keys;
    for (let key = 1; key <= 24; key++) {
        if (compatibility(reference, fromChromaticKey(key)) > 0)
            keys.push(key);
    }
    return keys;
}

/// Search (library search syntax) for the tracks that fit a deck's track: BPM
/// within kMatchingBpmPercent of the reference (also half and double tempo,
/// like bpmDistancePercent) and a compatible key. Built once when the track
/// of the deck changes; the database does the filtering. Alternatives are
/// written as "a | b", so every BPM range is repeated for each key.
/// No reference BPM: "id:0", which nothing matches.
function matchingQuery(referenceBpm, referenceChromaticKey) {
    if (!(referenceBpm > 0))
        return "id:0";
    const low = 1 - kMatchingBpmPercent / 100;
    const high = 1 + kMatchingBpmPercent / 100;
    // value * factor within [low, high] * reference, factor = 0.5, 1, 2
    const ranges = [];
    for (const factor of [1, 0.5, 2]) {
        const from = referenceBpm * low / factor;
        const to = referenceBpm * high / factor;
        ranges.push("bpm:" + from.toFixed(2) + "-" + to.toFixed(2));
    }
    const keys = compatibleChromaticKeys(referenceChromaticKey).map(openKeyText);
    const terms = [];
    for (const range of ranges) {
        if (keys.length === 0) {
            terms.push(range);
            continue;
        }
        for (const key of keys)
            terms.push(range + " key:" + key);
    }
    return terms.join(" | ");
}
