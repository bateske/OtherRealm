# Semantic checkpoints

The save payload restarts a scene through its normal bytecode initialization.
It does not serialize tasks, call stacks, framebuffer pages, resources, pointers
or audio state. `CheckpointValue` uses 38 bytes in RAM; its explicit `ORCP`
version 1 encoding uses 44 bytes in storage.

The persistence layer associates the payload with the current pack identity.
The device journal provides CRC validation, sequence numbers and two alternating
flash pages so interrupted writes retain the preceding record. The browser uses
an atomic localStorage value replacement and validates the decoded payload;
browser and device saves are separate. The checkpoint codec is deliberately
independent of SD, flash and browser storage.
Its own checks reject unknown versions, unsupported kinds, incorrect lengths,
unknown flag bits, reserved bytes and invalid original-game destinations.

Payload byte 10 bit 0 records game completion; byte 11 and the remaining flag
bits stay zero. The record remains 44 bytes and existing version-1 records load
with completion false. Completion is independent of the current checkpoint:
starting a new game and saving a fresh milestone retain it. A completed record
may contain an otherwise all-zero CheckpointNone value when no playable save
exists. Old firmware that required byte 10 to be zero cannot read completed
records. The two-page CRC/atomic replacement protocol is unchanged.

The original game's noninteractive final part 16007 earns completion after its
first successful tick, preserving the last playable checkpoint. Authored packs
emit system event 3 on victory; Signal Grove does this when exiting its final
gate. Entering a menu or a password screen never earns completion. This flag
selects an optional unlocked title resource and is written only when first
earned or alongside a changed checkpoint.

Save writes occur only when a milestone changes. If storage fails, the current
checkpoint remains available for Retry in RAM and the menu reports the failure;
it will not survive a restart. A later milestone attempts another write. The
device journal retains one installed game's progress. Its reserved pages are
shared platform storage, so saving another game can replace the previous one.
Text-patcher revisions built from the same original pack retain its identity.

## Original Amiga data

The supplied Amiga password code selects a part, writes its startup selector
to variable `00`, then requests that part. Its nineteen gameplay destinations
are:

| Part | Selectors |
| --- | --- |
| 16002 | 10 |
| 16003 | 20 |
| 16004 | 30, 31, 33, 35, 37, 39, 41–49 |
| 16005 | 50 |
| 16006 | 60 |

The city script updates variable `00` when the player earns a new checkpoint.
Selectors 41–48 reconstruct combinations of completed city events, so these
must remain distinct even when they start on the same screen. Screen variable
`67` is not a substitute for the checkpoint selector.

The water, arena and palace scripts do not use their startup selector. The
palace explicitly resets variable `00`; capture therefore normalizes these
parts to their password destinations. The jail's internal retry selector 21
skips its entry animation, but the actual password destination is 20. This
module preserves the nineteen original password semantics consistently.

The initial protection registers come from `Vm::resetVariables()`. No random
seed is required: the original random seed only randomizes the copy-protection
wheel. Intro, protection, password and ending parts are not checkpoints, and
cannot replace earned gameplay progress through `captureCheckpoint`.

RAWGL's broader restart-position table also contains selectors intended for
later releases. It is not the authoritative checkpoint list for these disks.
The list above was verified against the supplied private password bytecode.
`tests/checkpoint_tests.cpp` can discover that table directly, execute its
comparison routine, and compare the resulting frame, palette and screen after
40 ticks with a fresh semantic resume for every destination. All nineteen
destinations pass this comparison.

## Authored scene packs

Packs with a scene map opt into checkpointing by setting variable `E0` to a
nonzero milestone marker. Capture retains the part, marker and variables
`00` through `0F`. Increment the marker when inventory or another persistent
same-scene event changes. The part plus marker determines whether progress
changed; walking coordinates and animation timers do not cause repeated
storage writes.

Scene initialization must respect the restored variables. Signal Grove uses
variable `0F` as an initialization guard, preserves its crystal and gate state,
and increments `E0` for pickups, unlocking, room changes and victory. A new
scene must complete at least one VM tick before capture, preventing inherited
entry variables from being saved before the new script initializes them.

Call `resumeCheckpoint` to initialize a fresh game scene and restore the semantic
variables before its first tick. The session layer controls when to replace
the latest saved milestone, the intro-to-gameplay save boundary, and the
distinct meanings of Continue, Retry and New Game.

## Validation

After building the host tests:

```powershell
node build/checkpoint_tests.js demo/sd/OTHERWRL.PAK
node build/checkpoint_tests.js demo/sd/OTHERWRL.PAK private/sd/OTHERWRL.PAK
```

The first command checks codec rejection and original-demo resume behavior.
The second additionally verifies all nineteen original password destinations;
it requires the user's private game data. Neither command opens an audio device.
