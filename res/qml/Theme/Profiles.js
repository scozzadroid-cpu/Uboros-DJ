.pragma library

// Interface profiles ([Skin],interface_profile, Mixxx.Config.interfaceProfile):
// palette and waveform colors of each one. Theme.qml reads its colors from
// palette(Theme.profile).
//
// "uboros" is the original look: every value below is exactly what Theme.qml
// and WaveformDisplay.qml / WaveformOverview.qml had before the profiles
// existed. Do not change it: changes belong to "club" and "stack".
//
// Static data only (no animations, no shaders): switching profile re-evaluates
// the colors once.

// Order of the cycle (dashboard action) and of the cards (Settings > Interface)
var ids = ["uboros", "club", "stack"];

var names = {
    "uboros": "Uboros",
    "club": "Club",
    "stack": "Stack"
};

// One line each, Italian and English
var descriptions = {
    "uboros": {
        "it": "La vista originale: onde blu, ambra e bianche, deck compatti.",
        "en": "The original look: blue, amber and white waves, compact decks."
    },
    "club": {
        "it": "Grafite neutro con accento blu; griglia bianca sottile con la battuta in rosso.",
        "en": "Neutral graphite with a blue accent; thin white beat grid with the bar start in red."
    },
    "stack": {
        "it": "Quasi nero, colori saturi; onde colorate per frequenza: bassi rossi, medi verdi, alti azzurri.",
        "en": "Near black, vivid colors; waves colored by frequency: red lows, green mids, light blue highs."
    }
};

var palettes = {
    // The original look
    "uboros": {
        "accent": "#24499f",
        "accentColor": "#2f6fe0",
        "backgroundColor": "#16181c",
        "darkGray": "#0d0e11",
        "darkGray2": "#1d2026",
        "darkGray3": "#191b20",
        "deckLineColor": "#272b33",
        "sunkenBackgroundColor": "#0b0c0f",
        "toolbarBackgroundColor": "#0f1114",
        "buttonBorderColor": "#2b3038",
        "buttonBackgroundColor": "#1f2329",
        "midGray": "#8a9099",
        "textMutedColor": "#8a9099",
        // Decks 1..4: blue, orange, green, magenta
        "deck": ["#2fa8ff", "#ff8a2a", "#3ddc84", "#e05cff"],
        // Waveform: three bands (blue lows, amber mids, near-white highs)
        "waveLow": "#1f5fff",
        "waveMid": "#c9852f",
        "waveHigh": "#f1e7d6",
        "waveAxes": "#a1a1a1a1",
        "waveGain": 1.0,
        "waveBeat": "#60ffffff",
        "waveBackground": "#5e000000",
        "wavePreroll": "#3a3f47",
        "waveEndOfTrack": "#ff8872",
        // Added to the default zoom of the waveform (higher = more beats on screen)
        "waveZoomOffset": 0
    },
    // Neutral graphite, blue accent, decks 1 blue / 2 orange as in uboros,
    // same three bands, whiter thin beat lines (the bar start is red in the
    // beat renderer for every profile)
    "club": {
        "accent": "#2a5fc4",
        "accentColor": "#3d82f0",
        "backgroundColor": "#1b1b1d",
        "darkGray": "#0f0f10",
        "darkGray2": "#242426",
        "darkGray3": "#1f1f21",
        "deckLineColor": "#303033",
        "sunkenBackgroundColor": "#0c0c0d",
        "toolbarBackgroundColor": "#121213",
        "buttonBorderColor": "#37373a",
        "buttonBackgroundColor": "#272729",
        "midGray": "#8e8e93",
        "textMutedColor": "#8e8e93",
        "deck": ["#2fa8ff", "#ff8a2a", "#3ddc84", "#e05cff"],
        "waveLow": "#1f5fff",
        "waveMid": "#c9852f",
        "waveHigh": "#f1e7d6",
        "waveAxes": "#a1a1a1a1",
        "waveGain": 1.0,
        "waveBeat": "#98ffffff",
        "waveBackground": "#5e000000",
        "wavePreroll": "#38383b",
        "waveEndOfTrack": "#ff8872",
        "waveZoomOffset": 0
    },
    // Near black, saturated colors, decks 1/2 in clearly different hues,
    // waves colored by frequency (red/magenta lows, green mids, light blue
    // highs) on the same Filtered renderer, a bit more gain, fainter grid,
    // a bit wider zoom
    "stack": {
        "accent": "#0b52cf",
        "accentColor": "#1c7bff",
        "backgroundColor": "#08080a",
        "darkGray": "#040405",
        "darkGray2": "#101013",
        "darkGray3": "#0c0c0f",
        "deckLineColor": "#1c1c21",
        "sunkenBackgroundColor": "#030304",
        "toolbarBackgroundColor": "#050506",
        "buttonBorderColor": "#25252b",
        "buttonBackgroundColor": "#121216",
        "midGray": "#8a8d94",
        "textMutedColor": "#8a8d94",
        "deck": ["#1e8cff", "#ffc400", "#00e676", "#ff2bd6"],
        "waveLow": "#ff2d6a",
        "waveMid": "#2bd96b",
        "waveHigh": "#4cc4ff",
        "waveAxes": "#a1a1a1a1",
        "waveGain": 1.2,
        "waveBeat": "#38ffffff",
        "waveBackground": "#80000000",
        "wavePreroll": "#26262b",
        "waveEndOfTrack": "#ff8872",
        "waveZoomOffset": 1
    }
};

function isProfile(profile) {
    return Object.prototype.hasOwnProperty.call(palettes, profile);
}

function palette(profile) {
    return isProfile(profile) ? palettes[profile] : palettes["uboros"];
}

function name(profile) {
    return isProfile(profile) ? names[profile] : names["uboros"];
}

function description(profile, italian) {
    const d = descriptions[isProfile(profile) ? profile : "uboros"];
    return italian ? d.it : d.en;
}

// Next profile in the cycle uboros -> club -> stack -> uboros
function next(profile) {
    const i = ids.indexOf(profile);
    return ids[(i + 1) % ids.length];
}
