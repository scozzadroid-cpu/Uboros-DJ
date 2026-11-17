#!/usr/bin/env python3
"""Separate the tracks of a rekordbox USB drive into Mixxx stem sidecars.

Usage: stemify_usb.py <copy of the drive root> <out dir> [--limit N] [--only SUBSTR]

For every audio file under <root>/Contents writes <out>/<hash>.mxstem, where
<hash> = first 24 hex digits of SHA-1(relative path, UTF-8), the same name as
the MIXXX/ANLZ/<hash>.mxa analysis sidecar. The file is an NI-style stem MP4
(stream 0 = mix, 1 drums, 2 bass + melody ("other"), 3 vocals:
the user wants 3 stems only; moov/udta/stem manifest with 3 entries) with MP3 audio at the track's own sample rate, so beat grids and
cues computed on the original file stay valid. The extension is not an audio
one: CDJs browsing folders do not list it as a track.

Timeline: Mixxx decodes MP3 files with MAD (SoundSourceMp3), measured
+22.5 ms behind FFmpeg's gapless decoding (STATUS 2026-11-16 09:30). The
stems are made from the gapless decode, so they are delayed by the same
amount to line up with the original track in Mixxx.

index.tsv in <out> lists hash, relative path, seconds, status.
"""
import argparse
import hashlib
import io
import json
import os
import struct
import sys
import time

import av
import numpy as np
import torch
from demucs.apply import apply_model
from demucs.pretrained import get_model

AUDIO_EXT = (".mp3", ".m4a", ".mp4", ".aac", ".flac", ".wav", ".aif", ".aiff", ".ogg", ".opus")
MAD_DELAY_SECONDS = 0.0225
# libmp3lame priming (576 + 529 decoder delay): FFmpeg writes an edit list
# with media_time 0, so FFmpeg (and Mixxx) decode these samples as audio.
LAME_PRIMING_SAMPLES = 1105
MIX_BITRATE = 128000
STEM_BITRATE = 160000
# 3 stems (user decision): drums, bass + other ("melody"), vocals. Mixxx plays
# them as 4 with a silent third slot (SoundSourceSTEM), so the stem controls
# keep their slots: 1 drums, 2 bass + melody, 4 vocals.
STEM_GROUPS = [["drums"], ["bass", "other"], ["vocals"]]

MANIFEST = {
    "version": 1,
    "mastering_dsp": {
        "compressor": {"enabled": False, "ratio": 3, "output_gain": 0.5,
                       "release": 0.3, "attack": 0.003, "input_gain": 0.5,
                       "threshold": 0, "hp_cutoff": 300, "dry_wet": 50},
        "limiter": {"enabled": False, "release": 0.05, "threshold": 0,
                    "ceiling": -0.35},
    },
    "stems": [
        {"name": "Drums", "color": "#FF4040"},
        {"name": "Bass + Melody", "color": "#00C8FF"},
        {"name": "Vocals", "color": "#40FF60"},
    ],
}


def decode(path):
    """Decode to float32 (channels, frames), stereo, at the file's rate."""
    with av.open(path) as c:
        s = c.streams.audio[0]
        rate = s.codec_context.sample_rate
        res = av.AudioResampler(format="fltp", layout="stereo", rate=rate)
        chunks = []
        for frame in c.decode(s):
            for f in res.resample(frame):
                chunks.append(f.to_ndarray())
        for f in res.resample(None):
            chunks.append(f.to_ndarray())
    return np.concatenate(chunks, axis=1).astype(np.float32), rate


def encode_mp4(streams, rate, path):
    """streams: list of (channels, frames) float32 arrays -> MP4 with MP3 tracks."""
    with av.open(path, "w", format="mp4") as out:
        outs = []
        for i, _ in enumerate(streams):
            st = out.add_stream("libmp3lame", rate=rate, layout="stereo")
            st.bit_rate = MIX_BITRATE if i == 0 else STEM_BITRATE
            outs.append(st)
        # Interleave in blocks so the file is not one stream after the other
        block = 1152 * 40
        frames = streams[0].shape[1]
        for pos in range(0, frames, block):
            for st, data in zip(outs, streams):
                chunk = np.ascontiguousarray(np.clip(data[:, pos:pos + block], -1, 1))
                f = av.AudioFrame.from_ndarray(chunk, format="fltp", layout="stereo")
                f.sample_rate = rate
                for p in st.encode(f):
                    out.mux(p)
        for st in outs:
            for p in st.encode(None):
                out.mux(p)


