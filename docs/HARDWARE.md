# CHGame hardware validation

Measured on the connected CHGame Rev0 on 2026-10-08. The initial measurements
below used silent firmware. Version 0.3 adds piezo cues; its results are recorded
at the end of this document.

## What ran on the board

- The user's imported Amiga data pack, copied to `OTHERWRL.PAK` on the FAT32
  microSD card and verified by SHA-256 after copying.
- 60 ticks of the first gameplay scene with UP held, then a separate 120-tick
  movement/action test. Both produced framebuffer captures with no VM fault.
- The complete introduction twice: 2,640 VM ticks and 2,644 presented frames,
  automatically entering playable part 16002 with no fault. The second run
  used the final renderer stack fix.
- Two later chapters: 120 ticks each in parts 16003 and 16004, both with no
  fault and a measured stack high-water mark of 928 bytes.
- Both the polled SD reader and the faster DMA reader, on identical input.
  Their captured gameplay frame PNGs have the same SHA-256:
  `85f5b0107d80ff33c6b36a41a4fddf6ed3b8bcd74db0d646322c54ba0f026cc7`.

Local evidence lives in `build/device-test/`: `polled`, `dma`,
`baseline-run`, `full-intro-dma`, `final-intro-bright`, `final-full-intro`,
`final-jail`, and `final-city`, each containing a PNG and JSON report.
Those private gameplay screenshots are excluded from source distribution.

## Measured performance

The comparison starts part 16002 and runs 60 VM ticks with UP held. Both
runs present 62 frames and read 834 sectors. Display is 104x65 packed 4bpp,
scaled to a 128x80 window centered on the 128x128 display.

| Measurement | Polled CMD17, 12 MHz | DMA CMD18, 24 MHz |
|---|---:|---:|
| Total SD time | 1,305,472 us | 567,557 us |
| Mean cache-fill time | 1,565 us | 681 us |
| Total display time | 444,005 us | 443,194 us |
| Worst VM tick | 215,803 us | 155,699 us |
| Final paced tick | 79,994 us | 79,995 us |

DMA reduces SD time by 56.5%, a 2.30x improvement. The panel transfer averages
7.15 ms. The original script requests an 80 ms period in this scene; the
engine preserves that timing. Some heavy scene transitions still exceed it.
These are measurements for this card and content, not a guaranteed frame rate
for arbitrary packs.

The full intro using DMA recorded 13,771 sector reads, 8.86 seconds in SD I/O,
18.90 seconds presenting frames, and 108.59 seconds waiting for script pacing.
Its maximum tick took 180.6 ms. With the final stack fix, the complete intro
uses at most 928 of 2,048 reserved stack bytes, leaving 1,120 bytes unused.
The previous build had reached 1,744 bytes. Polygon scratch space is now in
a separate non-inlined function, and shape nesting is capped at eight.

| Later chapter, 120 ticks | Part 16003 | Part 16004 |
|---|---:|---:|
| Displayed frames | 122 | 122 |
| SD sectors | 1,191 | 4,782 |
| SD time | 906,087 us | 3,621,193 us |
| Display time | 872,150 us | 872,138 us |
| Pacing waits | 14,367,561 us | 2,376,010 us |
| Worst tick | 219,994 us | 175,906 us |
| Final tick | 79,992 us | 79,989 us |
| Peak stack | 928 B | 928 B |

The chapter scripts request differing periods throughout their sequences;
the final tick in each requested 80 ms. These are startup slices of those
chapters, not complete playthroughs.

## Memory and transport

The renderer keeps all four logical pages in RAM: 13,520 bytes total.
Scripts and shape resources stay on SD behind three shared LRU sectors
(1,536 bytes). The custom ST7735 driver uses two 256-byte scanlines and no
separate display framebuffer. Its idle scanlines also supply FAT setup and SD
streaming scratch space. Sixteen FAT extents support fragmented pack files.
No heap allocations are used by the engine.

The card and panel share SPI1. Every transfer is synchronous at their boundary;
the panel's DMA and the SD reader never own the bus simultaneously. Inside
presentation, scanline conversion overlaps the previous row's DMA transfer.
SD DMA writes directly into a cache sector and returns the SPI configuration
to the panel when done.

