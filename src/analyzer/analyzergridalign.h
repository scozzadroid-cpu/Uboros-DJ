#pragma once

#include <memory>
#include <vector>

#include "analyzer/analyzer.h"
#include "preferences/usersettings.h"

class FFTReal;

/// Places the beat grid read from a rekordbox USB drive on the beats of the
/// audio as Mixxx decodes it.
///
/// The rekordbox grid is in rekordbox's own time line: a decoder that starts
/// the audio earlier or later (MP3 encoder delay, see the timing table in
/// rekordboxfeature.cpp) or a grid that rekordbox placed badly leave the
/// kicks before or after the lines, and SYNC then puts two tracks out of
/// phase by the difference.
///
/// Measurement: spectral flux (onset strength) of the whole track, summed
/// around every beat of the grid for shifts of -80..+80 ms. The shift with
/// the most onsets is where the beats are. The grid (and the cues, that were
/// placed on it) is moved only when the peak is clear and the four quarters
/// of the track agree; otherwise the grid is marked unsure and kept.
///
/// Runs once per track (the result is in the beats sub-version, see
/// rekordboxconstants.h), never changes the files of the drive.
class AnalyzerGridAlign : public Analyzer {
  public:
    explicit AnalyzerGridAlign(UserSettingsPointer pConfig);
    ~AnalyzerGridAlign() override;

    static ConfigKey enabledConfigKey();

    bool initialize(const AnalyzerTrack& track,
            mixxx::audio::SampleRate sampleRate,
            mixxx::audio::ChannelCount channelCount,
            SINT frameLength) override;
    bool processSamples(const CSAMPLE* pIn, SINT count) override;
    void storeResults(TrackPointer pTrack) override;
    void cleanup() override;

  private:
    void processFrame();

    UserSettingsPointer m_pConfig;
    mixxx::audio::SampleRate m_sampleRate;
    mixxx::audio::ChannelCount m_channelCount;
    SINT m_frameLength = 0;

    std::unique_ptr<FFTReal> m_pFft;
    std::vector<double> m_window;
    std::vector<double> m_frame;
    std::vector<double> m_re;
    std::vector<double> m_im;
    std::vector<float> m_prevSpectrum;
    std::vector<float> m_spectrum;
    std::vector<float> m_pending; // mono samples not yet consumed
    std::vector<float> m_flux;    // one value per hop
};
