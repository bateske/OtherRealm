# Performance and compatibility evidence

These measurements count **512-byte SD-sector requests made by the engine in
host replays**. They are not measurements of device frame rate, SPI speed, card
latency, or LCD transfer time. A fast desktop file read cannot predict physical
SD-card latency. Device timing should be measured with the firmware's profiling
output on the connected board and its actual card.

The default engine uses three 512-byte cache slots shared by scripts, vector
shapes, palettes, and music timing events. True LRU replacement moves slot
indexes, never sector contents. Sequential byte reads use a one-comparison fast
path. Resource data remains in its SD pack; no whole script or shape bank is
loaded into RAM. The framebuffer remains four packed 4bpp pages at 104 × 65.

## Cache comparison

Measured October 8, 2026 with the private Amiga pack built by `build_pack.py`,
104 × 65 framebuffer, Emscripten `-O2`, and idle input. Intro runs 3000 cooperative
VM ticks; the other scenes run 1000. One VM tick can present multiple frames.
The intro replay crosses into part 16002 at tick 2636. The table reports total
sector reads, followed by the largest number in one VM tick in parentheses.

| Cache organization | Intro | Water | Jail | City | Host `sizeof(Game)` |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 script + 2 data | 16,899 (166) | 1,654 (166) | 57,563 (81) | 67,079 (149) | 16,328 B |
| 2 script + 1 data | 33,731 (1,547) | 9,229 (1,547) | 69,696 (615) | 58,562 (616) | 16,328 B |
| **3 shared, default** | **15,351 (84)** | **1,584 (71)** | **47,734 (63)** | **39,961 (56)** | **16,328 B** |
| 2 script + 2 data | 13,730 (166) | 1,506 (166) | 43,107 (68) | 47,014 (129) | 16,844 B |
| 4 shared | 12,200 (63) | 1,277 (32) | 42,114 (56) | 33,964 (47) | 16,844 B |

Three shared slots reduced the city replay's reads by 40.4% and its worst tick
from 149 to 56 sectors compared with the previous three-slot split cache, without
increasing `sizeof(Game)` in the host build. Four shared slots save further reads
but cost 516 bytes in that build. Host struct size excludes stack, device-library
state, USB buffers, and firmware globals; use the device linker map for RAM fit.

All five variants produced matching final framebuffer hashes, frame counts,
simulated game time, and polygon counts for each scenario. The independent
`pack_tests.cpp` also checks 10,000 mixed script/data byte reads against the source
bytes, sector-spanning copies, cache invalidation after scratch-buffer use,
resolution flags, resource bounds, and injected SD failures.

Reproduce the comparison after installing/configuring Emscripten:

```powershell
./tools/profile-cache.ps1 -EmsdkRoot D:/path/to/emsdk
```

It builds all five configurations, runs the four scenarios, rejects differing
replay results, and writes `build/cache-profile/results.csv`. To use another pack,
pass `-Pack`; the four default scene IDs must exist in that pack. Compile-time
`OR_CACHE_SLOTS` selects slot count and `OR_CODE_SLOTS=0` selects shared LRU.
Nonzero `OR_CODE_SLOTS` reserves that many slots for bytecode as a benchmarking
control. The default is `OR_CACHE_SLOTS=3`, `OR_CODE_SLOTS=0`.

## Where reads occur

The earlier 1-script/2-data profile shows why sharing helps. Of 67,081 sector
requests during a 1000-tick city audit, 61,959 were bytecode requests, 5,114 were
shape requests, and 8 were metadata/palette requests. The jail audit requested
36,327 code sectors and 21,228 shape sectors. Conversely, a single data slot
thrashes when a shape's hierarchy and child polygons occupy different sectors.
The shared cache serves whichever working set is active.

`tests/probe.cpp` reports median/p95/maximum reads per tick, resource-type totals,
hot sectors, and observed shape nesting. Its tracing adds a few initial metadata
reads; use `headless.cpp` for the comparison table. A minimal portable build is:

```powershell
$sources = @('engine/aw_game.cpp','engine/aw_pack.cpp','engine/aw_render.cpp','engine/aw_music.cpp','engine/aw_vm.cpp')
em++ -std=c++11 -O2 -Iengine @sources tests/probe.cpp -sENVIRONMENT=node -sNODERAWFS=1 -o build/probe.js
node build/probe.js private/sd/OTHERWRL.PAK 3000 16001
```

## Compatibility coverage and playable replays

The supplied original Amiga data passes extraction and resource CRC verification.
Host audits started parts 16001 through 16007, each for at least 1000 ticks; the
intro also ran through its transition into gameplay. Observed shape trees reached
depth 3, and the opening gameplay audit reached polygons with 40 vertices. These
paths produced no invalid shape, bytecode, or resource-read faults. This is
**partial game coverage**, not a completed Another World playthrough or proof
that every puzzle, checkpoint, and ending behaves correctly.

The private opening replay surfaces from the pool at tick 48, traverses screens
2–5, turns away from the beast, returns across screens 4–2, and reaches the vine
screen at tick 278. It is useful for testing controls and transitions. It does
not finish the level:

```powershell
node build/probe.js private/sd/OTHERWRL.PAK 340 16002 tests/replays/another-world-opening.csv
```

The original game's death sequence deliberately waits for the action button to
be released before showing its restart prompt; holding action continuously can
leave that sequence on a black frame. This was confirmed against the supplied
script, rather than treated as an engine crash.

The redistributable Signal Grove demo has a complete tested route: collect the
crystal, unlock the gate, reach victory, and restart with cleared inventory and
gate state. A separate test tries the gate without the crystal and confirms that
it blocks movement. `tests/demo_tests.cpp` exercises these conditions using only
VM input. The independent fixed-input replay also reaches victory and restarts:

```powershell
node build/probe.js demo/sd/OTHERWRL.PAK 250 16001 tests/replays/signal-grove-win.csv
```

Replay CSV rows are `exclusive_end_tick,direction_mask,action`; direction bits
are Right=1, Left=2, Down=4, Up=8. An optional output prefix after the replay path
saves screenshots and a CSV of all VM variables. Keep captures from the private
Amiga content under `private/` or `build/`.

All host and device test paths are silent: the tracker processes script timing
events without opening an audio device or emitting samples.