Initial compatible release: **21,220 bytes of flash image**, **17,536 bytes of static RAM**.
The Arduino summary reports 20,960 bytes of code; the raw image additionally
contains 260 initialized RAM bytes. Static RAM is 880 bytes below its 18,416 B
cap, separate from the reserved 2,048-byte stack. The profiling build uses
24,464 bytes of flash image and 17,988 bytes of static RAM.

Disassembly confirms each recursive shape call uses 80 bytes, while the
polygon rasterizer's 352-byte frame is incurred only at a leaf. At the allowed
depth, those renderer frames occupy at most 1,072 bytes before their callers
and interrupts. The full-intro and later-chapter stack measurements above are
the observed workload checks; arbitrary authored packs still need profiling.

## Reproduce

1. Build with `tools/build-device.ps1 -DebugBuild`. DMA is the default;
   `-PolledSd` builds the comparison reader.
2. Use `tools/upload-device.ps1 -DebugBuild -Port COM8`. The port can change
   after an SD reader upload; inspect the connected device before using it.
3. Install `pyserial` and `Pillow` in the Python environment used for tests.
4. Run `python tools/device-test.py --port COM8 --part 16002 --ticks 60
   --buttons 4 --out build/device-test/recheck`.
5. Build and upload the release afterward, omitting `-DebugBuild`.

The profile command reports real elapsed microseconds, SD reads, rendering
time, pacing waits, instruction count, and stack high-water mark. The snapshot
comes from the engine's framebuffer through USB; it is not a photograph of
the panel. The device-test tool releases injected input and lockstep after a
successful capture.

The workspace's CHSDtoUSB helper has its sounds disabled. It was used to copy
the pack, followed by a clean volume eject before uploading Otherrealm.
The SD card was not formatted and its other games and files were preserved.

## Initial release state (superseded below)

The initial silent release was installed and run on the connected handheld.
The bootloader accepted image CRC32 `665C36A2`. Bootloader v3 cannot read the
flash back through this protocol; its END command verifies the programmed CRC.
`GAMES/OTHERREA.CHG` was added for relaunching from the SD menu. Existing menu
indexes, games, and data were preserved; the menu also discovers unindexed CHG
files. The private `OTHERWRL.PAK` remains on the card, unchanged after profiling.

- Private pack SHA-256:
  `041be2c0f2c314ac45956f54c072e2042833aee4c96dd9f1d942111f3c7a2630`
- Installed menu package SHA-256:
  `131787f408d864ce0e0c144f1c833b4f175afb5acc64371272f6785b1f4c57de`

This initial release used START to skip/restart and SELECT to pause. The menu
revision below replaces these controls.

## Menu, checkpoint, and handheld-text revision

The later native menu font update is recorded at the end of this document.

START and SELECT now open the full 128x128 system menu. RESUME retains the
exact paused game state; RETRY SAVE reinitializes the last semantic checkpoint.
NEW GAME asks before replacing a checkpoint, WATCH INTRO preserves the save,
and EXIT returns to the CHGame platform menu. A selects; B returns. A fresh
launch with a save opens CONTINUE. Gameplay movement and action controls are
unchanged. Menu selection buttons are consumed until released, so selecting
CONTINUE cannot also trigger an action in the resumed game.

The menu streams through the same two scanlines as the game display and does
not modify any of the four logical drawing pages. Paused-page captures were
byte-identical before and after opening the menu; the frame count stayed fixed.
The letterbox is cleared once when leaving the full-screen menu.

Autosaves use CHGame's two shared 256-byte flash pages at 0xF500 and 0xF600.
Each contains a pack identity, sequence, payload length, 44-byte semantic
checkpoint, CRC32, and commit marker. The inactive page is written and verified
before it becomes newest. Unchanged checkpoints cause no write. Save programming
uses the idle display scratch buffer and a 238-byte SRAM function, with
interrupts masked until the flash controller finishes; disassembly confirms it
calls no flash code while programming. There are no SD writes during gameplay.

Before the first save write, both prior shared pages were backed up to local
`private/prior-save-pages.bin` (512 bytes, SHA-256
`c0245c1d03079ad30d89739edd7920651a97c38a01d63777fc0fd9bba0dc68ba`).
This remains private, excluded from the source and cartridge packages.

