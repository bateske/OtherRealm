// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "aw_game.h"
#include "aw_checkpoint.h"
namespace otherrealm {
struct SaveStorage {
    virtual bool load(uint32_t identity,uint8_t *data,uint16_t size)=0;
    virtual bool store(uint32_t identity,const uint8_t *data,uint16_t size)=0;
};
class Session {
public:
    enum Button { A=1,B=2,Up=4,Down=8,Left=16,Right=32,Start=64,Select=128 };
    Session(Game &game,SaveStorage &storage);
    bool begin(uint16_t part=16001,bool loadSaved=true);
    bool update(uint8_t buttons,bool advanceGame=true);
    bool animate(uint32_t milliseconds);
    bool visible()const{return mode!=Playing;}
    bool isTitle()const{return mode==Title;}
    bool soundOn()const{return audioOn;}
    bool takeRedraw(){bool d=dirty;dirty=false;return d;}
    // Present rows in order starting at zero, which prepares the dim palette.
    void paintRow(uint16_t *rgb565,uint8_t y)const;
    bool exitRequested()const{return exit;}
    uint8_t exitHoldProgress()const{return holdProgress;}
    bool hasSave()const{return saved.kind!=CheckpointNone;}
    bool saveFailed()const{return failedSave;}
    bool hasCompleted()const{return completed;}
    uint8_t selectedIndex()const{return selection;}
    uint32_t packIdentity()const{return identity;}
private:
    enum Mode {Playing,Pause,Boot,Death,ConfirmNew,Title};
    enum Action {Resume,Retry,NewGame,SkipIntro,Sound,Cancel,Erase};
    Game &game;
    SaveStorage &storage;
    CheckpointValue saved;
    uint32_t identity,lastAnimate,startSince;
    uint16_t phase;
    uint8_t previous,selection,mode,returnMode,heldMenuButtons,opacity,pending,burst,dust,effectSeed,holdProgress;
    bool dirty,exit,failedSave,titleBackdrop,audioOn,completed;
    mutable uint16_t uiPalette[16];
    enum {AmbientSparks=9,EffectSparks=15,SparkCount=AmbientSparks+EffectSparks};
    struct Spark {int8_t x,y;};
    mutable Spark sparks[SparkCount];
    bool inIntro()const{return !game.pack.hasSceneMap()&&game.part==16001;}
    void open(uint8_t next);
    uint8_t itemCount()const;
    Action item(uint8_t index)const;
    const char *label(Action action)const;
    bool activate(Action action);
    bool showTitle();
    void autosave(bool force=false);
    void prepareSparks(int x,int top,int width)const;
    uint8_t sparkStyle(unsigned index)const;
    void paintExitHold(uint16_t *out,uint8_t y)const;
};
}
