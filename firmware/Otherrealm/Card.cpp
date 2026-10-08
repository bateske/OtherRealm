#include "Card.h"
#include "Display.h"
#include <SdSpi.h>
#include <Arduino.h>

namespace device {
#if OTHERREALM_FAST_SD
static void blockReady(const uint8_t *, void *) {}
#endif
bool Card::begin(const char *name) {
    count_ = 0;
    bytes_ = reads_ = readMicros_ = 0;
    uint8_t *scratch = displayScratch();
    if (!sd::init()) { error_ = fat::E_READ; return false; }
    error_ = fat::mount(scratch);
    if (error_ < 0) return false;
    fat::File file;
    error_ = fat::find(name, file, scratch);
    if (error_ < 0) return false;
    error_ = fat::runs(file, runs_, 16, scratch);
    if (error_ <= 0) return false;
    count_ = error_;
    bytes_ = file.size;
    error_ = 0;
    return true;
}

bool Card::readSector(uint32_t block, uint8_t *dst) {
    if (!count_ || block >= (bytes_ + 511u) / 512u) return false;
    uint32_t start = micros();
    bool ok = false;
#if OTHERREALM_FAST_SD
    uint32_t remaining = block;
    for (unsigned i=0; i<count_; ++i) {
        if (remaining < runs_[i].blocks) {
            // DMA directly into the VM cache. The display scanlines are idle
            // while SD owns SPI1 and provide stream()'s second scratch buffer.
            ok = sd::stream(runs_[i].lba + remaining, 1, dst, displayScratch(), blockReady, nullptr);
            break;
        }
        remaining -= runs_[i].blocks;
    }
#else
    ok = fat::read(runs_, count_, block, dst);
#endif
    readMicros_ += micros() - start;
    ++reads_;
    if (!ok) { count_ = 0; error_ = fat::E_READ; }
    return ok;
}
}
