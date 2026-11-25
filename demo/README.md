# First Contact — a playable setup guide

**This is the guide bundled with Other Realm. Another World is not included.**
Walk through five physical stations to learn how to install your own game on
CHGame: an arrival beacon, a computer terminal, a disk archive, a conversion
workshop, and an SD-card dock. Abstract actions are paired with explicit
instructions for your real computer. The ending reminds you that playing the
guide has not purchased, downloaded, converted, or installed any files.

Left/Right moves, Up or B jumps, and A reads a station or advances to its next
page. Read every page at a station to open the path to the next room. Walk back
to reread completed stations. The introduction opens automatically on a new
game. Start or Select pauses; hold Start for three seconds to return to the
CHGame launcher. Sound can be toggled in the pause menu.

The real steps taught by the guide are:

1. Buy and install **Another World — 20th Anniversary Edition** from
   [Steam](https://store.steampowered.com/app/233550/Another_World__20th_Anniversary_Edition/)
   (app 233550), or install your existing purchase.
2. In Steam, choose **Library → Another World → Manage → Browse local files**.
   The original disks are in **`Bonus/rom Amiga/`**:
   `AnotherWorld_DiskA_nologo_noprotec.adf` and
   `AnotherWorld_DiskB_nologo_noprotec.adf`.
3. Extract the Windows patcher from the project's
   [releases](https://github.com/bateske/OtherRealm/releases). Open
   `Otherrealm-Patcher.exe`, select the Steam installation folder or its
   `Bonus/rom Amiga/` folder and a new, empty output folder, then choose
   **Build my cartridge**. Wait for all five steps to succeed.
4. Back up existing game files. Drag the **contents** of the generated `sdcard/`
   folder to a FAT16/FAT32 card's root: **`OTHERWRL.PAK`** and
   **`GAMES/OTHERREA.CHG`**. Do not put them inside an extra `sdcard/` folder.
   For an existing multi-game collection, import the generated personal
   `Otherrealm.chgame` with the CHGame tools to preserve its menu.
5. Safely eject the card, insert it into CHGame, and select **OTHER REALM**.
   The new pack replaces this guide with Another World.

See [installation details](../docs/INSTALL.md) for cartridge tools and upload
options. The patcher reads the original disks without changing them. Steam's
anniversary `game/DAT`, `game/BGZ`, and `game/TXT` files are not its input.

## Build and author

Walking, stations, gates, text cards, and the ending are script and resource
data streamed from SD. The guide requires no engine or firmware changes.
Build with ordinary Python 3.10+:

```powershell
python tools/build_demo.py
```

The output is `demo/sd/OTHERWRL.PAK`. Copy it to `/OTHERWRL.PAK` on the device SD
card alongside the Otherrealm application. Back up your personal pack first if
switching from Another World. `python tools/package_game.py --check-fixtures`
packages the guide as `build/dist/Otherrealm.chgame`.

Edit `first-contact.json` for room geometry and the sixteen RGB palette colors.
Edit the neighboring `guide-text.json` for narration and station pages.
`tools/build_guide.py` compiles those pages to native TX text resources using
the 3×5 font, with at most 25 columns and six lines per card. The guide builds
its own original title backdrop; `demo/title.bin` remains the older Signal
Grove fixture.

Geometry uses a virtual 320×200 canvas. The original `rect`, `strip`, `tree`,
and vector `text` primitives are defined by `tools/build_demo.py`. A strip's
horizontal extent must fit in 255 virtual units; split wider shapes or use a
parent group. Each scene has separate palette, script, and shape resources.
`Assembler` resolves labels and emits standard interpreter opcodes. The ORW1
directory connects the five scenes; each code and shape segment stays within
the 64 KiB address space.

The renderer bounds each top-level draw to 8,192 shape nodes and eight nesting
levels. Zoom 64 is normal size; values up to 4,096 are supported. Split large
compositions across draw instructions. Invalid geometry or resource reads
produce a reported fault.

Persistent state is kept in variables `00` through `0F`; milestone `E0` changes
when progress changes. Continue or Retry Save restores completed stations.
Walking and animation do not continually write saves. See the
[authored-pack checkpoint contract](../docs/checkpoints.md).

The older three-room **Signal Grove** crystal adventure remains an optional
authoring and regression example:

```powershell
python tools/build_demo.py --source demo/signal-grove.json --output build/signal-grove/OTHERWRL.PAK
```

The authored guide and Signal Grove JSON, prose, procedural artwork, and
generated content are dedicated to CC0-1.0. The engine and authoring tools
retain their source licenses. No Another World game data is included.
