// Host-side sector-cache coherency and resolution contract regression tests.
#include "../engine/aw_pack.h"
#include <stdio.h>
#include <stdlib.h>
using namespace otherrealm;
static void check(bool ok,const char*message){if(!ok){fprintf(stderr,"FAIL %s\n",message);exit(1);}}
struct MemoryDevice:Device {
    uint8_t data[8192];unsigned reads=0;bool broken=false;
    void put32(unsigned off,uint32_t value){for(unsigned i=0;i<4;++i)data[off+i]=value>>(i*8);}
    MemoryDevice(){for(unsigned i=0;i<sizeof(data);++i)data[i]=uint8_t(i*13+(i>>9)*17);put32(0,0x3157524f);put32(4,0x00200001);put32(8,4);put32(12,32);put32(16,512);put32(20,sizeof(data));put32(24,5);put32(28,0);}
    bool readSector(uint32_t sector,uint8_t*out)override{++reads;if(broken||sector>=16)return false;memcpy(out,data+sector*512,512);return true;}
    void display(const uint8_t*,const uint16_t*)override{}
    void waitFrame(uint16_t)override{}
};
int main(){
    MemoryDevice d;Pack p(d);check(p.open(),"resolution-independent vector scene map");
    Resource r={512,7680,5,1};
    uint32_t random=12345;
    for(unsigned i=0;i<10000;++i){random=random*1664525u+1013904223u;unsigned off=(random>>8)%r.size;check(p.byte(r,off,i&1)==d.data[r.offset+off],"mixed code/data reads remain coherent");}
    uint8_t copy[1700];check(p.copy(r,511,copy,sizeof(copy)),"copy across sectors");check(!memcmp(copy,d.data+1023,sizeof(copy)),"copied sector bytes correct");
    memset(p.scratch(),0xff,512);check(p.byte(r,5)==d.data[517],"scratch invalidates every cache tag");
#if OR_CODE_SLOTS == 0
    p.invalidate();unsigned firstReads=p.reads;
    p.byte(r,0,0);p.byte(r,512,1);p.byte(r,1024,0);p.byte(r,0,1);p.byte(r,512,0);p.byte(r,1024,1);
    check(p.reads-firstReads==3,"unified cache shares sectors across access lanes");
#endif
    d.put32(24,3);d.put32(28,(uint32_t(Height)<<16)|Width);check(p.open(),"matching converted dimensions");
    d.put32(28,(uint32_t(Height)<<16)|(Width+8));check(!p.open(),"mismatched converted dimensions rejected");
    d.put32(24,1);d.put32(28,0);check(!p.open(),"raw planar legacy pack rejected by packed renderer");
    d.put32(24,5);check(p.open(),"reopen after invalid pack");
    p.byte(r,r.size);check(p.error!=0,"resource bounds checked");
    check(p.open(),"reopen clears prior fault");d.broken=true;p.invalidate();p.byte(r,0);check(p.error!=0,"device read failure recorded");
    printf("Pack PASS: mixed cache lanes, copies, scratch invalidation, scene flags, bounds and SD errors. sizeof_pack=%zu\n",sizeof(Pack));
    return 0;
}
