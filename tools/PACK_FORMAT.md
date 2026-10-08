# Private Amiga content packing

The builder extracts the user's disk files and finds the 146-entry resource
directory in their original Amiga executable. It validates OFS block checksums,
file links, resource ranges, ByteKiller checksums, and final resource CRC32s.
No game data or executable-derived resource table is stored in this source tree.
The two optional logo bitmaps (18 and 19) may be absent in a `nologo` disk edition;
other missing resources fail the build.

Run with Python 3.10 or later; no packages are required:

```powershell
python tools/build_pack.py AnotherWorld_DiskA_nologo_noprotec.adf AnotherWorld_DiskB_nologo_noprotec.adf --width 104 --output private/sd/OTHERWRL.PAK
python tools/build_pack.py --verify private/sd/OTHERWRL.PAK
python -m unittest discover -s tools -p "test_*.py" -v
```

Copy `private/sd/OTHERWRL.PAK` to the SD card at the location expected by the
firmware. `--width` must match the firmware's logical framebuffer width.
The initial hardware target is 104 × 65. The builder scales the original
bitmap resources to this size; vector shapes and bytecode remain unchanged.
All original resources, including sound/music data, are retained, but retaining
sound resources does not enable audio output in the engine.

For host debugging, omit `--width` to preserve original four-plane bitmap data:

```powershell
python tools/build_pack.py AnotherWorld_DiskA_nologo_noprotec.adf AnotherWorld_DiskB_nologo_noprotec.adf --output private/raw/OTHERWRL.PAK --resources private/resources --banks private/banks
```

The builder creates a JSON manifest beside each pack with original bank ranges,
unpacked sizes, per-resource CRC32s, and source/pack SHA256s. Generated packs,
banks, extracted resources, disk images, and screenshots belong in ignored
private directories and must not be distributed with engine source.

## ORW1 format

All container integers are little endian. Unmodified game resources retain
their original big-endian fields. The file header is 32 bytes:

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | char[4] | `ORW1` |
| 4 | uint16 | Version, currently 1 |
| 6 | uint16 | Header size, 32 |
| 8 | uint32 | Resource count, initially 146 for these Amiga disks; adaptations append resources |
| 12 | uint32 | Resource table offset, 32 |
| 16 | uint32 | First payload offset, aligned up to 512 bytes |
| 20 | uint32 | Total file size, including sector padding |
| 24 | uint32 | Flags: bit 0 = Amiga-compatible payload encoding; bit 1 = converted bitmaps; bit 2 = custom scene map |
| 28 | uint16 | Converted bitmap width, or zero for original planes |
| 30 | uint16 | Converted bitmap height, or zero for original planes |

The resource table has one 16-byte entry per resource ID, in ID order:

| Offset | Type | Meaning |
| --- | --- | --- |
| 0 | uint32 | Absolute payload offset, aligned to 512 bytes |
| 4 | uint32 | Unpadded payload size |
| 8 | uint32 | IEEE CRC32 of payload, compatible with Python `zlib.crc32` |
| 12 | uint16 | Original resource type |
| 14 | uint16 | Flags: bit 0 = present; bit 1 = converted packed bitmap |

Absent entries have offset, size, CRC, and flags zero but retain their original
type. Present payloads are ordered by resource ID. Each payload's trailing bytes
up to the next 512-byte boundary are zero. There is no runtime decompression.
Sector caches can directly request `entry.offset + resource_relative_offset`;
bytecode and vector resources can be larger than RAM.

| Resource type | Content |
| --- | --- |
| 0 | Sound sample |
| 1 | Music/tracker data |
| 2 | Full-screen bitmap |
| 3 | Palette data |
| 4 | Script bytecode |
| 5 | Part-specific vector shapes |
| 6 | Shared vector shapes |
| 7 | Custom scene map, when header bit 2 is set |
| 8 | TX1 text at native framebuffer coordinates |
| 9 | Stable source identity: `ORID` plus a little-endian uint32 |
| 10 | Default 128×128 title backdrop |
| 11 | Optional title unlocked by completing the game |

Title resources are each 8,224 bytes: sixteen little-endian RGB565 colors and
8,192 bytes of packed 4-bit 128×128 pixels, high nibble first. A completed game
loads type 11 if present, otherwise type 10. Both borrow the existing framebuffer
pages and add no persistent title buffer. `patch_title.py --unlocked` adds or
replaces the alternate while preserving the source identity and script IDs.

Custom games set header bit 2 and store a scene map in the **last resource**.
Each scene record is ten bytes: five little-endian uint16 values for scene ID,
palette resource ID, bytecode resource ID, shape resource ID, and optional shared
shape resource ID (zero means no separate shared shape resource). Scene IDs must
be at least 16000. This allows content to define new scenes without recompiling
firmware or using the original game's fixed part table. A purely vector game may
clear the converted-bitmap flag and set both dimensions to zero, making the pack
independent of the selected framebuffer resolution. Any included bitmap still
needs the converted flag and the target's exact dimensions.

Converted bitmaps are rows of packed 4-bit palette indices, left pixel in the
high nibble. Height is `width * 5 / 8`. Sampling uses integer nearest-neighbour
coordinates `source_x = x * 320 / width`, `source_y = y * 200 / height`.
Original Amiga bitmaps are four contiguous 8000-byte planes, plane 0 the low
color bit, 40 bytes per row, highest bit first.

Header flag bit 3 marks a handheld adaptation. TX1 resources have an eight-byte
header: `T`, `X`, version 1, flags, box width, box height, background palette
index, reserved zero. Flag bit 0 clears the box before drawing; other flags are
invalid. NUL-terminated ASCII follows; LF starts a new line. The font uses 3 × 5
pixels, a four-pixel horizontal advance, and six-pixel lines. The resource box
must fit in the framebuffer. The patcher preserves the original source-pack CRC32
in type 9 so revisions to text layout retain the same save identity.

Two six-byte bytecode extensions support adapted packs. Opcode `1B` takes a
big-endian uint16 text resource ID, native X, native Y, and palette color.
Opcode `1C` takes a one-byte system event and four reserved zero bytes: event 0
is a no-op, 1 requests the retry menu, 2 requests Continue/New Game, and 3 records
game completion without opening a menu. Events 1–3 suspend remaining tasks for
that tick. The portable Session handles
the request before another VM tick runs. Unmodified original opcodes retain
their original semantics.

## Part resource mapping

The four IDs are palette, script, part shapes, and optional shared shapes.
Zero in the last column means that part does not request a shared shape segment.

| Part | Palette | Script | Shapes | Shared |
| --- | --- | --- | --- | --- |
| 16000, original protection screen | 20 | 21 | 22 | 0 |
| 16001, intro | 23 | 24 | 25 | 0 |
| 16002, water | 26 | 27 | 28 | 17 |
| 16003, jail | 29 | 30 | 31 | 17 |
| 16004, city | 32 | 33 | 34 | 17 |
| 16005, arena | 35 | 36 | 37 | 0 |
| 16006, palace | 38 | 39 | 40 | 17 |
| 16007, final | 41 | 42 | 43 | 17 |
| 16008/16009, password | 125 | 126 | 127 | 0 |

Format research references:
[rawgl resource handling](https://github.com/cyxx/rawgl/blob/master/resource.cpp),
[rawgl unpacker](https://github.com/cyxx/rawgl/blob/master/unpack.cpp), and
[Amiga/DOS technical notes](https://github.com/cyxx/rawgl/blob/master/docs/Amiga_DOS.md).
