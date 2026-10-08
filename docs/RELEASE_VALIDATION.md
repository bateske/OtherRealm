# 0.5.0 preview release validation

Validated on Windows and the connected CHGame Rev0 on 2026-10-08.

## Personal cartridge workflow

The source CLI and the frozen Windows application independently converted the
owner's English Amiga ADF pair. Both produced the exact currently installed
game pack: SHA-256
`6a7875fd37a2e20bc4e805f84373ac120d3355467eea3246abe230897fa677d7`.

The complete workflow validates and extracts the disks, patches handheld text
and menu entry points, captures both title scenes with the native C++ engine,
composes a CHGame-valid cover, and packages firmware and SD data. Cartridge ZIP
round trips, the official bootloader-format parser, and firmware identity pass.
The frozen application was also exercised with paths containing spaces.

The final `Upload-Cartridge.ps1` was tested against the uploader actually installed
by the board package. It extracted the personal cartridge, flashed the production
application, and launched it on COM8. The bootloader checked the image CRC in
flash (`73550E05`); it does not offer independent byte-by-byte readback.

Only the existing game's SD launcher entry was replaced to install the cover.
The game pack matched before and after; other card files were preserved. The
512-byte save journal matched its fresh backup byte for byte, with zero save
writes and no VM fault in the diagnostic check. The production build was left
running after validation.

## Automated checks

- 39 Python tool tests, including corrupt cartridge rejection, round-trip
  extraction, unchanged source files, existing-output protection, and cleanup
  after failed conversion.
- 15 text/title patch tests using synthetic public fixtures.
- Linux GitHub CI builds the native engine and capture helpers, then runs seven
  native CTest suites and the public Python suites.
- The original Signal Grove cartridge passes CHGame's conformance fixtures,
  cartridge/card round trips, and bootloader parser checks.
- Release ZIP CRC checks and an explicit public-package audit reject disk images,
  private paths, raw recordings, and any game pack other than the original demo.
- Local documentation links were checked. Native cover art was validated with
  the official CHGame palette rules and inspected with the launcher overlay.

Firmware is unchanged from the tested 0.4.2 device build: 33,808 bytes, with
17,980 bytes of static RAM. This release adds distribution tools, artwork, media,
documentation, and packaging tests. See [hardware history](HARDWARE.md) for
runtime profiling and [media provenance](MEDIA.md) for the gameplay GIF.

## Explicit release boundary

This is an **Amiga-compatible preview**. A local Steam 20th Anniversary install
was not available for conversion testing. Its DAT/BGZ/TXT layout is documented,
but this release does not claim to import it. The full original game has not
been played through end to end; tested coverage is detailed in
[PERFORMANCE.md](PERFORMANCE.md). No playable commercial game data is distributed.
