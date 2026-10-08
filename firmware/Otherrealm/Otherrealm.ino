// Otherrealm for CHGame Rev0, with lightweight piezo cues.
#include <Arduino.h>
#include <stdlib.h>
#include "Buttons.h"
#include "Display.h"
#include "Card.h"
#include "Persistence.h"
#include "aw_session.h"
#include "Audio.h"
AUDIO_STEPS(UI_CLICK) = { AUDIO_STEP(1100,0,12) };
static const audio::Effect uiEffects[] = { AUDIO_EFFECT(UI_CLICK,audio::SOFT) };

class Handheld:public otherrealm::Device {
public:
    device::Card card;
    uint32_t paceAnchor=0, displayUs=0, waitUs=0;
    bool readSector(uint32_t block,uint8_t *dst) override { return card.readSector(block,dst); }
    void tone(uint16_t hz,uint16_t ms) override {audio::blip(hz,ms,true);}
    void soundEnabled(bool on) override {audio::setOn(on);}
    void display(const uint8_t *pixels,const uint16_t *palette) override {
        uint32_t start=micros();
        device::displayPresent(pixels,otherrealm::Width,otherrealm::Height,palette);
        displayUs+=micros()-start;
    }
    void waitFrame(uint16_t ms) override {
        uint32_t start=micros();
        uint32_t deadline=paceAnchor+uint32_t(ms)*1000;
        while(int32_t(deadline-micros())>0) {}
        paceAnchor=micros();
        waitUs+=paceAnchor-start;
    }
};
static Handheld handheld;
static otherrealm::Game game(handheld);
class FlashSave:public otherrealm::SaveStorage {
public:
    bool load(uint32_t identity,uint8_t *data,uint16_t size) override {
        return device::saveLoad(identity,data,size);
    }
    bool store(uint32_t identity,const uint8_t *data,uint16_t size) override {
        return device::saveStore(identity,data,size);
    }
};
static FlashSave storage;
static otherrealm::Session session(game,storage);
static bool ready=false;
static uint32_t ticks=0,lastUs=0,maxUs=0;
#ifdef OTHERREALM_DEBUG
static uint32_t uiUs=0;
#endif

void menuRow(uint16_t *pixels,uint8_t y,void*){session.paintRow(pixels,y);}
void redraw() {
    if(!session.takeRedraw())return;
    if(session.visible()){
#ifdef OTHERREALM_DEBUG
        uint32_t start=micros();
#endif
        device::displayUi(menuRow,nullptr);
#ifdef OTHERREALM_DEBUG
        uiUs=micros()-start;
#endif
    }
    else {
        // Replace UI and letterbox margins in one scan, without clearing the
        // whole panel to black between the menu and the restored scene.
        device::displayUi(menuRow,nullptr);
        handheld.paceAnchor=micros();
    }
}

#ifdef OTHERREALM_DEBUG
static char command[24]; // Longest debug command is well below 24 bytes.
static uint8_t commandLength=0,injected=0;
static bool lockstep=false;
static uint32_t steps=0;
extern "C" uint32_t _susrstack[], _eusrstack[];

uint32_t stackUsed() {
    const uint32_t *p=_susrstack;
    while(p<_eusrstack&&*p==0xA5A5A5A5u)++p;
    return reinterpret_cast<uintptr_t>(_eusrstack)-reinterpret_cast<uintptr_t>(p);
}

