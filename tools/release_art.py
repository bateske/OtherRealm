#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compose native cartridge art from the user's rendered title resources.

No game pixels are included in this recipe. Output screenshots retain the
original game's rights; the original Other Realm wordmark is CC0.
"""
import argparse
from collections import Counter
import io
from pathlib import Path
import struct
import sys
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'CHGame' / 'tools'))
from chcart import picture
from patch_text import read_pack


def rgb565(c):
    return ((c >> 11) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31)


def decode_title(data):
    if len(data) != 8224:
        raise ValueError('Expected a 128 x 128 title resource')
    palette = [rgb565(c) for c in struct.unpack('<16H', data[:32])]
    image = Image.new('RGB', (128, 128))
    image.putdata([palette[(data[32+i//2] >> (0 if i & 1 else 4)) & 15] for i in range(16384)])
    return image


def cover(pack):
    resources = read_pack(Path(pack))[3]
    data = next((r.data for r in resources if r.kind == 11), None)
    if data is None:
        data = next(r.data for r in resources if r.kind == 10)
    scene = decode_title(data)
    # Reserve title gold; reduce only scene colors to the menu's own-color budget.
    colors = [c for c, _ in Counter(scene.getdata()).most_common(9)]
    colors += [(0, 0, 0), (255, 244, 214), (128, 128, 128)]
    mapping = {c: min(colors, key=lambda p: sum((c[k]-p[k])**2 for k in range(3)))
               for c in set(scene.getdata())}
    scene.putdata([mapping[c] for c in scene.getdata()])
    logo = Image.open(ROOT / 'assets' / 'title.png').convert('RGBA')
    # The panel-native wordmark has no scaling or antialiasing.
    for y in range(logo.height):
        for x in range(logo.width):
            r, g, b, a = logo.getpixel((x, y))
            if a:
                scene.putpixel((x+8, y+3), rgb565((r>>3)<<11|(g>>2)<<5|(b>>3)) if r > 50 else (0, 0, 0))
    ImageDraw.Draw(scene).rectangle((0, 0, 127, 127), outline=(0, 0, 0))
    out = io.BytesIO(); scene.save(out, 'PNG', optimize=True)
    if not picture.ready(out.getvalue()):
        raise ValueError('Cover exceeds CHGame native picture limits')
    return out.getvalue()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('pack', type=Path)
    p.add_argument('--output', type=Path, default=ROOT / 'docs' / 'cart.png')
    p.add_argument('--banner-source', type=Path, help='208x130 PPM captured by capture_title')
    a = p.parse_args(); a.output.parent.mkdir(parents=True, exist_ok=True)
    data = cover(a.pack); a.output.write_bytes(data)
    picture.preview(data, scale=4, mark=True, bar=.5).save(a.output.with_name('cart-menu-preview.png'))
    if a.banner_source:
        wide = Image.open(a.banner_source).convert('RGB')
        logo = Image.open(ROOT / 'assets' / 'title.png').convert('RGBA')
        wide.paste(logo, (8, 4), logo)
        wide.resize((1040, 650), Image.Resampling.NEAREST).save(a.output.with_name('banner.png'), optimize=True)
    print(f'Validated native CHGame cover: {a.output}')


if __name__ == '__main__':
    main()