def boxes(data, start, end):
    pos = start
    while pos + 8 <= end:
        size, typ = struct.unpack(">I4s", data[pos:pos + 8])
        hdr = 8
        if size == 1:
            size = struct.unpack(">Q", data[pos + 8:pos + 16])[0]
            hdr = 16
        elif size == 0:
            size = end - pos
        yield pos, size, hdr, typ.decode("latin1")
        pos += size


def box(typ, payload):
    return struct.pack(">I4s", 8 + len(payload), typ.encode()) + payload


def add_manifest(path):
    """moov/udta/stem; moov is the last box (no chunk offsets to rewrite)."""
    data = bytearray(open(path, "rb").read())
    top = list(boxes(data, 0, len(data)))
    mpos, msize, mhdr, _ = [b for b in top if b[3] == "moov"][0]
    assert mpos + msize == len(data) and mhdr == 8, "moov must be the last box"
    stem = box("stem", json.dumps(MANIFEST).encode())
    udta = [b for b in boxes(data, mpos + 8, mpos + msize) if b[3] == "udta"]
    if udta:
        upos, usize, _, _ = udta[0]
        new_udta = box("udta", bytes(data[upos + 8:upos + usize]) + stem)
        moov_payload = bytes(data[mpos + 8:upos]) + new_udta + bytes(data[upos + usize:mpos + msize])
    else:
        moov_payload = bytes(data[mpos + 8:mpos + msize]) + box("udta", stem)
    data = data[:mpos] + box("moov", moov_payload)
    open(path, "wb").write(data)


def rel_hash(rel):
    return hashlib.sha1(rel.encode("utf-8")).hexdigest()[:24]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    ap.add_argument("out")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--only", default="")
    ap.add_argument("--model", default="htdemucs")
    ap.add_argument("--overlap", type=float, default=0.25)
    ap.add_argument("--threads", type=int, default=0)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    torch.set_num_threads(args.threads or os.cpu_count())
    model = get_model(args.model)
    model.eval()
    groups = [[model.sources.index(n) for n in g] for g in STEM_GROUPS]

    files = []
    for d, _, names in os.walk(os.path.join(args.root, "Contents")):
        for n in sorted(names):
            if n.lower().endswith(AUDIO_EXT) and not n.startswith("._"):
                files.append(os.path.join(d, n))
    files.sort()
    if args.only:
        files = [f for f in files if args.only in f]
    if args.limit:
        files = files[:args.limit]

    index = open(os.path.join(args.out, "index.tsv"), "a")
    for i, path in enumerate(files, 1):
        rel = os.path.relpath(path, args.root).replace(os.sep, "/")
        h = rel_hash(rel)
        dst = os.path.join(args.out, h + ".mxstem")
        if os.path.exists(dst):
            print(f"[{i}/{len(files)}] skip {rel}", flush=True)
            continue
        t0 = time.time()
        try:
            mix, rate = decode(path)
            delay = int(round(MAD_DELAY_SECONDS * rate))
            wav = torch.from_numpy(mix)
            ref = wav.mean(0)
            mean, std = ref.mean(), ref.std() + 1e-8
            src = wav
            if rate != model.samplerate:
                import torchaudio.functional as F
                src = F.resample(wav, rate, model.samplerate)
            with torch.no_grad():
                est = apply_model(model, ((src - mean) / std)[None], shifts=0,
                                  split=True, overlap=args.overlap, progress=False)[0]
            est = est * std + mean
            if rate != model.samplerate:
                import torchaudio.functional as F
                est = F.resample(est, model.samplerate, rate)
            est = est[..., :mix.shape[1]].numpy()
            # Decoded timeline = LAME priming + what we write: write the
            # audio so that priming + it = MAD delay + track
            shift = delay - LAME_PRIMING_SAMPLES

            def place(a):
                if shift >= 0:
                    return np.concatenate([np.zeros((2, shift), np.float32), a], 1)
                return np.ascontiguousarray(a[:, -shift:])
            # est is de-normalized per source, so summing sources is exact
            streams = [place(mix)] + [place(sum(est[k] for k in g)) for g in groups]
            tmp = dst + ".tmp.mp4"
            encode_mp4(streams, rate, tmp)
            add_manifest(tmp)
            os.replace(tmp, dst)
            dt = time.time() - t0
            print(f"[{i}/{len(files)}] {dt:.0f}s {mix.shape[1] / rate:.0f}s audio {rel}", flush=True)
            index.write(f"{h}\t{rel}\t{dt:.0f}\tok\n")
        except Exception as e:  # keep going with the other tracks
            print(f"[{i}/{len(files)}] FAILED {rel}: {e}", flush=True)
            index.write(f"{h}\t{rel}\t0\tfailed {e}\n")
        index.flush()


if __name__ == "__main__":
    main()
