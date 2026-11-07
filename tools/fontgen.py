#!/usr/bin/env python3
"""Regenerate four 8x12 bitmap faces. Requires Pillow and the noted OFL/Vera fonts.
The generated include/font8x12.h is checked in; nxdk builds DO NOT require Pillow.
"""
from PIL import Image, ImageFont, ImageDraw
from pathlib import Path
import re
root=Path(__file__).resolve().parent.parent
header=(root/'tools/font8x12-bold-baseline.h').read_text()
original=[]
for line in header.splitlines():
    if re.search(r'/\* (\d+) \*/',line):
        original.append([int(x,16) for x in re.findall(r'0x([a-fA-F0-9]{2})',line)])
if len(original)!=95:
    raise RuntimeError('Expected existing 95-glyph font to preserve original bold')
fonts=[
    ('bold',None),
    ('regular','/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'),
    ('cousine','/usr/share/fonts/truetype/croscore/Cousine-Regular.ttf'),
    ('cousine_bold','/usr/share/fonts/truetype/croscore/Cousine-Bold.ttf'),
]
faces=[original]
for name,src in fonts[1:]:
    font=ImageFont.truetype(src,11)
    rows=[]
    for ch in range(32,127):
        im=Image.new('L',(8,12),0)
        # Draw every glyph against the SAME alphabet baseline.  The old
        # y=-3 offset sheared the ascenders off f/i/l at the top of
        # this 12px cell, making these letters look like t and |.
        # Anchor 'ls' is the font baseline, not Pillow's top-left box.
        ImageDraw.Draw(im).text((0,9),chr(ch),font=font,fill=255,anchor='ls')
        # Pillow rasterizes some underscore fonts beyond the bottom of
        # an 8x12 cell; draw a reliable baseline underscore explicitly.
        if ch==95:
            ImageDraw.Draw(im).line((0,10,6,10),fill=255)
        row=[sum(1<<(7-x) for x in range(8) if im.getpixel((x,y))>=95) for y in range(12)]
        if ch!=32 and not any(row):
            raise RuntimeError(f'{name} missing {chr(ch)}')
        rows.append(row)
    faces.append(rows)
with (root/'include/font8x12.h').open('w') as f:
    f.write('/* Fixed 8x12 bitmap faces. Original bold: DejaVu Sans Mono Bold (Vera license);\n'
            ' * regular: DejaVu Sans Mono (Vera license); Cousine: SIL Open Font License.\n'
            ' * Pre-rasterized data only; no TTF files are distributed. */\n'
            '#ifndef XSSH_FONT8X12_H\n#define XSSH_FONT8X12_H\n'
            '#include <stdint.h>\n#define XSSH_FONT_FACE_COUNT 4\n'
            'static const uint8_t xssh_fonts[4][95][12] = {\n')
    for i,face in enumerate(faces):
        f.write('  { /* '+fonts[i][0]+' */\n')
        for code,row in enumerate(face,32):
            f.write('    {'+','.join(f'0x{v:02x}' for v in row)+'}, /* '+str(code)+' */\n')
        f.write('  },\n')
    f.write('};\n#endif\n')
print('Generated',root/'include/font8x12.h')
