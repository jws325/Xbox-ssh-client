#!/usr/bin/env python3
"""Generate a small original demo video in XSV RGB565 format, no dependencies."""
import argparse, math, struct
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('output',type=Path)
p.add_argument('--if-missing',action='store_true')
a=p.parse_args()
if a.if_missing and a.output.exists():
    print('Keeping existing screensaver:',a.output)
    raise SystemExit(0)
a.output.parent.mkdir(parents=True,exist_ok=True)
w,h,fps,frames=160,120,12,48
with a.output.open('wb') as f:
    f.write(struct.pack('<8sIIII',b'XSSHRGB5',w,h,fps,frames))
    for n in range(frames):
        buf=bytearray(w*h*2)
        for y in range(h):
            for x in range(w):
                # Original moving Xbox-green aurora loops, safe offline demo.
                wave=math.sin(x*.077+n*.13)*math.cos(y*.067-n*.08)
                ring=math.sin(math.hypot(x-80,y-60)*.18-n*.25)
                v=max(0,min(1,.35+.27*wave+.25*ring))
                r=int(v*12);g=int(13+v*48);b=int(v*7)
                color=((r&31)<<11)|((g&63)<<5)|(b&31)
                struct.pack_into('<H',buf,(y*w+x)*2,color)
        f.write(buf)
print(f'Created {a.output}: {frames} RGB565 frames at {w}x{h}, {fps} fps')
