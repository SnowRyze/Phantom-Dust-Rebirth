"""Character select portraits for pdnet (gamepatches.cpp, MakeMnetCopy).

    py -I tools\\charsel\\make_portraits.py            crop the guides in source\\ into portraits\\, then pack them
    py -I tools\\charsel\\make_portraits.py --pack     only pack portraits\\*.png (after editing a portrait by hand)

Needs numpy and Pillow (py -m pip install numpy pillow).

The character select draws a 134 x 96 portrait from MNet.alr (sprites player_00..player_10, MNET sprites 38-48). pdnet
widens that file's texture MNet0800 (256 x 512 DXT5) to 2048 x 512 when the game opens it (the left quarter stays
the game's own) to hold the 11 monster portraits; the players keep the game's own portraits. Out: kit\\pdnet\\charsel-portraits.bin:

    'PDCP' u32 version (1) u32 images u32 sprites
    images x { u32 texture (index in MNet.alr), u16 w, u16 h, u32 mode, u32 size, DXT5 blocks (rows of 4 x 4 blocks) }
        mode 1: added to the right of the game's texture (MNet0800: 256 -> 1024 wide); mode 0: replaces it (same size)
    sprites x { char name[32]; u32 texture; u16 x, y, w, h }     pixel box in the finished texture

A sprite whose name the game's file already has would take the new art in its place; any other name is a new sprite
added after the game's 84 (monster_<name>.tga: the monster portraits the X toggle shows), in this file's order
(MNET sprites 84-94), then blank.tga (95, transparent), then portrait_<name>.tga (96 on, EXTRA).
"""
import os
import struct
import sys
from collections import deque

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import dxt  # noqa: E402  (from the phantom_dust_stage Blender add-on)

W, H = 134, 96                 # the game's portrait size
ART_W, ART_H = 1792, 512       # the part of MNet0800 pdnet adds (256 -> 2048 wide: 13 x 5 portraits)
CELL_W, CELL_H = 136, 98       # portrait + 2 px gap (no filtering bleed between portraits)
MNET0800_W = 256               # the game's width of MNet0800
COLUMNS = ART_W // CELL_W

# The game's order of player_NN (the character select's order); the file name in portraits\ for each.
PLAYERS = ['player', 'edgar', 'freia', 'meister', 'chunky', 'cuff_button', 'ph', 'know', 'tsubutaki', 'jd', 'sammah']
MONSTERS = ['vestio', 'ommato', 'anthro', 'guard', 'andro', 'ceno', 'partheno', 'catoptro', 'gyne', 'claustro', 'scoto']
# The Advanced list's portraits for characters the game has none of (the user's renders, 2026-11-20; source\npcs, cut out
# by hand-tuned flood fill, see the commit): sprites portrait_<name>.tga after blank.tga.
EXTRA = ['germano', 'belono', 'n03', 'spokesman', 'ubiquitous', 'kajikawa', 'tetsuya', 'arthur', 'reindeer', 'mac',
         'baroness', 'mikan', 'ai', 'maniac', 'kei']
DUPLICATES = ['ubiquitous', 'mac', 'baroness', 'reindeer']  # sprites 111-114 (the last that fit: 115)

# Cards in the two guide images (source\characters.png, source\monsters.png, 1000 x 559): left and top of each card's
# picture, in reading order. The crop leaves out the movement icon (bottom left) and the name bar (bottom).
CARDS = [(192, 97), (403, 97), (614, 97),
         (90, 249), (301, 249), (512, 249), (723, 249),
         (90, 401), (301, 401), (512, 401), (723, 401)]
GUIDE_ORDER = {
    'characters.png': (['freia', 'player', 'edgar', 'meister', 'chunky', 'cuff_button', 'ph', 'know', 'tsubutaki', 'jd', 'sammah'], 110),
    'monsters.png': (MONSTERS, 150),
}
CROP_X, CROP_W, CROP_H = 34, 152, 109
WIDE_ICON = {'cuff_button': 50, 'jd': 50}  # the dash icon is wider: start further right


def cutout(rgb, brightest):
    """The guide's white card background (flood-filled from the edges) becomes transparent."""
    a = np.asarray(rgb).astype(np.int32)
    h, w, _ = a.shape
    v = a.max(2)
    bg = (v > brightest) & (v - a.min(2) < 28)
    seen = np.zeros((h, w), bool)
    q = deque([(y, x) for y in range(h) for x in (0, w - 1) if bg[y, x]] +
              [(y, x) for x in range(w) for y in (0, h - 1) if bg[y, x]])
    for y, x in q:
        seen[y, x] = True
    while q:
        y, x = q.popleft()
        for yy, xx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
            if 0 <= yy < h and 0 <= xx < w and not seen[yy, xx] and bg[yy, xx]:
                seen[yy, xx] = True
                q.append((yy, xx))
    return Image.fromarray(np.dstack([a.astype(np.uint8), np.where(seen, 0, 255).astype(np.uint8)]), 'RGBA')


