# Otherrealm

A working, SD-streamed vector game engine for the CHGame Rev0 handheld.
The same C++ interpreter runs on the 48 MHz CH32X035 and in the local browser
preview. Games supply polygon hierarchies, palettes, cooperative bytecode tasks,
and scene directories on SD; game rules are not compiled into the firmware.

The imported game's complete intro and movement tests have run on the connected
device. The DMA SD path measured 2.30 times faster than the initial polled path
with identical captured frames. [Hardware results](HARDWARE.md).

Two games are ready:

- **Signal Grove**, a small original adventure with three rooms, an animated
  character, jumping, a crystal puzzle, a locked gate, and an ending. Its entire
  game pack is 24,064 bytes, including its full-screen title backdrop. [Edit the game](../demo/README.md).
- **Another World compatibility**, using the privately supplied Amiga disks.
  Extraction, the complete intro, opening gameplay, and startup replays of all
  seven game parts are tested. This is not a claim of a complete playthrough.
  The disk images and extracted content are excluded from version control and
  from the redistributable demo package.

## Play

The device reads `/OTHERWRL.PAK` from a FAT16/FAT32 SD card. The adapted private pack is
`private/handheld/OTHERWRL.PAK`; the original demo is `demo/sd/OTHERWRL.PAK`. Copy the
desired one under that root filename. Keep backups when switching games.

| Button | Action |
|---|---|
| D-pad | Move; Up jumps or swims upward |
| A | Action; run/shoot in Another World; interact in Signal Grove |
| B | Up/jump shortcut; back in menus |
| Start | Tap to pause/resume; hold three seconds to return to the system launcher |
| Select | Open the system menu |

In Another World's opening pool, hold **Up + A** to swim out. The Start menu
offers **Resume**, **Retry Save**, **New Game**, and **Sound**. A thin bar at the
top shows progress while holding Start to exit. Use Up/Down and A to select. Every boot shows the title;
press any button to start or open Continue/New Game. New Game asks before
replacing a checkpoint, then begins the intro. Start during the intro offers
**Skip Intro**. B on Continue/New Game returns to the title. Centered menus
fade over the dimmed scene, with gentle sparkles sweeping back and forth across
the selected text, dust when changing selection, and a soft upward poof when
confirming a choice. Sound toggles keep the menu open. Checkpoint text stays still.
The full-screen shoreline title shows Lester standing after his climb and changes to his raised-hand greeting after
you complete the game; this unlock survives starting a new game.
Menu text uses native **8×13 Misc Fixed Bold** glyphs, with a native **5×8**
status font, drawn directly to the 128×128 screen without scaling. These
public-domain fonts come from PixelLogoLab. In-game text is part of the 104×65
scene and still passes through the 128×80 nearest-neighbour presenter; that
fractional scale can make individual letter strokes look uneven.

The engine autosaves the nineteen original Amiga password destinations, including
checkpoints within the city. Continue returns to the latest earned checkpoint;
Resume in the pause menu keeps your exact current position in RAM. Death and
password screens in the adapted pack lead to the menu. Signal Grove saves room,
inventory, gate, and victory milestones. [Save details](checkpoints.md).
The device journal uses two reserved flash pages and retains one installed game's
save; switching games can replace it. Browser saves are separate, per pack, in
local storage. Sound defaults on, with lightweight piezo cues and a menu toggle.
Original sampled effects/music are not mixed. [Title, menus and asset patching](TITLE_AND_MENU.md).

For the desktop preview, run a local server in this directory:

```powershell
python -m http.server 7788 --bind 127.0.0.1
```

