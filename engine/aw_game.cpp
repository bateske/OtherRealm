#include "aw_game.h"
namespace otherrealm {
void Game::sound(uint16_t id,uint8_t frequency,uint8_t volume,uint8_t channel){
    // Lightweight piezo cues, not PCM playback. MOD timing stays in the tracker.
    (void)channel;
    if(volume)pack.device.tone(uint16_t(180+(id%17)*35+frequency*12),35+volume);
}
bool Game::begin(uint16_t p){
    frames=0;event=0;pendingPart=0;musicId=0xFFFF;renderer.reset();tracker.reset();
    return pack.open()&&loadPart(p,false);
}
bool Game::loadPart(uint16_t p,bool preserve){
    static const uint8_t ids[][4]={{20,21,22,0},{23,24,25,0},{26,27,28,17},{29,30,31,17},{32,33,34,17},{35,36,37,0},{38,39,40,17},{41,42,43,17},{125,126,127,0},{125,126,127,0}};
    uint16_t r[4]={0,0,0,0};
    if(pack.hasSceneMap()){
        Resource directory=pack.entry(pack.resourceCount()-1);
        if(directory.type!=7||directory.size%10){pack.error="Invalid scene directory";return false;}
        bool found=false;
        for(uint32_t off=0;off<directory.size&&!pack.error;off+=10){
            uint16_t id=pack.byte(directory,off)|(uint16_t(pack.byte(directory,off+1))<<8);
            if(id!=p)continue;
            for(unsigned i=0;i<4;++i)r[i]=pack.byte(directory,off+2+i*2)|(uint16_t(pack.byte(directory,off+3+i*2))<<8);
            found=true;break;
        }
        if(!found){pack.error="Scene not found in SD directory";return false;}
    }else{
        if(p<16000||p>16009){pack.error="Unsupported game part";return false;}
        for(unsigned i=0;i<4;++i)r[i]=ids[p-16000][i];
    }
    palettes=pack.entry(r[0]);code=pack.entry(r[1]);shapes[0]=pack.entry(r[2]);
    if(r[3])shapes[1]=pack.entry(r[3]);else shapes[1]=shapes[0];
    tracker.stop();pendingPalette=255;part=p;pendingPart=0;event=0;
    return !pack.error&&vm.reset(code.size,p,preserve);
}
const char*Game::fault()const{return pack.error?pack.error:renderer.error?renderer.error:vm.error()?vm.error():tracker.error();}
bool Game::tick(const VmInput&input){
    if(fault())return false;
    uint32_t before=frames;
    if(!vm.tick(input)||fault())return false;
    if(pendingPart&&!loadPart(pendingPart))return false;
    if(frames==before){pack.device.waitFrame(20);tracker.advance(20,vm.variable(0xF4));}
    return !fault();
}
void Game::present(uint8_t p,uint16_t ms){
    if(fault())return;
    if(pendingPalette!=255){renderer.palette(palettes,pendingPalette);pendingPalette=255;}
    if(fault())return;
    renderer.present(p);++frames;pack.device.waitFrame(ms);
    tracker.advance(ms,vm.variable(0xF4));
}
void Game::updateResource(uint16_t id){
    if(id>=16000){pendingPart=id;return;}
    if(!id){tracker.stop();return;}
    Resource r=pack.entry(id);
    if(r.type==2)renderer.bitmap(r);
    // Code/shapes/music stay on SD; sound samples need no loading in silent mode.
}
void Game::music(uint16_t id,uint16_t delay,uint8_t pos){
    if(id){musicId=id;module=pack.entry(id);tracker.play(id,delay,pos);}
    else if(delay)tracker.setDelay(delay);else tracker.stop();
}
uint32_t Game::resourceSize(uint16_t id){if(id!=musicId){musicId=id;module=pack.entry(id);}return module.size;}
uint8_t Game::readResourceByte(uint16_t id,uint32_t off){if(id!=musicId)resourceSize(id);return pack.byte(module,off);}
}
