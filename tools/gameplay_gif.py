#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Turn record_gameplay's real engine frames into an edited README GIF."""
import argparse
from array import array
import json
from pathlib import Path
import struct
import sys
from PIL import Image, ImageDraw


def records(path):
    result=[];clock=0
    with path.open('rb') as f:
        if f.read(12)!=b'ORGF'+struct.pack('<II',128,128):raise ValueError('Invalid engine recording')
        while header:=f.read(12):
            if len(header)!=12:raise ValueError('Truncated frame header')
            duration,chapter,tick=struct.unpack('<III',header)
            pixels=f.read(32768)
            if len(pixels)!=32768:raise ValueError('Truncated frame pixels')
            result.append(dict(start=clock,ms=duration,chapter=chapter,tick=tick,pixels=pixels))
            clock+=duration
    return result


def image(record):
    data=array('H',record['pixels'])
    if sys.byteorder!='little':data.byteswap()
    rgb=bytes(v for c in data for v in ((c>>11)*255//31,((c>>5)&63)*255//63,(c&31)*255//31))
    return Image.frombytes('RGB',(128,128),rgb)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('recording',type=Path)
    p.add_argument('--output',type=Path,default=Path('docs/gameplay.gif'))
    p.add_argument('--storyboard',action='store_true')
    a=p.parse_args();frames=records(a.recording)
    if a.storyboard:
        samples=[next(r for r in frames if r['start']>=t) for t in range(0,frames[-1]['start'],8000)]
        sheet=Image.new('RGB',(6*256,((len(samples)+5)//6)*280),'#0b1220');d=ImageDraw.Draw(sheet)
        for i,r in enumerate(samples):
            x=i%6*256;y=i//6*280;sheet.paste(image(r).resize((256,256),Image.Resampling.NEAREST),(x,y))
            d.text((x+4,y+259),f"{r['start']/1000:.1f}s chapter {r['chapter']} tick {r['tick']}",fill='white')
        sheet.save('build/gameplay-storyboard.png')
    # Each shot retains its original engine timing. Only cuts omit intervening footage.
    intro=[r for r in frames if r['chapter']==1];pool=[r for r in frames if r['chapter']==2]
    intro_end=intro[-1]['start'];pool_start=pool[0]['start']
    ranges=[(0,1920),(48000,51500),(intro_end-6500,intro_end-2500),
            (pool_start,pool_start+4500),(pool_start+9000,pool_start+15000),(171660,174000)]
    selected=[r for r in frames if any(lo<=r['start']<hi for lo,hi in ranges) or r['chapter']==3]
    out=[];durations=[];carry=0
    for r in selected:
        # GIFs need no more than 16.7 fps for this game; preserve elapsed duration.
        carry+=r['ms']
        if carry<60:continue
        out.append(image(r).resize((384,384),Image.Resampling.NEAREST).quantize(colors=64,method=Image.Quantize.MEDIANCUT))
        durations.append(max(20,round(carry/10)*10));carry=0
    if carry:durations[-1]+=carry
    a.output.parent.mkdir(parents=True,exist_ok=True)
    out[0].save(a.output,save_all=True,append_images=out[1:],duration=durations,loop=0,optimize=True,disposal=1)
    report=dict(source='Actual portable C++ Session/VM/renderer at 104x65, presented at 128x128',
                display_scale=3,frames=len(out),duration_ms=sum(durations),bytes=a.output.stat().st_size,
                edits='Cuts only; engine timing preserved within each shot',ranges_ms=ranges)
    a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
