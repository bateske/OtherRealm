#pragma once
#include <stdint.h>
#include <Fat.h>

namespace device {
class Card {
public:
    bool begin(const char *name = "OTHERWRLPAK");
    bool readSector(uint32_t block, uint8_t *dst);
    int error() const { return error_; }
    uint32_t bytes() const { return bytes_; }
    uint32_t reads() const { return reads_; }
    uint32_t readMicros() const { return readMicros_; }
    void resetStats() { reads_ = readMicros_ = 0; }
private:
    fat::Run runs_[16];
    uint32_t bytes_ = 0, reads_ = 0, readMicros_ = 0;
    uint8_t count_ = 0;
    int8_t error_ = 0;
};
}
