# Makes src\titlesub.h: "Community Edition" for the 32 rows pdnet adds under the title screen's "Phantom Dust" logo
# (gamepatches.cpp, FDLogo.alr). Four 560x32 masks: the letters, a dark outline, and top-left / bottom-right edges
# for a bevel. The colour comes from the logo itself at run time (the rusty metal inside its "D").
#   py tools\subtitle.py src\titlesub.h
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

W, H = 560, 32
font = ImageFont.truetype(r'C:\Windows\Fonts\georgiab.ttf', 24)
text, track = "Community Edition", 3
widths = [font.getlength(c) for c in text]
m = Image.new('L', (int(sum(widths) + track * len(text)) + 8, 40), 0)
draw, x = ImageDraw.Draw(m), 4
for c, w in zip(text, widths):
    draw.text((x, 2), c, font=font, fill=255)
    x += w + track
m = m.crop(m.getbbox())

letters = Image.new('L', (W, H), 0)
letters.paste(m, ((W - m.size[0]) // 2 + 40, (H - m.size[1]) // 2 - 2))
outline = letters.filter(ImageFilter.MaxFilter(3))
high = ImageChops.subtract(letters, ImageChops.offset(letters, 1, 1)).point(lambda v: v // 2)
low = ImageChops.subtract(letters, ImageChops.offset(letters, -1, -1)).point(lambda v: v // 2)

def array(name, image):
    data = image.tobytes()
    rows = ['    ' + ','.join(str(v) for v in data[i:i + 40]) + ',' for i in range(0, len(data), 40)]
    return 'const uint8_t %s[%d] = {\n%s\n};\n' % (name, len(data), '\n'.join(rows))

with open(sys.argv[1], 'w') as out:
    out.write('// "Community Edition" under the title screen logo (gamepatches.cpp), made by tools\\subtitle.py (Georgia Bold).\n')
    out.write('// kSubW x kSubH masks: letters, dark outline, bevel highlight and shadow.\n#pragma once\n#include <cstdint>\n')
    out.write('constexpr int kSubW = %d, kSubH = %d;\n' % (W, H))
    for name, image in (('kSubLetters', letters), ('kSubOutline', outline), ('kSubHigh', high), ('kSubLow', low)):
        out.write(array(name, image))
print(m.size)
