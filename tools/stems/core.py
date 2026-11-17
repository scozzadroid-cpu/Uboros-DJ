import torch
from einops import rearrange

class Core(torch.nn.Module):
    """htdemucs forward between STFT magnitude and mask (no complex ops)."""
    def __init__(s, m): super().__init__(); s.m = m
    def forward(s, mix, mag):
        self = s.m; x = mag; B, C, Fq, T = x.shape
        mean = x.mean(dim=(1, 2, 3), keepdim=True); std = x.std(dim=(1, 2, 3), keepdim=True)
        x = (x - mean) / (1e-5 + std)
        xt = mix; meant = xt.mean(dim=(1, 2), keepdim=True); stdt = xt.std(dim=(1, 2), keepdim=True)
        xt = (xt - meant) / (1e-5 + stdt)
        saved, saved_t, lengths, lengths_t = [], [], [], []
        for idx, encode in enumerate(self.encoder):
            lengths.append(x.shape[-1]); inject = None
            if idx < len(self.tencoder):
                lengths_t.append(xt.shape[-1]); tenc = self.tencoder[idx]; xt = tenc(xt)
                if not tenc.empty: saved_t.append(xt)
                else: inject = xt
            x = encode(x, inject)
            if idx == 0 and self.freq_emb is not None:
                frs = torch.arange(x.shape[-2], device=x.device)
                emb = self.freq_emb(frs).t()[None, :, :, None].expand_as(x)
                x = x + self.freq_emb_scale * emb
            saved.append(x)
        if self.bottom_channels:
            b, c, f, t = x.shape
            x = rearrange(x, "b c f t-> b c (f t)"); x = self.channel_upsampler(x)
            x = rearrange(x, "b c (f t)-> b c f t", f=f); xt = self.channel_upsampler_t(xt)
        x, xt = self.crosstransformer(x, xt)
        if self.bottom_channels:
            x = rearrange(x, "b c f t-> b c (f t)"); x = self.channel_downsampler(x)
            x = rearrange(x, "b c (f t)-> b c f t", f=f); xt = self.channel_downsampler_t(xt)
        for idx, decode in enumerate(self.decoder):
            skip = saved.pop(-1); x, pre = decode(x, skip, lengths.pop(-1))
            offset = self.depth - len(self.tdecoder)
            if idx >= offset:
                tdec = self.tdecoder[idx - offset]; length_t = lengths_t.pop(-1)
                if tdec.empty: pre = pre[:, :, 0]; xt, _ = tdec(pre, None, length_t)
                else: xt, _ = tdec(xt, saved_t.pop(-1), length_t)
        Sn = len(self.sources)
        x = x.view(B, Sn, -1, Fq, T) * std[:, None] + mean[:, None]
        xt = xt.view(B, Sn, -1, mix.shape[-1]) * stdt[:, None] + meant[:, None]
        return x, xt



class CoreN(torch.nn.Module):
    """Like Core, but inputs already normalized and outputs not de-normalized:
    the mean/std over the whole segment overflow in fp16, the CPU does them."""
    def __init__(s, m): super().__init__(); s.m = m
    def forward(s, mix, mag):
        self = s.m; x = mag; B, C, Fq, T = x.shape
        xt = mix
        saved, saved_t, lengths, lengths_t = [], [], [], []
        for idx, encode in enumerate(self.encoder):
            lengths.append(x.shape[-1]); inject = None
            if idx < len(self.tencoder):
                lengths_t.append(xt.shape[-1]); tenc = self.tencoder[idx]; xt = tenc(xt)
                if not tenc.empty: saved_t.append(xt)
                else: inject = xt
            x = encode(x, inject)
            if idx == 0 and self.freq_emb is not None:
                frs = torch.arange(x.shape[-2], device=x.device)
                emb = self.freq_emb(frs).t()[None, :, :, None].expand_as(x)
                x = x + self.freq_emb_scale * emb
            saved.append(x)
        if self.bottom_channels:
            b, c, f, t = x.shape
            x = rearrange(x, "b c f t-> b c (f t)"); x = self.channel_upsampler(x)
            x = rearrange(x, "b c (f t)-> b c f t", f=f); xt = self.channel_upsampler_t(xt)
        x, xt = self.crosstransformer(x, xt)
        if self.bottom_channels:
            x = rearrange(x, "b c f t-> b c (f t)"); x = self.channel_downsampler(x)
            x = rearrange(x, "b c (f t)-> b c f t", f=f); xt = self.channel_downsampler_t(xt)
        for idx, decode in enumerate(self.decoder):
            skip = saved.pop(-1); x, pre = decode(x, skip, lengths.pop(-1))
            offset = self.depth - len(self.tdecoder)
            if idx >= offset:
                tdec = self.tdecoder[idx - offset]; length_t = lengths_t.pop(-1)
                if tdec.empty: pre = pre[:, :, 0]; xt, _ = tdec(pre, None, length_t)
                else: xt, _ = tdec(xt, saved_t.pop(-1), length_t)
        Sn = len(self.sources)
        x = x.view(B, Sn, -1, Fq, T)
        xt = xt.view(B, Sn, -1, mix.shape[-1])
        return x, xt



class CoreN3(torch.nn.Module):
    """CoreN with 3 sources: drums, bass+other ("bass+melody"), vocals.
    Exact: the cac mask and the iSTFT are linear, so summing before them is
    the same as summing the separated audio."""
    def __init__(s, m): super().__init__(); s.c = CoreN(m)
    def forward(s, mix, mag):
        x, xt = s.c(mix, mag)  # sources: drums, bass, other, vocals
        x = torch.cat([x[:, 0:1], x[:, 1:2] + x[:, 2:3], x[:, 3:4]], dim=1)
        xt = torch.cat([xt[:, 0:1], xt[:, 1:2] + xt[:, 2:3], xt[:, 3:4]], dim=1)
        return x, xt
# NOTE (CoreN3): outputs are normalized; de-normalize with x*std + k*mean, k = [1, 2, 1]
# (bass+other is the sum of two sources, each of which carries the mean once).
