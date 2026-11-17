"""Export the htdemucs core (no STFT) to ONNX + one reference segment.
Usage: python -I export_onnx.py <outdir>"""
import sys, os, numpy as np, torch
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from core import Core
from demucs.pretrained import get_model
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
torch.set_num_threads(4)
torch.backends.mha.set_fastpath_enabled(False)  # aten::_native_multi_head_attention has no ONNX export
bag = get_model("htdemucs"); m = bag.models[0].eval()
L = int(m.segment * m.samplerate)
wav = torch.from_numpy(np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "clip.npy")))
ref = wav.mean(0); wav = (wav - ref.mean()) / (ref.std() + 1e-8)
mix = wav[None, :, 10 * 44100: 10 * 44100 + L].contiguous()
with torch.no_grad():
    mag = m._magnitude(m._spec(mix)).contiguous()
    core = Core(m).eval()
    x, xt = core(mix, mag)
print("L", L, "mix", tuple(mix.shape), "mag", tuple(mag.shape), "x", tuple(x.shape), "xt", tuple(xt.shape))
for n, t in (("mix", mix), ("mag", mag), ("x", x), ("xt", xt)):
    t.numpy().astype(np.float32).tofile(f"{out}/ref_{n}.f32")
with torch.no_grad():
    torch.onnx.export(core, (mix, mag), f"{out}/htdemucs_core.onnx", opset_version=17, dynamo=False,
                      input_names=["mix", "mag"], output_names=["x", "xt"])
import onnx
mo = onnx.load(f"{out}/htdemucs_core.onnx")
from collections import Counter
print("ops", Counter(n.op_type for n in mo.graph.node).most_common())
