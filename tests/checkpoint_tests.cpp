// SPDX-License-Identifier: GPL-2.0-or-later
// Portable semantic save tests. Optional second pack argument exercises all
// original Amiga password destinations against the supplied private bytecode.
#include "../engine/aw_checkpoint.h"
#include "../engine/aw_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace otherrealm;

static unsigned checks;
static void check(bool condition,const char *message){++checks;if(!condition){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
struct FileDevice:Device{
    FILE *file;uint32_t hash;
    explicit FileDevice(const char *path):file(fopen(path,"rb")),hash(0){}
    ~FileDevice(){if(file)fclose(file);}
    bool readSector(uint32_t sector,uint8_t *out){return file&&!fseek(file,long(sector)*512,SEEK_SET)&&fread(out,1,512,file)==512;}
    void display(const uint8_t *p,const uint16_t *palette){
        hash=2166136261u;for(unsigned i=0;i<PageBytes;++i)hash=(hash^p[i])*16777619u;
        for(unsigned i=0;i<16;++i){hash=(hash^(palette[i]&255))*16777619u;hash=(hash^(palette[i]>>8))*16777619u;}
    }
    void waitFrame(uint16_t){}
};

static void step(Game &game,uint8_t direction=0,bool action=false){
    VmInput input;input.directions=direction;input.action=action;
    if(!game.tick(input)){fprintf(stderr,"FAULT part=%u pc=%u: %s\n",game.part,game.vm.faultPc(),game.fault());exit(1);}
}
static void go(Game &game,uint16_t part){for(unsigned i=0;i<200&&game.part!=part;++i)step(game,VmInput::Right);check(game.part==part,"reach next authored scene");}
static void x(Game &game,int target){for(unsigned i=0;i<100&&abs(game.vm.variable(0)-target)>3;++i)step(game,game.vm.variable(0)<target?VmInput::Right:VmInput::Left);check(abs(game.vm.variable(0)-target)<=3,"reach authored interaction");}
static CheckpointValue amiga(unsigned part,int position){CheckpointValue value={};value.kind=CheckpointAmiga;value.part=part;value.variables[0]=position;return value;}

static void testCodec(){
    CheckpointValue value=amiga(16004,43),decoded={};uint8_t bytes[CheckpointPayloadBytes+1];
    check(sizeof(value)==38&&CheckpointPayloadBytes==44,"fixed small checkpoint sizes");
    check(encodeCheckpoint(value,bytes,sizeof(bytes)),"encode canonical Amiga checkpoint");
    check(!memcmp(bytes,"ORCP",4)&&bytes[4]==1&&bytes[5]==1&&bytes[6]==0x84&&bytes[7]==0x3e&&bytes[12]==43,"explicit little-endian save format");
    check(decodeCheckpoint(bytes,44,decoded)&&sameCheckpoint(value,decoded),"Amiga round trip");
    check(!decodeCheckpoint(bytes,43,decoded)&&!decodeCheckpoint(bytes,45,decoded),"reject truncated and oversized payloads");
    bytes[4]=2;check(!decodeCheckpoint(bytes,44,decoded),"reject future schema");bytes[4]=1;
    bytes[10]=2;check(!decodeCheckpoint(bytes,44,decoded),"reject unknown save flags");bytes[10]=0;
    bool completed=false;
    check(encodeCheckpoint(value,bytes,44,true)&&decodeCheckpoint(bytes,44,decoded,&completed)&&completed&&sameCheckpoint(value,decoded),"completion flag round trips without changing checkpoint");
    CheckpointValue empty={};
    check(encodeCheckpoint(empty,bytes,44,true)&&decodeCheckpoint(bytes,44,decoded,&completed)&&completed&&decoded.kind==CheckpointNone,"completion can persist without a gameplay checkpoint");
    check(!encodeCheckpoint(empty,bytes,44,false),"empty non-completion records remain invalid");
    check(encodeCheckpoint(value,bytes,44),"restore ordinary checkpoint fixture");
    bytes[12]=32;check(!decodeCheckpoint(bytes,44,decoded),"reject unsupported original checkpoint selector");
    value.kind=CheckpointScene;value.part=16010;value.marker=0xf123;value.variables[0]=-123;value.variables[3]=1;value.variables[8]=-32768;value.variables[15]=1;
    check(encodeCheckpoint(value,bytes,sizeof(bytes))&&decodeCheckpoint(bytes,44,decoded)&&!memcmp(&value,&decoded,sizeof(value)),"custom signed variables and marker round trip");
    decoded.variables[0]=220;check(sameCheckpoint(value,decoded),"movement alone does not request flash save");
    ++decoded.marker;check(!sameCheckpoint(value,decoded),"authored milestone requests new save");
    value.marker=0;check(!validCheckpoint(value),"custom scenes must opt into milestones");
    check(!validCheckpoint(amiga(16001,0))&&!validCheckpoint(amiga(16007,0))&&!validCheckpoint(amiga(16008,0)),"intro ending and password UI cannot replace progress");
    check(!validCheckpoint(amiga(16003,21))&&!validCheckpoint(amiga(16006,64)),"exclude non-password or later-release selectors");
}

static void testDemo(const char *path){
    FileDevice device(path);Game game(device);CheckpointValue saved={},moved={},restored={};
    check(game.begin(16001),"begin custom demo");
    check(!captureCheckpoint(game,saved),"do not save uninitialized entry variables");step(game);
    check(captureCheckpoint(game,saved)&&saved.marker==1,"authored initial milestone");
    step(game,VmInput::Right);check(captureCheckpoint(game,moved)&&sameCheckpoint(saved,moved),"walking does not cause autosave spam");
    go(game,16010);check(!captureCheckpoint(game,moved),"defer save until new scene executes once");step(game);
    x(game,160);step(game,0,true);step(game);
    check(captureCheckpoint(game,saved)&&saved.variables[3]==1,"crystal earns same-scene checkpoint");
    check(resumeCheckpoint(game,saved),"resume crystal checkpoint");
    check(game.vm.variable(3)==1&&game.vm.variable(15)==1&&game.vm.variable(CheckpointMarkerVariable)==int16_t(saved.marker),"restore inventory init guard and authored marker");
    step(game);check(game.vm.variable(3)==1&&game.part==16010,"scene init does not clear resumed inventory");
    check(captureCheckpoint(game,restored)&&sameCheckpoint(saved,restored),"resuming does not generate duplicate milestone");
    go(game,16011);step(game);x(game,202);step(game,0,true);step(game);
    check(captureCheckpoint(game,saved)&&saved.variables[4]==1,"unlocked gate earns checkpoint");
    check(resumeCheckpoint(game,saved),"resume unlocked gate");step(game);
    check(game.vm.variable(3)==1&&game.vm.variable(4)==1,"gate and inventory survive fresh VM initialization");
    for(unsigned i=0;i<50&&!game.vm.variable(6);++i)step(game,VmInput::Right);
    check(game.vm.variable(6)==1&&captureCheckpoint(game,saved),"resumed gate remains playable to victory");
    check(resumeCheckpoint(game,saved),"resume custom ending");step(game);check(game.vm.variable(6)==1,"custom ending state retained");
}

struct PasswordBinding{uint16_t part,position;uint8_t characters[4];};
struct PasswordGame:Game{
    bool redirect;
    explicit PasswordGame(Device &device):Game(device),redirect(false){}
    uint8_t readCode(uint16_t offset){
        // Exercise the actual private password comparison routine without
        // reproducing the slow cursor-entry UI. Later reads use original code.
        if(redirect&&part==16008&&offset<3){const uint8_t jump[]={7,6,0xfc};if(offset==2)redirect=false;return jump[offset];}
        return Game::readCode(offset);
    }
};

static void testPrivatePasswords(const char *path){
    FileDevice device(path);PasswordGame game(device);
    check(game.begin(16008),"open original password part");Resource code=game.pack.entry(126);
    PasswordBinding bindings[19];unsigned count=0;
    for(uint32_t at=0;at+31<=code.size;++at){
        bool match=true;
        for(unsigned i=0;i<4;++i)if(game.pack.byte(code,at+i*6)!=10||game.pack.byte(code,at+i*6+1)!=1||game.pack.byte(code,at+i*6+2)!=0x1e + i){match=false;break;}
        if(!match||game.pack.byte(code,at+24)!=0||game.pack.byte(code,at+25)!=0||game.pack.byte(code,at+28)!=25)continue;
        unsigned part=game.pack.byte(code,at+29)*256+game.pack.byte(code,at+30),position=game.pack.byte(code,at+26)*256+game.pack.byte(code,at+27);
        if(!validCheckpoint(amiga(part,position)))continue;
        check(count<19,"original password table contains at most 19 gameplay entries");
        PasswordBinding &b=bindings[count++];b.part=part;b.position=position;
        for(unsigned i=0;i<4;++i)b.characters[i]=game.pack.byte(code,at+i*6+3);
    }
    check(count==19&&!game.fault(),"discover all nineteen actual Amiga password destinations");
    for(unsigned i=0;i<count;++i){
        const PasswordBinding &b=bindings[i];CheckpointValue checkpoint=amiga(b.part,b.position),captured={};
        check(game.begin(16008),"reset password VM");game.redirect=true;
        for(unsigned n=0;n<4;++n)game.vm.variable(0x1e + n)=b.characters[n];
        step(game);check(game.part==b.part,"actual password dispatch selects expected part");
        for(unsigned n=0;n<40;++n)step(game);
        const uint32_t passwordHash=device.hash;const int16_t passwordScreen=game.vm.variable(0x67);
        check(resumeCheckpoint(game,checkpoint),"fresh semantic resume of original checkpoint");
        check(!captureCheckpoint(game,captured),"fresh original checkpoint waits for initialization");
        for(unsigned n=0;n<40;++n)step(game);
        if(device.hash!=passwordHash||game.vm.variable(0x67)!=passwordScreen){
            fprintf(stderr,"MISMATCH part=%u position=%u password=%08x/%d resumed=%08x/%d\n",b.part,b.position,passwordHash,passwordScreen,device.hash,game.vm.variable(0x67));exit(1);
        }
        check(captureCheckpoint(game,captured)&&sameCheckpoint(checkpoint,captured),"capture stable original milestone after resume");
        printf("PASSWORD PASS part=%u position=%u screen=%d frame=%08x\n",b.part,b.position,passwordScreen,passwordHash);
    }
}

int main(int argc,char **argv){
    testCodec();testDemo(argc>1?argv[1]:"demo/sd/OTHERWRL.PAK");
    if(argc>2)testPrivatePasswords(argv[2]);
    printf("Checkpoint PASS: %u checks, semantic saves and scene inventory resume.\n",checks);return 0;
}
