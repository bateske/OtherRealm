# Signal Grove

A small original vector adventure that runs entirely from the SD card. Three
rooms, the walking/jumping animation, collision limits, inventory, puzzle,
scene changes, and ending are bytecode and polygon data. The firmware contains
none of this game's rules. Sound uses lightweight piezo cues and can be toggled
from the pause menu.

Use Left/Right to move, Up or B to jump, and A to interact. Take the crystal in
the middle room, bring it to the gate in the third room, and press A near the
terminal. Walk through the open gate. Press A on the ending screen to restart.
Start or Select pauses the device. Hold Start for three seconds to return to
the CHGame launcher. New Game in the menu restarts the demo.

The generated `sd/OTHERWRL.PAK` is original redistributable content, separate
from the user's private Another World disk data. It contains no original game
code, imagery, fonts, strings, sounds, or music. Its vector assets have no fixed
output resolution; the same pack can run at the engine's supported widths.

Build with ordinary Python 3.10+:

```powershell
python tools/build_demo.py
```

Copy `demo/sd/OTHERWRL.PAK` to `/OTHERWRL.PAK` on the device SD card alongside the
Otherrealm application. Replacing this data file selects the game the engine
runs. Keep the private compatibility pack under a different name when switching.

Edit `signal-grove.json` to change colors and scenery. The sixteen colors are
RGB hex strings, quantized to the engine's 12-bit palette format. Geometry uses
a virtual 320 by 200 canvas:

| Object | Fields |
| --- | --- |
| `rect` | `color`, `box: [left, top, width, height]` |
| `strip` | `color`, ascending `rows: [[y, left, right], ...]` |
| `tree` | `position: [x, groundY]`, optional integer `size` |
| `text` | `color`, `position: [x, y]`, uppercase `text` |

The letters are original vector rectangle groups. A strip's horizontal extent
must fit in 255 virtual units; split wider shapes or use a parent group. Each
scene has a separate palette, script and vector resource, so its content is
streamed when needed. `tools/build_demo.py` contains the reusable `Assembler`
and `Shapes` primitives plus this game's `create_script` rules. The assembler
resolves labels, checks resource sizes and emits standard interpreter opcodes.
It needs no compiler packages, images or game disks.

The renderer bounds each top-level draw to 8,192 shape nodes and at most 8
levels of nested children. Zoom 64 is normal size; encoded zoom values up to
4,096 are supported. Split very large compositions across several draw
instructions. Invalid geometry and resource reads produce a reported fault.

The pack also demonstrates the optional ORW1 scene directory: scene IDs 16001,
16010 and 16011 map to resources on SD, exceeding the original game's fixed
part table. Split large adventures across additional scene records; each code
and vector segment stays within the interpreter's 64 KiB address space.

For autosave, this demo keeps its persistent variables in `00` through `0F`
and increments milestone variable `E0` at room changes, crystal pickup, gate
unlock and victory. Scene initialization respects the restored `0F` guard.
Walking and animation do not produce extra save writes. The semantic save
format and authoring contract are documented in `docs/checkpoints.md`.

`tests/demo_tests.cpp` replays the full game through player inputs, checking
jumping, scene changes, the locked gate, backtracking to collect the crystal,
unlocking, victory and restart. It writes diagnostic PPM frames under `build/`.

The authored JSON, procedural art and generated demo assets are dedicated to
the public domain under CC0-1.0. The Python authoring tools and engine remain
under the repository's GPL-2.0-or-later source license.
