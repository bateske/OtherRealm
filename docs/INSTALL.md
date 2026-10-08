# Build your personal cartridge

The public download contains the engine and patcher. Your original Another World
files stay on your computer. The generated `.chgame` and `OTHERWRL.PAK` contain
your game data and are for your personal use.

## Windows release tool

1. Extract the entire `Otherrealm-Patcher-Windows.zip` folder.
2. Open `Otherrealm-Patcher.exe`.
3. Select your original game folder and a new, empty output folder.
4. Choose **Build my cartridge**. All five steps must complete successfully.

The builder writes `Otherrealm.chgame`, `Otherrealm.bin`, a native cover,
`sdcard/`, and validation reports. It never changes your input files.

### Original game files

The validated profile currently accepts the English Amiga game, with both ADF
disks in one folder. It recovers the resource directory and strings from the
executable, checks the script revision, and rejects an unsupported revision
without writing a partial cartridge.

The [Steam 20th Anniversary edition](https://store.steampowered.com/app/233550/Another_World__20th_Anniversary_Edition/)
uses a different layout. Its installation is available through Steam Library →
Another World → Manage → Browse local files. The `game/DAT`, `game/BGZ` and
`game/TXT` directories belong to that edition; they are not Amiga disk images.
Steam import is pending validation against a local installation. Buying the
Steam edition alone does not currently supply the ADF input this profile needs.

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

1. Copy the **contents** of the generated `sdcard/` folder to the card root and
   safely eject it. This adds two files and does not replace menu indexes.
2. Connect CHGame by USB and run from the extracted patcher folder:

```powershell
.\Upload-Cartridge.ps1 -Cartridge C:\Games\Otherrealm-personal\Otherrealm.chgame -Port COM8
```

The script finds the installed CHGame uploader beneath Arduino15. Set
`-ArduinoData` if you use a custom Arduino data directory. It uploads only the
game application; it does not replace your bootloader.

Alternatively, after copying the SD data, run the installed uploader directly:

```powershell
chgame-upload -port COM8 -device rev0 flash Otherrealm.bin -verify -run
```

`chgame-upload` may not be on PATH; the helper handles its Boards Manager location.
The SD card remains necessary after flashing because the game streams its data.

## Developer method

See [BUILDING.md](../BUILDING.md) for compiling with Arduino CLI, copying the SD
data directly, rebuilding the simulator, and testing the source patcher.

## Troubleshooting

- **Unsupported script revision:** keep your input files unchanged and report
  the edition and validation error. The patcher deliberately avoids guessing.
- **Output folder is not empty:** choose a new folder; old builds are preserved.
- **Port busy:** close serial monitors and Web Serial connections, then retry.
- **Red screen / no game data:** put `OTHERWRL.PAK` in the card root, not inside
  an extra `sdcard` folder. Use FAT16/FAT32, not exFAT.
- **Another game was played:** CHGame currently shares a small flash save area.
  A different game's first save can replace this game's checkpoint.
