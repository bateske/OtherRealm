// Flash-controller sequences adapted from CHGame bootloader flash.c,
// Copyright (c) 2026 Kevin Bates, MIT. See THIRD_PARTY_FLASH_LICENSE.txt.
// This standalone journal avoids CHGfx's framebuffer and uses idle scanlines
// as the page buffer. No SD or display transfer may overlap saveStore().
#include "Persistence.h"
#include "Display.h"
#ifndef OTHERREALM_PERSISTENCE_TEST
#include <Arduino.h>
#endif
#include <string.h>

namespace device {
namespace {
const uint32_t PageA=0xF500,PageB=0xF600;
const uint32_t Magic=0x3257524F,Commit=0x51A7C0DE;
struct Header {
    uint32_t magic,pack,sequence;
    uint16_t bytes;
    uint8_t version,reserved;
};
static_assert(sizeof(Header)==16,"Unexpected save header layout");
bool failed=false;
uint32_t writes=0;
extern "C" uint32_t _data_lma,_data_vma,_edata;
#ifdef OTHERREALM_PERSISTENCE_TEST
alignas(4) uint8_t testPages[2][256];
unsigned testCut=256;
#endif

uint32_t crc32(const uint8_t *data,unsigned bytes) {
    uint32_t crc=0xFFFFFFFFu;
    while(bytes--) {
        crc^=*data++;
        for(unsigned bit=0;bit<8;++bit)
            crc=(crc>>1)^(0xEDB88320u&(0u-(crc&1)));
    }
    return ~crc;
}
const uint8_t *page(unsigned index) {
#ifdef OTHERREALM_PERSISTENCE_TEST
    return testPages[index];
#else
    return reinterpret_cast<const uint8_t *>(index?PageB:PageA);
#endif
}
bool valid(const uint8_t *p,uint32_t identity,uint16_t bytes) {
    const Header &h=*reinterpret_cast<const Header *>(p);
    return h.magic==Magic&&h.pack==identity&&h.bytes==bytes&&h.version==1&&
        *reinterpret_cast<const uint32_t *>(p+252)==Commit&&
        *reinterpret_cast<const uint32_t *>(p+248)==crc32(p,248);
}
int newest(uint32_t identity,uint16_t bytes) {
    bool a=valid(page(0),identity,bytes),b=valid(page(1),identity,bytes);
    if(a&&b) {
        uint32_t sa=reinterpret_cast<const Header *>(page(0))->sequence;
        uint32_t sb=reinterpret_cast<const Header *>(page(1))->sequence;
        return sa!=sb&&uint32_t(sa-sb)<0x80000000u?0:1;
    }
    return a?0:b?1:-1;
}

// Everything while FLASH is busy must execute from SRAM, including register
// saves/restores. This leaf uses no library calls and masks interrupt vectors
// until the page has been committed. 0x08000000 is the programming alias.
#ifdef OTHERREALM_PERSISTENCE_TEST
void programPage(uint32_t address,const uint32_t *words) {
    uint8_t *destination=testPages[address==PageB];
    memset(destination,0xFF,256);
    memcpy(destination,words,testCut);
}
#else
__attribute__((section(".gnu.linkonce.r.otherrealm.flash"),noinline))
void programPage(uint32_t address,const uint32_t *words) {
    uint32_t interruptState;
    __asm volatile("csrr %0, 0x800":"=r"(interruptState));
    __asm volatile("csrw 0x800, %0"::"r"(interruptState&~0x88u));
    FLASH->KEYR=0x45670123;FLASH->KEYR=0xCDEF89AB;
    FLASH->MODEKEYR=0x45670123;FLASH->MODEKEYR=0xCDEF89AB;
    uint32_t destination=address+0x08000000u;
    FLASH->CTLR|=0x00020000u;
    FLASH->ADDR=destination;
    FLASH->CTLR|=0x40u;
    while(FLASH->STATR&1){}
    FLASH->CTLR&=~0x00020000u;
    FLASH->CTLR|=0x00010000u;
    FLASH->CTLR|=0x00080000u;
    while(FLASH->STATR&1){}
    FLASH->CTLR&=~0x00010000u;
    for(unsigned i=0;i<64;++i) {
        FLASH->CTLR|=0x00010000u;
        reinterpret_cast<volatile uint32_t *>(destination)[i]=words[i];
        FLASH->CTLR|=0x00040000u;
        while(FLASH->STATR&1){}
        FLASH->CTLR&=~0x00010000u;
    }
    FLASH->CTLR|=0x00010000u;
    FLASH->ADDR=destination;
    FLASH->CTLR|=0x40u;
    while(FLASH->STATR&1){}
    FLASH->CTLR&=~0x00010000u;
    FLASH->CTLR|=0x8000u;
    __asm volatile("csrw 0x800, %0"::"r"(interruptState));
}
#endif
} // namespace

bool saveAvailable() {
#ifdef OTHERREALM_PERSISTENCE_TEST
    return !failed;
#else
    uint32_t end=reinterpret_cast<uint32_t>(&_data_lma)+
        reinterpret_cast<uint32_t>(&_edata)-reinterpret_cast<uint32_t>(&_data_vma);
    return !failed&&end<=PageA;
#endif
}
bool saveLoad(uint32_t identity,void *record,uint16_t bytes) {
    if(!saveAvailable()||!record||bytes>SaveMaxBytes)return false;
    int active=newest(identity,bytes);
    if(active<0)return false;
    memcpy(record,page(active)+sizeof(Header),bytes);
    return true;
}
bool saveStore(uint32_t identity,const void *record,uint16_t bytes) {
    if(!saveAvailable()||!record||bytes>SaveMaxBytes)return false;
    int active=newest(identity,bytes);
    if(active>=0&&!memcmp(page(active)+sizeof(Header),record,bytes))return true;
    uint8_t *buffer=displayScratch();
    // record may be supplied in the second half of displayScratch(), but must
    // not overlap its first 256 bytes, which become this new flash page.
    memset(buffer,0,256);
    Header &h=*reinterpret_cast<Header *>(buffer);
    h.magic=Magic;h.pack=identity;h.version=1;h.bytes=bytes;
    h.sequence=active<0?1:reinterpret_cast<const Header *>(page(active))->sequence+1;
    memcpy(buffer+sizeof(Header),record,bytes);
    *reinterpret_cast<uint32_t *>(buffer+248)=crc32(buffer,248);
    *reinterpret_cast<uint32_t *>(buffer+252)=Commit;
    uint32_t destination=active==0?PageB:PageA;
    programPage(destination,reinterpret_cast<const uint32_t *>(buffer));
    if(memcmp(page(destination==PageB),buffer,256)) {
        failed=true;
        return false;
    }
    ++writes;
    return true;
}
uint32_t saveWrites(){return writes;}
#ifdef OTHERREALM_PERSISTENCE_TEST
void saveTestReset(bool clearFlash) {
    if(clearFlash)memset(testPages,0xFF,sizeof(testPages));
    failed=false;writes=0;testCut=256;
}
void saveTestCut(unsigned bytes){testCut=bytes<=256?bytes:256;}
void saveTestCorrupt(unsigned slot,unsigned byte){testPages[slot][byte]^=1;}
#endif
} // namespace device
