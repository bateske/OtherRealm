// SPDX-License-Identifier: GPL-2.0-or-later
#include "aw_checkpoint.h"
#include "aw_game.h"
#include <string.h>

namespace otherrealm {
static bool validAmigaPosition(uint16_t part,int16_t position){
    switch(part){
    case 16002:return position==10;
    case 16003:return position==20;
    case 16004:return position==30||position==31||position==33||position==35||
        position==37||position==39||(position>=41&&position<=49);
    case 16005:return position==50;
    case 16006:return position==60;
    default:return false;
    }
}

bool validCheckpoint(const CheckpointValue &value){
    if(value.kind==CheckpointAmiga){
        if(value.marker||!validAmigaPosition(value.part,value.variables[0]))return false;
        for(unsigned i=1;i<CheckpointVariableCount;++i)if(value.variables[i])return false;
        return true;
    }
    return value.kind==CheckpointScene&&value.part>=16000&&value.marker!=0;
}

bool captureCheckpoint(const Game &game,CheckpointValue &value){
    if(game.fault()||!game.vm.tickCount())return false;
    CheckpointValue candidate={};candidate.part=game.part;
    if(game.pack.hasSceneMap()){
        candidate.kind=CheckpointScene;
        candidate.marker=static_cast<uint16_t>(game.vm.variable(CheckpointMarkerVariable));
        if(!candidate.marker)return false;
        for(unsigned i=0;i<CheckpointVariableCount;++i)candidate.variables[i]=game.vm.variable(i);
    }else{
        candidate.kind=CheckpointAmiga;
        switch(game.part){
        case 16002:candidate.variables[0]=10;break;
        case 16003:candidate.variables[0]=20;break;
        case 16004:candidate.variables[0]=game.vm.variable(0);break;
        case 16005:candidate.variables[0]=50;break;
        case 16006:candidate.variables[0]=60;break;
        default:return false; // Intros, protection/password screens and ending.
        }
    }
    if(!validCheckpoint(candidate))return false;
    value=candidate;return true;
}

bool sameCheckpoint(const CheckpointValue &a,const CheckpointValue &b){
    if(!validCheckpoint(a)||!validCheckpoint(b)||a.kind!=b.kind||a.part!=b.part)return false;
    return a.kind==CheckpointAmiga?a.variables[0]==b.variables[0]:a.marker==b.marker;
}

bool resumeCheckpoint(Game &game,const CheckpointValue &value){
    if(!validCheckpoint(value)||!game.begin(value.part))return false;
    if(game.pack.hasSceneMap()!=(value.kind==CheckpointScene))return false;
    if(value.kind==CheckpointAmiga)game.vm.variable(0)=value.variables[0];
    else{
        for(unsigned i=0;i<CheckpointVariableCount;++i)game.vm.variable(i)=value.variables[i];
        game.vm.variable(CheckpointMarkerVariable)=value.marker<0x8000?static_cast<int16_t>(value.marker):
            static_cast<int16_t>(static_cast<int32_t>(value.marker)-65536);
    }
    return true;
}

static void put16(uint8_t *p,uint16_t value){p[0]=value&255;p[1]=value>>8;}
static uint16_t get16(const uint8_t *p){return p[0]|(uint16_t(p[1])<<8);}
static int16_t signed16(uint16_t value){return value<0x8000?static_cast<int16_t>(value):static_cast<int16_t>(static_cast<int32_t>(value)-65536);}

bool encodeCheckpoint(const CheckpointValue &value,uint8_t *out,size_t size,bool completed){
    CheckpointValue empty={};
    if(!out||size<CheckpointPayloadBytes||(!validCheckpoint(value)&&!(completed&&!memcmp(&value,&empty,sizeof(value)))))return false;
    memset(out,0,CheckpointPayloadBytes);memcpy(out,"ORCP",4);out[4]=1;out[5]=uint8_t(value.kind);
    out[10]=completed?1:0;
    put16(out+6,value.part);put16(out+8,value.marker);
    for(unsigned i=0;i<CheckpointVariableCount;++i)put16(out+12+i*2,static_cast<uint16_t>(value.variables[i]));
    return true;
}

bool decodeCheckpoint(const uint8_t *data,size_t size,CheckpointValue &value,bool *completed){
    if(!data||size!=CheckpointPayloadBytes||memcmp(data,"ORCP",4)||data[4]!=1||(data[10]&~1)||data[11])return false;
    CheckpointValue candidate={};candidate.kind=data[5];candidate.part=get16(data+6);candidate.marker=get16(data+8);
    for(unsigned i=0;i<CheckpointVariableCount;++i)candidate.variables[i]=signed16(get16(data+12+i*2));
    CheckpointValue empty={};
    if(!validCheckpoint(candidate)&&!(data[10]&&!memcmp(&candidate,&empty,sizeof(candidate))))return false;
    value=candidate;if(completed)*completed=data[10]&1;return true;
}
} // namespace otherrealm
