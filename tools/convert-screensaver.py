#!/usr/bin/env python3
"""Convert standard video to XSSH raw RGB565 (XSV) with ffmpeg installed.
Usage: python3 tools/convert-screensaver.py input.mp4 release/screensaver.xsv --fps 12
A 320x240/12fps video uses 1.8 MiB/s uncompressed; no decoding on Xbox.
"""
import argparse, subprocess, struct
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('input',type=Path)
p.add_argument('output',type=Path)
p.add_argument('--width',type=int,default=320)
p.add_argument('--height',type=int,default=240)
p.add_argument('--fps',type=int,default=12)
a=p.parse_args()
if not 1 <= a.width <= 640 or not 1 <= a.height <= 480 or not 1 <= a.fps <= 30:
    p.error('Width 1–640, height 1–480, FPS 1–30')
if a.width%2 or a.height%2: p.error('width and height must be even')
a.output.parent.mkdir(parents=True,exist_ok=True)
# ffmpeg performs ALL expensive decoding, rescaling and colorspace work on PC.
filter=f'fps={a.fps},scale={a.width}:{a.height}:force_original_aspect_ratio=decrease,pad={a.width}:{a.height}:(ow-iw)/2:(oh-ih)/2:color=black,format=rgb565le'
cmd=['ffmpeg','-hide_banner','-loglevel','error','-y','-i',str(a.input),'-an','-vf',filter,'-f','rawvideo','-pix_fmt','rgb565le','-']
proc=subprocess.Popen(cmd,stdout=subprocess.PIPE)
frame_size=a.width*a.height*2
frames=0
try:
    with a.output.open('wb') as dest:
        dest.write(struct.pack('<8sIIII',b'XSSHRGB5',a.width,a.height,a.fps,0))
        while True:
            block=bytearray()
            while len(block)<frame_size:
                chunk=proc.stdout.read(frame_size-len(block))
                if not chunk:break
                block.extend(chunk)
            if not block:break
            if len(block)!=frame_size:raise RuntimeError('Incomplete frame from ffmpeg')
            dest.write(block)
            frames+=1
            if frames>=1_000_000:raise RuntimeError('Too many frames')
        dest.seek(20)
        dest.write(struct.pack('<I',frames))
    if proc.wait()!=0 or not frames:raise RuntimeError('ffmpeg conversion failed / zero frames')
except BaseException:
    proc.kill() if proc.poll() is None else None
    a.output.unlink(missing_ok=True)
    raise
print(f'Created {a.output}: {frames} frames at {a.width}x{a.height} @{a.fps}fps')
