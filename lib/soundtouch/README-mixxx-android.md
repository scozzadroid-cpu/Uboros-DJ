# SoundTouch 2.3.3 for Android (Ubøros DJ)

Sources of SoundTouch 2.3.3 (LGPL-2.1, see `COPYING.TXT`), copied unchanged from
the release tarball except:

* `source/FIRFilter.cpp`: `evaluateFilterStereo` and `evaluateFilterMulti` call the NEON
  loops of `source/fir_neon.h` (float samples, arm64). The 8 channel filter
  of the stems was 49 % of the audio thread (simpleperf, 19/11/2026); on the
  POCO F4 GT: 277 -> 28 ns per frame (8 channels, 64 taps), stereo 17 -> 7 ns, the
  results are identical (stereo differs by 1e-6: another order of the sums).
  Benchmark and check: `tools/fir-neon/build_bench.sh` (runs on the phone).
* `include/soundtouch/soundtouch_config.h` is the empty stub of the release.

Only built for Android (`src/CMakeLists.txt`); the other platforms use the SoundTouch
of vcpkg / the system. To go back to the library of vcpkg on Android remove
the `if(ANDROID)` branch of that CMake block.
