# Readable Amiga content on the handheld

`tools/patch_text.py` adapts a **privately generated** 104 × 65 Amiga pack. It
ships patch instructions, format validation, and an original small font, not
original game resources or dialogue. The original strings are extracted from
the user's `another` executable, preserving the wording of that edition.

The 104 × 65 game viewport is displayed on the 128 × 128 device. Story text uses
native 3 × 5 glyphs, four-pixel spacing and six-pixel lines. It is no longer
drawn at 320 × 200 and reduced until its strokes disappear. The firmware's
Continue/New Game and retry menus use the device UI; they replace password entry.

## Build and apply

Python 3.10 or later is sufficient; no extra Python packages are required.

```powershell
python tools/build_pack.py AnotherWorld_DiskA_nologo_noprotec.adf AnotherWorld_DiskB_nologo_noprotec.adf --width 104 --output private/sd/OTHERWRL.PAK --banks private/banks
python tools/patch_text.py private/sd/OTHERWRL.PAK private/handheld/OTHERWRL.PAK --executable private/banks/another
python tools/build_pack.py --verify private/handheld/OTHERWRL.PAK
```

Alternatively, pass `--adf AnotherWorld_DiskA_nologo_noprotec.adf` instead of
`--executable`. Install **`private/handheld/OTHERWRL.PAK`** as `OTHERWRL.PAK` on
the card. Keep `private/sd/OTHERWRL.PAK` as the original generated pack for
comparisons and future patcher revisions. The patcher requires firmware with
TX1 and system-event support; the original bytecode interpreter cannot execute
these extensions.

The current profile accepts the verified English Amiga script revision from
the supplied disks. It does **not** accept arbitrary Steam, DOS, anniversary,
French, or other platform/release files. It checks nine complete script SHA256s before editing.
Unknown revisions are rejected without modifying the destination. This avoids
silently applying scene-specific offsets to a different script edition.

Applying the patch to an already adapted, valid pack copies it byte for byte;
it never appends a second set of text resources. To use a newer patch profile,
run it against the original unpatched pack. Output is validated before an
atomic replacement of the destination. The adjacent `.PAK.patch.json` contains
private extracted strings, every original instruction address, replacement
layout, omission reason, bitmap edits, and source/output fingerprints. Keep that
manifest and all generated images private with the original assets.

## Complete text inventory

The audit walks every instruction boundary in all nine original script
resources, including unreachable legacy routines. It found **147 drawString
sites**. It also inspected all ten available bitmap resources and rendered the
intro at its original 320 × 200 resolution to identify lettering inside shapes.

| Scene / part | Code resource | Text sites | Adaptation |
| --- | ---: | ---: | --- |
| Title and protection / 16000 | 21 | 10 | Creator/music/port credits, error and symbol-selection instruction reflowed; title bitmap credits repaired |
| Laboratory intro / 16001 | 24 | 83 | Identification, two greeting captions, boot text, typed command, parameter editing, experiment prompt, theory/phases/results, practical confirmation, countdown and diagnostics |
| Water and beast / 16002 | 27 | 4 | Retry menu event; password text removed; one absent legacy string documented |
| Prison / 16003 | 30 | 3 | Retry menu event and password text removed |
| City / 16004 | 33 | 17 | Retry menu event and fifteen checkpoint-code branches removed |
| Arena / 16005 | 36 | 0 | No runtime text to alter |
| Palace / 16006 | 39 | 13 | Retry menu event; password removed; original numeric countdown converted |
| Ending / 16007 | 42 | 14 | Complete production credits, eight thanks names and final message reflowed; five absent legacy strings documented |
| Password / 16008 and 16009 | 126 | 3 | Whole password-entry script replaced with a Continue/New Game event and safe yield loop |

The original English executable contains 131 unique string IDs at a detected
table location. It includes PC-version credit strings even in this Amiga
edition. Six script references (`1FA`, `17C`–`180`) have no string in that
executable; they were already blank with the stock engine. These are explicitly
marked as absent legacy references rather than supplied with invented text.

All 102 nonempty, applicable runtime text sites become native text. Nine moving
parameter-cursor draws are suppressed after reflow; the actual `+` to `-`
parameter edit is preserved by redrawing the source parameter panel when its
original edit instruction runs. The typed command still appears character by
character. One empty original string remains a no-op. The remaining sites are
the four retry events, 22 access-code displays, three password-screen labels,
and six absent legacy references described above. No story sentence or credited
name is intentionally omitted.

| Bitmap resources | Finding and treatment |
| --- | --- |
| 71 | Delphine Software title and copyright. Redrawn with readable native lettering; present in this data but no script load was found |
| 83 | Another World logo and small author/music credits. Large logo preserved, both full credited names redrawn underneath |
| 67–70, 72–73, 144–145 | Landscapes and backgrounds, no readable natural-language text; unchanged |
| 18–19 | Optional logos absent from the supplied `nologo` edition; unavailable content cannot be inspected or repaired |

