#include "../firmware/Otherrealm/Persistence.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    fprintf(stderr,"%s:%d: check failed: %s\n",__FILE__,__LINE__,#condition); \
    exit(1); } } while (0)

namespace device {
uint8_t *displayScratch(){alignas(4) static uint8_t scratch[512];return scratch;}
}

int main() {
    using namespace device;
    uint8_t first[44],second[44],out[44];
    memset(first,0x19,sizeof(first));memset(second,0xA7,sizeof(second));
    const uint32_t pack=0xBADE1234;
    saveTestReset(true);
    CHECK(!saveLoad(pack,out,sizeof(out)));
    CHECK(saveStore(pack,first,sizeof(first))&&saveWrites()==1);
    CHECK(saveStore(pack,first,sizeof(first))&&saveWrites()==1);
    CHECK(!saveLoad(pack+1,out,sizeof(out)));
    CHECK(!saveLoad(pack,out,sizeof(out)-1));
    CHECK(!saveStore(pack,nullptr,44));
    CHECK(!saveStore(pack,first,SaveMaxBytes+1));
    // Every incomplete page program must preserve the previous committed page.
    for(unsigned cut=0;cut<256;++cut) {
        saveTestReset(true);
        CHECK(saveStore(pack,first,sizeof(first)));
        saveTestCut(cut);
        CHECK(!saveStore(pack,second,sizeof(second)));
        CHECK(!saveAvailable());
        saveTestReset(false);
        CHECK(saveLoad(pack,out,sizeof(out))&&!memcmp(out,first,sizeof(out)));
        CHECK(saveStore(pack,second,sizeof(second)));
        CHECK(saveLoad(pack,out,sizeof(out))&&!memcmp(out,second,sizeof(out)));
    }
    // A bit upset in either metadata, payload, checksum, or commit marker
    // invalidates the latest record and falls back to the previous record.
    for(unsigned byte=0;byte<256;++byte) {
        saveTestReset(true);
        CHECK(saveStore(pack,first,sizeof(first)));
        CHECK(saveStore(pack,second,sizeof(second)));
        saveTestCorrupt(1,byte);
        CHECK(saveLoad(pack,out,sizeof(out))&&!memcmp(out,first,sizeof(out)));
    }
    uint8_t maximum[SaveMaxBytes],restored[SaveMaxBytes];
    for(unsigned i=0;i<sizeof(maximum);++i)maximum[i]=uint8_t(i);
    saveTestReset(true);
    CHECK(saveStore(pack,maximum,sizeof(maximum)));
    CHECK(saveLoad(pack,restored,sizeof(restored)));
    CHECK(!memcmp(maximum,restored,sizeof(maximum)));
    printf("Flash journal PASS: %u checks, 256 interrupted writes, 256 corruption cases, identity, duplicate and size checks.\n",checks);
}
