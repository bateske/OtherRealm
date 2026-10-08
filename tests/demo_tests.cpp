// Exercises the original, freely redistributable SD game through VM input.
// No game logic is implemented here; assertions inspect the script variables.
#include "../engine/aw_game.h"
#include <stdio.h>
#include <stdlib.h>

using namespace otherrealm;
class DemoDevice : public Device {
public:
    FILE *file;
    uint32_t frames, elapsed;
    const uint8_t *pixels;
    const uint16_t *palette;
    DemoDevice(const char *path) : file(fopen(path,"rb")), frames(0), elapsed(0), pixels(0), palette(0) {}
    ~DemoDevice() { if(file) fclose(file); }
    bool readSector(uint32_t sector,uint8_t *out) {
        return file && !fseek(file,long(sector)*512,SEEK_SET) && fread(out,1,512,file)==512;
    }
    void display(const uint8_t *p,const uint16_t *c) { pixels=p;palette=c;++frames; }
    void waitFrame(uint16_t ms) { elapsed+=ms; }
    void save(const char *path) {
        if(!pixels)return;
        FILE *f=fopen(path,"wb");if(!f)return;
        fprintf(f,"P6\n%d %d\n255\n",Width,Height);
        for(int i=0;i<Width*Height;++i){
            uint16_t c=palette[(pixels[i/2]>>((i&1)?0:4))&15];
            unsigned char rgb[]={static_cast<unsigned char>(((c>>11)&31)*255/31),
                static_cast<unsigned char>(((c>>5)&63)*255/63),static_cast<unsigned char>((c&31)*255/31)};
            fwrite(rgb,1,3,f);
        }
        fclose(f);
    }
};

static void check(bool condition,const char *message) {
    if(!condition){fprintf(stderr,"FAIL: %s\n",message);exit(1);}
}
static void step(Game &game,uint8_t directions=0,bool action=false) {
    VmInput input;input.directions=directions;input.action=action;
    if(!game.tick(input)){
        fprintf(stderr,"FAULT part=%u pc=%u task=%u: %s\n",game.part,game.vm.faultPc(),game.vm.faultTask(),game.fault());exit(1);
    }
}
static void moveToScene(Game &game,uint8_t direction,uint16_t part) {
    for(int i=0;i<200 && game.part!=part;++i)step(game,direction);
    check(game.part==part,"scene transition");step(game);
}
static void moveToX(Game &game,int target) {
    for(int i=0;i<100 && abs(game.vm.variable(0)-target)>3;++i)
        step(game,game.vm.variable(0)<target?VmInput::Right:VmInput::Left);
    check(abs(game.vm.variable(0)-target)<=3,"walk to coordinate");
}
static void action(Game &game) { step(game,0,false);step(game,0,true);step(game,0,false); }

int main(int argc,char **argv) {
    DemoDevice device(argc>1?argv[1]:"demo/sd/OTHERWRL.PAK");
    static Game game(device);
    check(game.begin(16001),"begin original demo");step(game);
    check(game.pack.hasSceneMap(),"scene map extension active");
    check(game.vm.variable(0)==42&&game.vm.variable(3)==0,"initial player and inventory");
    device.save("build/demo-start.ppm");
    step(game,VmInput::Up);check(game.vm.variable(8)<155,"jump responds to up");
    for(int i=0;i<30;++i)step(game);
    check(game.vm.variable(8)==155&&game.vm.variable(9)==0,"jump lands on ground");
    moveToScene(game,VmInput::Right,16010);device.save("build/demo-shrine.ppm");
    moveToScene(game,VmInput::Right,16011);
    for(int i=0;i<100;++i)step(game,VmInput::Right);
    action(game);
    check(game.vm.variable(0)==230&&game.vm.variable(4)==0&&game.vm.variable(6)==0,"locked gate blocks without crystal");
    device.save("build/demo-locked.ppm");
    moveToScene(game,VmInput::Left,16010);moveToX(game,160);action(game);
    check(game.vm.variable(3)==1,"action collects crystal only near shrine");
    device.save("build/demo-crystal.ppm");
    moveToScene(game,VmInput::Right,16011);moveToX(game,202);action(game);
    check(game.vm.variable(4)==1&&game.vm.variable(3)==1,"crystal unlocks gate across scene transition");
    device.save("build/demo-gate.ppm");
    for(int i=0;i<50&&!game.vm.variable(6);++i)step(game,VmInput::Right);
    check(game.vm.variable(6)==1,"walk through open gate reaches victory");
    device.save("build/demo-win.ppm");
    action(game);step(game);
    check(game.part==16001&&game.vm.variable(3)==0&&game.vm.variable(4)==0&&game.vm.variable(6)==0,"victory action restarts cleanly");
    printf("Demo PASS: jump, all 3 SD scenes, blocked gate, backtracking, crystal pickup, unlock, victory, restart. frames=%u game_ms=%u reads=%u\n",device.frames,device.elapsed,game.pack.reads);
    return 0;
}
