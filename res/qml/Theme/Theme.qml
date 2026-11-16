import QtQuick 2.12
pragma Singleton

QtObject {
    // OLED burn-in protection (Mixxx.Config.burnInProtection, set by main.qml):
    // the whites that stay on the screen for hours are a bit dimmer
    property bool dimWhites: false
    property color playMarkerColor: dimWhites ? "#8c8c8c" : "#D9D9D9"
    property color accent: "#24499f"
    property color accentColor: "#2f6fe0"
    property color backgroundColor: "#16181c"
    property color blue: "#01dcfc"
    property color bpmSliderBarColor: green
    property color buttonNormalColor: midGray
    property color crossfaderBarColor: red
    property color crossfaderOrientationColor: lightGray
    property color darkGray: "#0d0e11"
    property color darkGray2: "#1d2026"
    property color darkGray3: "#191b20"
    property color deckActiveColor: green
    property color deckBackgroundColor: darkGray
    property color deckLineColor: "#272b33"
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
    property color midGray: "#8a9099"
    property color panelSplitterBackground: backgroundColor
    property color panelSplitterHandleActive: lightGray2
    property color panelSplitterHandle: midGray
    property color pflActiveButtonColor: blue
    property color red: "#ea2a4e"
    property color samplerColor: blue
    property color sunkenBackgroundColor: "#0b0c0f"
    property color textColor: white
    property color toolbarActiveColor: white
    property color toolbarBackgroundColor: "#0f1114"
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
    // Deck colors, like the Pioneer decks: 1 blue, 2 orange, 3 green, 4 magenta
    readonly property var deckColors: ["#2fa8ff", "#ff8a2a", "#3ddc84", "#e05cff"]
    function deckColor(group) {
        const match = /(\d+)\]$/.exec(group || "");
        const index = match ? parseInt(match[1]) - 1 : 0;
        return deckColors[(index % deckColors.length + deckColors.length) % deckColors.length];
    }
    // Camelot wheel colors (1..12), as on the usual key wheels
    function camelotColor(n) {
        return Qt.hsla(((n - 1) * 30 + 90) % 360 / 360, 0.75, 0.55, 1);
    }
    property color textMutedColor: "#8a9099"
    property color buttonBorderColor: "#2b3038"
    property color buttonBackgroundColor: "#1f2329"
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