Host journal tests passed every incomplete write length (0 through 255 bytes)
and corruption at every byte position, recovering the preceding valid record.
They also cover pack identity, record length, duplicate suppression, and the
maximum 224-byte hardware payload. On the handheld, advancing to part 16003
and back to 16002 exercised both actual flash pages: both retained valid CRCs,
with sequence 3 correctly superseding sequence 2. Retry did not write again.
After uploading and launching a fresh debug image, CONTINUE restored the
saved opening checkpoint with zero flash writes in that new application run.

Menu/save evidence is in `build/device-test/menu-original`,
`menu-original-relaunch`, `menu-journal-rotation`, `menu-adapted`, and
`menu-adapted-relaunch`. The final held-button-fix build also passed a fresh
adapted-pack launch: CONTINUE and retry restored part 16002 with zero writes.
`tools/device-menu-test.py` reproduces pause, repeated START edges, retry,
confirmation/cancel, journal rotation, and CRC checks; run it again after a
fresh application launch with `--resume` to check persistent CONTINUE.

The new release is **26,336 bytes of flash image**, **17,844 bytes of static
RAM**, leaving 572 bytes before the static limit plus the separate 2,048-byte
stack reservation. The final debug image is 29,912 bytes, with 18,288 bytes of
static RAM and 128 bytes before that limit. The compiler's small-memory notice
refers to this static margin; the linker reserves the stack separately.

The original unpatched pack remains at local `private/sd/OTHERWRL.PAK`.
The adapted pack was copied from `private/handheld/OTHERWRL.PAK` to the card's
`OTHERWRL.PAK`, then hash-verified before clean eject. It is 1,439,232 bytes,
SHA-256 `860c1368d5e76099cc9f283c57e80f105d2a4623f301642f4b13df2f83f52798`.

The complete adapted introduction passed on the board: 2,640 VM ticks and
2,644 displayed frames, automatically entering part 16002 with a valid
checkpoint and no VM or save fault. It read 14,968 sectors (9.947 seconds of
SD time), spent 19.094 seconds presenting and 105.001 seconds in pacing waits.
The worst tick took 190.214 ms. Peak stack use was 1,008 of 2,048 bytes,
leaving 1,040 bytes unused. This run was paused at selected text screens for
USB framebuffer captures; it is not an uninterrupted wall-clock benchmark.

Actual framebuffer captures at intro ticks 1150, 1450, 1600, 1775, and 1875
show the identification, command, parameter, result, and countdown text at
native handheld resolution. Evidence is in `build/device-test/text-<tick>`
and `menu-text-full-intro`. The captures are engine framebuffer readbacks,
not panel photographs. The imported private screens stay out of the release
and source packages.

The final silent release is installed and launched on the connected handheld.
The bootloader accepted image CRC32 `50BD964B`; its protocol verifies the
programmed CRC but cannot read the image back. `GAMES/OTHERREA.CHG` was updated
to match this release, after checking that its existing hash was our previously
installed package. The adapted private pack was preserved and reverified;
other games, files, and menu indexes were not changed. The opening checkpoint
is available through CONTINUE.

- Release image SHA-256:
  `56bfbeda80249c045ea79034447047ba7556346130394a08251ebbd1702bde10`
- Installed SD menu cartridge SHA-256:
  `4d73ef79238944358b93bee47c9c18f428808fc1dca6f94287b75cd8ccf6c55f`

## Native menu fonts (0.2.1)

The system menu now draws public-domain Misc Fixed **8×13 Bold** and **5×8
Regular** glyphs from PixelLogoLab directly at display resolution. Every font
pixel maps to exactly one panel pixel. The five menu rows and both footer lines
fit, with no enlarged 3×5 letters. Menu capture:
`build/device-test/menu-native-font/pause.png` (4× nearest-neighbour preview),
with its exact 128×128 counterpart `pause-native.png`.

On-device capture verified the saved checkpoint pages were byte-identical before
and after displaying the new menu, with zero save writes and no fault. The
existing session checks and browser menu/persistence checks passed. Font data
is held in flash: static RAM stays **17,844 bytes** in release and **18,288
bytes** in debug. The release image is **27,704 bytes**, 1,368 bytes larger
than 0.2.0. Both builds retain the separate 2,048-byte stack reservation.

