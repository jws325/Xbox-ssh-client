#!/usr/bin/env python3
"""Generate small pre-rasterized symbols (never needed for nxdk compilation)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
p=Path(__file__).resolve().parent.parent
codepoints=[0x00b0,0x00b1,0x00b7,0x00d7,0x2022,0x2026,0x2190,0x2191,0x2192,0x2193,0x2194,0x21b5,0x21d1,0x21d3,0x25b2,0x25bc,0x25c0,0x25b6,0x25c6,0x25cf,0x25cb,0x25a0,0x25a1,0x25aa,0x25ab,0x2713,0x2717,0x2714,0x2718,0x2248,0x2264,0x2265,0x2260,0x00a3,0x20ac,0x03bc,0x2020,0x00a9,0x00ae,0x2122]
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf',11)
out=['/* Pre-rendered symbols, generated with tools/gen-unicode-glyphs.py. */','#ifndef XSSH_UNICODE_GLYPHS_H','#define XSSH_UNICODE_GLYPHS_H','#include <stdint.h>',f'#define XSSH_UNICODE_GLYPHS {len(codepoints)}','static const uint32_t xssh_unicode_points[XSSH_UNICODE_GLYPHS]={'+','.join(hex(i) for i in codepoints)+'};','static const uint8_t xssh_unicode_bits[XSSH_UNICODE_GLYPHS][12]={']
for cp in codepoints:
    im=Image.new('L',(8,12),0)
    ImageDraw.Draw(im).text((0,-3),chr(cp),font=font,fill=255)
    bits=[sum(0x80>>x for x in range(8) if im.getpixel((x,y))>=75) for y in range(12)]
    if not any(bits): raise ValueError(cp)
    out.append(' {'+','.join('0x%02x'%b for b in bits)+'}, /* U+%04X */'%cp)
out+=['};','#endif']
(p/'include/unicode_glyphs.h').write_text('\n'.join(out)+'\n')
