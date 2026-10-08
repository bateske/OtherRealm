// SPDX-License-Identifier: GPL-2.0-or-later
// Semantic checkpoints: resume scene initialization, not framebuffer/VM dumps.
#ifndef OTHERREALM_AW_CHECKPOINT_H
#define OTHERREALM_AW_CHECKPOINT_H
#include <stddef.h>
#include <stdint.h>

namespace otherrealm {
class Game;
enum CheckpointKind { CheckpointNone=0, CheckpointAmiga=1, CheckpointScene=2 };
enum { CheckpointVariableCount=16, CheckpointPayloadBytes=44, CheckpointMarkerVariable=0xe0 };

struct CheckpointValue {
    uint16_t part;
    uint16_t kind;
    uint16_t marker;
    int16_t variables[CheckpointVariableCount];
};
static_assert(sizeof(CheckpointValue)==38,"Checkpoint RAM layout changed");

// Call after a successful complete Game::tick. A newly loaded scene must run
// at least one tick before its state is eligible. Failure leaves value alone.
bool captureCheckpoint(const Game &game,CheckpointValue &value);
bool validCheckpoint(const CheckpointValue &value);
// Milestone equality intentionally ignores moving actor coordinates in custom
// scenes. Authors increment VAR(E0) for meaningful same-scene changes.
bool sameCheckpoint(const CheckpointValue &a,const CheckpointValue &b);
// Reinitialize tasks/resources, then restore the semantic startup variables.
bool resumeCheckpoint(Game &game,const CheckpointValue &value);

// Explicit little-endian ORCP v1 payload. Storage owns atomicity, sequence,
// CRC and pack identity; no compiler struct layout or pointers reach disk.
bool encodeCheckpoint(const CheckpointValue &value,uint8_t *out,size_t size,bool completed=false);
bool decodeCheckpoint(const uint8_t *data,size_t size,CheckpointValue &value,bool *completed=0);
} // namespace otherrealm
#endif
