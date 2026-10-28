"""Generate public-domain-style bitmap data from an installed open font.
A derived bitmap is bundled; its source is DejaVu Sans Mono (Bitstream Vera license).
"""
from PIL import Image, ImageFont, ImageDraw
from pathlib import Path
src = '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
font = ImageFont.truetype(src, 11)
rows=[]
for ch in range(32,127):
    im=Image.new('1',(8,12),0)
    d=ImageDraw.Draw(im)
    d.text((0,-1),chr(ch),font=font,fill=1)
    rows.append([sum((1<<(7-x)) if im.getpixel((x,y)) else 0 for x in range(8)) for y in range(12)])
out=Path(__file__).resolve().parent.parent/'include/font8x12.h'
with out.open('w') as f:
    f.write('/* 8x12 ASCII glyph masks. Derived from DejaVu Sans Mono; Bitstream Vera license. */\n#ifndef XSSH_FONT8X12_H\n#define XSSH_FONT8X12_H\n#include <stdint.h>\nstatic const uint8_t xssh_font[95][12] = {\n')
    for n,row in enumerate(rows):
        f.write('    {'+','.join(f'0x{v:02x}' for v in row)+'}, /* '+str(n+32)+' */\n')
    f.write('};\n#endif\n')
print(out)
