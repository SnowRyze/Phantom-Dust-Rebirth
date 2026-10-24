# Makes src\titlesub.h: "Rebirth" for the rows pdnet adds under the title screen's "Phantom Dust" logo (gamepatches.cpp,
# FDLogo.alr). Masks for the logo texture (560 wide, kSubH rows): the letters (chipped), a dark outline, pits, and
# top-left / bottom-right edges for a bevel; the colour comes from the logo itself at run time (the metal inside its
# "D"). Plus the glow texture's mask (576 wide, 8 px margin, kGlowH rows from row 96), drawn over its bottom rows.
#   py tools\subtitle.py src\titlesub.h [FDLogo.alr preview.png]
import random, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

W, H, GW, GH = 560, 32, 576, 48
random.seed(11)
# Grenze Gotisch (SIL OFL 1.1, github.com/google/fonts ofl/grenzegotisch): GRENZE env var or next to this script.
import os
font = ImageFont.truetype(os.environ.get('GRENZE', os.path.join(os.path.dirname(__file__), 'GrenzeGotisch.ttf')), 36)
font.set_variation_by_name('Bold')
text, track = "Rebirth", 6
widths = [font.getlength(c) for c in text]
m = Image.new('L', (int(sum(widths) + track * len(text)) + 8, 40), 0)
draw, x = ImageDraw.Draw(m), 4
for c, w in zip(text, widths):
    draw.text((x, 2), c, font=font, fill=255)
    x += w + track
m = m.crop(m.getbbox())

clean = Image.new('L', (W, H), 0)
clean.paste(m, ((W - m.size[0]) // 2, (H - m.size[1]) // 2 - 2))

def noise(size, scale, seed):
    random.seed(seed)
    small = Image.new('L', (size[0] // scale + 2, size[1] // scale + 2))
    small.putdata([random.randrange(256) for _ in range(small.size[0] * small.size[1])])
    return small.resize((small.size[0] * scale, small.size[1] * scale), Image.BICUBIC).crop((0, 0) + size)

# Chipped edges: the edge pixels lose some coverage where the noise is low.
edge = ImageChops.subtract(clean, clean.filter(ImageFilter.MinFilter(3)))
chips = noise((W, H), 2, 1).point(lambda v: 255 if v < 90 else 0)
letters = ImageChops.subtract(clean, ImageChops.multiply(edge, chips))
outline = clean.filter(ImageFilter.MaxFilter(3))
pits = ImageChops.multiply(letters, noise((W, H), 1, 2).point(lambda v: min(255, max(0, (v - 150) * 3))))
pits = ImageChops.add(pits, ImageChops.multiply(letters, noise((W, H), 3, 3).point(lambda v: 110 if v < 70 else 0)))
high = ImageChops.subtract(letters, ImageChops.offset(letters, 1, 1)).point(lambda v: v * 2 // 3)
low = ImageChops.subtract(letters, ImageChops.offset(letters, -1, -1)).point(lambda v: v * 2 // 3)

# Glow: like the logo's own (logo_03), a widened, softened silhouette.
glow = Image.new('L', (GW, GH), 0)
glow.paste(clean, (8, 8))
glow = glow.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(2)).point(lambda v: v * 235 // 255)

def array(name, image):
    data = image.tobytes()
    rows = ['    ' + ','.join(str(v) for v in data[i:i + 40]) + ',' for i in range(0, len(data), 40)]
    return 'const uint8_t %s[%d] = {\n%s\n};\n' % (name, len(data), '\n'.join(rows))

with open(sys.argv[1], 'w') as out:
    out.write('// "Rebirth" under the title screen logo (gamepatches.cpp), made by tools\\subtitle.py (Grenze Gotisch Bold).\n')
    out.write('// kSubW x kSubH masks for the logo: letters, dark outline, pits, bevel highlight and shadow;\n')
    out.write('// kGlowW x kGlowH for the glow texture, from its row 96.\n#pragma once\n#include <cstdint>\n')
    out.write('constexpr int kSubW = %d, kSubH = %d, kGlowW = %d, kGlowH = %d;\n' % (W, H, GW, GH))
    for name, image in (('kSubLetters', letters), ('kSubOutline', outline), ('kSubPits', pits), ('kSubHigh', high),
                        ('kSubLow', low), ('kSubGlow', glow)):
        out.write(array(name, image))

if len(sys.argv) > 3: # preview: the same steps as gamepatches.cpp, over the game's logo and glow
    d = open(sys.argv[2], 'rb').read()
    logo = Image.frombytes('RGBA', (560, 96), d[0x1000 + 0x4000:0x1000 + 0x38800], 'raw', 'BGRA')
    g = Image.frombytes('L', (576, 112), d[0x1000 + 0x38800:0x1000 + 0x48400])
    rows = Image.new('RGBA', (W, H), (0, 0, 0, 0)); px = rows.load(); lp = logo.load()
    L, O, P, Hh, Lo = (i.load() for i in (letters, outline, pits, high, low))
    def over(p, c, a):
        if not a: return p
        under = p[3] * (255 - a) // 255; o = a + under
        return tuple((c[k] * a + p[k] * under) // o for k in range(3)) + (o,)
    for y in range(H):
        for x in range(W):
            mr, mg, mb, _ = lp[330 + x % 45, 15 + y % 73]
            b = lambda v: min(255, v * 15 // 10)
            p = (0, 0, 0, 0)
            p = over(p, (20, 10, 5), O[x, y]); p = over(p, (b(mr), b(mg), b(mb)), L[x, y])
            p = over(p, (0, 0, 0), P[x, y]); p = over(p, (255, 230, 190), Hh[x, y]); p = over(p, (0, 0, 0), Lo[x, y])
            px[x, y] = p
    tall = Image.new('RGBA', (560, 96 + H)); tall.paste(logo, (0, 0)); tall.paste(rows, (0, 96))
    gt = Image.new('L', (576, 112 + GH - 16)); gt.paste(g, (0, 0))
    gt.paste(ImageChops.lighter(gt.crop((0, 96, 576, 96 + GH)), glow), (0, 96))
    bg = Image.new('RGBA', (600, 170), (25, 25, 30, 255))
    glowc = Image.new('RGBA', gt.size, (255, 150, 40, 255)); glowc.putalpha(gt.point(lambda v: v * 3 // 5))
    bg.alpha_composite(glowc, (12, 5)); bg.alpha_composite(tall, (20, 13))
    bg.resize((1200, 340), Image.LANCZOS).save(sys.argv[3])
print(m.size)
