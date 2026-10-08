// SPDX-License-Identifier: GPL-2.0-or-later
// Deterministic, silent footage from the real Session/VM/renderer, not mock art.
#include "../engine/aw_session.h"
#include <stdio.h>
#include <stdlib.h>
using namespace otherrealm;
struct Capture:Device {
    FILE *file;uint32_t elapsed;
    Capture(const char *p):file(fopen(p,"rb")),elapsed(0){}
    ~Capture(){if(file)fclose(file);}
    bool readSector(uint32_t s,uint8_t*d){return file&&!fseek(file,long(s)*512,SEEK_SET)&&fread(d,1,512,file)==512;}
    void display(const uint8_t*,const uint16_t*){}
    void waitFrame(uint16_t ms){elapsed+=ms;}
};
struct NoSave:SaveStorage {
    bool load(uint32_t,uint8_t*,uint16_t){return false;}
    bool store(uint32_t,const uint8_t*,uint16_t){return true;}
};
static void write32(FILE*f,uint32_t v){for(unsigned i=0;i<4;++i)fputc(uint8_t(v>>(i*8)),f);}
int main(int argc,char**argv){
    if(argc!=3){fprintf(stderr,"record_gameplay PACK OUTPUT.orgf\n");return 2;}
    Capture device(argv[1]);static Game game(device);NoSave storage;Session session(game,storage);
    if(!session.begin())return 1;
    FILE*out=fopen(argv[2],"wb");if(!out)return 1;
    fwrite("ORGF",1,4,out);write32(out,128);write32(out,128);
    uint32_t time=0,count=0;uint16_t row[128];
    auto frame=[&](uint32_t duration,unsigned chapter){
        write32(out,duration);write32(out,chapter);write32(out,game.vm.tickCount());
        for(unsigned y=0;y<128;++y){session.paintRow(row,y);for(unsigned x=0;x<128;++x){fputc(row[x]&255,out);fputc(row[x]>>8,out);}}
        ++count;
    };
    for(unsigned i=0;i<32;++i){time+=60;session.animate(time);frame(60,0);}
    session.update(Session::A,false);session.update(0,false);
    for(unsigned i=0;i<5500&&game.part==16001;++i){
        uint32_t before=device.elapsed;
        if(!session.update(0)){fprintf(stderr,"Intro: %s\n",game.fault());return 1;}
        unsigned dt=device.elapsed-before;if(!dt)dt=20;time+=dt;session.animate(time);frame(dt,1);
    }
    if(game.part!=16002){fprintf(stderr,"Intro did not reach pool\n");return 1;}
    for(unsigned i=0;i<390&&!session.visible();++i){
        uint8_t buttons=0;
        if(game.vm.variable(0x67)!=2&&i<170)buttons=Session::Up|Session::A;
        else if(i>=170)buttons=Session::Right|Session::A;
        uint32_t before=device.elapsed;
        if(!session.update(buttons)){fprintf(stderr,"Gameplay: %s\n",game.fault());return 1;}
        unsigned dt=device.elapsed-before;if(!dt)dt=20;time+=dt;session.animate(time);frame(dt,2);
        if(i==215){
            session.update(Session::Start,false);session.update(0,false);
            for(unsigned j=0;j<55;++j){
                if(j==22){session.update(Session::Down,false);session.update(0,false);}
                if(j==34){session.update(Session::Up,false);session.update(0,false);}
                time+=60;session.animate(time);frame(60,3);
            }
            session.update(Session::B,false);session.update(0,false);
            for(unsigned j=0;j<16;++j){time+=60;session.animate(time);frame(60,3);}
        }
    }
    fclose(out);printf("Recorded %u real engine frames; %u ms, final part %u.\n",count,time,game.part);return 0;
}
