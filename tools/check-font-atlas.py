#!/usr/bin/env python3
"""Verify every generated font face has nonblank printable glyphs."""
from pathlib import Path
import re
header=(Path(__file__).resolve().parent.parent/'include/font8x12.h').read_text()
faces=[];face=[]
for line in header.splitlines():
    m=re.search(r'/\* (\d+) \*/',line)
    if m:
        values=[int(x,16) for x in re.findall(r'0x([a-fA-F0-9]{2})',line)]
        assert len(values)==12
        face.append(values)
        if int(m.group(1))==126:
            faces.append(face);face=[]
assert len(faces)==4, f'Expected 4 fonts, got {len(faces)}'
for i,face in enumerate(faces):
    assert len(face)==95
    assert not any(face[0])
    for c in range(33,127):assert any(face[c-32]),(i,c)
    pixels=sum(v.bit_count() for row in face for v in row)
    assert pixels>400,(i,pixels)
    print(f'PASS font {i}: 94 nonblank glyphs, {pixels} pixels')

# Pixel-level guard against the November 2026 Cousine clipping regression.
# The upper hooks of f/l and the dot on i must survive rasterization,
# while descenders must remain within the cell.
for face_id in (1,2,3):
    font=faces[face_id]
    def rows(ch):return font[ord(ch)-32]
    i=rows('i'); f=rows('f'); ell=rows('l'); t=rows('t'); g=rows('g')
    assert any(i[y] for y in (0,1,2)),(face_id,'missing i dot')
    assert any(i[y] for y in (4,5,6,7,8,9)),(face_id,'missing i stem')
    assert any(f[y] for y in (0,1,2)),(face_id,'missing f ascender')
    assert f!=t,(face_id,'f indistinguishable from t')
    assert ell!=t,(face_id,'l indistinguishable from t')
    assert any(g[y] for y in (9,10,11)),(face_id,'g descender clipped')
print('PASS font ascenders, i dots, l/f distinction, descenders')
