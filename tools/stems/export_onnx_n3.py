"""Export CoreN (normalized in/out) to ONNX, static graph (ORT basic folding) + reference data.
Usage: python -I export_onnx_n.py <dir with ref_mix/ref_mag.f32>"""
import sys, os, numpy as np, torch, onnxruntime as ort
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from core import CoreN3 as CoreN
from demucs.pretrained import get_model
torch.set_num_threads(4); torch.backends.mha.set_fastpath_enabled(False)
d = sys.argv[1]
m = get_model("htdemucs").models[0].eval(); core = CoreN(m).eval()
mix = torch.from_numpy(np.fromfile(f"{d}/ref_mix.f32", np.float32).reshape(1, 2, 343980))
mag = torch.from_numpy(np.fromfile(f"{d}/ref_mag.f32", np.float32).reshape(1, 4, 2048, 336))
# same as Core: torch.std is unbiased
xn = (mag - mag.mean()) / (1e-5 + mag.std()); xtn = (mix - mix.mean()) / (1e-5 + mix.std())
with torch.no_grad(): x, xt = core(xtn, xn)
for n, t in (("xn3", x), ("xtn3", xt)):
    t.numpy().astype(np.float32).tofile(f"{d}/ref_{n}.f32")
# check: de-normalized output == Core reference
raw = f"{d}/htdemucs_coren3.onnx"
with torch.no_grad():
    torch.onnx.export(core, (xtn, xn), raw, opset_version=17, dynamo=False,
                      input_names=["mix", "mag"], output_names=["x", "xt"])
so = ort.SessionOptions(); so.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_BASIC
so.optimized_model_filepath = f"{d}/htdemucs_coren3_static.onnx"
ort.InferenceSession(raw, so, providers=["CPUExecutionProvider"])
print("ok")
