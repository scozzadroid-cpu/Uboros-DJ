#!/usr/bin/env python3
"""Convert 4-stem .mxstem sidecars (mix, drums, bass, other, vocals) to 3 stems
(mix, drums, bass + melody, vocals) without separating again.

Usage: to3stems.py <file.mxstem>...   (rewrites each file in place, atomically)

Only bass + other is decoded, summed and re-encoded (one MP3 generation more for
that stem); mix, drums and vocals are copied packet by packet, so their timing
(MAD delay, LAME priming) stays exactly as stemify_usb.py made it. Files that
already have 3 stems are left alone.
"""
import json
import os
import sys

import av
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stemify_usb import MANIFEST, STEM_BITRATE, add_manifest  # noqa: E402


def decode_stream(path, index):
    with av.open(path) as c:
        s = c.streams.audio[index]
        frames = [f.to_ndarray() for f in c.decode(s)]
        return np.concatenate(frames, axis=1).astype(np.float32), s.codec_context.sample_rate


def convert(path):
    with av.open(path) as c:
        n = len(c.streams.audio)
    if n == 4:
        print(f"{path}: already 3 stems")
        return
    if n != 5:
        print(f"{path}: {n} audio streams, skipped")
        return
    bass, rate = decode_stream(path, 2)
    other, _ = decode_stream(path, 3)
    melody = np.clip(bass + other, -1, 1)
    tmp = path + ".tmp3.mp4"
    with av.open(path) as src, av.open(tmp, "w", format="mp4") as out:
        ins = list(src.streams.audio)
        # Stream order in the new file: mix, drums, bass + melody, vocals
        o_mix = out.add_stream_from_template(ins[0])
        o_drums = out.add_stream_from_template(ins[1])
        enc = out.add_stream("libmp3lame", rate=rate, layout="stereo")
        enc.bit_rate = STEM_BITRATE
        o_vocals = out.add_stream_from_template(ins[4])
        target = {ins[0].index: o_mix, ins[1].index: o_drums, ins[4].index: o_vocals}
        # The decoded stems start with the LAME priming of the original file
        # (FFmpeg decodes it as audio); the new encoder adds its own priming,
        # so drop the old one to keep the stem aligned with the others.
        melody = melody[:, 1105:]
        packets = []
        block = 1152 * 40
        for pos in range(0, melody.shape[1], block):
            f = av.AudioFrame.from_ndarray(np.ascontiguousarray(melody[:, pos:pos + block]),
                                           format="fltp", layout="stereo")
            f.sample_rate = rate
            packets.extend(enc.encode(f))
        packets.extend(enc.encode(None))
        for p in src.demux(ins[0], ins[1], ins[4]):
            if p.dts is None:
                continue
            p.stream = target[p.stream.index]
            out.mux(p)
        for p in packets:
            out.mux(p)
    add_manifest(tmp)
    os.replace(tmp, path)
    print(f"{path}: converted to 3 stems")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        convert(p)
