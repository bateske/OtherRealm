#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Adapt a privately generated Amiga pack for Otherrealm's 104x65 viewport.

No original game resources or dialogue are included. Strings are recovered from
the user's executable. Every modification is recorded in the private manifest.
"""
from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import struct
import textwrap
import zlib

from build_pack import HEADER, ENTRY, PRESENT, PACKED_BITMAP, DataError, OfsDisk, aligned, verify_pack
from build_demo import FONT

ADAPTED = 8
WIDTH, HEIGHT = 104, 65
PROFILE = "amiga-en-handheld-v1"
# Fingerprints identify layouts; they contain no original resource bytes.
CODE_HASHES = {
    21: "dd45a7f3b31d34ca7e609d293e5ef73367cbce05d9914f9ae8009dc60a139e8e",
    24: "0ab56174078bda1ed000a5ba0419a9c83c8b735afb75f638d666d1f44f0ec18e",
    27: "cdd7ac14dd21c0af268ca67f6294cbe56be73ced18e5130231199f706a5f3dc2",
    30: "590ae5aae922ea10f29f3f6b2faba65bc31ebf09ae3090ddc5dbb1e16e55211c",
    33: "fdb818c2c7aedd2c05fb0d4db8a1723d9e1422ea09b0f709af7025e8b470d3a1",
    36: "75395a0aee65541c4597e132ad289f5fd0c473601c8d7f567a0b8281277d2536",
    39: "5cb36ba5d3664cf77b05b4d4c9cc69ffe92fa1a3271ad517c105eb1279607afe",
    42: "9c10183590f7f3c8042d6eaddd6878b7c7374aa8e99d11b10a8af4c9ba7f1ab7",
    126: "a63aeb8a058f9404ad2b0046673e5704b2763cc5c70409ee7498f6976f486da8",
}
SCENES = {21: "title-and-protection", 24: "laboratory-intro", 27: "water-and-beast",
          30: "prison", 33: "city", 36: "arena", 39: "palace", 42: "ending", 126: "password"}
VECTOR_LABELS = {
    # Code PC: shape byte offset, native label, x, y. Digit children are skipped
    # because the preceding ROOM label now includes the original room number.
    0x15b3: (0xe6fa, "ROOM 3", 2, 2), 0x15b7: (0xe752, "", 0, 0),
    0x15c0: (0xe6fa, "ROOM 1", 2, 29), 0x15c4: (0xe772, "", 0, 0),
    0x125c: (0xe77e, "LAB", 78, 54), 0x15d2: (0xe77e, "LAB", 78, 54),
}


@dataclass
class Resource:
    data: bytes
    kind: int
    flags: int


def read_pack(path: Path) -> tuple[bytes, int, int, list[Resource]]:
    verify_pack(path)
    data = path.read_bytes()
    _, _, _, count, table, _, _, flags, dimensions = HEADER.unpack_from(data)
    result = []
    for i in range(count):
        offset, size, _, kind, entry_flags = ENTRY.unpack_from(data, table + i * ENTRY.size)
        result.append(Resource(data[offset:offset + size], kind, entry_flags))
    return data, flags, dimensions, result


def encode_pack(resources: list[Resource], flags: int, dimensions: int) -> bytes:
    payload = aligned(HEADER.size + len(resources) * ENTRY.size)
    out = bytearray(payload)
    for i, resource in enumerate(resources):
        if resource.data:
            offset = len(out)
            out.extend(resource.data)
            out.extend(bytes(aligned(len(out)) - len(out)))
            crc = zlib.crc32(resource.data)
        else:
            offset = crc = 0
        ENTRY.pack_into(out, HEADER.size + i * ENTRY.size, offset, len(resource.data), crc,
                        resource.kind, resource.flags)
    HEADER.pack_into(out, 0, b"ORW1", 1, HEADER.size, len(resources), HEADER.size,
                     payload, len(out), flags, dimensions)
    return bytes(out)


def extract_strings(executable: bytes) -> tuple[dict[int, str], int]:
    """Find the executable's word-ID/zero-terminated/even-aligned text table.

    Records end with at least two zero bytes; an extra zero aligns the next ID.
    The sentinel is FFFF. Searching candidates avoids an executable-version
    address assumption. Validate the complete table, not a loose text match.
    """
    for match in re.finditer(b"\x00\x01", executable):
        start = cursor = match.start()
        if start & 1:
            continue
        strings: dict[int, str] = {}
        for _ in range(512):
            if cursor + 2 > len(executable):
                break
            identity = int.from_bytes(executable[cursor:cursor + 2], "big")
            cursor += 2
            if identity == 0xffff:
                required = {1, 2, 0x1a, 0x35, 0x13d, 0x182, 0x185, 0x190, 0x265}
                if len(strings) >= 100 and required <= strings.keys():
                    return strings, start
                break
            if not 0 < identity < 4096:
                break
            end = executable.find(b"\0", cursor, min(cursor + 2048, len(executable)))
            if end < 0 or end + 1 >= len(executable) or executable[end + 1] != 0:
                break
            raw = executable[cursor:end]
            if any(c not in (10, 13) and not 32 <= c <= 126 for c in raw):
                break
            strings.setdefault(identity, raw.decode("ascii").replace("\r", "\n"))
            cursor = (end + 3) & ~1
    raise DataError("supported original executable string table not found")


def instructions(code: bytes):
    """Decode every original instruction, including variable-length shapes.

    A validated boundary walk prevents matching opcode bytes inside operands.
    Supports the two Otherrealm extension opcodes for output verification.
    """
    lengths = (4, 3, 3, 4, 3, 1, 1, 3, 4, 4, 0, 3, 4, 2, 3, 3,
               2, 1, 6, 3, 4, 4, 4, 4, 6, 3, 6, 6, 6)
    cursor = 0
    while cursor < len(code):
        op = code[cursor]
        # Original Amiga resource segments may end in a single alignment byte.
        # It is not an executable instruction or a permitted branch target.
        if cursor == len(code) - 1 and op == 0 and not len(code) & 1:
            return
        if op & 0x80:
            length = 4
        elif op & 0x40:
            length = 5 + int(not op & 0x30) + int(not op & 0x0c) + int((op & 3) in (1, 2))
        elif op == 10:
            if cursor + 1 >= len(code):
                raise DataError("truncated conditional instruction")
            operand = code[cursor + 1]
            length = 7 if operand & 0x40 and not operand & 0x80 else 6
        elif op < len(lengths):
            length = lengths[op]
        else:
            raise DataError(f"unknown opcode 0x{op:02x} at 0x{cursor:04x}")
        if cursor + length > len(code):
            raise DataError(f"truncated opcode at 0x{cursor:04x}")
        yield cursor, op, code[cursor:cursor + length]
        cursor += length


def branch_targets(code: bytes) -> dict[int, int]:
    result = {}
    boundaries = set()
    for pc, op, raw in instructions(code):
        boundaries.add(pc)
        if op in (4, 7, 8, 9, 10):
            result[pc] = int.from_bytes(raw[-2:], "big")
    for pc, target in result.items():
        if target not in boundaries and not (code[pc] == 8 and target == 0xfffe):
            raise DataError(f"branch at 0x{pc:04x} targets a non-instruction 0x{target:04x}")
    return result


def clean(text: str) -> str:
    # The original font stores the copyright sign at ASCII '}'. Keep its meaning.
    text = text.upper().replace("}", "(C)")
    text = re.sub(r"\.{2,}", " ", text)
    # This heading is deliberately letter-spaced in the source.
    if re.fullmatch(r"(?:[A-Z]\s+){4,}[A-Z]\s*", text):
        text = "".join(text.split())
    return "\n".join(" ".join(line.split()) for line in text.splitlines() if line.strip())


def wrapped(text: str, columns: int, *, paragraph: bool = False) -> str:
    text = clean(text)
    lines = [" ".join(text.split())] if paragraph else text.splitlines()
    return "\n".join(line for source in lines for line in textwrap.wrap(
        source, columns, break_long_words=True, break_on_hyphens=False))


def text_resource(text: str, width: int, height: int, background: int = 2, clear_box: bool = True) -> bytes:
    if not 0 < width <= WIDTH or not 0 < height <= HEIGHT or not 0 <= background < 16:
        raise DataError("native text box outside viewport")
    if any(len(line) * 4 - 1 > width for line in text.splitlines()) or len(text.splitlines()) * 6 > height:
        raise DataError(f"native text does not fit {width}x{height}: {text!r}")
    return b"TX\x01" + bytes((int(clear_box), width, height, background, 0)) + text.encode("ascii") + b"\0"


def layout(resource: int, pc: int, identity: int, x: int, y: int, text: str):
    """Return x,y,width,height,background,text,clear-box; no invented dialogue."""
    nx, ny, w, h, bg, clear_box = 2, min(y * HEIGHT // 200, HEIGHT - 6), 100, 6, 0, True
    paragraph = False
    if resource == 24:
        bg = 2
        if identity == 0x192:
            nx, ny, w, h = 46, 4, 56, 6
        elif identity in (0x190, 0x191):
            nx, ny, w, h, paragraph = 2, 51, 100, 12, True
        elif identity == 2:
            nx, ny, w, h, paragraph = 2, 8, 100, 30, True
        elif identity == 0x1a:
            nx, ny, w, h, paragraph = 2, 27, 100, 36, True
        elif identity == 0x38:
            nx, ny, w, h, paragraph = 2, 1, 100, 24, True
        elif identity == 0x22:
            nx, ny, w, h = 2, 0, 100, 8
            text = text.strip(" -")
        elif identity in (0x31, 0x32, 0x33):
            nx, ny, w, h, paragraph = 2, 46, 100, 18, True
            text = text.lstrip("- ")
        elif identity == 0x34:
            nx, ny, w, h = 34, 51, 32, 6
        elif identity == 0x35:
            nx, ny, w, h = 2, 29, 100, 36
            text = text.lstrip("- ")
        elif identity == 0x36:
            nx, ny, w, h, paragraph = 2, 26, 100, 12, True
        elif identity == 0x23:
            nx, ny, w, h, paragraph = 2, 22, 100, 12, True
        elif identity in (*range(0x24, 0x2c), 0x4b, 0x4c):
            nx, ny, w, h = 48, 40, 12, 6
        elif identity == 0x39:
            nx, ny, w, h = 2, 56, 100, 6
        elif identity == 0x194:
            nx, ny, w, h = 94, 56, 8, 6
        elif 0x14 <= identity <= 0x18:
            nx, ny, w, h = 2, 49, 100, 6
        elif 0x3e <= identity <= 0x47:
            nx, ny, w, h = (2, 39 if y == 120 else 45, 100, 6)
            if identity in (0x44, 0x45):
                nx, ny, w, h = 2, 7, 4, 6
        elif identity == 7 or 0x0a <= identity <= 0x13 or identity in (3, 4, 0x21):
            nx, ny, w, h = max(2, 4 + (x - 3) * 4), (56 if y == 158 else 49), 4, 6
            clear_box = identity != 0x21
        else:
            raise DataError(f"unclassified intro string {identity:03x} at {pc:04x}")
    elif resource == 42:
        credit_y = {0x258: 4, 0x259: 11, 0x25a: 18, 0x25b: 25, 0x25c: 39, 0x25d: 46,
                    0x263: 0, 0x264: 10, 0x265: 0}
        if identity not in credit_y:
            raise DataError(f"unclassified ending string {identity:03x}")
        ny = credit_y[identity]
        h = {0x25b: 12, 0x25d: 12, 0x264: 54, 0x265: 12}.get(identity, 6)
        paragraph = identity == 0x25b
    elif resource == 21:
        ny = {0x181: 24, 0x182: 34, 0x183: 20, 0x184: 33, 0x185: 42,
              0x186: 20, 0x187: 32, 0x188: 44, 0x144: 32, 0x142: 12}.get(identity, ny)
        h = 24 if identity == 0x142 else (12 if identity == 0x183 else 6)
        paragraph = True
    else:
        # Palace's original restart countdown remains visible beneath the menu.
        nx, ny, w, h = 50, 32, 4, 6
    result = wrapped(text, w // 4, paragraph=paragraph)
    return nx, ny, w, h, bg, result, clear_box


def event(number: int) -> bytes:
    return bytes((0x1c, number, 0, 0, 0, 0))


def patch_bitmap(data: bytes, index: int, strings: dict[int, str]) -> tuple[bytes, dict]:
    pixels = [n for byte in data for n in (byte >> 4, byte & 15)]
    if len(pixels) != WIDTH * HEIGHT:
        raise DataError("title bitmap is not preconverted to 104x65")
    if index == 83:
        # Leave the large logo untouched; its original credits occupy y>=50.
        fg = Counter(p for p in pixels[50 * WIDTH:] if p).most_common(1)[0][0]
        pixels[50 * WIDTH:] = [0] * ((HEIGHT - 50) * WIDTH)
        rows = [(clean(strings[0x182]), 51), (clean(strings[0x185]), 58)]
    else:
        fg = Counter(p for p in pixels if p).most_common(1)[0][0]
        pixels[:] = [0] * len(pixels)
        rows = [(clean(strings[0x49]), 27), ("(C) 1991", 51), (clean(strings[0x49]), 58)]
    font = dict(FONT)
    font.update({"0": [7,5,5,5,7], "1": [2,6,2,2,7], "9": [7,5,7,1,7],
                 "(": [1,2,2,2,1], ")": [4,2,2,2,4]})
    for text, y in rows:
        x = (WIDTH - (len(text) * 4 - 1)) // 2
        if x < 0:
            raise DataError("bitmap credit too wide")
        for ch in text:
            if ch not in font:
                raise DataError(f"unsupported bitmap title character {ch!r}")
            for dy, bits in enumerate(font[ch]):
                for dx in range(3):
                    if bits & (4 >> dx):
                        pixels[(y + dy) * WIDTH + x + dx] = fg
            x += 4
    return bytes((pixels[i] << 4) | pixels[i + 1] for i in range(0, len(pixels), 2)), {
        "resource": index, "action": "native-bitmap-credit", "rows": rows,
        "large_logo_preserved": index == 83,
        "copyright_source": "visible original bitmap 71" if index == 71 else None}


def patch(input_path: Path, output_path: Path, executable: bytes | None = None,
          manifest_path: Path | None = None) -> dict:
    original, flags, dimensions, resources = read_pack(input_path)
    manifest = {"profile": PROFILE, "input_sha256": hashlib.sha256(original).hexdigest(),
                "viewport": [WIDTH, HEIGHT], "sites": [], "bitmap_patches": []}
    if flags & ADAPTED:
        identities = [r.data for r in resources if r.kind == 9]
        if len(identities) != 1 or len(identities[0]) != 8 or identities[0][:4] != b"ORID":
            raise DataError("adapted input has no unique valid original-pack identity")
        result = original
        manifest["already_adapted"] = True
        manifest["original_pack_crc32"] = int.from_bytes(identities[0][4:], "little")
    else:
        if flags != 3 or dimensions != WIDTH | (HEIGHT << 16) or len(resources) != 146:
            raise DataError("patch profile requires an original 146-resource Amiga 104x65 pack")
        if executable is None:
            raise DataError("first application requires --executable or --adf to recover original text")
        strings, table = extract_strings(executable)
        manifest["executable_sha256"] = hashlib.sha256(executable).hexdigest()
        manifest["string_table_offset"] = table
        manifest["string_count"] = len(strings)
        for identity, digest in CODE_HASHES.items():
            if resources[identity].kind != 4 or hashlib.sha256(resources[identity].data).hexdigest() != digest:
                raise DataError(f"resource {identity}: unsupported original script revision; no patch written")
        text_ids: dict[bytes, int] = {}
        for resource in CODE_HASHES:
            before = resources[resource].data
            code = bytearray(before)
            targets = branch_targets(before)
            for pc, op, raw in instructions(before):
                if op != 0x12:
                    continue
                identity, x, y, color = struct.unpack(">HBBB", raw[1:])
                source = strings.get(identity)
                site = {"code_resource": resource, "scene": SCENES[resource], "offset": pc,
                        "original_id": identity, "original_text": source,
                        "original_position": [x, y], "color": color}
                if resource == 126:
                    replacement, reason = event(0), "password screen replaced by autosave menu"
                elif identity == 0x13d:
                    replacement, reason = event(1), "retry-checkpoint menu event"
                elif identity == 0x13c or 0x15e <= identity <= 0x174:
                    replacement, reason = event(0), "access code replaced by persistent checkpoint"
                elif source is None:
                    replacement, reason = event(0), "legacy ID absent from original executable table"
                elif resource == 24 and identity == 0x21 and 0x11b8 <= pc <= 0x1221:
                    replacement, reason = event(0), "decorative parameter cursor omitted after text reflow"
                elif not source.strip():
                    replacement, reason = event(0), "empty original string"
                else:
                    layout_id, layout_source = identity, source
                    if resource == 24 and identity == 0x1c:
                        # The original overwrites only '+' with '-'. Reflow the
                        # same source panel when that edit occurs.
                        layout_id = 0x1a
                        layout_source = strings[0x1a].replace("g+", "g-")
                        site["derived_from_id"] = 0x1a
                    nx, ny, w, h, bg, text, clear_box = layout(resource, pc, layout_id, x, y, layout_source)
                    data = text_resource(text, w, h, bg, clear_box)
                    if data not in text_ids:
                        text_ids[data] = len(resources)
                        resources.append(Resource(data, 8, PRESENT))
                    rid = text_ids[data]
                    replacement = b"\x1b" + struct.pack(">HBBB", rid, nx, ny, color)
                    reason = "native text"
                    site.update({"text_resource": rid, "text": text, "position": [nx, ny],
                                 "box": [w, h], "background": bg, "clear": clear_box})
                site["action"] = reason
                manifest["sites"].append(site)
                code[pc:pc + 6] = replacement
            if resource == 21:
                # The original localized title masks part of bitmap 83's baked
                # French credits. Its old rectangle also cuts our new lines.
                pc = 0x00d9
                if before[pc:pc + 4] != bytes((0x88, 0x2c, 160, 100)):
                    raise DataError("unexpected original title credit mask")
                code[pc:pc + 4] = b"\x07\x00\xdd\x11"
                manifest.setdefault("vector_patches", []).append({
                    "code_resource": 21, "offset": pc, "shape_offset": 0x1058,
                    "action": "old title-credit eraser disabled for native bitmap credits"})
            if resource == 24:
                for pc, (shape_offset, label, x, y) in VECTOR_LABELS.items():
                    raw = before[pc:pc + 4]
                    if not raw[0] & 0x80 or (((raw[0] & 0x7f) << 8) | raw[1]) * 2 != shape_offset:
                        raise DataError(f"unexpected vector label shape at {pc:04x}")
                    destination = pc + 4
                    if label:
                        data = text_resource(label, len(label) * 4, 6, 2)
                        if data not in text_ids:
                            text_ids[data] = len(resources)
                            resources.append(Resource(data, 8, PRESENT))
                        destination = len(code)
                        code.extend(b"\x1b" + struct.pack(">HBBB", text_ids[data], x, y, 6))
                        code.extend(b"\x07" + struct.pack(">H", pc + 4))
                    code[pc:pc + 4] = b"\x07" + struct.pack(">H", destination) + b"\x11"
                    manifest.setdefault("vector_patches", []).append({
                        "code_resource": resource, "offset": pc, "shape_offset": shape_offset,
                        "text": label, "position": [x, y], "trampoline": destination if label else None,
                        "action": "native panel label" if label else "digit merged into room label"})
            if resource == 126:
                # A fresh entry raises Continue/New Game once, then safely yields.
                code = bytearray(event(2) + b"\x06\x07\x00\x06")
            else:
                after_targets = branch_targets(bytes(code))
                if any(after_targets.get(pc) != destination for pc, destination in targets.items()):
                    raise DataError(f"resource {resource}: patch moved a branch target")
            resources[resource] = Resource(bytes(code), 4, PRESENT)
        for index in (71, 83):
            modified, description = patch_bitmap(resources[index].data, index, strings)
            resources[index] = Resource(modified, 2, PRESENT | PACKED_BITMAP)
            manifest["bitmap_patches"].append(description)
        identity = zlib.crc32(original)
        resources.append(Resource(b"ORID" + struct.pack("<I", identity), 9, PRESENT))
        manifest["original_pack_crc32"] = identity
        manifest["native_text_resources"] = len(text_ids)
        manifest["unchanged_bitmap_resources"] = [67, 68, 69, 70, 72, 73, 144, 145]
        manifest["unavailable_bitmap_resources"] = [18, 19]
        result = encode_pack(resources, flags | ADAPTED, dimensions)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_name(output_path.name + ".tmp")
    temporary.write_bytes(result)
    try:
        manifest["output"] = verify_pack(temporary)
        temporary.replace(output_path)
    finally:
        if temporary.exists():
            temporary.unlink()
    manifest_path = manifest_path or output_path.with_suffix(output_path.suffix + ".patch.json")
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="private 104x65 ORW1 pack")
    parser.add_argument("output", type=Path, help="adapted private pack (usually another directory)")
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--executable", type=Path, help="original 'another' extracted from the ADF")
    source.add_argument("--adf", type=Path, help="original Disk A containing 'another'")
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()
    try:
        executable = args.executable.read_bytes() if args.executable else None
        if args.adf:
            executable = OfsDisk(args.adf).files().get("another")
            if executable is None:
                raise DataError("ADF does not contain the original executable 'another'")
        report = patch(args.input, args.output, executable, args.manifest)
        print(f"{'Verified existing' if report.get('already_adapted') else 'Patched'} {args.output}: "
              f"{len(report['sites'])} text sites, {report['output']['bytes']:,} bytes")
        print(f"SHA256 {report['output']['sha256']}")
    except (DataError, OSError, ValueError) as error:
        parser.exit(1, f"error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
