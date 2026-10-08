// Synthetic, redistributable renderer and game-restart regression fixtures.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "../engine/aw_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace otherrealm;

static unsigned checks;
static void check(bool value,const char *message){++checks;if(!value){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
static int sx(int x){return x>=0?x*Width/320:-((-x*Width+319)/320);}
static int sy(int y){return y>=0?y*Height/200:-((-y*Height+199)/200);}

struct MemoryDevice : Device {
    uint8_t data[16384];
    const uint8_t *frame;
    uint32_t reads;
    MemoryDevice():frame(0),reads(0){
        memset(data,0,sizeof(data));
        memcpy(data,"ORW1",4);le16(4,1);le16(6,32);le32(8,5);le32(12,32);
        le32(16,512);le32(20,sizeof(data));le32(24,5);
        uint8_t palette[32]={};palette[0]=15;palette[1]=0;palette[2]=0;palette[3]=240;palette[4]=0;palette[5]=15;palette[6]=15;palette[7]=255;
        entry(0,3,512,palette,sizeof(palette));
        const uint8_t polygon[]={0xc3,40,20,4,40,0,40,20,0,20,0,0};
        entry(1,5,1024,polygon,sizeof(polygon));
        const uint8_t code[]={17};entry(2,4,6144,code,sizeof(code));
        const uint8_t brokenMusic[]={0};entry(3,1,6656,brokenMusic,sizeof(brokenMusic));
        const uint8_t directory[]={0x81,0x3e,0,0,2,0,1,0,0,0};
        entry(4,7,7168,directory,sizeof(directory));
    }
    void le16(unsigned at,uint16_t value){data[at]=value&255;data[at+1]=value>>8;}
    void le32(unsigned at,uint32_t value){for(unsigned i=0;i<4;++i)data[at+i]=uint8_t(value>>(i*8));}
    static uint32_t crc(const uint8_t *p,unsigned n){uint32_t c=0xffffffffu;while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^((c&1)?0xedb88320u:0);}return ~c;}
    void entry(unsigned id,unsigned type,unsigned offset,const uint8_t *p,unsigned n){
        unsigned at=32+id*16;le32(at,offset);le32(at+4,n);le32(at+8,crc(p,n));le16(at+12,type);le16(at+14,1);memcpy(data+offset,p,n);
    }
    bool readSector(uint32_t sector,uint8_t *out){if(sector>=sizeof(data)/512)return false;++reads;memcpy(out,data+sector*512,512);return true;}
    void display(const uint8_t *p,const uint16_t *){frame=p;}
    void waitFrame(uint16_t){}
    unsigned pixel(int x,int y)const{return (frame[(y*Width+x)/2]>>((x&1)?0:4))&15;}
};

struct Fixture {
    MemoryDevice device;
    Pack pack;
    Renderer renderer;
    Fixture():pack(device),renderer(pack){check(pack.open(),"synthetic pack opens");}
    Resource shapes(const uint8_t *p,unsigned n){device.entry(1,5,1024,p,n);check(pack.open(),"reload synthetic shapes");renderer.reset();renderer.select(1);return pack.entry(1);}
    Resource native(const uint8_t *p,unsigned n){device.entry(1,8,1024,p,n);check(pack.open(),"reload synthetic native text");renderer.reset();renderer.select(1);return pack.entry(1);}
    void show(uint8_t page=1){renderer.present(page);check(!pack.error&&!renderer.error,"render without fault");}
};

static void testRectangleAndNibbles(){
    Fixture f;Resource shape=f.pack.entry(1);f.renderer.select(1);f.renderer.shape(shape,0,160,100,64);f.show();
    for(int y=0;y<Height;++y)for(int x=0;x<Width;++x){
        bool inside=x>=sx(140)&&x<=sx(180)&&y>=sy(90)&&y<sy(110);
        check(f.device.pixel(x,y)==(inside?3u:0u),"rectangle endpoints and clipping");
    }
    const uint8_t point[]={0xc5,0,1,4,0,0,0,1,0,1,0,0};shape=f.shapes(point,sizeof(point));
    f.renderer.fill(1,2);f.renderer.shape(shape,0,3,4,64);f.show();
    check(f.device.pixel(sx(3),sy(4))==5,"point updates selected nibble");
    check(f.device.pixel(sx(3)^1,sy(4))==2,"point preserves adjacent nibble");
    f.renderer.fill(1,2);f.renderer.shape(shape,0,-1,4,64);f.show();
    check(f.device.pixel(0,sy(4))==2,"negative point does not leak onto left edge");
}

static void testSpecialColorsAndGroups(){
    Fixture f;
    uint8_t rectangle[]={0xd0,40,20,4,40,0,40,20,0,20,0,0};
    Resource shape=f.shapes(rectangle,sizeof(rectangle));f.renderer.fill(1,3);f.renderer.shape(shape,0,160,100,64);f.show();
    check(f.device.pixel(sx(160),sy(100))==11,"color 16 sets high palette bit");
    check(f.device.pixel(0,0)==3,"color 16 preserves outside region");
    rectangle[0]=0xd1;shape=f.shapes(rectangle,sizeof(rectangle));
    f.renderer.fill(0,6);f.renderer.fill(1,2);f.renderer.shape(shape,0,160,100,64);f.show();
    check(f.device.pixel(sx(160),sy(100))==6&&f.device.pixel(0,0)==2,"color 17 restores corresponding page-zero region");
    f.renderer.select(0);f.renderer.shape(shape,0,160,100,64);f.show(0);
    check(f.device.pixel(sx(160),sy(100))==6,"color 17 is safe on page zero itself");
    const uint8_t group[]={2,2,3,0,0x80,5,10,20,6,0, 0xc3,40,20,4,40,0,40,20,0,20,0,0};
    shape=f.shapes(group,sizeof(group));f.renderer.shape(shape,0,150,80,128);f.show();
    check(f.device.pixel(sx(166),sy(114))==6,"nested origin, child position, zoom and explicit color");
}

static void testPagesAndScrolling(){
    Fixture f;for(uint8_t p=0;p<4;++p)f.renderer.fill(p,p+1);
    f.show(0xfe);check(f.device.pixel(0,0)==3,"initial front page is two");
    f.show(0xff);check(f.device.pixel(0,0)==2,"swap presents previous back page");
    f.renderer.fill(0xfe,5);f.renderer.fill(0xff,6);f.show(0xfe);check(f.device.pixel(0,0)==5,"front alias follows swap");
    f.renderer.copy(0xff,3,0);f.show(3);check(f.device.pixel(0,0)==6,"copy resolves back-page alias");
    f.renderer.fill(1,5);f.renderer.fill(2,6);f.renderer.copy(0x81,2,20);f.show(2);
    for(int y=0;y<Height;++y)check(f.device.pixel(0,y)==(y<sy(20)?6u:5u),"positive scroll preserves uncovered destination rows");
    f.renderer.fill(2,6);f.renderer.copy(0x81,2,-20);f.show(2);
    for(int y=0;y<Height;++y)check(f.device.pixel(0,y)==(y<Height+sy(-20)?5u:6u),"negative scroll clips and preserves uncovered rows");
    f.renderer.fill(2,9);f.renderer.copy(0x82,2,20);f.show(2);check(f.device.pixel(0,Height-1)==9,"scrolling same page is a no-op");
    f.renderer.copy(0x81,2,200);f.show(2);check(f.device.pixel(0,0)==9,"out-of-height scroll is ignored");
    f.renderer.copy(0x42,1,0);f.show(1);check(f.device.pixel(0,0)==9,"copy strips bit-six source flag");
}

static void testPalette(){
    Fixture f;f.renderer.palette(f.pack.entry(0),0);
    const uint16_t *p=f.renderer.colors();check(p[0]==0xf800&&p[1]==0x07e0&&p[2]==0x001f&&p[3]==0xffff,"Amiga 12-bit palette to RGB565");
}

static void testMalformedShapes(){
    Fixture f;
    const uint8_t odd[]={0xc3,10,10,5};Resource shape=f.shapes(odd,sizeof(odd));
    f.renderer.shape(shape,0,160,100,64);check(f.renderer.error,"odd vertex count rejected");
    const uint8_t cycle[]={2,0,0,0,0,0,0,0};shape=f.shapes(cycle,sizeof(cycle));
    f.renderer.shape(shape,0,160,100,64);check(f.renderer.error&&strstr(f.renderer.error,"nesting"),"cyclic shape graph is bounded");
    const uint8_t truncated[]={0xc3,10,10,4,0};shape=f.shapes(truncated,sizeof(truncated));
    f.renderer.shape(shape,0,160,100,64);check(f.pack.error,"truncated vertices cannot read past resource");
    const uint8_t descending[]={0xc3,10,10,4,10,10,10,0,0,0,0,10};shape=f.shapes(descending,sizeof(descending));
    f.renderer.shape(shape,0,160,100,64);check(f.renderer.error,"descending polygon edges rejected");
    const uint8_t wide[]={0xc3,255,255,4,255,0,0,0,255,0,0,0};shape=f.shapes(wide,sizeof(wide));
    f.renderer.shape(shape,0,0,0,65535);check(f.renderer.error&&strstr(f.renderer.error,"zoom"),"extreme zoom rejected before fixed-point arithmetic");
    shape=f.shapes(wide,sizeof(wide));f.renderer.shape(shape,0,0,0,4096);
    check(!f.renderer.error&&!f.pack.error,"largest supported zoom handles signed edge deltas without overflow");

    // Four 16-way group levels would expand to 65,536 polygon leaves. Depth
    // alone cannot reject this acyclic resource; the node budget must stop it.
    uint8_t tree[4*68+12]={};
    for(unsigned level=0;level<4;++level){
        unsigned at=level*68;tree[at]=2;tree[at+1]=100;tree[at+2]=100;tree[at+3]=15;
        for(unsigned i=0;i<16;++i){unsigned child=(level+1)*68/2;tree[at+4+i*4]=child>>8;tree[at+5+i*4]=child&255;tree[at+6+i*4]=100;tree[at+7+i*4]=100;}
    }
    const uint8_t leaf[]={0xc3,4,4,4,4,0,4,4,0,4,0,0};memcpy(tree+4*68,leaf,sizeof(leaf));
    shape=f.shapes(tree,sizeof(tree));f.renderer.shape(shape,0,160,100,64);
    check(f.renderer.error&&strstr(f.renderer.error,"budget"),"branching shape graph is bounded");
    check(f.renderer.polygons<8192,"node budget stops expansion before thousands more leaves");
}

static void testGameRestartAfterMusicFault(){
    MemoryDevice device;
    const uint8_t badCode[]={0x1a,0,3,0,0,0,6};device.entry(2,4,6144,badCode,sizeof(badCode));
    Game game(device);VmInput input;
    check(game.begin(16001),"game starts with synthetic scene map");
    check(!game.tick(input)&&game.tracker.error(),"invalid module creates a reported game fault");
    const uint8_t fixedCode[]={17};device.entry(2,4,6144,fixedCode,sizeof(fixedCode));
    check(game.begin(16001)&&!game.fault()&&game.tick(input),"game restart clears prior tracker fault");
}

static void testNativeText(){
    Fixture f;
    uint8_t a[]={'T','X',1,0,3,5,2,0,'A',0};
    Resource resource=f.native(a,sizeof(a));f.renderer.fill(1,6);f.renderer.nativeText(resource,1,2,15);f.show();
    const uint8_t rows[]={2,5,7,5,5};
    for(int y=0;y<Height;++y)for(int x=0;x<Width;++x){
        bool ink=y>=2&&y<7&&x>=1&&x<4&&(rows[y-2]&(4>>(x-1)));
        check(f.device.pixel(x,y)==(ink?15u:6u),"transparent native A uses exact device pixels and preserves neighbors");
    }
    const uint8_t opaque[]={'T','X',1,1,7,11,2,0,'A','\n','B',0};
    resource=f.native(opaque,sizeof(opaque));f.renderer.fill(1,6);f.renderer.nativeText(resource,1,2,15);f.show();
    check(f.device.pixel(1,2)==2&&f.device.pixel(2,2)==15,"opaque box clears behind its glyph");
    check(f.device.pixel(7,12)==2&&f.device.pixel(8,12)==6,"opaque odd-aligned box preserves exterior nibble");
    check(f.device.pixel(1,8)==15&&f.device.pixel(2,8)==15&&f.device.pixel(3,8)==2,"newline places next glyph six rows lower");
    resource=f.native(a,sizeof(a));f.renderer.nativeText(resource,Width-3,Height-5,7);f.show();
    check(f.device.pixel(Width-1,Height-1)==7,"native text may end exactly at frame edge");
    for(unsigned field=0;field<8;++field){
        uint8_t bad[sizeof(a)];memcpy(bad,a,sizeof(a));
        switch(field){case 0:bad[0]='B';break;case 1:bad[1]='Y';break;case 2:bad[2]=2;break;
        case 3:bad[3]=2;break;case 4:bad[4]=0;bad[3]=1;break;case 5:bad[5]=0;break;
        case 6:bad[6]=16;break;case 7:bad[7]=1;break;}
        resource=f.native(bad,sizeof(bad));f.renderer.nativeText(resource,1,2,15);
        check(f.renderer.error,"reject invalid native text header before drawing");
    }
    resource=f.native(a,sizeof(a));f.renderer.nativeText(resource,1,2,16);check(f.renderer.error,"reject invalid native ink color");
    resource=f.native(a,sizeof(a));f.renderer.nativeText(resource,Width-2,Height-5,7);check(f.renderer.error,"reject box that crosses frame edge");
    resource=f.native(a,sizeof(a));resource.type=5;f.renderer.nativeText(resource,1,2,7);check(f.renderer.error,"native text requires resource type eight");
    resource=f.native(a,8);f.renderer.nativeText(resource,1,2,7);check(f.renderer.error,"reject truncated native text header");
    resource=f.native(a,9);f.renderer.nativeText(resource,1,2,7);check(f.renderer.error,"reject unterminated native text payload");
    a[8]=31;resource=f.native(a,sizeof(a));f.renderer.nativeText(resource,1,2,7);check(f.renderer.error,"reject unsupported control characters");a[8]='A';
    a[4]=2;resource=f.native(a,sizeof(a));f.renderer.nativeText(resource,1,2,7);check(f.renderer.error,"glyph width must fit declared box");a[4]=3;
    a[5]=4;resource=f.native(a,sizeof(a));f.renderer.nativeText(resource,1,2,7);check(f.renderer.error,"glyph height must fit declared box");
}

int main(){
    testRectangleAndNibbles();testSpecialColorsAndGroups();testPagesAndScrolling();
    testPalette();testMalformedShapes();testGameRestartAfterMusicFault();testNativeText();
    printf("Renderer PASS: %u checks, %dx%d, clipping/nibbles/pages/scroll/palette/shape bounds/restart/native text.\n",checks,Width,Height);
    return 0;
}