Code 21 at `00D9` originally draws shape 22:`1058` to erase part of the title's
baked French credits. That obsolete eraser is bypassed so it does not cut holes
in the newly laid-out credits. This was verified in actual title-script renders.

Intro shape resource 25 contains tiny `room`, `3`, `1`, and `LAB` polygons at
offsets `E6FA`, `E752`, `E772`, and `E77E`. Six call sites at `125C`, `15B3`,
`15B7`, `15C0`, `15C4`, and `15D2` now draw `ROOM 3`, `ROOM 1`, and `LAB` in the
native font. Four short appended trampolines replace the original four-byte
shape instructions; standalone room digits are merged into their preceding
label. Existing branch destinations are preserved. Window-close symbols and
the large keypad's diegetic numerals remain original vectors. The password
wheel's vector alphabet is replaced along with its entire screen. Room labels
are temporarily covered by dense full-width story panels, then restored with
the original background pages, just as the existing scene composition dictates.

## Script and save interfaces

The wire format is specified in [PACK_FORMAT.md](../tools/PACK_FORMAT.md).
The patch appends type-8 TX1 resources, adds header flag bit 3, and leaves sound,
music, palettes, gameplay shapes, and unrelated bitmaps untouched.

Original `12 id_hi id_lo x y color` instructions become six-byte
`1B resource_hi resource_lo native_x native_y color` instructions. All original
branch addresses remain valid. Native-text resources are deduplicated.

Opcode `1C event 00 00 00 00` is a six-byte system event. Event 0 is a no-op;
event 1 requests Retry/Continue; event 2 requests Continue/New Game. Events 1
and 2 stop remaining script tasks for that tick, allowing Session to show the
menu before more original password-screen code executes.

The four retry event addresses are resource 27:`00DA`, 30:`00DC`, 33:`01C6`, and
39:`0376`. Password resource 126 starts with event 2 followed by a yield loop.
Checkpoint recovery comes from original scene state rather than the displayed
password string. The original password script names 19 destinations: water
(selector 10), prison (20), city (30, 35, 37, 33, 31, 39, 41, 42, 43, 49, 44,
45, 46, 47, 48), arena (50), and palace (60). The Session module owns checkpoint
selection, validation, saving, and the menu; the text patcher does not fabricate
new recovery locations.

A type-9 identity payload contains `ORID` followed by the original unpatched
pack's CRC32. Text-layout revisions rebuilt from that same original pack retain
the same save identity. This is an identifier, not a cryptographic integrity
claim; pack resources also have CRC32s and manifests include SHA256s.

## Validation and limits

```powershell
python -m unittest discover -s tests -p test_patch_text.py -v
python tools/test_build_pack.py
python -m unittest discover -s tools -p "test_*.py" -v
```

Twelve patcher tests and twenty existing packer tests pass. Tests cover string
table alignment and false candidates, every shape-operand encoding, branch
boundaries, native text bounds, resource validation, duplicate identities,
idempotence, and (when the user's private assets exist) all 147 real text sites,
seven vector edits, unchanged original destinations, and unrelated resources.

An actual headless replay ran 2,700 ticks of the adapted intro into water, with
the same transition at tick **2636** and the same 153,340 ms script time as the
original pack. Sector reads increased from 14,545 to 15,741 because text now
streams from SD; the maximum remained 84 sectors in one tick. These are host
logical I/O counts, not measured device frame rates. The adapted pack is
1,439,232 bytes, containing 85 text resources and one identity resource.

Before/after renders inspect the identification and greeting screens, boot and
typed command, parameter changes, theory/phases/results, countdown/diagnostics,
title cards and all ending credit pages. For title and credits, private QA
fixtures enter the existing routines directly; they are not shipped packs and
do not demonstrate a complete game playthrough. Generated comparison sheets are
under `private/text-audit/comparison-1.png` through `comparison-3.png` locally.

The original palette fades, animation timing, scrolling credit timing and
music synchronization remain in charge. Dense text covers more of the terminal
background so all original wording can fit. The original engine's letter-spaced
decorative headings are normalized, punctuation-only dot leaders are removed,
and line breaks are recomposed. This profile is intentionally specific to the
104 × 65 viewport, not a general translation system.

The approach was informed by
[Gil Megidish's Hebrew translation project](https://github.com/gmegidish/another-world-hebrew),
which patches animation data to draw localized lettering. This patcher uses
streamed native text instead of expanding each character into many individual
pixel-shaped script calls. No source or translated artwork was copied from
that project.
