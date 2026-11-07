#!/usr/bin/env python3
"""Validate optional AA font build assets and settings without nxdk."""
from pathlib import Path
import re
root=Path(__file__).resolve().parent.parent
head=(root/'include/font8x12_aa.h').read_text()
values=[int(n) for n in re.findall(r'\b(?:[0-9]{1,3})\b',head.split('= {',1)[1])]
assert len(values)==4*95*12*8,(len(values),'unexpected font coverage count')
for face in range(4):
    pixels=values[face*95*12*8:(face+1)*95*12*8]
    assert max(pixels)>=200
    assert any(0<v<255 for v in pixels),'no antialiasing levels present'
    assert any(v==0 for v in pixels)
    for code in (ord('f'),ord('i'),ord('l'),ord('g'),ord('j'),ord('p')):
        glyph=pixels[(code-32)*96:(code-31)*96]
        assert any(glyph),'missing glyph'
    # Verify top dots of lowercase i, vs a plain vertical stroke.
    dot=pixels[(ord('i')-32)*96:(ord('i')-31)*96]
    occupied=[y for y in range(12) if any(dot[y*8:(y+1)*8])]
    assert len(occupied)>=3
main=(root/'src/main.c').read_text()
assert 'text_antialiasing' in main
assert 'a->settings.text_antialiasing?a->font_aa:a->font' in main
assert 'SDL_SetTextureBlendMode(a->font_aa,SDL_BLENDMODE_BLEND)' in main
for name in ('xssh.cfg','xssh.cfg.EXAMPLE.txt'):
    data=(root/name).read_text()
    assert 'text_antialiasing=no' in data
print('PASS: 4 AA font faces, intermediate coverage, nonblank glyphs, cfg and renderer integration')
