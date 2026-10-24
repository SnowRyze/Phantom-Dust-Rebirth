import sys, random
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

W,H=360,40
def mask(fontspec, size, track, var=None):
    f=ImageFont.truetype(fontspec,size)
    if var: f.set_variation_by_name(var)
    txt="Community Edition"
    big=Image.new('L',(W*4,H*4),0); d=ImageDraw.Draw(big)
    x=0
    for ch in txt:
        d.text((x,8),ch,font=f,fill=255)
        x+= (f.getlength(ch)+track)
    bbox=big.getbbox(); m=big.crop(bbox)
    return m
m=mask('C:\\Windows\\Fonts\\bahnschrift.ttf',26,9,b'SemiBold Condensed')
random.seed(7); px=m.load()
for _ in range(m.size[0]*m.size[1]//60):
    x=random.randrange(m.size[0]); y=random.randrange(m.size[1]); px[x,y]=px[x,y]//3
P=12; X0=575-m.size[0]-P; Y0=222-P; RW=m.size[0]+2*P; RH=m.size[1]+2*P
let=Image.new('L',(RW,RH),0); let.paste(m,(P,P))
glow=let.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(5))
def arr(name,im):
    b=im.tobytes(); rows=[]
    for i in range(0,len(b),32): rows.append('    '+','.join(str(v) for v in b[i:i+32])+',')
    return 'const uint8_t %s[%d] = {\n%s\n};\n'%(name,len(b),'\n'.join(rows))
h='// "Community Edition" under the title screen logo (gamepatches.cpp TitleSubtitle), made by tools\\subtitle.py\n'
h+='// (Bahnschrift SemiBold Condensed, speckled). kSubLetters darkens, kSubGlow lightens; both kSubW x kSubH at kSubX, kSubY.\n#pragma once\n#include <cstdint>\n'
h+='constexpr int kSubX = %d, kSubY = %d, kSubW = %d, kSubH = %d;\n'%(X0,Y0,RW,RH)
h+=arr('kSubLetters',let)+arr('kSubGlow',glow)
open(sys.argv[1],'w').write(h); print(X0,Y0,RW,RH)
