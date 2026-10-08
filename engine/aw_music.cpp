// SPDX-License-Identifier: GPL-2.0-or-later
// Otherrealm silent music event clock, rewritten October 2026.
// Event format based on Raw/RawGL, copyright (C) 2004-2005 Gregory Montoir.
#include "aw_music.h"

namespace otherrealm {

SilentMusic::SilentMusic(MusicReader &reader)
    : reader_(reader), size_(0), accumulatedMs_(0), periodMs_(0), eventsProcessed_(0),
      resource_(0), order_(0), orderCount_(0), row_(0), playing_(false), error_(0) {}

void SilentMusic::reset() {
    playing_ = false;
    error_ = 0;
    size_ = accumulatedMs_ = periodMs_ = eventsProcessed_ = 0;
    resource_ = 0;
    order_ = orderCount_ = row_ = 0;
}

uint16_t SilentMusic::word(uint32_t offset) {
    if (offset >= size_ || size_ - offset < 2) {
        error_ = "music read beyond resource";
        playing_ = false;
        return 0;
    }
    const uint8_t hi = reader_.readResourceByte(resource_, offset);
    return static_cast<uint16_t>((hi << 8) | reader_.readResourceByte(resource_, offset + 1));
}

void SilentMusic::setDelay(uint16_t delay) {
    // Original Paula event clock, with the same integer millisecond rounding
    // as RawGL. A minimum period keeps malformed modules from spinning forever.
    periodMs_ = static_cast<uint32_t>(delay) * 60000u / 7159092u;
    if (!periodMs_) periodMs_ = 1;
}

bool SilentMusic::play(uint16_t resource, uint16_t delay, uint8_t position) {
    playing_ = false;
    error_ = 0;
    resource_ = resource;
    size_ = reader_.resourceSize(resource);
    accumulatedMs_ = eventsProcessed_ = 0;
    row_ = 0;
    order_ = position;
    if (size_ < 0xc0) { error_ = "truncated music header"; return false; }
    orderCount_ = reader_.readResourceByte(resource_, 0x3f);
    if (!orderCount_ || orderCount_ > 128 || position >= orderCount_) {
        error_ = "invalid music order table"; return false;
    }
    setDelay(delay ? delay : word(0));
    if (error_) return false;
    playing_ = true;
    // Audio mixers process the first row immediately, not after one period.
    accumulatedMs_ = periodMs_;
    return true;
}

void SilentMusic::row(int16_t &syncVariable) {
    const uint8_t pattern = reader_.readResourceByte(resource_, 0x40u + order_);
    const uint32_t offset = 0xc0u + static_cast<uint32_t>(pattern) * 1024u + row_ * 16u;
    if (offset >= size_ || size_ - offset < 16) {
        error_ = "music pattern beyond resource";
        playing_ = false;
        return;
    }
    for (unsigned channel = 0; channel < 4; ++channel) {
        const uint32_t event = offset + channel * 4;
        if (word(event) == 0xfffd) {
            const uint16_t value = word(event + 2);
            syncVariable = value < 0x8000 ? static_cast<int16_t>(value)
                : static_cast<int16_t>(static_cast<int32_t>(value) - 65536);
        }
    }
    ++eventsProcessed_;
    if (++row_ == 64) {
        row_ = 0;
        if (++order_ == orderCount_) playing_ = false;
    }
}

void SilentMusic::advance(uint32_t elapsedMs, int16_t &syncVariable) {
    if (!playing_) return;
    // At most 128*64 rows exist, so even a long elapsed interval is bounded.
    const uint32_t room = 0xffffffffu - accumulatedMs_;
    accumulatedMs_ += elapsedMs > room ? room : elapsedMs;
    while (playing_ && accumulatedMs_ >= periodMs_) {
        accumulatedMs_ -= periodMs_;
        row(syncVariable);
    }
}

} // namespace otherrealm
