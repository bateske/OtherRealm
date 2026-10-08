// SPDX-License-Identifier: GPL-2.0-or-later
// Render the user's own resources at 208x130, then crop a native 128x128 title.
#include "../engine/aw_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace otherrealm;
struct Capture:Device {
    FILE *file;
    explicit Capture(const char *path):file(fopen(path,"rb")){}
    ~Capture(){if(file)fclose(file);}
    bool readSector(uint32_t s,uint8_t *dst){return file&&!fseek(file,long(s)*512,SEEK_SET)&&fread(dst,1,512,file)==512;}
    void display(const uint8_t*,const uint16_t*){}
    void waitFrame(uint16_t){}
};
int main(int argc,char **argv){
    if(argc<3){fprintf(stderr,"capture_title SOURCE_PACK_208 OUTPUT [shoreline|greeting|ticks] [part=16002]\n");return 2;}
    Capture device(argv[1]);static Game game(device);
    if(!game.begin(argc>4?atoi(argv[4]):16002))return 1;
    bool greeting=argc>3&&!strcmp(argv[3],"greeting");
    bool shoreline=argc<=3||!strcmp(argv[3],"shoreline");
    unsigned ticks=greeting?0:shoreline?500:atoi(argv[3]);
    if(greeting){
        // Still from the original greeting cinematic: bytecode 0x13C6..0x13FB.
        // Only composition instructions live here; all pixels, polygons and
        // palette come from the user's game pack, never engine distribution.
        const Resource shapes=game.pack.entry(28);
        // Guard this edition-specific recipe against different bank layouts.
        Resource code=game.pack.entry(27);
        static const uint8_t anchor[]={0x0e,0,11,0x0d,0,0xf1,0x06,160,100};
        for(unsigned i=0;i<sizeof(anchor);++i)if(game.pack.byte(code,0x13c6+i)!=anchor[i]){
            fprintf(stderr,"Unsupported greeting layout; expected original Amiga scene\n");return 1;
        }
        game.renderer.select(0);game.renderer.fill(0,11);
        game.renderer.shape(shapes,0xE20C,160,100,64);
        // Reframe the original figure below the logo, at the same native scale.
        game.renderer.shape(shapes,0x0188,160,150,64);
        // Omit the foreground guard silhouette: after centering Lester for a
        // square crop it would cover the greeting hand.
        game.renderer.shape(shapes,0xE0C4,160,150,64);
        game.renderer.palette(game.pack.entry(26),11);game.renderer.present(0);
        if(game.fault()){fprintf(stderr,"%s\n",game.fault());return 1;}
    }else{
        if(shoreline){
            Resource code=game.pack.entry(27);
            const uint8_t anchor[]={8,22,0x40,0x34,8,10,0x20,0xd8};
            for(unsigned i=0;i<sizeof(anchor);++i)if(game.pack.byte(code,0x21fb+i)!=anchor[i]){
                fprintf(stderr,"Unsupported shoreline control handoff\n");return 1;
            }
        }
        bool ready=false;
        for(unsigned i=0;i<ticks;++i){
            VmInput input;
            // Release swim input once the exterior scene starts so it cannot
            // become an immediate jump when the climb animation finishes.
            if(!shoreline||game.vm.variable(0x67)!=2){input.directions=i<170?VmInput::Up:VmInput::Right;input.action=true;}
            if(!game.tick(input)){fprintf(stderr,"%s\n",game.fault());return 1;}
            // The control task starts during the climb. Wait for its settled,
            // upright pose (shape 0x8786), before the pool creature reaches him.
            if(shoreline&&game.vm.taskPc(20)==0x22f0&&game.vm.taskPc(22)!=Vm::InactiveTask){
                ticks=i+1;ready=true;break;
            }
        }
        if(shoreline&&!ready){fprintf(stderr,"Shoreline standing frame not reached\n");return 1;}
    }
    FILE *out=fopen(argv[2],"wb");if(!out)return 1;
    const uint16_t *pal=game.renderer.colors();
    for(unsigned i=0;i<16;++i){fputc(pal[i]&255,out);fputc(pal[i]>>8,out);}
    const uint8_t *pixels=game.renderer.frame();
    for(unsigned y=0;y<128;++y)for(unsigned x=0;x<128;x+=2){
        unsigned at=(y+(Height-128)/2)*Width+x+(Width-128)/2;
        fputc(pixels[at/2],out);
    }
    fclose(out);
    char preview[512];snprintf(preview,sizeof(preview),"%s.ppm",argv[2]);out=fopen(preview,"wb");
    if(out){fprintf(out,"P6\n%d %d\n255\n",Width,Height);for(unsigned at=0;at<Width*Height;++at){unsigned c=pal[(pixels[at/2]>>(at&1?0:4))&15];fputc((c>>11)*255/31,out);fputc(((c>>5)&63)*255/63,out);fputc((c&31)*255/31,out);}fclose(out);}
    printf("Captured native 128x128 from %dx%d, part %u, tick %u.\n",Width,Height,game.part,ticks);return 0;
}
