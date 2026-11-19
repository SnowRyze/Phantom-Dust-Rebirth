"""Block compression (DXT1 / DXT5, the formats the game's own textures use) in numpy.

New textures used to be written as plain BGRA: four to eight times the size of the game's own. A map whose
textures all come from another map (a game original imported and exported over a different slot) then made a
67 MB model file, which the game does not load. Compressed, the same textures take the game's usual space.

encode(rgba) takes an (h, w, 4) uint8 array, top row first, and returns (format byte, bytes):
0x0C (DXT1) when the texture is opaque, 0x0F (DXT5) when it uses alpha.
"""
import numpy as np

BC1, BC3 = 0x0C, 0x0F


def _blocks(rgba):
    """(h, w, 4) -> (number of blocks, 16, 4) float, padded to whole 4 x 4 blocks by repeating the edge."""
    h, w = rgba.shape[:2]
    H, W = max(4, (h + 3) // 4 * 4), max(4, (w + 3) // 4 * 4)
    if (H, W) != (h, w):
        rgba = np.pad(rgba, ((0, H - h), (0, W - w), (0, 0)), mode='edge')
    b = rgba.reshape(H // 4, 4, W // 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 4)
    return b.astype(np.float32)


def _to565(c):
    c = np.clip(np.round(c), 0, 255).astype(np.uint32)
    return ((c[..., 0] >> 3) << 11) | ((c[..., 1] >> 2) << 5) | (c[..., 2] >> 3)


def _from565(v):
    r = (v >> 11) & 31
    g = (v >> 5) & 63
    b = v & 31
    return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], -1).astype(np.float32)


def _fit(px, hi, lo):
    """End colours (float) -> (c0, c1 as 565, palette index per texel, squared error per block)."""
    c0, c1 = _to565(hi), _to565(lo)
    swap = c0 < c1
    c0, c1 = np.where(swap, c1, c0), np.where(swap, c0, c1)
    e0, e1 = _from565(c0), _from565(c1)
    pal = np.stack([e0, e1, (2 * e0 + e1) / 3, (e0 + 2 * e1) / 3], 1)          # (n, 4, 3)
    dist = ((px[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)             # (n, 16, 4)
    idx = dist.argmin(-1).astype(np.uint32)
    idx[c0 == c1] = 0
    err = np.take_along_axis(dist, idx[..., None].astype(np.int64), -1)[..., 0].sum(1)
    return c0, c1, idx, err


def _colour_blocks(px):
    """px: (n, 16, 3) float -> (n, 8) uint8 DXT1 colour blocks (always the 4-colour mode)."""
    n = len(px)
    mean = px.mean(1, keepdims=True)
    d = px - mean
    cov = np.einsum('nki,nkj->nij', d, d)
    axis = np.ones((n, 3), np.float32)
    for _ in range(6):                                     # power iteration: the main colour direction
        axis = np.einsum('nij,nj->ni', cov, axis)
        axis /= np.maximum(np.linalg.norm(axis, axis=1, keepdims=True), 1e-6)
    t = np.einsum('nki,ni->nk', d, axis)
    lo = mean[:, 0] + axis * t.min(1, keepdims=True)
    hi = mean[:, 0] + axis * t.max(1, keepdims=True)
    c0, c1, idx, err = _fit(px, hi, lo)
    # refine the end colours once by least squares on the chosen palette positions, keep it where it helps
    w = np.array([1.0, 0.0, 2.0 / 3.0, 1.0 / 3.0], np.float32)[idx]            # weight of colour 0 per texel
    a, b = w, 1.0 - w
    aa, bb, ab = (a * a).sum(1), (b * b).sum(1), (a * b).sum(1)
    ap, bp = np.einsum('nk,nki->ni', a, px), np.einsum('nk,nki->ni', b, px)
    det = aa * bb - ab * ab
    ok = np.abs(det) > 1e-6
    det = np.where(ok, det, 1.0)[:, None]
    hi2 = (bb[:, None] * ap - ab[:, None] * bp) / det
    lo2 = (aa[:, None] * bp - ab[:, None] * ap) / det
    r0, r1, ridx, rerr = _fit(px, np.where(ok[:, None], hi2, hi), np.where(ok[:, None], lo2, lo))
    better = rerr < err
    c0, c1 = np.where(better, r0, c0), np.where(better, r1, c1)
    idx = np.where(better[:, None], ridx, idx)
    bits = (idx << (2 * np.arange(16, dtype=np.uint32))).sum(1).astype(np.uint32)
    out = np.zeros((n, 8), np.uint8)
    out[:, 0], out[:, 1] = c0 & 255, c0 >> 8
    out[:, 2], out[:, 3] = c1 & 255, c1 >> 8
    for k in range(4):
        out[:, 4 + k] = (bits >> (8 * k)) & 255
    return out


def _alpha_blocks(a):
    """a: (n, 16) float -> (n, 8) uint8 DXT5 alpha blocks (8-value mode)."""
    n = len(a)
    a0 = np.clip(np.round(a.max(1)), 0, 255).astype(np.uint32)
    a1 = np.clip(np.round(a.min(1)), 0, 255).astype(np.uint32)
    f0, f1 = a0.astype(np.float32)[:, None], a1.astype(np.float32)[:, None]
    k = np.arange(2, 8, dtype=np.float32)[None, :]
    pal = np.concatenate([f0, f1, ((8 - k) * f0 + (k - 1) * f1) / 7], 1)       # (n, 8)
    idx = np.abs(a[:, :, None] - pal[:, None, :]).argmin(-1).astype(np.uint64)
    idx[a0 == a1] = 0
    bits = (idx << (3 * np.arange(16, dtype=np.uint64))).sum(1)
    out = np.zeros((n, 8), np.uint8)
    out[:, 0], out[:, 1] = a0, a1
    for j in range(6):
        out[:, 2 + j] = ((bits >> np.uint64(8 * j)) & np.uint64(255)).astype(np.uint8)
    return out


def needs_alpha(rgba):
    return bool((rgba[..., 3] < 250).any())


def encode(rgba, alpha=None):
    """One mip level: (h, w, 4) uint8 RGBA, top row first -> (format, bytes)."""
    alpha = needs_alpha(rgba) if alpha is None else alpha
    b = _blocks(rgba)
    colour = _colour_blocks(b[:, :, :3])
    if not alpha:
        return BC1, colour.tobytes()
    return BC3, np.concatenate([_alpha_blocks(b[:, :, 3]), colour], 1).tobytes()
