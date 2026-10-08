#include "aw_pack.h"
namespace otherrealm {
uint8_t *Pack::fetch(uint32_t off,uint8_t lane) {
    if(error) return blocks[0];
    uint32_t tag=off>>9;
#if OR_CODE_SLOTS
    const unsigned first=lane?OR_CODE_SLOTS:0;
    const unsigned end=lane?OR_CACHE_SLOTS:OR_CODE_SLOTS;
#else
    (void)lane;
    const unsigned first=0,end=OR_CACHE_SLOTS;
#endif
    unsigned position=first;
    // Sequential byte fetches take this single comparison fast path.
    if(tags[order[first]]==tag){++hits;return blocks[order[first]];}
    while(++position<end && tags[order[position]]!=tag){}
    bool found=position<end;
    if(!found)position=end-1;
    const unsigned slot=order[position];
    if(!found){
        if(!device.readSector(tag,blocks[slot])){error="SD read failed";return blocks[0];}
        tags[slot]=tag;++reads;
    }else ++hits;
    for(unsigned i=position;i>first;--i)order[i]=order[i-1];
    order[first]=uint8_t(slot);
    return blocks[slot];
}
uint8_t Pack::absolute(uint32_t off,uint8_t lane) {return fetch(off,lane)[off&511];}
uint32_t Pack::le32(uint32_t off) {
    uint32_t v=0;for(uint8_t i=0;i<4;++i)v|=uint32_t(absolute(off+i,1))<<(8*i);return v;
}
bool Pack::open() {
    error=0; invalidate();
    if(le32(0)!=0x3157524Fu || le32(4)!=0x00200001u || le32(12)!=32) {error="Invalid ORW1 pack";return false;}
    uint32_t n=le32(8);length=le32(20);
    if(!n||n>65535||length<32+n*16u) {error="Invalid resource table";return false;}
    count=uint16_t(n);
    uint32_t dimensions=le32(28);
    flags=le32(24);
    if(flags&2){
        if(dimensions!=(uint32_t(Height)<<16|Width)){error="Pack resolution mismatch";return false;}
    }else if(!(flags&4)||dimensions){error="Pack resolution mismatch";return false;}
    return !error;
}
Resource Pack::entry(uint16_t id) {
    Resource r={0,0,0,0};
    if(id>=count) {error="Resource ID out of range";return r;}
    uint32_t p=32+uint32_t(id)*16;
    r.offset=le32(p);r.size=le32(p+4);uint32_t tf=le32(p+12);r.type=tf&65535;r.flags=tf>>16;
    if(r.size&&(r.offset>length||r.size>length-r.offset))error="Resource exceeds pack";
    return r;
}
uint32_t Pack::identity() {
    uint32_t hash=2166136261u;
    for(uint16_t i=0;i<count&&!error;++i){
        Resource r=entry(i);
        if(r.type==9&&r.size==8){
            uint8_t data[8];
            if(copy(r,0,data,8)&&!memcmp(data,"ORID",4))
                return uint32_t(data[4])|uint32_t(data[5])<<8|uint32_t(data[6])<<16|uint32_t(data[7])<<24;
        }
        // Resource CRCs and types bind saves to the installed game's content.
        for(unsigned b=0;b<8;++b){hash^=absolute(32+uint32_t(i)*16+8+b,1);hash*=16777619u;}
    }
    return hash;
}
uint8_t Pack::byte(const Resource&r,uint32_t off,uint8_t lane) {
    if(off>=r.size) {error="Resource read out of range";return 0;}
    return absolute(r.offset+off,lane);
}
bool Pack::copy(const Resource&r,uint32_t off,uint8_t*dst,uint32_t n) {
    if(off>r.size||n>r.size-off){error="Resource copy out of range";return false;}
    while(n&&!error) {
        const uint8_t *block=fetch(r.offset+off,1);
        uint32_t at=(r.offset+off)&511, take=512-at;if(take>n)take=n;
        if(error)return false;
        memcpy(dst,block+at,take);dst+=take;off+=take;n-=take;
    }return !error;
}
}
