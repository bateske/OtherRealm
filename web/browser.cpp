#include "../engine/aw_session.h"
#include <emscripten/emscripten.h>
#include <string.h>
using namespace otherrealm;
EM_JS(void,playTone,(uint16_t hz,uint16_t ms),{if(Module.playTone)Module.playTone(hz,ms);});
EM_JS(void,enableSound,(int enabled),{if(Module.enableSound)Module.enableSound(!!enabled);});
class Browser:public Device {
public:
    const uint8_t*data=0;uint32_t size=0,delay=0;
    bool readSector(uint32_t s,uint8_t*d)override{uint32_t off=s*512;if(off>size||size-off<512)return false;memcpy(d,data+off,512);return true;}
    void display(const uint8_t*,const uint16_t*)override{}
    void waitFrame(uint16_t ms)override{delay+=ms;}
    void tone(uint16_t hz,uint16_t ms)override{playTone(hz,ms);}
    void soundEnabled(bool enabled)override{enableSound(enabled);}
};
static Browser device;
static Game game(device);
EM_JS(int,loadSave,(uint32_t id,uint8_t *data,uint16_t size),{
    try{const a=JSON.parse(localStorage.getItem('otherrealm.save.v1.'+(id>>>0)));
    if(!Array.isArray(a)||a.length!==size||!a.every(x=>Number.isInteger(x)&&x>=0&&x<256))return 0;
    HEAPU8.set(a,data);return 1;}catch(e){return 0;}
});
EM_JS(int,storeSave,(uint32_t id,const uint8_t *data,uint16_t size),{
    try{localStorage.setItem('otherrealm.save.v1.'+(id>>>0),JSON.stringify(Array.from(HEAPU8.subarray(data,data+size))));return 1;}catch(e){return 0;}
});
class BrowserStorage:public SaveStorage {
    bool load(uint32_t id,uint8_t*data,uint16_t size)override{return loadSave(id,data,size);}
    bool store(uint32_t id,const uint8_t*data,uint16_t size)override{return storeSave(id,data,size);}
};
static BrowserStorage storage;
static Session session(game,storage);
static uint16_t screen[128*128];
extern "C" {
EMSCRIPTEN_KEEPALIVE int or_init(const uint8_t*data,uint32_t n,int part){device.data=data;device.size=n;device.delay=0;return session.begin(part);}
EMSCRIPTEN_KEEPALIVE int or_buttons(int buttons){device.delay=0;return session.update(buttons);}
EMSCRIPTEN_KEEPALIVE int or_tick(int d,int action){return or_buttons((d&1?32:0)|(d&2?16:0)|(d&4?8:0)|(d&8?4:0)|(action?1:0));}
EMSCRIPTEN_KEEPALIVE int or_menu(){return session.visible();}
EMSCRIPTEN_KEEPALIVE int or_title(){return session.isTitle();}
EMSCRIPTEN_KEEPALIVE int or_sound(){return session.soundOn();}
EMSCRIPTEN_KEEPALIVE int or_animate(uint32_t ms){return session.animate(ms);}
EMSCRIPTEN_KEEPALIVE int or_saved(){return session.hasSave();}
EMSCRIPTEN_KEEPALIVE int or_completed(){return session.hasCompleted();}
EMSCRIPTEN_KEEPALIVE int or_save_failed(){return session.saveFailed();}
EMSCRIPTEN_KEEPALIVE int or_exit(){return session.exitRequested();}
EMSCRIPTEN_KEEPALIVE int or_hold(){return session.exitHoldProgress();}
EMSCRIPTEN_KEEPALIVE uint32_t or_identity(){return session.packIdentity();}
EMSCRIPTEN_KEEPALIVE const uint16_t*or_screen(){
    for(unsigned y=0;y<128;++y)session.paintRow(screen+y*128,y);
    return screen;
}
EMSCRIPTEN_KEEPALIVE int or_part(){return game.part;}
EMSCRIPTEN_KEEPALIVE int or_frames(){return game.frames;}
EMSCRIPTEN_KEEPALIVE int or_delay(){return device.delay;}
EMSCRIPTEN_KEEPALIVE int or_width(){return Width;}
EMSCRIPTEN_KEEPALIVE int or_height(){return Height;}
EMSCRIPTEN_KEEPALIVE const uint8_t*or_frame(){return game.renderer.frame();}
EMSCRIPTEN_KEEPALIVE const uint16_t*or_palette(){return game.renderer.colors();}
EMSCRIPTEN_KEEPALIVE const char*or_error(){return game.fault();}
}
