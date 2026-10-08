#!/usr/bin/env python3
"""Build a sector-aligned Otherrealm resource pack from privately owned Amiga ADFs.

Uses only Python's standard library. No original game data is part of this tool.
The resource directory is recovered from the executable on the supplied disk.
Format references: cyxx/rawgl resource.cpp, unpack.cpp, docs/Amiga_DOS.md.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib

SECTOR = 512
HEADER = struct.Struct("<4sHHIIIIII")
ENTRY = struct.Struct("<IIIHH")
AMIGA_ENTRY = struct.Struct(">BBIBBIII")
PRESENT = 1
PACKED_BITMAP = 2
PARTS = (
    (20, 21, 22, 0), (23, 24, 25, 0), (26, 27, 28, 17),
    (29, 30, 31, 17), (32, 33, 34, 17), (35, 36, 37, 0),
    (38, 39, 40, 17), (41, 42, 43, 17), (125, 126, 127, 0),
    (125, 126, 127, 0),
)


def be32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def aligned(value: int) -> int:
    return (value + SECTOR - 1) & -SECTOR


class DataError(ValueError):
    """Input image, compressed resource, or pack failed validation."""


class OfsDisk:
    """Read the directory and checked linked data blocks of an Amiga DOS0 disk."""
    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        if len(self.data) % SECTOR or len(self.data) < 3 * SECTOR:
            raise DataError(f"{path}: invalid disk image length")
        if self.data[:4] != b"DOS\0":
            raise DataError(f"{path}: expected an Amiga DOS0/OFS ADF")
        self.count = len(self.data) // SECTOR

    def block(self, sector: int) -> bytes:
        if not 2 <= sector < self.count:
            raise DataError(f"{self.path}: sector {sector} outside disk")
        block = self.data[sector * SECTOR:(sector + 1) * SECTOR]
        if sum(struct.unpack(">128I", block)) & 0xFFFFFFFF:
            raise DataError(f"{self.path}: checksum failed at sector {sector}")
        return block

    def files(self) -> dict[str, bytes]:
        # Nonbootable volumes may not initialize the boot-block root pointer.
        root_sector = be32(self.data, 8)
        if not 2 <= root_sector < self.count:
            root_sector = self.count // 2
        root = self.block(root_sector)
        if be32(root, 0) != 2 or be32(root, 508) != 1 or be32(root, 12) != 72:
            raise DataError(f"{self.path}: invalid OFS root directory")
        result: dict[str, bytes] = {}
        visited = {root_sector}

        def walk(directory: bytes, prefix: str = "") -> None:
            for sector in struct.unpack_from(">72I", directory, 24):
                while sector:
                    if sector in visited:
                        raise DataError(f"{self.path}: directory cycle at {sector}")
                    visited.add(sector)
                    block = self.block(sector)
                    name_len = block[432]
                    if be32(block, 0) != 2 or name_len > 30:
                        raise DataError(f"{self.path}: invalid file header at {sector}")
                    name = block[433:433 + name_len].decode("latin1")
                    if not name or any(c in name for c in "/\\\0"):
                        raise DataError(f"{self.path}: unsafe disk filename")
                    full_name = (prefix + name).lower()
                    kind = be32(block, 508)
                    if kind == 2:
                        walk(block, full_name + "/")
                    elif kind == 0xFFFFFFFD:
                        if full_name in result:
                            raise DataError(f"{self.path}: duplicate filename {full_name}")
                        result[full_name] = self.read_file(sector, block)
                    else:
                        raise DataError(f"{self.path}: unsupported file type {kind:#x}")
                    sector = be32(block, 496)

        walk(root)
        return result

    def read_file(self, header_sector: int, header: bytes) -> bytes:
        size, sector = be32(header, 324), be32(header, 16)
        out = bytearray()
        visited: set[int] = set()
        sequence = 1
        while sector:
            if sector in visited:
                raise DataError(f"{self.path}: file cycle at {sector}")
            visited.add(sector)
            block = self.block(sector)
            kind, owner, seq, length, nxt = struct.unpack_from(">5I", block)
            if kind != 8 or owner != header_sector or seq != sequence or length > 488:
                raise DataError(f"{self.path}: invalid OFS data block {sector}")
            if len(out) + length > size:
                raise DataError(f"{self.path}: file exceeds declared size")
            out.extend(block[24:24 + length])
            sequence += 1
            sector = nxt
        if len(out) != size:
            raise DataError(f"{self.path}: file truncated ({len(out)} != {size})")
        return bytes(out)


@dataclass(frozen=True)
class Resource:
    kind: int
    bank: int
    offset: int
    packed_size: int
    size: int


def find_directory(executable: bytes) -> tuple[int, list[Resource]]:
    # Empty resource zero, stored with a pointer to bank01. The complete table
    # is validated; this signature alone never establishes an executable match.
    signature = bytes.fromhex("0000000000000001000000000000000000000000")
    candidates = []
    start = executable.find(signature)
    while start >= 0:
        resources = []
        cursor = start
        while cursor + AMIGA_ENTRY.size <= len(executable):
            status, kind, pointer, rank, bank, offset, packed, size = AMIGA_ENTRY.unpack_from(executable, cursor)
            if status == 255:
                break
            if status != 0 or kind > 6 or pointer != 0 or rank != 0 or not 1 <= bank <= 15:
                break
            if packed > size or size > 1024 * 1024 or offset > 16 * 1024 * 1024:
                break
            resources.append(Resource(kind, bank, offset, packed, size))
            cursor += AMIGA_ENTRY.size
        if len(resources) == 146 and executable[cursor] == 255:
            if all(resources[a].kind == 3 and resources[b].kind == 4 and resources[c].kind == 5
                   for a, b, c, _ in PARTS):
                candidates.append((start, resources))
        start = executable.find(signature, start + 1)
    if len(candidates) != 1:
        raise DataError(f"expected one validated Amiga resource directory; found {len(candidates)}")
    return candidates[0]


def bytekiller_unpack(data: bytes, expected_size: int) -> bytes:
    """Decode the game's backwards bitstream, checking bounds and XOR checksum."""
    if len(data) < 12 or len(data) % 4:
        raise DataError("invalid ByteKiller stream length")
    size = be32(data, len(data) - 4)
    if size != expected_size or not 0 < size <= 1024 * 1024:
        raise DataError(f"ByteKiller size {size} differs from directory {expected_size}")
    checksum = be32(data, len(data) - 8)
    bits = be32(data, len(data) - 12)
    checksum ^= bits
    cursor = len(data) - 16
    output = bytearray(size)
    remaining = size

    def next_bit() -> int:
        nonlocal bits, checksum, cursor
        value = bits & 1
        bits >>= 1
        if not bits:
            if cursor < 0:
                raise DataError("truncated ByteKiller bitstream")
            word = be32(data, cursor)
            cursor -= 4
            checksum ^= word
            value = word & 1
            bits = 0x80000000 | (word >> 1)
        return value

    def read_bits(count: int) -> int:
        value = 0
        for _ in range(count):
            value = (value << 1) | next_bit()
        return value

    while remaining:
        literal = False
        if next_bit() == 0:
            if next_bit() == 0:
                count = read_bits(3) + 1
                literal = True
            else:
                count, offset_bits = 2, 8
        else:
            code = read_bits(2)
            if code == 3:
                count = read_bits(8) + 9
                literal = True
            elif code == 2:
                count, offset_bits = read_bits(8) + 1, 12
            else:
                count, offset_bits = code + 3, code + 9
        count = min(count, remaining)
        if literal:
            for _ in range(count):
                remaining -= 1
                output[remaining] = read_bits(8)
        else:
            offset = read_bits(offset_bits)
            if offset == 0 or remaining - 1 + offset >= size:
                raise DataError("ByteKiller reference outside decoded data")
            for _ in range(count):
                remaining -= 1
                output[remaining] = output[remaining + offset]
    if checksum:
        raise DataError("ByteKiller checksum mismatch")
    return bytes(output)


