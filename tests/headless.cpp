#include "../engine/aw_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
using namespace otherrealm;
class Host:public Device {
public:
    FILE *file;uint32_t displays=0,elapsed=0,hash=2166136261u;const uint8_t*frame=0;const uint16_t*pal=0;
    explicit Host(const char*path){file=fopen(path,"rb");}
    ~Host(){if(file)fclose(file);}
    bool readSector(uint32_t s,uint8_t*d)override{return file&&fseek(file,long(s)*512,SEEK_SET)==0&&fread(d,1,512,file)==512;}
    void display(const uint8_t*f,const uint16_t*p)override{frame=f;pal=p;++displays;hash=2166136261u;for(int i=0;i<PageBytes;++i)hash=(hash^f[i])*16777619u;}
    void waitFrame(uint16_t ms)override{elapsed+=ms;}
    void save(const char*prefix,unsigned tick){
        if(!frame)return;char path[512];snprintf(path,sizeof(path),"%s-%06u.ppm",prefix,tick);FILE*out=fopen(path,"wb");if(!out)return;
        fprintf(out,"P6\n%d %d\n255\n",Width,Height);
        for(int i=0;i<Width*Height;++i){unsigned c=pal[(frame[i/2]>>((i&1)?0:4))&15];uint8_t rgb[3]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};fwrite(rgb,1,3,out);}fclose(out);
    }
};
int main(int argc,char**argv){
    if(argc<2){fprintf(stderr,"usage: headless PACK [ticks=5000] [part=16001] [frame-prefix] [input=idle|escape|right] [capture-interval=100]\n");return 2;}
    Host host(argv[1]);static Game game(host);unsigned ticks=argc>2?strtoul(argv[2],0,10):5000;uint16_t part=argc>3?atoi(argv[3]):16001;
    const char*mode=argc>5?argv[5]:"idle";unsigned interval=argc>6?atoi(argv[6]):100;
    auto start=std::chrono::steady_clock::now();
    if(!game.begin(part)){fprintf(stderr,"BEGIN: %s\n",game.fault());return 1;}
    uint16_t previousPart=0;uint32_t maxReads=0;unsigned t=0;
    for(;t<ticks;++t){
        VmInput input;
        if(!strcmp(mode,"escape")){input.directions=game.part==16002&&game.vm.tickCount()<170?VmInput::Up:VmInput::Right;input.action=true;}
        if(!strcmp(mode,"right")){input.directions=VmInput::Right;input.action=true;}
        uint32_t before=game.pack.reads;
        if(!game.tick(input)){fprintf(stderr,"FAULT tick=%u part=%u pc=%u task=%u op=%u: %s\n",t,game.part,game.vm.faultPc(),game.vm.faultTask(),game.vm.faultOpcode(),game.fault());host.save("build/fault",t);return 1;}
        uint32_t reads=game.pack.reads-before;if(reads>maxReads)maxReads=reads;
        if(game.part!=previousPart){printf("PART tick=%u part=%u frames=%u game_ms=%u\n",t,game.part,game.frames,host.elapsed);previousPart=game.part;}
        if(argc>4&&interval&&t%interval==0)host.save(argv[4],t);
        if(t%500==0)printf("FRAME tick=%u frames=%u hash=%08x reads=%u sync=%d instructions=%u\n",t,game.frames,host.hash,game.pack.reads,game.vm.variable(0xF4),game.vm.instructionsLastTick());
    }
    auto duration=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    printf("PASS ticks=%u frames=%u part=%u game_ms=%u host_ms=%lld SD_sectors=%u max_sectors_per_tick=%u polygons=%u sizeof_game=%zu hash=%08x\n",t,game.frames,game.part,host.elapsed,(long long)duration,game.pack.reads,maxReads,game.renderer.polygons,sizeof(Game),host.hash);
    if(argc>4)host.save(argv[4],t);
    return 0;
}
