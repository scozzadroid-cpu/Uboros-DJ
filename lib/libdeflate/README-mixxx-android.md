# libdeflate 1.26 for Android (Ubøros DJ)

`lib/`, `libdeflate.h`, `common_defs.h` and `COPYING` of libdeflate v1.26
(https://github.com/ebiggers/libdeflate, commit 92e6a0db9fa848d742f9eb286c92afc60f2c3dda,
MIT licence, see `COPYING`), copied unchanged from the tag.

Used through `src/util/fastdeflate.{h,cpp}` for the analysis blobs (waveforms,
1-2.5 MB each) that `qCompress`/`qUncompress` (zlib) handled. Measured on the
POCO F4 GT (prime core, 14.5 MB of real analysis files, the static zlib of the
build environment): level 6 compression 981 -> 235 ms (x4.2), level 1 162 -> 72 ms
(x2.25), decompression 49 -> 25 ms (x1.9). Both libraries read each other's streams.

Only built for Android (`src/CMakeLists.txt`, target `libdeflate-android`); the
other platforms keep zlib through Qt. To update: copy the same files of a newer tag.
