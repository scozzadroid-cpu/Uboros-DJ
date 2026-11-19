import QtQuick 2.12
import "Profiles.js" as Profiles
pragma Singleton

QtObject {
    // Interface profile ("uboros", "club" or "stack"): set by main.qml from
    // Mixxx.Config.interfaceProfile. The palette and the waveform colors
    // below follow it; "uboros" keeps the original values (see Profiles.js)
    property string profile: "uboros"
    readonly property var pal: Profiles.palette(profile)
    // Layout of the profile (blocks of main.qml, DeckRow, WaveformDisplay,
    // Library, TrackList...). "uboros" is the original layout: every layout
    // value below has the original number as its default branch
    readonly property bool isClub: profile === "club"
    readonly property bool isStack: profile === "stack"
    // Overview of each deck on its waveform (compact DJ view): Club full width
    // under the waveform, Stack thin above it; 0 = it stays in the deck
    readonly property int waveOverviewHeight: isClub ? 15 : (isStack ? 10 : 0)
    // Height of the waveforms: Stack x1.25 up to 150 dp (uboros: x1, up to 120)
    readonly property real waveHeightScale: isStack ? 1.25 : 1.0
    readonly property int waveHeightMax: isStack ? 150 : 120
    // Share of the width for the sources of the library (0 = the original
    // width: 170 dp max on a phone, 415 dp otherwise)
    readonly property real librarySidebarShare: isClub ? 0.22 : (isStack ? 0.18 : 0.0)
    // Rows and text of the track list
    readonly property int trackRowHeight: isStack ? 18 : (isClub ? 22 : (Qt.platform.os === "android" ? 22 : 30))
    readonly property int trackFontSize: isStack ? 11 : (Qt.platform.os === "android" ? 12 : 14)
    // OLED burn-in protection (Mixxx.Config.burnInProtection, set by main.qml):
    // the whites that stay on the screen for hours are a bit dimmer
    property bool dimWhites: false
    property color playMarkerColor: dimWhites ? "#8c8c8c" : "#D9D9D9"
    property color accent: pal.accent
    property color accentColor: pal.accentColor
    property color backgroundColor: pal.backgroundColor
    property color blue: "#01dcfc"
    property color bpmSliderBarColor: green
    property color buttonNormalColor: midGray
    property color crossfaderBarColor: red
    property color crossfaderOrientationColor: lightGray
    property color darkGray: pal.darkGray
    property color darkGray2: pal.darkGray2
    property color darkGray3: pal.darkGray3
    property color deckActiveColor: green
    property color deckBackgroundColor: darkGray
    property color deckLineColor: pal.deckLineColor
    property color deckTextColor: white
    property color effectColor: yellow
    property color effectUnitColor: red
    property color embeddedBackgroundColor: "#a0000000"
    property color eqFxColor: red
    property color eqHighColor: white
    property color eqLowColor: white
    property color eqMidColor: white
    property color gainKnobColor: blue
    property color green: "#85c85b"
    property color knobBackgroundColor: "#262626"
    property color lightGray2: "#b0b0b0"
    property color lightGray: "#747474"
    property color midGray: pal.midGray
    property color panelSplitterBackground: backgroundColor
    property color panelSplitterHandleActive: lightGray2
    property color panelSplitterHandle: midGray
    property color pflActiveButtonColor: blue
    property color red: "#ea2a4e"
    property color samplerColor: blue
    property color sunkenBackgroundColor: pal.sunkenBackgroundColor
    property color textColor: white
    property color toolbarActiveColor: white
    property color toolbarBackgroundColor: pal.toolbarBackgroundColor
    property color volumeSliderBarColor: blue
    property color warningColor: "#7D3B3B"
    property color waveformBeatColor: lightGray
    property color waveformCursorColor: white
    property color waveformMarkerDefault: '#ff7a01'
    property color waveformMarkerIntroOutroColor: '#2c5c9a'
    property color waveformMarkerLabel: Qt.rgba(255, 255, 255, 0.8)
    property color waveformMarkerLoopColor: '#00b400'
    property color waveformMarkerLoopColorDisabled: '#FFFFFF'
    property color waveformPostrollColor: midGray
    property color waveformPrerollColor: midGray
    property color white: dimWhites ? "#c4c7cb" : "#e4e7eb"
    property color yellow: "#fca001"
    // Deck colors (uboros: 1 blue, 2 orange, 3 green, 4 magenta), by profile
    readonly property var deckColors: pal.deck
    function deckColor(group) {
        const match = /(\d+)\]$/.exec(group || "");
        const index = match ? parseInt(match[1]) - 1 : 0;
        return deckColors[(index % deckColors.length + deckColors.length) % deckColors.length];
    }
    // Camelot wheel colors (1..12), as on the usual key wheels
    function camelotColor(n) {
        return Qt.hsla(((n - 1) * 30 + 90) % 360 / 360, 0.75, 0.55, 1);
    }
    property color textMutedColor: pal.textMutedColor
    property color buttonBorderColor: pal.buttonBorderColor
    property color buttonBackgroundColor: pal.buttonBackgroundColor
    // Scrolling waveforms and overviews (WaveformDisplay.qml, WaveformOverview.qml),
    // by profile
    property color waveLow: pal.waveLow
    property color waveMid: pal.waveMid
    property color waveHigh: pal.waveHigh
    property color waveAxes: pal.waveAxes
    property real waveGain: pal.waveGain
    property color waveBeat: pal.waveBeat
    property color waveBackground: pal.waveBackground
    property color wavePreroll: pal.wavePreroll
    property color waveEndOfTrack: pal.waveEndOfTrack
    // Added to the default zoom of the waveform
    property int waveZoomOffset: pal.waveZoomOffset
    // Names, descriptions and cycle of the profiles (Settings > Interface, dashboard)
    readonly property var profileIds: Profiles.ids
    function paletteOf(id) {
        return Profiles.palette(id);
    }
    function profileName(id) {
        return Profiles.name(id);
    }
    function profileDescription(id, italian) {
        return Profiles.description(id, italian);
    }
    function nextProfile(id) {
        return Profiles.next(id);
    }
    property int buttonFontPixelSize: 10
    property int textFontPixelSize: 14
    property string fontFamily: "Open Sans"
    property string imgBpmSliderBackground: "images/slider_bpm.svg"
    property string imgButton: "images/button.svg"
    property string imgButtonPressed: "images/button_pressed.svg"
    property string imgCrossfaderBackground: "images/slider_crossfader.svg"
    property string imgCrossfaderHandle: "images/slider_handle_crossfader.svg"
    property string imgKnob: "images/knob.svg"
    property string imgKnobMini: "images/miniknob.svg"
    property string imgKnobMiniShadow: "images/miniknob_shadow.svg"
    property string imgKnobShadow: "images/knob_shadow.svg"
    property string imgMicDuckingSlider: "images/slider_micducking.svg"
    property string imgMicDuckingSliderHandle: "images/slider_handle_micducking.svg"
    property string imgPopupBackground: imgButton
    property string imgSectionBackground: "images/section.svg"
    property string imgSliderHandle: "images/slider_handle.svg"
    property string imgVolumeSliderBackground: "images/slider_volume.svg"
}