The SD launcher was updated and read-back hash verified; the private game pack
and its text layout are unchanged. In-game text still shares the 104×65 scene
framebuffer. Nearest-neighbour expansion to 128×80 duplicates some rows and
columns, which can make strokes uneven. Eliminating that requires a separate
display-resolution text layer with correct page-copy and scrolling semantics;
it is independent of this menu font update.

- Release image SHA-256:
  `821c9f9d1e007365c7d45ebe8dc93d4976f58b651546e031afc8873d7de584b4`
- Installed SD launcher SHA-256:
  `50b946aac5d856a83d9cd3f046369fe1178767085c088215a47187a95a324ecd`

The menu title now reads OTHER REALM (one space). The rebuilt release and SD launcher were installed and CRC/hash verified. Firmware size and RAM usage are unchanged. Current release SHA-256: 83972f2fae9fe9db90d48a2a39adc11a2c6a3fd82884528aea4b133607626934. Current SD launcher SHA-256: bbb760b23114d61c91dbb97a44d5bbb07d7abbff74c50fb4ca0c7afa6125d149.

## Version 0.3 title, animated menus and sound

Normal startup presents the 128×128 shoreline and native bitmap OTHER REALM
logo, with a flashing prompt. The title resource is loaded into the existing
four framebuffer pages; the palette cache adds 32 bytes, with no extra screen
buffer. The pause menu dims the actual scene and animates in/out. New Game
starts the intro, and its pause menu offers Skip Intro. Watch Intro is removed.

On-device verification captured title, Continue and the paused shoreline in
`build/device-test/title-menu/`. The final debug build measured title redraw at
18,902 µs, Continue menu at 26,601 µs, and the gameplay overlay at 32,099 µs.
Moving dimming out of the pixel/row loops cut the original overlay's roughly
55 ms to 32 ms. Frame duration varies with text and scene; these are redraw
measurements, not a promise of 60 fps animation.

The test resumed the existing water checkpoint, ran 105 ticks with Up+A, opened
the menu, toggled sound off/on and resumed. There were zero VM faults and zero
save writes. The flash journal matched its pre-update backup byte for byte.
PB10 speaker PWM measured 142 timer counts during an 880 Hz cue and zero when
muted. This verifies lightweight piezo cues, not original PCM/music playback.
Peak measured stack was 1,008 of the separately reserved 2,048 bytes.

Release: 30,824-byte binary, 17,924 bytes static RAM, 492 bytes unused below
the reserved stack. Debug: 34,704-byte binary, 18,376 static bytes, 40 bytes
below the same reserved stack. The release remains the installed build.
The private title pack is 1,447,936 bytes, 8,704 more than the previous pack,
and retains original save identity 1889060119.

- Release SHA-256: `d8ea83208b9be421fae6d5b5ddb48c06aa7c94bbedb34579b9d40f3ed6a26a69`
- SD launcher SHA-256: `b6be2e79f779309d5e8b399f576fc16546ffd481e45fd885bacb726c7173b345`
- Private pack SHA-256: `e89f1b7af78192db7a8bb2f9c016f4644bc977893d5d47a27911afe5f926c3dd`

