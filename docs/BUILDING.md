# Building

Android arm64, Qt 6.9, NDK 27. This is the build used for the app; it is a CMake build of the Mixxx tree in this repository.

1. Install JDK 17, Android SDK + NDK 27.2.12479018 and download the Mixxx Android dependency bundle named in `build/env.sh`
   (`BUILDENV_URL`) into `buildenv/`.
2. Create **your own** signing keystore and export the `QT_ANDROID_KEYSTORE_*` variables (see `build/env.sh`).
3. Stem separation (`-DSTEM=ON`) needs a MNN checkout and three `.mnn` model files that are **not** in this repository:
   export `MIXXX_MNN_DIR`, `MIXXX_STEM_MODEL`, `MIXXX_STEM_LIGHT_MODEL`, `MIXXX_STEM_BEST_MODEL`.
   The scripts in `tools/stems/` export the networks to ONNX; convert them to MNN with MNN's own converter.
   Model weights have their own licenses (htdemucs: MIT; check the license of every model before redistributing it).
4. `build/build.sh amidi`, the APK ends up in `../build-amidi/android-build/`.

QML files are packaged as plain assets: a syntax error shows up on the phone as an empty window, so `build/build.sh` parses them first
with `qmlformat` from the dependency bundle.
