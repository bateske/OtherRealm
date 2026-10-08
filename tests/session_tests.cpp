// SPDX-License-Identifier: GPL-2.0-or-later
// Session policy exercised through public button input and persistent payloads.
#include "../engine/aw_session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace otherrealm;

static unsigned checks;
static uint32_t clockMs=0;
static void settle(Session &s){clockMs+=960; if(!s.animate(clockMs))abort();}
static void check(bool value,const char *message){++checks;if(!value){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
struct FileDevice:Device{
    FILE *file;unsigned frames;
    explicit FileDevice(const char *path):file(fopen(path,"rb")),frames(0){}
    ~FileDevice(){if(file)fclose(file);}
    bool readSector(uint32_t sector,uint8_t *out){return file&&!fseek(file,long(sector)*512,SEEK_SET)&&fread(out,1,512,file)==512;}
    void display(const uint8_t *,const uint16_t *){++frames;}
    void waitFrame(uint16_t){}
};
struct MemoryStorage:SaveStorage{
    uint8_t payload[CheckpointPayloadBytes];uint32_t identity;unsigned loads,stores;
    bool available,failStore;
    MemoryStorage():identity(0),loads(0),stores(0),available(false),failStore(false){memset(payload,0,sizeof(payload));}
    bool load(uint32_t id,uint8_t *out,uint16_t size){++loads;check(size==sizeof(payload),"storage load has exact versioned size");if(!available||id!=identity)return false;memcpy(out,payload,size);return true;}
    bool store(uint32_t id,const uint8_t *data,uint16_t size){++stores;check(size==sizeof(payload),"storage store has exact versioned size");if(failStore)return false;identity=id;available=true;memcpy(payload,data,size);return true;}
    CheckpointValue value()const{CheckpointValue v={};check(decodeCheckpoint(payload,sizeof(payload),v),"stored payload decodes");return v;}
    void seed(uint32_t id,const CheckpointValue &v){identity=id;available=true;check(encodeCheckpoint(v,payload,sizeof(payload)),"seed valid checkpoint");}
};
static void update(Session &session,uint8_t buttons=0,bool advance=true){check(session.update(buttons,advance),"session update succeeds");settle(session);}
static void tap(Session &session,uint8_t button){update(session,0,false);update(session,button,false);update(session,0,false);}
static void down(Session &session,unsigned n=1){while(n--)tap(session,Session::Down);}
static uint32_t menuHash(Session &session){
    uint32_t hash=2166136261u;uint16_t guarded[130];
    for(unsigned y=0;y<128;++y){guarded[0]=0x1234;guarded[129]=0xabcd;session.paintRow(guarded+1,y);
        check(guarded[0]==0x1234&&guarded[129]==0xabcd,"menu painter stays inside 128-pixel row");
        for(unsigned x=1;x<=128;++x)hash=(hash^guarded[x])*16777619u;
    }return hash;
}
static void testPresentation(const char *path){
    FileDevice device(path);Game game(device);MemoryStorage storage;Session session(game,storage);
    check(session.begin()&&session.isTitle(),"every normal boot presents title");
    const uint32_t title=menuHash(session),ticks=game.vm.tickCount();
    check(session.animate(720),"advance title clock");
    check(menuHash(session)!=title&&game.vm.tickCount()==ticks,"prompt color pulses without running gameplay");
    check(session.update(Session::A,false)&&!session.visible(),"title press starts new game");
    check(session.update(0)&&session.hasSave(),"demo starts and earns first save");
    uint8_t frame[PageBytes];uint16_t palette[16];memcpy(frame,game.renderer.frame(),sizeof(frame));memcpy(palette,game.renderer.colors(),sizeof(palette));
    check(session.update(Session::Start,false),"open animated pause");
    uint32_t opening=menuHash(session);check(session.animate(744),"first animation step");
    uint32_t partial=menuHash(session);check(session.animate(936),"finish opening");
    check(opening!=partial&&partial!=menuHash(session),"menu progresses through distinct transition frames");
    const uint32_t paused=game.vm.tickCount();
    check(session.update(0,false)&&session.update(Session::B,false),"begin closing");
    check(session.visible(),"closing retains paused overlay until fade finishes");
    check(session.animate(960)&&session.visible(),"closing has intermediate frame");
    check(session.animate(1320)&&session.visible(),"selection lingers while other options dissolve");
    check(session.animate(1824)&&!session.visible(),"confirmation burst then fade returns to gameplay");
    check(game.vm.tickCount()==paused&&!memcmp(frame,game.renderer.frame(),sizeof(frame))&&!memcmp(palette,game.renderer.colors(),sizeof(palette)),"pause animation leaves VM, pixels and palette intact");
    uint16_t row[128];
    for(unsigned y=0;y<128;++y){
        session.paintRow(row,y);
        for(unsigned x=0;x<128;++x){
            uint16_t expected=0;
            if(y>=24&&y<104){unsigned at=((y-24)*Height/80)*Width+x*Width/128;expected=palette[(frame[at/2]>>(at&1?0:4))&15];}
            check(row[x]==expected,"menu dismissal scans restored gameplay directly, including letterbox margins");
        }
    }
    tap(session,Session::Start);down(session,3);tap(session,Session::A);check(!session.soundOn(),"sound menu turns audio off");
    tap(session,Session::A);check(session.soundOn(),"sound menu turns audio on");
}
static void testTextOnlyOverlay(const char *path){
    FileDevice device(path);Game game(device);MemoryStorage storage;Session session(game,storage);
    check(session.begin(16001,false),"begin overlay regression check");update(session);
    tap(session,Session::Start);
    uint16_t row[128];
    for(unsigned y=0;y<128;++y){
        session.paintRow(row,y);
        // Wider motes may reach the old slab's boundary; screen edges stay clear.
        for(unsigned x=0;x<=127;x+=127){
            unsigned c=0;if(y>=24&&y<104){unsigned at=((y-24)*Height/80)*Width+x*Width/128;const uint8_t *f=game.renderer.frame();c=game.renderer.colors()[(f[at/2]>>(at&1?0:4))&15];}
            unsigned expected=((((c>>11)*88)>>8)<<11)|((((((c>>5)&63)*88)>>8))<<5)|(((c&31)*88)>>8);
            check(row[x]==expected,"only text and pip overlay the uniformly dimmed scene");
        }
    }
}
static void testAmbientContinuity(const char *path){
    FileDevice device(path);Game game(device);MemoryStorage storage;Session session(game,storage);
    check(session.begin(16001,false)&&session.update(0),"begin particle continuity check");
    check(session.update(Session::Start,false)&&session.update(0,false)&&session.animate(24000),"settle ambient particles over paused scene");
    static uint16_t before[128*128];uint16_t row[128];
    for(unsigned y=0;y<128;++y)session.paintRow(before+y*128,y);
    // Return to the same label at the same time, but with its dust effect active.
    // Falling ambient motes below the dust cloud must keep their exact pixels.
    check(session.update(Session::Down,false)&&session.update(0,false)&&session.update(Session::Up,false)&&session.update(0,false),"disturb the current label without advancing the ambient clock");
    unsigned distantMotes=0,addedDust=0;
    for(unsigned y=0;y<128;++y){
        session.paintRow(row,y);
        if(y>=50&&y<107){
            check(!memcmp(before+y*128,row,sizeof(row)),"disturbed dust does not replace the ambient falling particles");
            for(unsigned x=0;x<128;++x)if(row[x]==0x5474||row[x]==0x83AC||row[x]==0xFFDE)++distantMotes;
        }else if(y<50)for(unsigned x=0;x<128;++x)if(row[x]!=before[y*128+x])++addedDust;
    }
    check(distantMotes>0&&addedDust>0,"continuity comparison contains both falling motes and newly disturbed dust");
}
static void go(Session &session,Game &game,uint16_t part){for(unsigned i=0;i<200&&game.part!=part;++i)update(session,Session::Right);check(game.part==part,"session traverses authored scene");update(session);}
static void moveX(Session &session,Game &game,int x){for(unsigned i=0;i<100&&abs(game.vm.variable(0)-x)>3;++i)update(session,game.vm.variable(0)<x?Session::Right:Session::Left);check(abs(game.vm.variable(0)-x)<=3,"session reaches authored interaction");}
static void action(Session &session){update(session);update(session,Session::A);update(session);}

static void testMenusAndInput(const char *path){
    FileDevice device(path);Game game(device);MemoryStorage storage;Session session(game,storage);
    check(Session::Start==64&&Session::Select==128,"menu input matches CHGame button masks");
    check(session.begin(16001,false)&&!session.visible()&&!session.hasSave()&&storage.loads==0,"fresh game can bypass persistent load");
    check(session.takeRedraw()&&!session.takeRedraw(),"redraw flag is consumed once");
    update(session,0,false);check(game.vm.tickCount()==0&&storage.stores==0,"lockstep input-only update does not run game");
    update(session);check(session.hasSave()&&storage.stores==1&&!session.saveFailed(),"first authored playable tick autosaves");
    const CheckpointValue first=storage.value();
    for(unsigned i=0;i<20;++i)update(session,Session::Right);
    check(storage.stores==1&&game.vm.variable(0)>first.variables[0],"movement does not repeatedly write flash");
    tap(session,Session::Start);check(session.visible(),"start pauses game");
    const uint32_t ticks=game.vm.tickCount(),frames=device.frames;
    for(unsigned i=0;i<20;++i)update(session);
    check(game.vm.tickCount()==ticks&&device.frames==frames,"pause freezes all VM and render work");
    update(session,Session::Down,false);const uint8_t selected=session.selectedIndex();
    for(unsigned i=0;i<20;++i)update(session,Session::Down,false);
    check(session.selectedIndex()==selected&&selected==1,"held menu direction moves only on press edge while particles animate");
    tap(session,Session::Up);check(session.selectedIndex()==0,"menu up returns to resume");
    tap(session,Session::B);check(!session.visible()&&game.vm.tickCount()==ticks,"B resumes exact paused VM state");
    tap(session,Session::Select);down(session);tap(session,Session::A);
    check(!session.visible()&&game.vm.tickCount()==0&&game.vm.variable(0)==first.variables[0],"retry save restarts semantic checkpoint rather than paused position");
    update(session);check(storage.stores==1,"retrying same milestone does not write storage");
    tap(session,Session::Start);down(session,2);tap(session,Session::A);
    check(session.visible()&&session.hasSave()&&storage.stores==1,"new game first requests replacement confirmation");
    tap(session,Session::B);check(session.visible()&&session.hasSave(),"B cancels replacement and keeps pause menu");
    down(session,2);tap(session,Session::A);down(session);tap(session,Session::A);
    check(!session.visible()&&!session.hasSave()&&game.vm.tickCount()==0&&storage.stores==1,"confirmed new game leaves previous disk record until playable checkpoint");
    update(session);check(session.hasSave()&&storage.stores==2&&storage.value().variables[0]==first.variables[0],"new game replaces progress at valid initial milestone");
    game.systemEvent(1);update(session);check(session.visible()&&game.event==0,"death system event is consumed into menu");
    tap(session,Session::A);check(!session.visible()&&game.vm.tickCount()==0,"death menu defaults to retry saved checkpoint");
    update(session);game.systemEvent(2);update(session);check(session.visible(),"ending event opens main menu");
    tap(session,Session::Up);tap(session,Session::A);check(!session.exitRequested()&&!session.soundOn(),"boot menu wraps up to sound with no exit item");
}

static void testPersistenceAndFailures(const char *path){
    FileDevice device(path);Game game(device);MemoryStorage storage;Session session(game,storage);
    check(session.begin()&&session.isTitle()&&!session.hasSave()&&storage.loads==1,"missing save shows title");tap(session,Session::A);update(session);
    const uint32_t identity=session.packIdentity();const CheckpointValue first=storage.value();
    check(identity==storage.identity,"storage uses current content identity");
    check(session.begin()&&session.visible()&&session.hasSave(),"valid stored progress opens continue menu");
    const unsigned writes=storage.stores;tap(session,Session::A);tap(session,Session::A);update(session);
    check(!session.visible()&&storage.stores==writes&&game.vm.variable(0)==first.variables[0],"continue restores checkpoint without duplicate save");
    storage.payload[0]='X';check(session.begin()&&session.isTitle()&&!session.hasSave(),"malformed payload cannot leak partial saved state");
    storage.seed(identity^1,first);check(session.begin()&&!session.hasSave(),"save from different installed pack is ignored");
    storage.available=false;storage.failStore=true;check(session.begin(),"begin with failed writer");tap(session,Session::A);update(session);
    check(session.hasSave()&&session.saveFailed()&&!storage.available,"failed persistent write retains RAM retry checkpoint and status");
    tap(session,Session::Start);down(session);tap(session,Session::A);update(session);
    check(!session.visible()&&game.vm.variable(0)==first.variables[0],"RAM-only retry works after failed storage");
    storage.failStore=false;go(session,game,16010);
    check(!session.saveFailed()&&storage.available&&storage.value().part==16010,"next earned milestone repairs failed save status");
    check(session.begin(16001,false)&&!session.hasSave(),"explicit fresh session ignores prior save");
    game.systemEvent(1);update(session);check(session.visible()&&!session.hasSave(),"death before valid autosave offers new game");
    tap(session,Session::A);check(!session.visible()&&game.vm.tickCount()==0,"new game from empty death menu needs no confirmation");
}

static void testDemoMilestones(const char *path){
    FileDevice device(path);Game game(device);MemoryStorage storage;Session session(game,storage);
    check(session.begin(),"begin milestone replay");tap(session,Session::A);update(session);go(session,game,16010);moveX(session,game,160);action(session);
    check(storage.value().variables[3]==1&&game.vm.variable(3)==1,"crystal pickup autosaves same-scene inventory");
    const unsigned earned=storage.stores;for(unsigned i=0;i<10;++i)update(session,Session::Left);
    check(storage.stores==earned,"walking after pickup preserves checkpoint without writes");
    check(session.begin()&&session.visible(),"cold restart finds crystal save");tap(session,Session::A);tap(session,Session::A);update(session);
    check(game.part==16010&&game.vm.variable(3)==1,"continue after cold restart retains crystal and scene");
    go(session,game,16011);moveX(session,game,202);action(session);
    check(storage.value().variables[4]==1,"gate interaction earns persistent checkpoint");
    check(session.begin(),"cold restart after gate");tap(session,Session::A);tap(session,Session::A);update(session);
    for(unsigned i=0;i<50&&!game.vm.variable(6);++i)update(session,Session::Right);
    check(game.vm.variable(6)==1&&storage.value().variables[6]==1,"continued gate save remains playable through saved victory");
    check(session.hasCompleted(),"authored victory event earns persistent completion");
    const unsigned victoryWrites=storage.stores;
    check(session.begin()&&session.visible(),"cold restart at victory");tap(session,Session::A);
    check(session.hasCompleted(),"authored completion reloads with ordinary title fallback");
    update(session,Session::A); // Do not release the physical menu selection key.
    for(unsigned i=0;i<3;++i)update(session,Session::A);
    check(game.part==16011&&game.vm.variable(6)==1&&storage.stores==victoryWrites,"held continue key cannot restart victory or replace its save");
    update(session);update(session,Session::A);update(session);
    check(game.part==16001&&game.vm.variable(6)==0,"action is available again after menu key is released");
}

static void testMenuKeyConsumption(const char *path){
    FileDevice device(path);Game game(device);MemoryStorage storage;Session session(game,storage);
    check(session.begin(),"begin menu input consumption");tap(session,Session::A);update(session);
    tap(session,Session::Start);update(session,Session::A);update(session,Session::A);
    check(!session.visible()&&game.vm.variable(0xfa)==0,"holding A to resume does not perform gameplay action");
    update(session);update(session,Session::A);check(game.vm.variable(0xfa)==1,"fresh A press after release performs action");
    update(session);tap(session,Session::Start);update(session,Session::B);
    for(unsigned i=0;i<3;++i)update(session,Session::B);
    check(!session.visible()&&game.vm.variable(8)==155,"holding B to go back does not jump");
    update(session);update(session,Session::B);check(game.vm.variable(8)<155,"fresh B press after release jumps");
}

static void testCustomMenus(const char *path){
    FileDevice device(path);Game game(device);MemoryStorage storage;Session session(game,storage);
    check(session.begin(),"begin unsaved custom menu check");tap(session,Session::A);
    tap(session,Session::Start);down(session,2);tap(session,Session::A);
    check(!session.exitRequested()&&!session.soundOn(),"custom pause without save ends with sound");
    check(session.begin(),"begin saved custom menu check");tap(session,Session::A);update(session);
    tap(session,Session::Start);down(session,3);tap(session,Session::A);
    check(!session.exitRequested()&&session.soundOn(),"custom pause with save ends with sound");
    check(session.begin()&&session.visible(),"open saved custom boot menu");tap(session,Session::A);down(session,2);tap(session,Session::A);
    check(!session.exitRequested()&&!session.soundOn(),"custom boot ends with sound without an exit or intro item");
}

static void testHoldAndStaticFooter(const char *path){
    FileDevice device(path);Game game(device);MemoryStorage storage;Session session(game,storage);
    uint16_t row[128],footer[8*128];
    check(session.begin(),"start title hold check");
    check(session.update(Session::Start,false)&&session.animate(1000)&&session.isTitle(),"Start press waits on title to distinguish tap from hold");
    check(session.animate(2499)&&session.exitHoldProgress()==47&&!session.exitRequested(),"hold progress follows real elapsed milliseconds");
    session.paintRow(row,0);session.paintRow(row,1);
    check(row[16]==0xAEFC&&row[62]==0xAEFC&&row[63]==0x1929,"hold indicator draws proportional fill over title");
    check(session.animate(3999)&&!session.exitRequested(),"holding less than three seconds never exits");
    check(session.animate(4000)&&session.exitRequested()&&session.isTitle(),"three-second title hold exits without starting game");
    check(storage.stores==0,"exit hold does not touch save storage");

    check(session.begin(),"restart after exit");
    check(session.update(Session::Start,false)&&session.animate(5000),"start short title press");
    check(session.update(0,false)&&session.animate(5100)&&!session.visible()&&!session.exitHoldProgress(),"short Start release launches title normally and clears bar");
    check(session.update(0)&&session.hasSave(),"earn initial checkpoint");
    const uint32_t ticks=game.vm.tickCount();const unsigned writes=storage.stores;
    check(session.update(Session::Start,false)&&session.animate(5200)&&session.visible(),"Start press pauses gameplay immediately");
    check(session.animate(6400)&&session.exitHoldProgress()==38,"hold progress also runs from gameplay pause");
    check(session.update(0,false)&&session.animate(6500)&&session.visible()&&!session.exitHoldProgress(),"releasing opening Start cancels exit and keeps pause open");
    for(unsigned y=0;y<128;++y){session.paintRow(row,y);if(y>=109&&y<117)memcpy(footer+(y-109)*128,row,sizeof(row));}
    check(session.animate(11000),"advance idle menu clock");
    for(unsigned y=0;y<128;++y){session.paintRow(row,y);if(y>=109&&y<117)check(!memcmp(footer+(y-109)*128,row,sizeof(row)),"checkpoint footer stays stationary across particle cycles");}
    check(session.update(Session::Start,false)&&session.animate(11100)&&session.visible(),"Start press in menu does not close before release");
    check(session.update(0,false)&&session.animate(11200)&&session.visible(),"short Start release begins confirmation fade");
    check(session.animate(12160)&&!session.visible(),"short Start tap resumes after fade");
    check(game.vm.tickCount()==ticks&&storage.stores==writes,"holds and pause animations preserve VM and checkpoint");
    check(session.update(Session::Start,false)&&session.animate(0xfffffc00u),"hold can begin near clock wrap");
    check(session.animate(0xfffffc00u+2999u)&&!session.exitRequested(),"wrapped elapsed time respects threshold");
    check(session.animate(0xfffffc00u+3000u)&&session.exitRequested(),"wrapped elapsed time exits at exactly three seconds");
}

static void testOriginalIntroPolicy(const char *path){
    FileDevice device(path);Game game(device);MemoryStorage storage;Session session(game,storage);
    check(game.begin(16001),"open original pack for intro policy");
    CheckpointValue city={};city.kind=CheckpointAmiga;city.part=16004;city.variables[0]=49;
    storage.seed(game.pack.identity(),city);uint8_t original[CheckpointPayloadBytes];memcpy(original,storage.payload,sizeof(original));
    check(session.begin()&&session.isTitle(),"saved boot always shows title");
    tap(session,Session::A);check(session.visible()&&!session.isTitle(),"title opens continue menu");
    tap(session,Session::A);check(game.part==16004&&game.vm.variable(0)==49,"continue retains city checkpoint");update(session);
    tap(session,Session::Start);down(session,2);tap(session,Session::A);down(session);tap(session,Session::A);
    check(game.part==16001&&!session.hasSave()&&storage.stores==0,"new original game begins intro without early overwrite");
    for(unsigned i=0;i<30;++i)update(session);
    check(!memcmp(original,storage.payload,sizeof(original)),"intro preserves prior flash save until playable checkpoint");
    tap(session,Session::Start);down(session);tap(session,Session::A);
    check(game.part==16002&&!session.visible(),"skip intro enters first gameplay scene");update(session);
    check(storage.stores==1&&storage.value().part==16002&&storage.value().variables[0]==10,"pool checkpoint replaces city save");
    storage.available=false;check(session.begin()&&session.isTitle(),"unsaved original boot shows title");
    tap(session,Session::B);check(game.part==16001&&!session.visible(),"any title button starts intro for new player");
    for(unsigned i=0;i<5000&&game.part==16001;++i)update(session);
    check(game.part==16002&&!session.visible(),"intro naturally flows into gameplay");update(session);
    check(game.loadPart(16008),"load legacy password screen");const uint32_t ticks=game.vm.tickCount();update(session,0,false);
    check(session.visible()&&game.vm.tickCount()==ticks,"legacy password screen routes to menu without VM execution");
}

static void titleMatches(Game &game,unsigned kind){
    bool found=false,match=true;
    for(unsigned i=0;i<game.pack.resourceCount();++i){
        Resource r=game.pack.entry(i);if(r.type!=kind)continue;found=true;
        for(unsigned p=0;p<8192;++p)if(game.renderer.titleFrame()[p]!=game.pack.byte(r,32+p))match=false;
        for(unsigned c=0;c<16;++c)if(game.renderer.colors()[c]!=(game.pack.byte(r,c*2)|(unsigned(game.pack.byte(r,c*2+1))<<8)))match=false;
    }
    check(found&&match,"title loads exact pixels and palette for earned/default resource");
}
static void testCompletionUnlock(const char *path){
    FileDevice device(path);Game game(device);MemoryStorage storage;Session session(game,storage);
    check(game.begin(16006),"open palace pack for completion test");
    CheckpointValue palace={};palace.kind=CheckpointAmiga;palace.part=16006;palace.variables[0]=60;
    storage.seed(game.pack.identity(),palace);
    check(session.begin()&&!session.hasCompleted(),"legacy save remains valid without unlocked title");titleMatches(game,10);
    for(unsigned i=0;i<3;++i){tap(session,Session::A);tap(session,Session::B);check(session.isTitle()&&!game.vm.tickCount(),"B returns boot menu to title without advancing or clearing save");}
    check(storage.stores==0,"title navigation never writes progress");
    tap(session,Session::A);tap(session,Session::A);update(session);
    check(game.loadPart(16007),"enter original noninteractive ending");
    update(session,0,false);check(!session.hasCompleted(),"unexecuted ending does not unlock early");
    const unsigned before=storage.stores;update(session);
    bool flag=false;CheckpointValue decoded={};
    check(session.hasCompleted()&&storage.stores==before+1&&decodeCheckpoint(storage.payload,sizeof(storage.payload),decoded,&flag)&&flag&&decoded.part==16006,"ending persists completion while retaining last playable checkpoint");
    for(unsigned i=0;i<8;++i)update(session);
    check(storage.stores==before+1,"ending writes completion only once");
    check(session.begin()&&session.hasCompleted(),"completion survives cold session restart");titleMatches(game,11);
    tap(session,Session::A);down(session);tap(session,Session::A);down(session);tap(session,Session::A);
    check(game.part==16001&&session.hasCompleted()&&!session.hasSave(),"new game preserves completion independently of progress");
    tap(session,Session::Start);down(session);tap(session,Session::A);update(session);
    flag=false;check(decodeCheckpoint(storage.payload,sizeof(storage.payload),decoded,&flag)&&flag&&decoded.part==16002,"fresh game's initial checkpoint retains earned title");
    check(session.begin()&&session.hasCompleted(),"unlock survives a new game's save and restart");titleMatches(game,11);
}

int main(int argc,char **argv){
    const char *demo=argc>1?argv[1]:"demo/sd/OTHERWRL.PAK";
    testPresentation(demo);testTextOnlyOverlay(demo);testAmbientContinuity(demo);testMenusAndInput(demo);testPersistenceAndFailures(demo);testDemoMilestones(demo);testMenuKeyConsumption(demo);testCustomMenus(demo);testHoldAndStaticFooter(demo);
    if(argc>2){testOriginalIntroPolicy(argv[2]);testCompletionUnlock(argv[2]);}
    printf("Session PASS: %u checks, menu input, lockstep, continue/retry/new, failures, milestones, title, intro/skip, audio menu.\n",checks);return 0;
}
