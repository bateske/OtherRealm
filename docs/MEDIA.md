# Release artwork and recording

The cover is an actual render of Lester's raised-hand greeting, with the original
Other Realm wordmark overlaid at native resolution. The README banner uses the
shoreline scene after Lester has climbed out. Both backgrounds were rendered
locally from the owner's Amiga resources by this engine.

`cart.png` is exactly 128 × 128 and passes the CHGame cover-art palette check.
It leaves a dark one-pixel edge for the launcher border. The title is not scaled;
the background alone is quantized to fit the platform palette. The companion
`cart-menu-preview.png` shows the launcher border and installation progress area.

The patcher renders each buyer's backgrounds from their own game files and
constructs their cover during packaging. It does not use the promotional cover
as a source for gameplay or include a captured original scene in the patcher.

## Reproduce the assets

Build the title helper with `OR_WIDTH=208`, as in [BUILDING.md](../BUILDING.md),
and provide your own adapted pack and a 208 × 130 shoreline PPM:

```text
python tools/release_art.py private/handheld/OTHERWRL.PAK --output docs/cart.png --banner-source private/shoreline.bin.ppm
```

For the GIF, `tools/record_gameplay.cpp` runs the real C++ Session, VM, and renderer
at the same 104 × 65 resolution as the handheld. It records 128 × 128 presented
RGB565 frames and their durations, including the actual menu effects. It uses
scripted button input and a disposable in-memory save store, not the player's save.

```text
build/native/record_gameplay private/handheld/OTHERWRL.PAK build/gameplay.orgf
python tools/gameplay_gif.py build/gameplay.orgf --output docs/gameplay.gif
```

On Windows, add `.exe` as needed. The GIF selects shots from the title, intro,
opening pool, climbing out, movement, and menu. Cuts shorten the presentation;
shot timings remain engine timings. It is silent and enlarged 3× with nearest
neighbor sampling for the README. It is simulator capture, not footage of the
physical screen. `gameplay.json` records frame count, duration, and selected ranges.

## Rights and redistribution

These promotional images and footage depict *Another World*, created by Éric
Chahi, and retain the original game's rights. They are not GPL or CC0 assets for
reuse in other games. The Other Realm wordmark is original CC0 artwork. Playable
original data, disk images, raw recordings, and personal `.chgame` files are
excluded from public source and release packages.