void report() {
    Serial.print("PERF part=");Serial.print(game.part);
    Serial.print(" frames=");Serial.print(game.frames);
    Serial.print(" ticks=");Serial.print(ticks);
    Serial.print(" last_us=");Serial.print(lastUs);
    Serial.print(" max_us=");Serial.print(maxUs);
    Serial.print(" sd_reads=");Serial.print(handheld.card.reads());
    Serial.print(" sd_us=");Serial.print(handheld.card.readMicros());
    Serial.print(" display_us=");Serial.print(handheld.displayUs);
    Serial.print(" wait_us=");Serial.print(handheld.waitUs);
    Serial.print(" polys=");Serial.print(game.renderer.polygons);
    Serial.print(" instructions=");Serial.print(game.vm.instructionsLastTick());
    Serial.print(" stack=");Serial.print(stackUsed());
    Serial.print(" menu=");Serial.print(session.visible());
    Serial.print(" title=");Serial.print(session.isTitle());
    Serial.print(" sound=");Serial.print(audio::on());
    Serial.print(" ui_us=");Serial.print(uiUs);
    Serial.print(" saved=");Serial.print(session.hasSave());
    Serial.print(" completed=");Serial.print(session.hasCompleted());
    Serial.print(" save_failed=");Serial.print(session.saveFailed());
    Serial.print(" writes=");Serial.print(device::saveWrites());
    Serial.print(" fault=");Serial.println(game.fault()?game.fault():"none");
}
void handleCommand() {
    switch(command[0]) {
    case '?':Serial.println("OTHER REALM 0.4.2");break;
    case 'P':report();break;
    case 'T':
        audio::blip(880,160,true);
        delay(3); // The 1 kHz sequencer programs PWM on its next interrupt.
        Serial.print("TONE pwm=");Serial.print(TIM1->CH2CVR);
        Serial.print(" enabled=");Serial.println(audio::on());break;
    case 'K':injected=uint8_t(strtoul(command+1,0,16));Serial.println("OK");break;
    case 'L':lockstep=command[1]!='0';steps=0;Serial.println("OK");break;
    case 'N':steps=strtoul(command+1,0,10);if(!steps)steps=1;lockstep=true;break;
    case 'G': {
        uint16_t part=uint16_t(strtoul(command+1,0,10));
        ready=session.begin(part,false);handheld.paceAnchor=micros();
        ticks=lastUs=maxUs=0;handheld.displayUs=handheld.waitUs=0;handheld.card.resetStats();
        Serial.println(ready?"OK":"ERR");break;
    }
    case 'S': {
        Serial.print("FRAME ");Serial.print(otherrealm::Width);Serial.print(' ');
        Serial.print(otherrealm::Height);Serial.print(' ');
        Serial.println(otherrealm::PageBytes+32);
        Serial.write(game.renderer.frame(),otherrealm::PageBytes);
        Serial.write(reinterpret_cast<const uint8_t*>(game.renderer.colors()),32);
        break;
    }
    case 'F': {
        Serial.println("FLASH 512");
        Serial.write(reinterpret_cast<const uint8_t *>(0xF500),512);
        break;
    }
    case 'U': {
        Serial.println("UI 128 128 32768");
        uint16_t *row=reinterpret_cast<uint16_t *>(device::displayScratch());
        for(unsigned y=0;y<128;++y){session.paintRow(row,uint8_t(y));Serial.write(reinterpret_cast<uint8_t *>(row),256);}
        break;
    }
    default:Serial.println("ERR");break;
    }
}
void pollDebug() {
    while(Serial.available()) {
        char c=Serial.read();
        if(c=='\r')continue;
        if(c=='\n'){command[commandLength]=0;handleCommand();commandLength=0;}
        else if(commandLength<sizeof(command)-1)command[commandLength++]=c;
    }
}
#endif

void setup() {
#ifdef OTHERREALM_DEBUG
    uint32_t *sp;
    __asm volatile("mv %0, sp":"=r"(sp));
    for(uint32_t *p=_susrstack;p<sp-4;++p)*p=0xA5A5A5A5u;
#endif
    device::buttonsBegin();
    device::displayBegin();
    audio::begin(uiEffects,1,true);
#ifdef OTHERREALM_DEBUG
    Serial.begin(115200);
#endif
    if(handheld.card.begin())ready=session.begin(16001);
    if(!ready)device::displaySolid(0x7800);
    handheld.paceAnchor=micros();
}

void loop() {
#ifdef OTHERREALM_DEBUG
    pollDebug();
#endif
    uint8_t buttons=device::buttonsRead();
#ifdef OTHERREALM_DEBUG
    buttons|=injected;
#endif
    if(!ready){delay(10);return;}
    bool advance=true;
#ifdef OTHERREALM_DEBUG
    advance=!lockstep||steps;
#endif
    bool wasMenu=session.visible();
    uint32_t beforeTick=game.vm.tickCount();
    uint16_t beforePart=game.part;
    uint32_t start=micros();
    ready=session.update(buttons,advance);
    if(advance&&!wasMenu&&(game.vm.tickCount()!=beforeTick||game.part!=beforePart)){
        lastUs=micros()-start;++ticks;
        if(lastUs>maxUs)maxUs=lastUs;
#ifdef OTHERREALM_DEBUG
        if(lockstep&&steps&&!--steps){Serial.print("OK ");Serial.println(ticks);}
#endif
    }
    if(ready)ready=session.animate(millis());
    redraw();
    if(session.exitRequested())NVIC_SystemReset();
#ifdef OTHERREALM_DEBUG
    if(!ready){Serial.print("FAULT ");Serial.println(game.fault()?game.fault():"unknown");}
#endif
    if(session.visible()||!advance)delay(10);
}
