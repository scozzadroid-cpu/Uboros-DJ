# Roadmap

## Open hardware tests (need the controller)
- Browse knob behaviour, fast SHIFT+jog search, loading another track while analysing, same track on two decks, eject.
- Hot cue saved then app force-stopped; wake-from-idle by the controller.
- Auto Mix (never tried seriously), one-effect-at-a-time beat effects, the filter knob with a stem track.
- Import USB drive without root (the root mount and the USB library cannot be used together).

## Stems
- GPU separation (an OpenCL fp16 runtime is the fastest so far: ~2 s per 7.8 s chunk); STFT/iSTFT in C++ overlapped with the GPU.
- Two quality levels: a light model for instant separation on load, the high-quality model prepared in advance (charging / overnight) and swapped in.
- Better-quality candidate models, further GPU tuning when decks are stopped.

## Compatibility
- Test on Android 14 and on slower phones; a lighter mode for 4 GB RAM (native heap is ~590 MB today; the APK is ~240 MB).
- Run without root everywhere; remove unused libraries and Qt styles from the APK.

## Library and grids
- Automatic correction of beat grids from USB drives; review the MP3 timing fix.
- Faster SHIFT+jog search, aggregated underrun diagnostics, re-open files after USB read errors.

## Interface ideas (not decided)
- "In N beats" countdown to the next cue/phrase and tempo range display; mixer, effects and settings in the new style.
- Beat effects with one slot and the beat knob as length in beats; ReplayGain boost slider in the dashboard; crossfader state on screen.

## Project
- Source clean-up, build instructions without private paths, F-Droid-friendly build, upstream pull requests where changes are general.
