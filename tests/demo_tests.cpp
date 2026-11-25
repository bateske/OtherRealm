// Replays the complete public setup adventure through real VM inputs.
#include "../engine/aw_game.h"
#include <stdio.h>
#include <stdlib.h>
using namespace otherrealm;
class DemoDevice:public Device {
public:
    FILE *file;unsigned frames;const uint8_t *pixels;const uint16_t *palette;
    explicit DemoDevice(const char *path):file(fopen(path,"rb")),frames(0),pixels(0),palette(0){}
    ~DemoDevice(){if(file)fclose(file);}
    bool readSector(uint32_t sector,uint8_t *out){return file&&!fseek(file,long(sector)*512,SEEK_SET)&&fread(out,1,512,file)==512;}
    void display(const uint8_t *p,const uint16_t *c){pixels=p;palette=c;++frames;}
    void waitFrame(uint16_t){}
    void save(const char *name){
        char path[128];snprintf(path,sizeof(path),"build/guide-%s.ppm",name);
        FILE *out=fopen(path,"wb");if(!out||!pixels){if(out)fclose(out);return;}
        fprintf(out,"P6\n%d %d\n255\n",Width,Height);
        for(int i=0;i<Width*Height;++i){uint16_t c=palette[(pixels[i/2]>>((i&1)?0:4))&15];
            unsigned char rgb[]={static_cast<unsigned char>((c>>11)*255/31),static_cast<unsigned char>(((c>>5)&63)*255/63),static_cast<unsigned char>((c&31)*255/31)};
            fwrite(rgb,1,3,out);}
        fclose(out);
    }
};
static void check(bool value,const char *message){if(!value){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
static void step(Game &game,uint8_t direction=0,bool action=false){
    VmInput input;input.directions=direction;input.action=action;
    if(!game.tick(input)){fprintf(stderr,"FAULT part=%u pc=%u: %s\n",game.part,game.vm.faultPc(),game.fault());exit(1);}
}
static void action(Game &g){step(g);step(g,0,true);step(g);}
static void moveX(Game &g,int target){
    for(unsigned i=0;i<100&&abs(g.vm.variable(0)-target)>3;++i)step(g,g.vm.variable(0)<target?VmInput::Right:VmInput::Left);
    check(abs(g.vm.variable(0)-target)<=3,"walk to station");
}
static void go(Game &g,uint16_t part,uint8_t direction=VmInput::Right){
    for(unsigned i=0;i<150&&g.part!=part;++i)step(g,direction);
    check(g.part==part,"walk between guide scenes");step(g);
}
int main(int argc,char **argv){
    DemoDevice device(argc>1?argv[1]:"demo/sd/OTHERWRL.PAK");static Game g(device);
    check(g.begin(),"begin guide");step(g);
    check(g.pack.hasSceneMap()&&g.vm.variable(4)==1&&g.vm.variable(7)==0,"setup identity opens automatically");
    device.save("intro");
    for(unsigned i=0;i<20;++i)step(g,VmInput::Right,true);
    check(g.vm.variable(0)==40&&g.vm.variable(4)==2,"reading blocks movement and held action advances only once");
    action(g);action(g);check(!g.vm.variable(4),"all intro cards close");
    action(g);check(!g.vm.variable(4)&&!g.vm.variable(3),"remote action cannot complete a station");
    step(g,VmInput::Up);check(g.vm.variable(8)<171,"jump works");
    for(unsigned i=0;i<30;++i)step(g);
    check(g.vm.variable(8)==171,"jump lands");
    const uint16_t parts[]={16001,16010,16011,16012,16013};
    const unsigned pages[]={2,3,4,4,4};
    const char *names[]={"arrival","terminal","archive","workshop","dock"};
    for(unsigned room=0;room<5;++room){
        check(g.part==parts[room],"correct route order");
        for(unsigned i=0;i<100;++i)step(g,VmInput::Right);
        check(g.part==parts[room]&&g.vm.variable(0)==280&&g.vm.variable(3)==int(room),"unread station gates next room");
        moveX(g,215);device.save(names[room]);
        step(g,VmInput::Up,true);check(!g.vm.variable(4),"jumping past a prop cannot activate it");
        for(unsigned i=0;i<30;++i)step(g);
        action(g);check(g.vm.variable(4)==1&&g.vm.variable(7)==1,"nearby action opens station instructions");
        for(unsigned page=0;page<pages[room];++page){
            char name[80];snprintf(name,sizeof(name),"%s-page-%u",names[room],page+1);device.save(name);
            check(g.vm.variable(3)==int(room),"step only completes after last card");
            action(g);
        }
        check(!g.vm.variable(4)&&g.vm.variable(3)==int(room+1),"reading activates prop and unlocks route");
        char name[80];snprintf(name,sizeof(name),"%s-active",names[room]);device.save(name);
        if(room==2){
            go(g,16010,VmInput::Left);moveX(g,215);action(g);
            for(unsigned page=0;page<3;++page)action(g);
            check(g.vm.variable(3)==3,"rereading preserves carried disk crate and progress");go(g,16011);
        }
        if(room<4)go(g,parts[room+1]);
    }
    for(unsigned i=0;i<60&&!g.vm.variable(6);++i)step(g,VmInput::Right);
    check(g.vm.variable(6)&&g.vm.variable(7)==2&&g.vm.variable(4)==1&&g.event==3,"exit marks guide completion");step(g);device.save("complete");
    action(g);device.save("checklist");action(g);
    check(!g.vm.variable(4)&&g.vm.variable(3)==5&&g.part==16013,"ending returns to dock without pretending to load commercial data");
    go(g,16012,VmInput::Left);check(g.vm.variable(3)==5,"completed guide can be revisited");
    check(g.begin(),"new game resets guide");step(g);
    check(g.vm.variable(3)==0&&g.vm.variable(6)==0&&g.vm.variable(4)==1,"new game reopens explicit intro");
    printf("Guide PASS: five stations, all instruction pages, held inputs, gated route, jump, backtracking, completion and new game; %u frames.\n",device.frames);
}