Host checks: 4,572 session assertions (including complete natural intro and
skip), 13,752 renderer assertions, all nineteen original password/checkpoint
comparisons, 2,314 flash journal checks, fourteen patcher tests, and browser
movement/menu/persistence/title/skip/storage-failure integration checks.
The original demo playthrough and official cartridge/card validation pass.## Version 0.3.1 presentation refinementsThe pause overlay now draws only text and its animated selector over the dimmedgame frame. Its selection strip and divider are removed. Dismissing the menustreams the undimmed frame and letterbox margins directly, without an interveningfull-panel black clear. All 16,384 pixels match the pre-pause image on the host,in the browser and in device scanout readback.The title uses the original greeting cinematic, reframed from the user's ownpolygon data with the foreground guard omitted to keep Lester's raised handvisible. The native OTHER REALM wordmark uses original angular black/goldletterforms. PRESS A BUTTON has no background strip and pulses white to sceneblue-gray over 7.68 seconds. No gameplay or input timing changed.Final device captures are in `build/device-test/ui-refinement/`. Title redrawmeasured 16,592 microseconds, the Continue menu 22,797 microseconds and the pauseoverlay 28,363 microseconds. Peak stack remained 1,008 of 2,048 bytes. Soundon/off still measured PWM 142/0. The 512-byte save journal was byte-identical tothe backup before the update, with zero save writes and no VM fault.Release: 31,704 bytes with 17,924 bytes static RAM. Debug: 35,588 bytes with18,376 static bytes. RAM is unchanged from 0.3. The card's private pack andlauncher passed read-back SHA-256 verification, and the final release wasaccepted and launched by the bootloader with image CRC32 `8193E98F`.- Release SHA-256: `88cb7089c058573e6c48d550ed111a5ffa0e31656263aa6959c9892d96785bfe`- SD launcher SHA-256: `141cb79b951499fcf4692f17048af6d015a5045900bc4ce5f57b9548352f0391`- Private pack SHA-256: `5797d84f660a28400181f05d81917f9f3c65a7599b2f5b0e294d5162a810def4`Checks passed: 21,218 session assertions, 13,752 renderer assertions, browsermovement/menu/persistence/intro/skip checks, a targeted white-blue-white pulseand exact-resume comparison, plus official cartridge/card conformance checks.The private background remains excluded from public game and source bundles.## Version 0.4 completion unlock and enchanted menusThe full 128×128 shoreline is again the default title. A second 8,224-byteresource holds the raised-hand greeting, selected only by the persistentcompletion bit. The card grows by 8,704 bytes including sector padding, to1,456,640 bytes. The existing pack identity and 44-byte save payload size stayunchanged. Completion is earned in the original final cinematic, survives newgames, and does not replace the last playable checkpoint with an ending scene.Menus have centered labels, no game-name heading, brighter selected text, aslow moving glint and falling motes. Selection sends motes upward while theother labels dissolve, then slides out; Sound emits particles without leavingor fading the options. The saved-status line scrolls left and A/B are brighterthan their labels. B on the main menu returns to the title without a save write.The particle cache is fourteen three-byte descriptors (42 bytes). Includingstate/alignment, release static RAM increases by 44 bytes to **17,968 bytes**,leaving 448 bytes below the separately reserved 2,048-byte stack. Debug uses18,396 static bytes after shrinking its command buffer from 48 to 24 bytes.Measured peak stack is 1,040 bytes, leaving 1,008 of the reserved stack unused.Release image: **33,316 bytes**. Debug image: **37,188 bytes**.The connected device measured title redraw at 17.072 ms, Continue at 30.456 ms,idle pause effects at 37.082–37.881 ms and confirmation at 41.140 ms. Idleparticles update on a 60 ms clock; confirmation carries fractional frame timeso display cost does not stretch the intended roughly 864 ms burst/slide.These are scanout measurements, not a 60 fps claim. No particle simulation runsduring gameplay.Device captures and profiles are in `build/device-test/magic-menu/`. Checksverified the title's full-panel pixels against the SD resource, B-to-title,Sound remaining open, the confirmation delay, and an exact pixel match afterresume. The 512-byte flash journal matched its pre-update backup byte for byte,with zero writes and no VM fault. Completion and new-game retention were testedon host/browser fixtures; the user's real save was not marked complete.Host checks: 20,792 session assertions, 154 checkpoint assertions including allnineteen original destinations, 13,752 renderer assertions, 2,314 journal checks,fifteen patcher tests, the original demo playthrough, browser integration anddeterministic menu/title captures. VM scheduling, malformed bytecode and allthree system-event checks also pass. The ordinary title is also used when acompleted pack has no alternate resource. Official cartridge/card checks pass.The final release is installed and running; the bootloader accepted CRC32`9B1AB0B8`. Both updated SD files passed read-back hash checks. Other card filesand menu indexes were preserved.- Release SHA-256: `fc2fbcd76a789a6025ab714192b8b261fb8978833e98c5105616eb301eadcd6e`- SD launcher SHA-256: `cd951e45ed29cd9c9f6f321adabbf9f6b36149663302df76d1931ee64e363aa2`- Private pack SHA-256: `f5c8bf5b05f4f0baa787b54aa004f194a580f05bad6de8ca6de7554aa1124e9e`