Open [the preview](http://127.0.0.1:7788/web/), click **Play Signal Grove**, or
load the private `.PAK` through its file picker. Arrow keys move, Space/Z is
action, X jumps, Enter/P opens the menu, and Escape goes back. This is the actual portable C++ engine compiled
to WebAssembly. Game files are read locally and never uploaded.

## Build for the handheld

Use the CHGame Arduino board package 0.3.0 or compatible and Arduino CLI. The
build uses CHSd from a local checkout of the platform; CHGfx's framebuffer and
renderer are not linked. Set up the platform source if it is absent:

```powershell
git clone https://github.com/bateske/CHGame CHGame
git -C CHGame checkout 6f51ea690aff5c3ef893502dbdcbfbd170ab832c
./tools/build-device.ps1
./tools/upload-device.ps1 -Port COM8
```

`build/device/release/Otherrealm.ino.bin` is the application image. Uploading uses
the existing bootloader and its verify operation. It does not replace the
bootloader. The debug variant (`-DebugBuild` on both commands) adds serial input,
frame capture, and timing reports. See [device testing](../tools/device-test.py).

`python tools/package_game.py --check-fixtures` creates a `.chgame` cartridge and SD-card ZIP under
`build/dist/`, using the official CHGame format tooling. This package contains
**only the original Signal Grove assets**. Packaging additionally requires Pillow.
Re-run it after changing firmware. Import the cartridge into an existing game
collection to preserve its menu; the generated SD ZIP is a standalone card layout.
`python tools/package_source.py` creates `build/dist/Otherrealm-source.zip`
with the engine, patcher, tests, browser preview, and original demo, using an
explicit file allowlist that excludes the private game data.

## Build content

No external Python packages are needed to generate the game packs:

```powershell
python tools/build_demo.py
python tools/build_pack.py AnotherWorld_DiskA_nologo_noprotec.adf AnotherWorld_DiskB_nologo_noprotec.adf --width 104 --output private/sd/OTHERWRL.PAK
python tools/build_pack.py --verify private/sd/OTHERWRL.PAK
python tools/patch_text.py private/sd/OTHERWRL.PAK private/handheld/OTHERWRL.PAK --adf AnotherWorld_DiskA_nologo_noprotec.adf
```

The patcher extracts text from your own executable, lays it out with a native
3 × 5 font, adapts the two bitmap title/credit screens, and redirects password
and retry prompts to the engine menu. It writes a new pack and a detailed change
manifest without changing your input disks. The patcher source contains recipes,
not the original game assets. [Screen audit and patch format](TEXT_PATCHES.md).

The importer recovers the resource table directly from the supplied executable,
validates Amiga filesystem blocks and compressed resource checksums, decompresses
on the computer, converts the few bitmap resources, and aligns payloads to SD
sectors. Runtime bytecode and shapes remain on SD. There is no dependence on
preinstalled original game files beyond the input disks.

[The pack specification](../tools/PACK_FORMAT.md) describes the format. The optional
scene directory supports 16-bit scene/resource IDs, instead of the original
game's fixed ten-part table. Each script and shape segment has a 64 KiB address
space; large adventures use additional scenes. A FAT32 file is limited to under
4 GiB. SD capacity increases total content; it does not eliminate these per-scene
limits or SD seek costs.

## Engine design

| Component | Purpose |
|---|---|
| `engine/aw_vm.*` | 64 cooperative tasks, 256 signed variables, bounded bytecode execution |
| `engine/aw_render.*` | Integer polygon strips, shape hierarchies, zoom, page copy/scroll, palette changes |
| `engine/aw_pack.*` | Three shared 512-byte LRU cache slots; random resource access through the SD pack |
| `engine/aw_game.*` | Scene bindings, resource updates, display/timing callbacks |
| `engine/aw_music.*` | Silent Amiga music events needed by script synchronization |
| `engine/aw_session.*` | Pause/retry/continue menu and checkpoint save policy |
| `engine/aw_checkpoint.*` | Semantic checkpoint codec and scene restart |
| `firmware/Otherrealm/` | Direct ST7735 scanline/DMA presentation, buttons, FAT/SD adapter |
| `tools/build_demo.py` | Original content generator, label-resolving assembler, vector authoring primitives |

Four **104 × 65, 16-color** pages occupy 13,520 bytes. The display expands them
to **128 × 80**, centered on the 128 × 128 panel, preserving the original 8:5
aspect ratio. Pages stay in RAM, so normal animation never writes to the SD card.
The presenter needs two scanlines rather than another full display framebuffer.
All device runtime storage is statically allocated; no game-resource heap is
required. `OR_WIDTH` and `OR_CACHE_SLOTS` are build-time tuning parameters.

This design favors compact vector content. It also retains the original game's
occasional bitmap resources for compatibility; Signal Grove is entirely vector
drawn. The engine has no sound mixer, desktop SDL dependency, or
visual scene editor yet.

## Tests and profiling

Portable tests use Emscripten and Node; supply `-EmsdkRoot` when the SDK is not
at the sibling CH32EMU project's `.tools/emsdk` location:

```powershell
./tools/build-host.ps1 -EmsdkRoot D:/path/to/emsdk
./tools/test-vm.ps1 -EmsdkRoot D:/path/to/emsdk
node build/pack_tests.js
node build/render_tests.js
node build/demo_tests.js
node build/session_tests.js demo/sd/OTHERWRL.PAK private/sd/OTHERWRL.PAK
node build/checkpoint_tests.js demo/sd/OTHERWRL.PAK private/sd/OTHERWRL.PAK
node build/persistence_tests.js
node build/headless.js private/sd/OTHERWRL.PAK 3000 16001
python -m unittest discover -s tools -p test_build_pack.py
python -m unittest discover -s tools -p test_patch_text.py
```

The demo replay covers jumping, all room transitions, failure to open the gate
without the crystal, backtracking, collection, unlock, victory, and restart.
The cache tests include 10,000 mixed script/data reads and failure cases.
[Performance and compatibility evidence](PERFORMANCE.md) distinguishes
desktop sector counts from measured hardware timings. Device screenshots and
profiling output are kept under `build/device-test/`.

## Credits and license

The compatibility implementation follows Gregory Montoir's RAW/RawGL and Fabien
Sanglard's annotated interpreter. The engine is **GPL-2.0-or-later**; the original
Signal Grove content is **CC0-1.0**. CHSd and the display initialization adapted
from CHGfx retain their MIT notices. [Third-party details](../THIRD_PARTY.md).

Another World is not included in the redistributable package. Its original
content belongs to its respective rights holders and is loaded only from the
user's own local data.
