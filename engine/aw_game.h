#pragma once
#include "aw_render.h"
#include "aw_vm.h"
#include "aw_music.h"
namespace otherrealm {
class Game:public VmHost,public MusicReader {
public:
    explicit Game(Device &device):pack(device),renderer(pack),vm(*this),tracker(*this),part(0),frames(0),event(0),pendingPart(0),pendingPalette(255),musicId(0xFFFF){}
    bool begin(uint16_t startPart=16001);
    bool tick(const VmInput &input);
    bool loadPart(uint16_t next,bool preserve=true);
    const char *fault()const;
    Pack pack;
    Renderer renderer;
    Vm vm;
    SilentMusic tracker;
    uint16_t part;
    uint32_t frames;
    uint8_t event;
    uint8_t readCode(uint16_t off)override{return pack.byte(code,off,0);}
    void drawShape(bool secondary,uint16_t off,int16_t x,int16_t y,uint16_t zoom)override {renderer.shape(shapes[secondary?1:0],off,x,y,zoom);}
    void setPalette(uint8_t p)override{pendingPalette=p;}
    void selectPage(uint8_t p)override{renderer.select(p);}
    void fillPage(uint8_t p,uint8_t c)override{renderer.fill(p,c);}
    void copyPage(uint8_t s,uint8_t d,int16_t y)override{renderer.copy(s,d,y);}
    void present(uint8_t p,uint16_t ms)override;
    void drawString(uint16_t id,uint8_t x,uint8_t y,uint8_t c)override{renderer.string(id,x,y,c);}
    void drawNativeText(uint16_t id,uint8_t x,uint8_t y,uint8_t c)override{renderer.nativeText(pack.entry(id),x,y,c);}
    void systemEvent(uint8_t e)override{event=e;}
    void updateResource(uint16_t id)override;
    void music(uint16_t id,uint16_t delay,uint8_t pos)override;
    void sound(uint16_t id,uint8_t frequency,uint8_t volume,uint8_t channel)override;
    uint32_t resourceSize(uint16_t id)override;
    uint8_t readResourceByte(uint16_t id,uint32_t off)override;
private:
    Resource code,palettes,shapes[2],module;
    uint16_t pendingPart;
    uint8_t pendingPalette;
    uint16_t musicId;
};
}