## Version 0.4.1 stationary fades, standing title and Start hold

The default private title now captures the original shoreline at tick 170, after
Lester finishes climbing and stands upright. The capture recipe releases swim
input on entering the exterior, checks the edition's control-handoff bytecode,
and stops at task 20 PC 0x22F0. The earned greeting title, pack identity, dimensions
and save format are unchanged.

Menus fade at fixed coordinates. The checkpoint footer stays centered and still.
Idle sparks start across a wider area, fall farther, and sweep back and forth;
selection changes disturb a small dust cloud. Confirmation particles have varied
horizontal velocity and mild gravity, with the existing delayed fade. The Exit
item is removed. Holding Start three seconds requests the platform reset, with a
96-pixel progress bar. Releasing early cancels it; tapping still pauses/resumes.

Release binary: **33,744 bytes**, with **17,976 bytes** static RAM, just eight
more RAM bytes than 0.4. Debug binary: **37,628 bytes**, with 18,404 static bytes.
The separately reserved stack remains 2,048 bytes; measured peak remains 1,040.
No extra frame buffer is allocated. Release has 440 bytes below the stack reserve.

Device title redraw measured 16.756 ms, Continue 28.603 ms, idle pause
34.397-34.530 ms, and confirmation 37.826 ms. Captures/profiles are in
`build/device-test/fade-menu/`. The hold bar filled 31 of 96 pixels after one
second; cancellation kept the menu open. A fresh hold kept the app running at
2.65 seconds and returned to bootloader v3 after three seconds, verified through
its HELLO response after USB re-enumeration. A restart retained the save journal
byte for byte, with zero writes during these tests.

Passed: 20,814 session assertions, 13,752 renderer assertions, browser integration,
deterministic UI captures (stationary footer, fades, dust/poof, hold bar, both
titles), exact device/host resume pixel comparisons, and official cartridge/card
round trips. The final release is installed and running with image CRC32
`1714D1CF`. Both SD files passed readback SHA-256 checks.

- Release SHA-256: `8d1f894c3e9eaa7f59572eb7f92f8902ac65770fd7ff8fe17b31ea92ea68e944`
- SD launcher SHA-256: `3ebf442933f272b995fa223e74c8c7c453f4e31ba0af7bd46bac7ec8f8085441`
- Private pack SHA-256: `6a7875fd37a2e20bc4e805f84373ac120d3355467eea3246abe230897fa677d7`

## Version 0.4.2 overlapping menu particles

Ambient sparkles now have their own slots and continue throughout disturbed-dust
and confirmation effects. The steady selection effect is reduced from twelve
to nine particles (25%); the confirmation poof from fourteen to ten (29%).
Disturbed dust increases from eight to fifteen motes (88%). Positions remain
cached once per frame, with appearance derived only on rows each mote touches.
The 24 two-byte positions use 48 bytes rather than the old 42-byte cache; layout
padding means total static RAM rises by only four bytes.

Release binary: 33,808 bytes; static RAM: 17,980 bytes. Debug binary: 37,692 bytes;
static RAM: 18,408 bytes. Peak stack remained 1,040 of the reserved 2,048 bytes.
The handheld measured idle pause redraws at 35.358-35.440 ms, overlapping dust
at 35.738 ms, and confirmation at 39.123 ms. Device captures and profiles are in
`build/device-test/particles-menu/`.

All 20,876 session assertions passed, including a new pixel comparison proving
that adding disturbed dust retains the existing falling motes. Browser visual
checks passed. Device resume restored the exact paused pixels, the save journal
matched its fresh backup byte for byte, and no save writes or VM faults occurred.
Official cartridge/card round trips passed. Game content and title assets did
not change; only the launcher cartridge on SD is updated.

- Release SHA-256: `52ecd515262b23cc3c76dcd2fdfc2434a6f2f2705d587b01bcf869c077f1d03d`
- SD launcher SHA-256: `63f7a6e84748cd04d02b7453d5ff4cd56bbccec874b2a2d0071c88127e6bbcaa`

The SD cartridge passed readback verification. The release is installed and
running, accepted by the bootloader with image CRC32 73550E05.

