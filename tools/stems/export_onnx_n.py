"""Export CoreN (normalized in/out) to ONNX, static graph (ORT basic folding) + reference data.
Usage: python -I export_onnx_n.py <dir with ref_mix/ref_mag.f32>"""
import sys, os, numpy as np, torch, onnxruntime as ort
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from core import CoreN
from demucs.pretrained import get_model
torch.set_num_threads(4); torch.backends.mha.set_fastpath_enabled(False)
d = sys.argv[1]
m = get_model("htdemucs").models[0].eval(); core = CoreN(m).eval()
mix = torch.from_numpy(np.fromfile(f"{d}/ref_mix.f32", np.float32).reshape(1, 2, 343980))
mag = torch.from_numpy(np.fromfile(f"{d}/ref_mag.f32", np.float32).reshape(1, 4, 2048, 336))
# same as Core: torch.std is unbiased
xn = (mag - mag.mean()) / (1e-5 + mag.std()); xtn = (mix - mix.mean()) / (1e-5 + mix.std())
with torch.no_grad(): x, xt = core(xtn, xn)
for n, t in (("magn", xn), ("mixn", xtn), ("xn", x), ("xtn", xt)):
    t.numpy().astype(np.float32).tofile(f"{d}/ref_{n}.f32")
# check: de-normalized output == Core reference
xr = np.fromfile(f"{d}/ref_x.f32", np.float32); xd = (x * mag.std().add(1e-5) + mag.mean()).numpy().ravel()
print("denorm vs Core SNR %.1f dB" % (10 * np.log10((xr**2).sum() / ((xr - xd)**2).sum())))
raw = f"{d}/htdemucs_coren.onnx"
with torch.no_grad():
    torch.onnx.export(core, (xtn, xn), raw, opset_version=17, dynamo=False,
                      input_names=["mix", "mag"], output_names=["x", "xt"])
so = ort.SessionOptions(); so.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_BASIC
so.optimized_model_filepath = f"{d}/htdemucs_coren_static.onnx"
ort.InferenceSession(raw, so, providers=["CPUExecutionProvider"])
print("ok")
