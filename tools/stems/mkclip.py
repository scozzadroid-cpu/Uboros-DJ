import av, numpy as np, sys, wave
p, out = sys.argv[1], sys.argv[2]
with av.open(p) as c:
    s=c.streams.audio[0]; r=s.codec_context.sample_rate
    res=av.AudioResampler(format="fltp",layout="stereo",rate=44100)
    ch=[]
    for f in c.decode(s):
        for g in res.resample(f): ch.append(g.to_ndarray())
x=np.concatenate(ch,1)[:, 60*44100:105*44100].astype(np.float32)
np.save(out, x); print(r, x.shape)
