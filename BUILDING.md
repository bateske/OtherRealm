# Building Other Realm — developer method

The Windows release tool is the shortest route to a personal cartridge. This
guide is for rebuilding firmware, editing the engine, or using the patcher source.
Commands below run from the repository root in PowerShell unless noted otherwise.

## 1. Install the board and tools

Install Git, Python 3.10 or newer, and Arduino CLI. Add the official CHGame board
index, then install the CHGame core:

```powershell
arduino-cli config add board_manager.additional_urls https://github.com/bateske/CHGame/releases/latest/download/package_chgame_index.json
arduino-cli core update-index
arduino-cli core install CHGame:ch32v
git clone https://github.com/bateske/CHGame CHGame
git -C CHGame checkout 6f51ea690aff5c3ef893502dbdcbfbd170ab832c
```

The release was built with board package 0.3.0 and the platform source revision
above. The checkout provides CHSd and the official cartridge-format tools.
Its source and notices remain available from that revision. Pillow is required
by the source packager; a C++ compiler builds the title renderer:

```powershell
python -m pip install pillow ziglang
```

## 2. Compile the Arduino application

```powershell
./tools/build-device.ps1
```

The script selects `CHGame:ch32v:rev0`, size optimization/LTO, the nano runtime,
game peripherals, and upload-only USB. It sets the engine include directory and
links CHSd. The output is `build/device/release/Otherrealm.ino.bin`.
The corresponding `.elf` and `.map` are available in the same build directory.

Use `-DebugBuild` to add serial frame capture and profiling. The production build
keeps that diagnostic interface out to leave more RAM for gameplay.

## 3. Build a game pack and cartridge

The current commercial-game profile accepts the tested English Amiga ADF pair.
See [supported inputs](docs/INSTALL.md#original-game-files) before choosing files.
Build the native title renderer, then run the one-step patcher:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
$core = Get-ChildItem engine/aw_*.cpp | ForEach-Object FullName
python -m ziglang c++ @core tools/capture_title.cpp -DOR_WIDTH=208 -std=c++11 -O2 -static -o build/capture_title.exe
python tools/patch_game.py --adf C:/Games/Original/DiskA.adf C:/Games/Original/DiskB.adf --output private/my-game
```

`--source C:/Games/Original` can discover both ADFs in a folder instead. Use
`--firmware` or `--renderer` to select other built binaries. Inputs stay unchanged;
the output must be a new or empty folder. A full cartridge only appears after all
validation, text adaptation, native title capture, and package checks pass.

For the freely distributable original game:

```powershell
python tools/build_demo.py
python tools/package_game.py --check-fixtures
```

This produces Signal Grove under `build/dist/` and never uses the private game.
Only this original demo cartridge may be included in the public release.

## 4. Copy the SD files directly and upload with Arduino's tools

Copy the **contents** of `private/my-game/sdcard/` to a FAT16/FAT32 card root.
The resulting paths must be:

```text
OTHERWRL.PAK
GAMES/OTHERREA.CHG
```

Preserve other files on the card. Safely eject it, insert it in CHGame, and either
choose OTHER REALM in the launcher or upload the compiled Arduino application:

```powershell
./tools/upload-device.ps1 -Port COM8
```

Replace COM8 with your port. This invokes the board package's `chgame-upload`
bootloader uploader with verification; it does not replace the bootloader. Use
`-DebugBuild` if uploading the diagnostic build. For Signal Grove, use its
generated SD layout or put `demo/sd/OTHERWRL.PAK` at the card root.

You can also use the [cartridge install helper](tools/Upload-Cartridge.ps1) with
`-Python` pointing to your interpreter. The game's resources still need the SD
card after a USB upload. The firmware alone cannot supply them.

## Simulator and tests

With Emscripten and Node installed:

```powershell
./tools/build-host.ps1 -EmsdkRoot D:/path/to/emsdk
./tools/test-vm.ps1 -EmsdkRoot D:/path/to/emsdk
python -m http.server 7788 --bind 127.0.0.1
```

Open `http://127.0.0.1:7788/web/`. Choose Signal Grove or open a local pack.
Arrow keys move, Space/Z acts, X jumps, Enter/P opens the menu, and Escape goes
back. Files stay local to the browser; device and browser saves are separate.

For native tests, use CMake and a C++11 compiler on Linux, macOS, or Windows:

```text
cmake -S . -B build/native
cmake --build build/native --parallel
ctest --test-dir build/native --output-on-failure
python -m unittest discover -s tools -p "test_*.py"
python -m unittest discover -s tests -p "test_*.py"
```

The public CI uses the original demo. Tests which exercise proprietary data need
your own private pack; none is downloaded or bundled by CI.

## Build the standalone Windows patcher

After building release firmware and checking out CHGame:

```powershell
python -m pip install -r tools/requirements-release.txt
./tools/build-release.ps1 -Python python
```

The complete `build/release/Otherrealm-Patcher/` folder is the distributable app;
keep its `_internal` folder beside the EXE. It contains the native renderer,
firmware, Python/Tk runtime, image library, and license notices, with no commercial
game data. `--source`, `--adf`, and `--extract-cart` also work as command-line
options on the EXE; errors are best viewed through the GUI or source CLI.

`python tools/package_source.py` creates the source ZIP from an explicit allowlist.
It includes the generated browser runtime when present. A plain Git checkout needs
the host build before the browser preview will run.

See [media reproduction](docs/MEDIA.md) for the cover and gameplay recording.
