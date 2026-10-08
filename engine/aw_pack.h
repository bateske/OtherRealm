#pragma once
#include <stdint.h>
#include <string.h>

#ifndef OR_WIDTH
#define OR_WIDTH 104
#endif
#ifndef OR_CACHE_SLOTS
#define OR_CACHE_SLOTS 3
#endif
#ifndef OR_CODE_SLOTS
// Zero shares every cache slot between scripts and shape/data reads.
// Nonzero reserves that many slots for scripts, useful for profiling variants.
#define OR_CODE_SLOTS 0
#endif
namespace otherrealm {
enum { Width=OR_WIDTH, Height=OR_WIDTH*5/8, PageBytes=Width*Height/2 };
static_assert(Width%8==0, "Width must be a multiple of eight");
static_assert(OR_CACHE_SLOTS>=2 && OR_CACHE_SLOTS<=255, "Cache must have 2..255 slots");
static_assert(OR_CODE_SLOTS>=0 && OR_CODE_SLOTS<OR_CACHE_SLOTS, "Invalid code cache partition");
struct Device {
    virtual bool readSector(uint32_t sector, uint8_t *dst)=0;
    virtual void display(const uint8_t *packed, const uint16_t *rgb565)=0;
    // Called at each original game's display opcode, including repeated frames.
    virtual void waitFrame(uint16_t milliseconds)=0;
    virtual void tone(uint16_t,uint16_t){}
    virtual void soundEnabled(bool){}
};
struct Resource { uint32_t offset,size; uint16_t type,flags; };
class Pack {
public:
    explicit Pack(Device &d):device(d),reads(0),hits(0),error(0),count(0),length(0),flags(0) { invalidate(); }
    bool open();
    uint16_t resourceCount()const{return count;}
    bool hasSceneMap()const{return flags&4;}
    bool isAdapted()const{return flags&8;}
    uint32_t identity();
    Resource entry(uint16_t id);
    uint8_t byte(const Resource &r,uint32_t off,uint8_t lane=1);
    bool copy(const Resource &r,uint32_t off,uint8_t *dst,uint32_t n);
    void invalidate() { for(unsigned i=0;i<OR_CACHE_SLOTS;++i){tags[i]=0xFFFFFFFFu;order[i]=uint8_t(i);} }
    uint8_t *scratch() { invalidate(); return blocks[0]; }
    Device &device;
    uint32_t reads,hits;
    const char *error;
private:
    uint32_t tags[OR_CACHE_SLOTS];
    alignas(4) uint8_t blocks[OR_CACHE_SLOTS][512];
    uint16_t count;
    uint32_t length,flags;
    // MRU-to-LRU slot indexes. Moving indexes never copies sector contents.
    uint8_t order[OR_CACHE_SLOTS];
    uint8_t *fetch(uint32_t off,uint8_t lane);
    uint8_t absolute(uint32_t off,uint8_t lane);
    uint32_t le32(uint32_t off);
};
}