def crop_guides():
    out = os.path.join(HERE, 'portraits')
    os.makedirs(out, exist_ok=True)
    for guide, (names, brightest) in GUIDE_ORDER.items():
        im = Image.open(os.path.join(HERE, 'source', guide)).convert('RGB')
        for name, (x, y) in zip(names, CARDS):
            left = x + WIDE_ICON.get(name, CROP_X)
            box = (left, y, left + CROP_W, y + CROP_H)
            cutout(im.crop(box), brightest).resize((W, H), Image.LANCZOS).save(os.path.join(out, name + '.png'))


def teal_tint(monster_imgs):
    """The monsters in the players' blue-green: each monster's brightness is matched to the players' portraits'
    (same spread of dark and light), then coloured by the players' colour for that brightness. Both taken from the
    player portraits in portraits\\ (the guide's art, the game's look)."""
    px = []
    for name in PLAYERS:
        a = np.asarray(Image.open(os.path.join(HERE, 'portraits', name + '.png')).convert('RGBA')).reshape(-1, 4)
        px.append(a[a[:, 3] > 200][:, :3])
    p = np.concatenate(px).astype(np.float64)
    lum = p @ [0.299, 0.587, 0.114]
    ramp = np.zeros((256, 3))
    have = np.zeros(256, bool)
    for i in range(256):
        m = np.abs(lum - i) < 4
        if m.sum() > 20:
            ramp[i], have[i] = p[m].mean(0), True
    idx = np.arange(256)
    for c in range(3):
        ramp[:, c] = np.interp(idx, idx[have], ramp[have, c])
    targets = np.percentile(lum, np.linspace(0, 100, 101))
    out = []
    for img in monster_imgs:
        a = np.asarray(img).astype(np.float64)
        solid = a[..., 3] > 0
        l = a[..., :3] @ [0.299, 0.587, 0.114]
        src = np.percentile(l[solid], np.linspace(0, 100, 101))
        mapped = np.interp(l, src, targets)
        rgb = ramp[np.clip(mapped, 0, 255).astype(int)]
        out.append(Image.fromarray(np.dstack([rgb, a[..., 3]]).clip(0, 255).astype(np.uint8), 'RGBA'))
    return out


def pack():
    """The 11 monster portraits go in the 768 x 512 added to the right of MNet0800. The players keep the game's own
    portraits (the user's choice, 2026-11-20); portraits\\<player>.png are kept for later but not packed."""
    art = np.zeros((ART_H, ART_W, 4), np.uint8)
    sprites = []
    imgs = []
    for name in MONSTERS + EXTRA:
        img = Image.open(os.path.join(HERE, 'portraits', name + '.png')).convert('RGBA')
        imgs.append(img if img.size == (W, H) else img.resize((W, H), Image.LANCZOS))
    imgs = teal_tint(imgs)
    cells = [('monster_%s.tga' % n, img) for n, img in zip(MONSTERS, imgs)] + [('blank.tga', None)] + \
            [('portrait_%s.tga' % n, img) for n, img in zip(EXTRA, imgs[len(MONSTERS):])]
    for k, (sprite, img) in enumerate(cells):
        x, y = (k % COLUMNS) * CELL_W, (k // COLUMNS) * CELL_H
        if img is not None:  # blank.tga: an empty (transparent) cell, for places the Advanced list leaves empty
            art[y:y + H, x:x + W] = np.asarray(img)
        sprites.append((sprite, 9, MNET0800_W + x, y))
    # The second models (Mac 2 ...) show the same picture but need sprites of their own (the Advanced list draws all
    # 30 entries at once, and each sprite is one object on screen): portrait_<name>2.tga, the same pixels.
    boxes = {s[0]: (s[2], s[3]) for s in sprites}
    for name in DUPLICATES:
        x, y = boxes['portrait_%s.tga' % name]
        sprites.append(('portrait_%s2.tga' % name, 9, x, y))
    fmt, blocks = dxt.encode(art, alpha=True)
    assert fmt == dxt.BC3 and len(blocks) == ART_W * ART_H
    data = bytearray(b'PDCP' + struct.pack('<3I', 1, 1, len(sprites)))
    data += struct.pack('<I2HII', 9, ART_W, ART_H, 1, len(blocks)) + blocks  # mode 1: added right of the game's texture
    for sprite, tex, x, y in sprites:
        data += struct.pack('<32sI4H', sprite.encode(), tex, x, y, W, H)
    path = os.path.join(HERE, '..', '..', 'kit', 'pdnet', 'charsel-portraits.bin')
    with open(path, 'wb') as f:
        f.write(data)
    print('%d portraits -> %s (%d bytes)' % (len(sprites), os.path.normpath(path), len(data)))


if __name__ == '__main__':
    if '--pack' not in sys.argv:
        crop_guides()
    pack()
