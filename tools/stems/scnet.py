import os, sys, time, resource, numpy as np, torch
S = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
eng = sys.argv[1]; STEP = float(sys.argv[2]); NT = int(os.environ.get("NT", "4"))
torch.set_num_threads(NT)
SEG, NFFT, HOP, NPAD = 485100, 4096, 1024, 475 * 1024
onx = f"{S}/dl/scnet.int8.onnx"
if eng == "ort":
    import onnxruntime as ort
    so = ort.SessionOptions(); so.intra_op_num_threads = NT; so.inter_op_num_threads = 1
    sess = ort.InferenceSession(onx, so, providers=["CPUExecutionProvider"])
    run = lambda x: sess.run(None, {"mix_spec": x})[0]
else:
    import openvino as ov
    cm = ov.Core().compile_model(onx, "CPU", {"INFERENCE_NUM_THREADS": NT, "PERFORMANCE_HINT": "LATENCY"})
    req = cm.create_infer_request()
    run = lambda x: req.infer({0: x})[cm.output(0)].copy()
win = torch.ones(NFFT)
def sep(seg):  # seg (2, n<=SEG) -> (4, 2, n)
    n = seg.shape[1]; x = torch.nn.functional.pad(seg, (0, NPAD - n))
    z = torch.stft(x, NFFT, HOP, window=win, normalized=True, center=True, return_complex=True)  # 2,2049,476
    inp = torch.view_as_real(z).permute(0, 3, 1, 2).reshape(1, 4, 2049, -1).numpy().astype(np.float32)
    y = torch.from_numpy(run(inp)).reshape(4, 2, 2, 2049, -1).permute(0, 1, 3, 4, 2).contiguous()
    y = torch.view_as_complex(y).reshape(8, 2049, -1)
    w = torch.istft(y, NFFT, HOP, window=win, normalized=True, center=True, length=NPAD)
    return w.reshape(4, 2, -1)[..., :n]
wav = torch.from_numpy(np.load(f"{S}/clip.npy")); ref = wav.mean(0); mean, std = ref.mean(), ref.std() + 1e-8
r0 = resource.getrusage(resource.RUSAGE_SELF); t0 = time.time()
x = (wav - mean) / std; N = x.shape[1]; step = int(SEG * STEP); fade = SEG // 10
out = torch.zeros(4, 2, N); acc = torch.zeros(N)
starts = list(range(0, max(N - SEG, 0) + 1, step))
if starts[-1] + SEG < N: starts.append(N - SEG)
with torch.no_grad():
    for i, s in enumerate(starts):
        e = min(s + SEG, N); w = torch.ones(e - s)
        if i > 0: w[:fade] = torch.linspace(0, 1, fade)
        if i < len(starts) - 1: w[-fade:] = torch.linspace(1, 0, fade)
        out[..., s:e] += sep(x[:, s:e]) * w; acc[s:e] += w
est = (out / acc * std).numpy()
dt = time.time() - t0; r1 = resource.getrusage(resource.RUSAGE_SELF)
cpu = (r1.ru_utime - r0.ru_utime) + (r1.ru_stime - r0.ru_stime)
np.save(f"{S}/out_scnet_{eng}_{STEP}.npy", est)
print(f"scnet {eng} step={STEP} NT={NT}: {len(starts)} seg, wall {dt:.1f}s = {dt/(N/44100):.3f} s/s audio, cpu {cpu:.0f}s, maxrss {r1.ru_maxrss/1024:.0f}MB", flush=True)
