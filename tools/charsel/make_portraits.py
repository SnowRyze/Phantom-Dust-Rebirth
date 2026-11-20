"""Character select portraits for pdnet (gamepatches.cpp, MakeMnetCopy).

    py -I tools\\charsel\\make_portraits.py            crop the guides in source\\ into portraits\\, then pack them
    py -I tools\\charsel\\make_portraits.py --pack     only pack portraits\\*.png (after editing a portrait by hand)

Needs numpy and Pillow (py -m pip install numpy pillow).

The character select draws a 134 x 96 portrait from MNet.alr (sprites player_00..player_10, MNET sprites 38-48). pdnet
widens that file's texture MNet0800 (256 x 512 DXT5) to 1024 x 512 when the game opens it (the left quarter stays
the game's own) and replaces MNet0810 / MNet0820, which hold only portraits. Out: kit\\pdnet\\charsel-portraits.bin:

    'PDCP' u32 version (1) u32 images u32 sprites
    images x { u32 texture (index in MNet.alr), u16 w, u16 h, u32 mode, u32 size, DXT5 blocks (rows of 4 x 4 blocks) }
        mode 1: added to the right of the game's texture (MNet0800: 256 -> 1024 wide); mode 0: replaces it (same size)
    sprites x { char name[32]; u32 texture; u16 x, y, w, h }     pixel box in the finished texture

Sprites named player_NN.tga take the new art in place of the game's; any other name is a new sprite added after the
game's 84 (monster_<name>.tga: the monster portraits the X toggle shows), in this file's order (MNET sprites 84-94).
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
ART_W, ART_H = 768, 512        # the part of MNet0800 pdnet adds
CELL_W, CELL_H = 136, 98       # portrait + 2 px gap (no filtering bleed between portraits)
MNET0800_W = 256               # the game's width of MNet0800
COLUMNS = ART_W // CELL_W

# The game's order of player_NN (the character select's order); the file name in portraits\ for each.
PLAYERS = ['player', 'edgar', 'freia', 'meister', 'chunky', 'cuff_button', 'ph', 'know', 'tsubutaki', 'jd', 'sammah']
MONSTERS = ['vestio', 'ommato', 'anthro', 'guard', 'andro', 'ceno', 'partheno', 'catoptro', 'gyne', 'claustro', 'scoto']

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


def pack():
    """Players 00-03 and the monsters go in the 768 x 512 added to MNet0800; players 04-10 keep their own boxes
    in MNet0810 / MNet0820, whose whole art (only those portraits) is replaced."""
    images = {9: np.zeros((ART_H, ART_W, 4), np.uint8), 10: np.zeros((512, 256, 4), np.uint8), 11: np.zeros((256, 256, 4), np.uint8)}
    sprites = []
    in_widened = [('player_%02d.tga' % i, PLAYERS[i]) for i in range(4)] + [('monster_%s.tga' % n, n) for n in MONSTERS]
    for k, (sprite, name) in enumerate(in_widened):
        x, y = (k % COLUMNS) * CELL_W, (k // COLUMNS) * CELL_H
        sprites.append((sprite, name, 9, x, y, MNET0800_W + x, y))
    for i in range(4, 11):  # the game's own boxes: 98 rows apart from the top
        tex, row = (10, i - 4) if i < 9 else (11, i - 9)
        sprites.append(('player_%02d.tga' % i, PLAYERS[i], tex, 0, row * 98, 0, row * 98))
    for sprite, name, tex, x, y, _, _ in sprites:
        img = Image.open(os.path.join(HERE, 'portraits', name + '.png')).convert('RGBA')
        if img.size != (W, H):
            img = img.resize((W, H), Image.LANCZOS)
        images[tex][y:y + H, x:x + W] = np.asarray(img)
    data = bytearray(b'PDCP' + struct.pack('<3I', 1, len(images), len(sprites)))
    for tex, art in images.items():
        fmt, blocks = dxt.encode(art, alpha=True)
        assert fmt == dxt.BC3 and len(blocks) == art.shape[0] * art.shape[1]
        mode = 1 if tex == 9 else 0  # 1: added to the right of the game's texture; 0: replaces it
        data += struct.pack('<I2HII', tex, art.shape[1], art.shape[0], mode, len(blocks)) + blocks
        Image.fromarray(art).save(os.path.join(HERE, 'sheet-mnet%d.png' % (800 + (tex - 9) * 10)))
    for sprite, name, tex, _, _, x, y in sprites:
        data += struct.pack('<32sI4H', sprite.encode(), tex, x, y, W, H)
    path = os.path.join(HERE, '..', '..', 'kit', 'pdnet', 'charsel-portraits.bin')
    with open(path, 'wb') as f:
        f.write(data)
    print('%d portraits -> %s (%d bytes)' % (len(sprites), os.path.normpath(path), len(data)))


if __name__ == '__main__':
    if '--pack' not in sys.argv:
        crop_guides()
    pack()
