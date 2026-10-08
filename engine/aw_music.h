// SPDX-License-Identifier: GPL-2.0-or-later
// Silent Amiga music synchronization. Event format based on Raw/RawGL by
// Gregory Montoir. Rewritten October 2026; no sample decoding or audio output.
#ifndef OTHERREALM_AW_MUSIC_H
#define OTHERREALM_AW_MUSIC_H
#include <stdint.h>

namespace otherrealm {

class MusicReader {
public:
    virtual ~MusicReader() {}
    virtual uint32_t resourceSize(uint16_t id) = 0;
    virtual uint8_t readResourceByte(uint16_t id, uint32_t offset) = 0;
};

class SilentMusic {
public:
    explicit SilentMusic(MusicReader &reader);
    // Restart a game after a failed module without retaining its old fault.
    void reset();
    bool play(uint16_t resource, uint16_t delay = 0, uint8_t position = 0);
    void setDelay(uint16_t delay);
    void stop() { playing_ = false; }
    // Elapsed *game* time supports both real-time device playback and fast
    // headless simulation. Writes only the original music synchronization var.
    void advance(uint32_t elapsedMs, int16_t &syncVariable);
    bool playing() const { return playing_; }
    const char *error() const { return error_; }
    uint32_t eventsProcessed() const { return eventsProcessed_; }
private:
    MusicReader &reader_;
    uint32_t size_, accumulatedMs_, periodMs_, eventsProcessed_;
    uint16_t resource_;
    uint8_t order_, orderCount_, row_;
    bool playing_;
    const char *error_;
    uint16_t word(uint32_t offset);
    void row(int16_t &syncVariable);
};

} // namespace otherrealm
#endif
