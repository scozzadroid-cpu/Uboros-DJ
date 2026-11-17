import sys, numpy as np
S = sys.argv[1]; ref = np.load(f"{S}/out_torch_nt4.npy")  # htdemucs order: drums bass other vocals
mix = np.load(f"{S}/clip.npy")
def sdr(r, e): return 10*np.log10((r**2).sum() / (((r-e)**2).sum() + 1e-12) + 1e-12)
def three(a): return {"vocals": a[3], "drums": a[0], "bass+other": a[1]+a[2]}
R = three(ref)
for f in sys.argv[2:]:
    e = np.load(f"{S}/{f}"); E = three(e)
    # 3-stem by subtraction: bass+other := mix - vocals - drums
    sub = mix - E["vocals"] - E["drums"]
    print(f, " ".join(f"{k}={sdr(R[k], E[k]):.1f}" for k in R), f"| b+o(sottraz)={sdr(R['bass+other'], sub):.1f}",
          f"| resid(mix-sum)={10*np.log10((mix**2).sum()/((mix-e.sum(0))**2).sum()):.1f}dB")
