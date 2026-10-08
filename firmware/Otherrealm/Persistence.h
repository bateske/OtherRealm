#pragma once
#include <stdint.h>

namespace device {
// Two 256-byte flash pages; the old valid record survives an interrupted save.
enum { SaveMaxBytes = 224 };
bool saveAvailable();
bool saveLoad(uint32_t packIdentity, void *record, uint16_t bytes);
bool saveStore(uint32_t packIdentity, const void *record, uint16_t bytes);
uint32_t saveWrites();
#ifdef OTHERREALM_PERSISTENCE_TEST
void saveTestReset(bool clearFlash);
void saveTestCut(unsigned bytes);
void saveTestCorrupt(unsigned slot,unsigned byte);
#endif
}
