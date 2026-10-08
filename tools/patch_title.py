#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Attach a privately rendered 128x128 title backdrop without changing save IDs.

The backdrop is 32 bytes of little-endian RGB565 palette, then 8192 bytes of
4-bit pixels (high nibble first). Run capture_title.js on a 208x130 source pack.
Only the user's pack contains the game imagery. The engine supplies its own logo.
"""
import argparse
import struct
from pathlib import Path
from patch_text import Resource, read_pack, encode_pack
from build_pack import HEADER, ENTRY, verify_pack, DataError


def attach(pack_path, backdrop_path, output, unlocked_path=None):
    data, flags, dimensions, resources = read_pack(pack_path)
    backdrop = backdrop_path.read_bytes()
    if len(backdrop) != 8224:
        raise DataError('Title backdrop must contain exactly 8224 bytes')
    unlocked = unlocked_path.read_bytes() if unlocked_path else None
    if unlocked is not None and len(unlocked) != 8224:
        raise DataError('Unlocked title backdrop must contain exactly 8224 bytes')
    additions = []
    if not any(r.kind == 9 for r in resources):
        identity = 2166136261
        for i in range(len(resources)):
            for b in data[32+i*16+8:32+i*16+16]:
                identity = ((identity ^ b) * 16777619) & 0xffffffff
        additions.append(Resource(b'ORID'+struct.pack('<I', identity), 9, 1))
    title = Resource(backdrop, 10, 1)
    matches = [i for i, r in enumerate(resources) if r.kind == 10]
    if matches:
        resources[matches[0]] = title
    else:
        additions.append(title)
    if unlocked is not None:
        alternate = Resource(unlocked, 11, 1)
        matches = [i for i, r in enumerate(resources) if r.kind == 11]
        if matches:
            resources[matches[0]] = alternate
        else:
            additions.append(alternate)
    # A custom scene directory must remain last; its resource ID isn't referenced.
    at = len(resources)-1 if flags & 4 else len(resources)
    resources[at:at] = additions
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix+'.tmp')
    temporary.write_bytes(encode_pack(resources, flags, dimensions))
    try:
        result = verify_pack(temporary)
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    return result


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('pack', type=Path)
    p.add_argument('backdrop', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--unlocked', type=Path, help='alternate title earned by completing the game')
    a = p.parse_args()
    print(attach(a.pack, a.backdrop, a.output, a.unlocked))
