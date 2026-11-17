# Improvements over the upstream engine

All of this is implemented in the (not yet published) source tree and runs on the author's phone. Several items still need
verification with the controller attached; those are listed in [ROADMAP.md](ROADMAP.md).

## MIDI and controller
- Android MIDI backend using NDK AMidi (Android 10+), with hot-plug and automatic mapping selection.
- Initial LED state of the controller set correctly; LOAD buttons; browse knob (press = focus sidebar/list, turn = scroll, SHIFT+press = waveform view).
- Beat effects: one effect at a time, level/depth absolute, stronger Flanger, Bitcrusher, pumping Compressor; performance-pad effects.
- The filter knob drives stems; filter cutoff in octaves with resonant Q; the controller's firmware smart fader kept off.
- Headphone cue/master mix fixed, headphones get the main mix without cue.

## Audio and performance
- Oboe output by default, saved settings, fallback to default devices when saved ones are missing, use of the configured device when plugged in later.
- No underflows from idle network workers; aggregated audio-callback timing log when audio drops.
- 60 Hz instead of 120 Hz render; waveforms not redrawn when hidden; waveform scene graph no longer rebuilt per frame; optional three-band waveform drawn by the GPU.
- One analysis thread per loaded tracks and one for bulk analysis; analysis and stem separation at SCHED_BATCH; analysis cancelled for a track replaced in its deck.
- SQLite in WAL mode; cover art as thumbnails; one shared track proxy per loaded track.
- Short reads no longer truncate a track forever; file re-opened after a failed read; one in-flight read during fast search.
- Android: MP3 read into memory instead of mapped; native code built for API 34.

## Library and USB drives
- USB-drive and local-files sections in the QML sidebar; search in `/storage/*` for DJ exports, including mounted volumes.
- "Import USB drive" without root through the USB host API (Apache-2.0 USB library), read-only on the drive.
- Analyses kept on the USB drive in a separate folder (never inside the vendor's own folder), so the drive stays compatible with club players; device analyses written off the GUI thread.
- Beat grid fixes for MP3s on exported drives (decoder timing), constant grids imported as one exact tempo, grid measured against the audio; unverified grids shown with a yellow BPM and a question mark.
- Harmonic key notation by default; harmonic and BPM-proximity highlighting in the track list; automatic analysis of tracks without BPM; half/double/triplet BPM snapped to the file tag.
- Loaded tracks (cues, grids, keys) saved when the app leaves the foreground.

## Stems
- Playback of 3-stem files (drums, bass + melody, vocals).
- On-device separation with a native neural separator and a light, faster mode; progressive 8-channel source, disk cache with LRU clean-up, quality setting and detection of phones too slow.
- Separation started by the filter knob; stem column in the track lists; `STEMS` badge on the deck bar.

## Interface (QML)
- Compact phone layout, alternating Library / Mixer views, WAVES view, dashboard with stem, cache, analysis and waveform options.
- Touch track list (vertical scroll, long press = "Load to"), scrolling title when it does not fit.
- Club-player-style deck bar: BPM, delta "+2.0 (+1.6%)", remaining time (red in the last 30 s), harmonic key pill, deck colour and number, MASTER / SYNC / MT (key lock) / Q / LOOP indicators, beat counter "1.1 ■□□□".
- Full-track overview under each deck; three-band waveform colours.
- Settings usable on a phone; Auto Mix with filter and echo transitions; selectable effects limited to Echo, Reverb, Flanger, Bitcrusher.
- OLED burn-in protection (dimmer whites, slow pixel orbiting, screen off when idle, woken by the first touch or controller input).
- Whole screen used (including the camera cut-out), both landscape orientations.
