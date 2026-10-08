// SPDX-License-Identifier: GPL-2.0-or-later
#include "aw_session.h"
#include "aw_ui_font.h"
#include "aw_title_art.h"
#include <string.h>
namespace otherrealm {
Session::Session(Game &g,SaveStorage &s):game(g),storage(s),identity(0),lastAnimate(0),startSince(UINT32_MAX),phase(0),previous(0),selection(0),mode(Playing),returnMode(Boot),heldMenuButtons(0),opacity(0),pending(255),burst(0),dust(0),effectSeed(0),holdProgress(0),dirty(false),exit(false),failedSave(false),titleBackdrop(false),audioOn(true),completed(false){memset(&saved,0,sizeof(saved));}
bool Session::begin(uint16_t part,bool loadSaved){
    previous=selection=heldMenuButtons=opacity=burst=dust=effectSeed=holdProgress=0;startSince=UINT32_MAX;phase=0;pending=255;mode=Playing;dirty=true;exit=failedSave=titleBackdrop=completed=false;
    memset(&saved,0,sizeof(saved));
    if(!game.begin(part))return false;
    identity=game.pack.identity();
    uint8_t payload[CheckpointPayloadBytes];
    if(loadSaved&&storage.load(identity,payload,sizeof(payload)))decodeCheckpoint(payload,sizeof(payload),saved,&completed);
    if(loadSaved&&!showTitle())return false;
    game.pack.device.soundEnabled(audioOn);
    return !game.fault();
}
bool Session::showTitle(){
    if(!titleBackdrop)titleBackdrop=game.renderer.loadTitle(completed);
    mode=Title;opacity=burst=dust=0;pending=255;dirty=true;return !game.fault();
}
void Session::open(uint8_t next){if(!visible()||mode==Title)opacity=0;mode=next;selection=burst=dust=0;pending=255;dirty=true;}
uint8_t Session::itemCount()const{
    if(mode==ConfirmNew)return 2;
    return 2+(hasSave()?1:0)+(mode==Pause?1:0)+(mode==Pause&&inIntro()?1:0);
}
Session::Action Session::item(uint8_t i)const{
    if(mode==ConfirmNew)return i?Erase:Cancel;
    if(mode==Pause){if(!i--)return Resume;if(inIntro()){if(!i--)return SkipIntro;}}
    if(hasSave()){if(!i--)return Retry;}
    if(!i--)return NewGame;
    return Sound;
}
const char*Session::label(Action a)const{
    switch(a){case Resume:return "RESUME";case Retry:return mode==Boot?"CONTINUE":"RETRY SAVE";
    case NewGame:return "NEW GAME";case SkipIntro:return "SKIP INTRO";case Sound:return audioOn?"SOUND ON":"SOUND OFF";
    case Cancel:return "KEEP SAVE";case Erase:return "NEW GAME";}
    return "";
}
bool Session::activate(Action a){
    if(a==Sound){audioOn=!audioOn;game.pack.device.soundEnabled(audioOn);game.pack.device.tone(1600,25);burst=28;dust=0;effectSeed=uint8_t(phase);dirty=true;return true;}
    if(a==Cancel){open(returnMode);return true;}
    if(a==NewGame){if(hasSave()){returnMode=mode;open(ConfirmNew);return true;}a=Erase;}
    if(opacity){pending=a;burst=28;dust=0;effectSeed=uint8_t(phase);opacity=8;dirty=true;return true;}
    switch(a){
    case Retry:if(!resumeCheckpoint(game,saved))return false;break;
    case Erase:
        if(!game.begin(16001))return false;
        // Replace the disk record only when a valid playable checkpoint exists.
        memset(&saved,0,sizeof(saved));failedSave=false;break;
    case SkipIntro:if(!game.begin(16002))return false;break;
    default:break;
    }
    titleBackdrop=false;heldMenuButtons=previous&(A|B|Start|Select);mode=Playing;dirty=true;return true;
}
bool Session::animate(uint32_t now){
    // This clock is independent of VM ticks, menu animation and frame rate.
    if(previous&Start){
        if(startSince==UINT32_MAX)startSince=now;
        uint32_t held=now-startSince;
        uint8_t next=held>=3000?96:uint8_t(held*96/3000);
        if(next!=holdProgress){holdProgress=next;dirty=true;}
        if(held>=3000){exit=true;return true;}
    }
    uint32_t elapsed=now-lastAnimate;if(elapsed<24)return true;
    unsigned steps=elapsed/24;if(steps>48)steps=48;
    // Carry sub-frame time so a 30 ms display pass does not stretch a 24 ms
    // animation step; confirmation duration stays tied to wall-clock time.
    lastAnimate=now-elapsed%24;uint16_t nextPhase=uint16_t(now/60);
    if(visible()){
        if(phase!=nextPhase)dirty=true;
        if(mode!=Title){
            if(dust){dust=dust>steps?dust-steps:0;dirty=true;}
            if(burst){unsigned used=steps<burst?steps:burst;burst-=used;steps-=used;dirty=true;}
            if(pending!=255){opacity=opacity>steps?opacity-steps:0;dirty=true;
                if(!opacity){Action a=Action(pending);pending=255;if(!activate(a))return false;}}
            else if(opacity<8){opacity=opacity+steps>8?8:opacity+steps;dirty=true;}
        }
    }
    phase=nextPhase;return true;
}
void Session::autosave(bool force){
    CheckpointValue next;
    if(captureCheckpoint(game,next)&&(!hasSave()||!sameCheckpoint(saved,next))){saved=next;force=true;}
    if(!force)return;
    uint8_t payload[CheckpointPayloadBytes];
    failedSave=!encodeCheckpoint(saved,payload,sizeof(payload),completed)||!storage.store(identity,payload,sizeof(payload));
}
bool Session::update(uint8_t buttons,bool advanceGame){
    bool openedByStart=heldMenuButtons&Start;
    uint8_t released=previous&~buttons,pressed=buttons&~previous;
    heldMenuButtons&=buttons;previous=buttons;
    if(pressed&Start){startSince=UINT32_MAX;holdProgress=0;}
    if(released&Start){startSince=UINT32_MAX;holdProgress=0;dirty=true;}
    bool startTap=(released&Start)&&!openedByStart;
    if(exit||pending!=255)return true;
    if(mode==Title){
        if((pressed&~Start)||startTap){game.pack.device.tone(1320,45);if(hasSave())open(Boot);else return activate(Erase);}
        return true;
    }
    if(visible()){
        if(((pressed&B)||startTap)&&mode==Pause){selection=0;return activate(Resume);}
        if(((pressed&B)||startTap)&&mode==ConfirmNew)return activate(Cancel);
        if((pressed&B)&&mode==Boot)return showTitle();
        if(pressed&Up){selection=selection?selection-1:itemCount()-1;burst=0;dust=24;effectSeed=uint8_t(phase);dirty=true;game.pack.device.tone(1100,12);}
        if(pressed&Down){selection=(selection+1)%itemCount();burst=0;dust=24;effectSeed=uint8_t(phase);dirty=true;game.pack.device.tone(1100,12);}
        if(pressed&A){game.pack.device.tone(1600,25);return activate(item(selection));}
        return true;
    }
    if(pressed&(Start|Select)){open(Pause);if(pressed&Start)heldMenuButtons|=Start;return true;}
    if(!game.pack.hasSceneMap()&&(game.part==16008||game.part==16009)){open(Boot);return true;}
    if(!advanceGame)return true;
    buttons&=~heldMenuButtons;VmInput input;
    if(buttons&Right)input.directions|=VmInput::Right;
    if(buttons&Left)input.directions|=VmInput::Left;
    if(buttons&Down)input.directions|=VmInput::Down;
    if(buttons&(Up|B))input.directions|=VmInput::Up;
    input.action=buttons&A;
    if(!game.tick(input))return false;
    // Part 16007 is the noninteractive escape/credits sequence on these disks.
    // Authored packs signal completion explicitly with system event 3.
    bool won=game.event==3||(!game.pack.hasSceneMap()&&game.part==16007&&game.vm.tickCount());
    bool earned=won&&!completed;if(earned)completed=true;
    uint8_t event=game.event;game.event=0;
    if(event==1||event==2){open(event==1?Death:Boot);return true;}
    autosave(earned);return true;
}
static uint16_t blend(uint16_t under,uint16_t over,unsigned alpha){
    if(alpha>=256)return over;
    unsigned inv=256-alpha;
    return uint16_t(((((over>>11)*alpha+(under>>11)*inv)>>8)<<11)|
        (((((over>>5)&63)*alpha+((under>>5)&63)*inv)>>8)<<5)|
        (((over&31)*alpha+(under&31)*inv)>>8));
}
static void lineText(uint16_t*out,int y,const char*s,int x,int top,bool small,uint16_t color,unsigned alpha=256,int glow=-100){
    const int width=small?UiStatusWidth:UiMenuWidth,height=small?UiStatusHeight:UiMenuHeight;
    if(y<top||y>=top+height)return;
    for(;*s&&x<128;++s,x+=width){uint8_t bits=uiFontRow(uint8_t(*s),uint8_t(y-top),small);
        for(int col=0;col<width;++col){int p=x+col;if(p>=0&&p<128&&(bits&(128>>col))){
            int distance=p-glow;if(distance<0)distance=-distance;
            uint16_t ink=distance<5?blend(color,0xFFFF,(5-distance)*48):color;
            out[p]=blend(out[p],ink,alpha);
        }}}
}
static uint16_t dim(uint16_t c,unsigned light){
    if(light==256)return c;
    return uint16_t((((c>>11)*light)>>8)<<11)|uint16_t(((((c>>5)&63)*light)>>8)<<5)|uint16_t(((c&31)*light)>>8);
}
static unsigned sweep(unsigned p){p&=255;return p<128?p:255-p;}
void Session::prepareSparks(int x,int top,int width)const{
    // Ambient and transient effects coexist. Cache only two-byte positions;
    // appearance is cheap to derive on the few rows touched by each particle.
    // A traverse takes 7.68 seconds in each direction, with no gameplay work.
    static const int8_t wave[16]={0,1,2,2,3,2,2,1,0,-1,-2,-2,-3,-2,-2,-1};
    for(unsigned index=0;index<SparkCount;++index){
        Spark &s=sparks[index];s.x=-128;
        const bool ambient=index<AmbientSparks;
        unsigned i=ambient?index:index-AmbientSparks;
        int px,py;unsigned age;
        if(ambient&&!i){
            // A breathing five-pixel glint leads the softer falling motes.
            px=x+int(sweep(phase)*unsigned(width-1)/127);
            py=top+5+wave[(phase/4)&15];
        }else if(ambient){
            age=(phase+i*5)&63;
            if(age>55)continue;
            uint16_t born=uint16_t(phase-age);
            unsigned h=(i+1)*4051+born*811;h^=h>>7;
            px=x+int(sweep(born)*unsigned(width-1)/127)+int(h&63)-31+int(age)*(int((h>>7)&7)-3)/16;
            py=top-6+int((h>>3)&15)+int(age*age)/72;
        }else if(burst){
            if(i>=10)continue;
            int elapsed=28-burst-int(i%4);
            if(elapsed<0||elapsed>25)continue;
            age=unsigned(elapsed);
            unsigned h=(i+1)*4051+effectSeed*811;h^=h>>7;
            px=x-6+int(h%unsigned(width+12))+int(age)*(int((h>>7)&15)-7)/7;
            py=top+int((h>>3)%11)-int(age)*(3+int((h>>11)&7))/5+int(age*age)/48;
        }else if(dust){
            age=24-dust;
            unsigned h=(i+1)*4051+effectSeed*811;h^=h>>7;
            px=x-3+int(h%unsigned(width+6))+int(age)*(int((h>>7)%7)-3)/6;
            py=top+int((h>>3)%11)+int(age)*(int((h>>11)&7)-4)/6+int(age*age)/100;
        }else continue;
        if(px<2||px>125||py<-8||py>106)continue;
        s.x=int8_t(px);s.y=int8_t(py);
    }
}
uint8_t Session::sparkStyle(unsigned index)const{
    unsigned age,level,radius,i=index;
    if(index<AmbientSparks){
        if(!i){radius=((phase/8)&3)==2?2:1;level=2;}
        else{
            age=(phase+i*5)&63;
            radius=age>=10&&age<28&&i%3==0?1:0;
            level=age<6?1:age<35?2:1;
        }
    }else{
        i-=AmbientSparks;
        if(burst){
            age=28-burst-i%4;
            radius=age>1&&age<12?1:0;
            level=age<3?2:age<13?3:age<20?2:1;
        }else{
            age=24-dust;
            radius=age>3&&age<12&&i%3==0?1:0;
            level=age<15?2:1;
        }
    }
    return uint8_t(level|(radius<<2)|((i%4==0)?16:0));
}
void Session::paintExitHold(uint16_t*out,uint8_t y)const{
    if(holdProgress<10||y<1||y>2)return;
    for(unsigned x=0;x<96;++x)out[x+16]=x<holdProgress?0xAEFC:0x1929;
}
void Session::paintRow(uint16_t*out,uint8_t y)const{
    const uint16_t ink=0xFF9A,muted=0x94D4;
    unsigned light=(mode==Title||mode==Playing)?256:256-opacity*21;
    const uint8_t *pixels=titleBackdrop?game.renderer.titleFrame():game.renderer.frame();
    // Prepare sixteen colors once per screen, not once per row/pixel. A tiny
    // palette cache saves thousands of software multiplies on RV32EC.
    if(!y)for(unsigned i=0;i<16;++i)uiPalette[i]=dim(game.renderer.colors()[i],light);
    if(titleBackdrop){
        const uint8_t *src=pixels+y*64;
        for(unsigned x=0;x<128;x+=2){uint8_t pair=*src++;out[x]=uiPalette[pair>>4];out[x+1]=uiPalette[pair&15];}
    }else if(mode==Title){
        uint16_t color=(y<85)?0x1169:(y<105?0x1929:0x0843);
        for(unsigned x=0;x<128;++x)out[x]=color;
    }else if(y>=24&&y<104){
        const uint8_t *src=pixels+((y-24)*Height/80)*(Width/2);unsigned pos=0;
        for(unsigned x=0;x<128;++x,pos+=Width){unsigned sx=pos>>7;out[x]=uiPalette[(src[sx>>1]>>(sx&1?0:4))&15];}
    }else memset(out,0,256);
    if(mode==Title){
        int row=int(y)-TitleTop;
        if(row>=0&&row<TitleHeight)for(unsigned x=0;x<TitleWidth;++x){
            unsigned at=row*TitleWidth+x,c=(titleSprite[at/2]>>(at&1?0:4))&15;
            if(c)out[(128-TitleWidth)/2+x]=titlePalette[c];
        }
        // A 7.68-second triangle ramp: always visible, from white to sky blue.
        // One RGB565 blend per text row; no alpha layer or additional buffer.
        if(y>=117&&y<125){
            unsigned t=(phase>>1)&63;t=t<=32?t:64-t;
            const unsigned blue=0x42B0; // Blue-gray from the greeting scene palette.
            unsigned r=31-((31-(blue>>11))*t>>5);
            unsigned g=63-((63-((blue>>5)&63))*t>>5);
            unsigned b=31-((31-(blue&31))*t>>5);
            lineText(out,y,"PRESS A BUTTON",29,117,true,uint16_t((r<<11)|(g<<5)|b));
        }
        paintExitHold(out,y);return;
    }
    // Also used to replace the UI directly with the undimmed gameplay frame.
    if(mode==Playing){paintExitHold(out,y);return;}
    const unsigned count=itemCount();
    const int first=(104-int(count)*16)/2;
    const int row=int(y);
    unsigned alpha=unsigned(opacity)*32;
    unsigned others=pending==255?alpha:alpha*(burst>12?burst-12:0)/16;
    const char *selected=label(item(selection));int selectedWidth=int(strlen(selected))*UiMenuWidth;
    int selectedX=(128-selectedWidth)/2,selectedTop=first+selection*16;
    if(!y)prepareSparks(selectedX,selectedTop,selectedWidth);
    if(mode==ConfirmNew||mode==Death){
        const char *title=mode==ConfirmNew?"REPLACE SAVE?":"TRY AGAIN";
        lineText(out,row,title,(128-int(strlen(title))*UiStatusWidth)/2,8,true,muted,others);
    }
    int glow=selectedX+int(sweep(phase)*unsigned(selectedWidth-1)/127);
    for(uint8_t i=0;i<count;++i){
        const char *text=label(item(i));int x=(128-int(strlen(text))*UiMenuWidth)/2;
        lineText(out,row,text,x,first+i*16,false,i==selection?ink:muted,i==selection?alpha:others,i==selection?glow:-100);
    }
    for(unsigned i=0;i<SparkCount;++i){
        const Spark &s=sparks[i];if(s.x==-128)continue;
        int dy=row-s.y;if(dy<-2||dy>2)continue;
        uint8_t style=sparkStyle(i);int radius=(style>>2)&3;if(dy<-radius||dy>radius)continue;
        unsigned level=style&3;bool gold=style&16;
        uint16_t color=level==3?0xFFDE:level==2?(gold?0xDE57:0xAEFC):(gold?0x83AC:0x5474);
        int extent=dy==0?radius:0;
        for(int x=s.x-extent;x<=s.x+extent;++x)if(x>=0&&x<128)
            out[x]=blend(out[x],dy==0&&x==s.x&&level>1?0xFFDE:color,alpha);
    }
    const char *status=failedSave?"SAVE FAILED - RAM ONLY":hasSave()?"AUTO CHECKPOINT SAVED":"NEW JOURNEY";
    int statusWidth=int(strlen(status))*UiStatusWidth,statusX=(128-statusWidth)/2;
    lineText(out,row,status,statusX,109,true,failedSave?0xFB20:muted,others);
    const char *hint="A SELECT   B BACK";int hintX=(128-int(strlen(hint))*UiStatusWidth)/2;
    lineText(out,row,hint,hintX,120,true,muted,others);
    lineText(out,row,"A",hintX,120,true,ink,others);
    lineText(out,row,"B",hintX+int(strchr(hint,'B')-hint)*UiStatusWidth,120,true,ink,others);
    paintExitHold(out,y);
}
}