def convert_bitmap(data: bytes, width: int) -> bytes:
    """Nearest-neighbour Amiga 4-plane 320x200 -> high-nibble-first packed 4bpp."""
    if len(data) != 32000:
        raise DataError("Amiga bitmap must contain four 8000-byte planes")
    height = width * 5 // 8
    out = bytearray(width * height // 2)
    for y in range(height):
        sy = y * 200 // height
        for x in range(width):
            sx = x * 320 // width
            pos, mask = sy * 40 + (sx >> 3), 0x80 >> (sx & 7)
            color = sum((1 << plane) if data[pos + plane * 8000] & mask else 0 for plane in range(4))
            out[(y * width + x) >> 1] |= color << (0 if x & 1 else 4)
    return bytes(out)


def verify_pack(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < HEADER.size:
        raise DataError("truncated pack header")
    magic, version, header_size, count, table, payload, total, flags, dimensions = HEADER.unpack_from(data)
    if magic != b"ORW1" or version != 1 or header_size != HEADER.size:
        raise DataError("unsupported pack format")
    if total != len(data) or table != HEADER.size or count > 65536:
        raise DataError("invalid pack length/directory")
    if payload != aligned(table + count * ENTRY.size) or payload > total or total % SECTOR:
        raise DataError("invalid pack payload boundary")
    width, height = dimensions & 65535, dimensions >> 16
    if flags & ~15 or bool(flags & PACKED_BITMAP) != bool(dimensions):
        raise DataError("invalid pack flags/dimensions")
    if dimensions and (width % 2 or height != width * 5 // 8):
        raise DataError("invalid bitmap dimensions")
    previous_end, present = payload, 0
    for index in range(count):
        offset, size, crc, kind, entry_flags = ENTRY.unpack_from(data, table + index * ENTRY.size)
        if kind > 11 or (kind == 7 and not flags & 4) or entry_flags & ~3:
            raise DataError(f"resource {index}: unsupported type/flags")
        if not entry_flags & PRESENT:
            if offset or size or crc or entry_flags:
                raise DataError(f"resource {index}: nonempty missing entry")
            continue
        if not size or offset % SECTOR or offset < previous_end or offset + size > total:
            raise DataError(f"resource {index}: invalid resource bounds")
        if entry_flags & PACKED_BITMAP and (kind != 2 or not dimensions or size != width * height // 2):
            raise DataError(f"resource {index}: inconsistent bitmap metadata")
        if zlib.crc32(data[offset:offset + size]) != crc:
            raise DataError(f"resource {index}: CRC32 mismatch")
        if kind == 8:
            text = data[offset:offset + size]
            if (size < 9 or text[:3] != b"TX\x01" or text[3] & ~1 or
                    not 0 < text[4] <= 104 or not 0 < text[5] <= 65 or
                    text[6] > 15 or text[7] or text[-1] or
                    any(c != 10 and not 32 <= c <= 126 for c in text[8:-1])):
                raise DataError(f"resource {index}: invalid native text payload")
        if kind == 9 and (size != 8 or data[offset:offset + 4] != b"ORID"):
            raise DataError(f"resource {index}: invalid original-pack identity")
        if kind in (10, 11) and size != 8224:
            raise DataError(f"resource {index}: title must be RGB565 palette + 128x128 4bpp pixels")
        previous_end = aligned(offset + size)
        present += 1
    if flags & 4:
        if not count:
            raise DataError("scene map requires a resource directory")
        offset, size, _, kind, entry_flags = ENTRY.unpack_from(data, table + (count - 1) * ENTRY.size)
        if kind != 7 or not entry_flags & PRESENT or not size or size % 10:
            raise DataError("invalid scene map resource")
        scenes = set()
        for cursor in range(offset, offset + size, 10):
            scene, palette, code, shapes, shared = struct.unpack_from("<5H", data, cursor)
            if scene < 16000 or scene in scenes:
                raise DataError("invalid or duplicate scene ID")
            scenes.add(scene)
            for resource, expected in ((palette, (3,)), (code, (4,)), (shapes, (5,)), (shared, (5, 6))):
                if resource == shared == 0 and expected == (5, 6):
                    continue
                if resource >= count - 1:
                    raise DataError(f"scene {scene}: resource ID outside directory")
                _, resource_size, _, resource_type, resource_flags = ENTRY.unpack_from(data, table + resource * ENTRY.size)
                if not resource_size or not resource_flags & PRESENT or resource_type not in expected:
                    raise DataError(f"scene {scene}: incompatible resource type")
    return {"resources": count, "present": present, "bytes": total, "width": width, "height": height,
            "sha256": hashlib.sha256(data).hexdigest()}


def build(images: list[Path], output: Path, width: int | None = None,
          resource_dir: Path | None = None, banks_dir: Path | None = None) -> dict:
    if width is not None and (width < 16 or width > 320 or width % 8):
        raise DataError("--width must be a multiple of 8 between 16 and 320")
    files: dict[str, bytes] = {}
    for image in images:
        for name, data in OfsDisk(image).files().items():
            # Only game files are combined: volume-local icons may legitimately differ.
            if "/" in name or not (name == "another" or name.startswith("bank")):
                continue
            if name in files and files[name] != data:
                raise DataError(f"disks contain conflicting versions of {name}")
            files[name] = data
    if "another" not in files:
        raise DataError("game executable 'another' not found on the supplied disks")
    directory_offset, resources = find_directory(files["another"])
    if banks_dir:
        banks_dir.mkdir(parents=True, exist_ok=True)
        for name, data in files.items():
            (banks_dir / name).write_bytes(data)
    if resource_dir:
        resource_dir.mkdir(parents=True, exist_ok=True)
    payload_offset = aligned(HEADER.size + len(resources) * ENTRY.size)
    packed = bytearray(payload_offset)
    manifest = {"format": "ORW1", "version": 1, "source": "Amiga", "directory_offset": directory_offset,
                "images": [{"file": image.name, "sha256": hashlib.sha256(image.read_bytes()).hexdigest()} for image in images],
                "width": width or 320, "height": width * 5 // 8 if width else 200, "resources": []}
    for index, resource in enumerate(resources):
        bank = files.get(f"bank{resource.bank:02x}")
        item = {"id": index, "type": resource.kind, "bank": resource.bank,
                "bank_offset": resource.offset, "original_size": resource.size, "packed_size": resource.packed_size}
        if resource.size == 0:
            data = b""
            item["missing"] = "empty original entry"
        elif bank is None:
            # The supplied nologo disk edition deliberately omits these two logos.
            if index not in (18, 19) or resource.bank != 5 or resource.kind != 2:
                raise DataError(f"resource {index}: missing required bank{resource.bank:02x}")
            data = b""
            item["missing"] = "optional logo bank05 absent"
        else:
            if resource.offset + resource.packed_size > len(bank):
                raise DataError(f"resource {index}: bank range exceeds bank file")
            data = bank[resource.offset:resource.offset + resource.packed_size]
            if resource.packed_size != resource.size:
                try:
                    data = bytekiller_unpack(data, resource.size)
                except DataError as error:
                    raise DataError(f"resource {index}: {error}") from error
        entry_flags = PRESENT if data else 0
        if data and width is not None and resource.kind == 2:
            data = convert_bitmap(data, width)
            entry_flags |= PACKED_BITMAP
        offset = len(packed) if data else 0
        crc = zlib.crc32(data)
        ENTRY.pack_into(packed, HEADER.size + index * ENTRY.size, offset, len(data), crc, resource.kind, entry_flags)
        if data:
            packed.extend(data)
            packed.extend(bytes(aligned(len(packed)) - len(packed)))
            if resource_dir:
                (resource_dir / f"{index:03d}.bin").write_bytes(data)
        item.update(offset=offset, size=len(data), crc32=f"{crc:08x}", flags=entry_flags)
        manifest["resources"].append(item)
    dimensions = (width | (width * 5 // 8) << 16) if width else 0
    flags = 1 | (PACKED_BITMAP if width else 0)
    HEADER.pack_into(packed, 0, b"ORW1", 1, HEADER.size, len(resources), HEADER.size,
                     payload_offset, len(packed), flags, dimensions)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(packed)
    manifest["pack"] = verify_pack(output)
    output.with_suffix(output.suffix + ".json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("images", nargs="*", type=Path, help="privately supplied DOS0 Amiga ADF images")
    parser.add_argument("--output", type=Path, default=Path("private/sd/OTHERWRL.PAK"))
    parser.add_argument("--width", type=int, help="preconvert bitmap resources to packed 4bpp at this width (height = width*5/8)")
    parser.add_argument("--resources", type=Path, help="optionally save individual unpacked resources for debugging")
    parser.add_argument("--banks", type=Path, help="optionally save original bank files and executable")
    parser.add_argument("--verify", type=Path, help="verify an existing pack instead of building")
    args = parser.parse_args()
    try:
        if args.verify:
            print(json.dumps(verify_pack(args.verify), indent=2))
            return 0
        if not args.images:
            parser.error("provide the original disk images, or --verify PACK")
        manifest = build(args.images, args.output, args.width, args.resources, args.banks)
        print(f"Built {args.output}: {manifest['pack']['present']}/{manifest['pack']['resources']} resources, {manifest['pack']['bytes']:,} bytes")
        print(f"Recovered original resource directory at executable offset 0x{manifest['directory_offset']:x}")
        for item in manifest["resources"]:
            if item.get("missing") == "optional logo bank05 absent":
                print(f"Optional resource {item['id']} omitted: {item['missing']}")
        print(f"Verified SHA256 {manifest['pack']['sha256']}")
        return 0
    except (DataError, OSError, struct.error) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
