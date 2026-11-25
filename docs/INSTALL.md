# Build your personal cartridge

The public download contains the engine, patcher, and **First Contact**, a
playable setup guide. Another World is not included. The guide's actions are a
rehearsal; perform the purchase, conversion, and card copy on your computer.
Your original files stay on your computer. The generated personal `.chgame` and
`OTHERWRL.PAK` contain your game data and are for your personal use.

## Windows release tool

1. Extract the entire `Otherrealm-Patcher-Windows.zip` folder.
2. Open `Otherrealm-Patcher.exe`.
3. Select the Steam game's `Bonus/rom Amiga/` folder and a new, empty output folder.
4. Choose **Build my cartridge**. All five steps must complete successfully.

The builder writes `Otherrealm.chgame`, `Otherrealm.bin`, a native cover,
`sdcard/`, and validation reports. It never changes your input files.

### Original game files

Buy and install [Another World — 20th Anniversary Edition on Steam](https://store.steampowered.com/app/233550/Another_World__20th_Anniversary_Edition/)
(app 233550). If you own it already, install it from your Library. Choose
**Library → Another World → Manage → Browse local files** to open its installed
folder. Its **`Bonus/rom Amiga/`** folder contains both supported original disks:

```text
AnotherWorld_DiskA_nologo_noprotec.adf
AnotherWorld_DiskB_nologo_noprotec.adf
```

Choose `Bonus/rom Amiga/` in the patcher. This also works with older patcher
builds. Updated builds can find the disks from the Steam installation folder
itself. No extraction of the disks is needed.
Both disks must be present. The English Amiga pair included in the local Steam
installation has been verified through the complete cartridge conversion.
The patcher reads the disks, recovers the resource directory and strings, checks
the script revision, and leaves the original files unchanged. Unsupported
revisions stop without writing a partial cartridge. The anniversary resources
in `game/DAT`, `game/BGZ`, and `game/TXT` are not used by this conversion.

## Install with the CHGame cartridge tools

With the official CHGame desktop tools installed and a FAT16/FAT32 card mounted:

```powershell
chgame cart deploy .\Otherrealm-personal\Otherrealm.chgame --card E:\ --port COM8 --no-flash
```

Safely eject the card. Put it in CHGame and choose **OTHER REALM** in its menu;
the bootloader installs and starts it. A single-game import preserves the
existing card menu and other games. Use the correct drive and port for your PC.
The desktop `chgame` Python tools are installed separately from Boards Manager:

```powershell
git clone https://github.com/bateske/CHGame
python -m pip install ./CHGame
```

### Upload through the board package

The Boards Manager package includes `chgame-upload`, which flashes a firmware
image. It does not directly parse `.chgame` archives. The included helper validates
your cartridge, extracts its firmware, and calls that exact uploader:

1. Back up existing game files, then drag the **contents** of the generated
   `sdcard-rev0/` or `sdcard-rev1/` folder to the card root: **`OTHERWRL.PAK`** and
   **`GAMES/OTHERREA.CHG`**. Do not nest them inside an `sdcard/` folder. Safely
   eject the card. This replaces the guide's data and application files without
   replacing menu indexes. Use the cartridge tools above to add a menu entry to
   an existing multi-game collection.
2. Connect CHGame by USB and run from the extracted patcher folder:

```powershell
.\Upload-Cartridge.ps1 -Cartridge C:\Games\Otherrealm-personal\Otherrealm.chgame -Port COM8
```

The script finds the installed CHGame uploader beneath Arduino15. Set
`-ArduinoData` if you use a custom Arduino data directory. It uploads only the
game application; it does not replace your bootloader.

Alternatively, after copying the SD data, run the installed uploader directly:

```powershell
chgame-upload -port COM8 flash Otherrealm.bin -device rev0 -verify -run
```

`chgame-upload` may not be on PATH; the helper handles its Boards Manager location.
The SD card remains necessary after flashing because the game streams its data.

## Developer method

See [BUILDING.md](../BUILDING.md) for compiling with Arduino CLI, copying the SD
data directly, rebuilding the simulator, and testing the source patcher.

## Troubleshooting

- **Unsupported script revision:** keep your input files unchanged and report
  the edition and validation error. The patcher deliberately avoids guessing.
- **No supported Amiga files found:** finish the Steam download, then choose the
  Another World installation folder or `Bonus/rom Amiga/`. Confirm that both
  `.adf` disks are present; do not rename the anniversary edition's resource files.
- **Output folder is not empty:** choose a new folder; old builds are preserved.
- **Port busy:** close serial monitors and Web Serial connections, then retry.
- **Red screen / no game data:** put `OTHERWRL.PAK` in the card root, not inside
  an extra `sdcard` folder. Use FAT16/FAT32, not exFAT.
- **Another game was played:** CHGame currently shares a small flash save area.
  A different game's first save can replace this game's checkpoint.

## Board revisions

Use CHGame board package 0.4.1 or later. The public and personal cartridges
contain both `rev0` and `rev1` binaries. Select the device when deploying:
`--device rev0` or `--device rev1`. Rev 1.5 uses Rev 1.

For manual copying, use `sdcard-rev0/` or `sdcard-rev1/`; the legacy `sdcard/`
folder is Rev 0 only. With Upload-Cartridge.ps1, pass `-Device rev0` or
`-Device rev1`. Never upload a raw binary for the other revision.
