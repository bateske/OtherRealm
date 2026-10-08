// Host-only deterministic input replay and execution auditing.
#define main otherrealm_headless_entry
#include "headless.cpp"
#undef main
#include <vector>
#include <map>
#include <algorithm>

struct ProbeHost:Host {
    std::map<uint32_t,uint32_t> sectors;
    explicit ProbeHost(const char*path):Host(path){}
    bool readSector(uint32_t sector,uint8_t*dst)override{++sectors[sector];return Host::readSector(sector,dst);}
    uint32_t read32(){uint8_t b[4]={0,0,0,0};fread(b,1,4,file);return b[0]|uint32_t(b[1])<<8|uint32_t(b[2])<<16|uint32_t(b[3])<<24;}
    void profile(){
        unsigned types[9]={0};fseek(file,8,SEEK_SET);unsigned count=read32();
        std::map<uint32_t,unsigned> sectorType;
        for(unsigned id=0;id<count;++id){fseek(file,32+id*16,SEEK_SET);unsigned off=read32(),size=read32();read32();unsigned type=read32()&65535;if(type>7)type=8;for(unsigned s=off/512;size&&s<(off+size+511)/512;++s)sectorType[s]=type;}
        std::vector<std::pair<uint32_t,uint32_t> > hottest;
        for(const auto&it:sectors){auto found=sectorType.find(it.first);unsigned type=found==sectorType.end()?8:found->second;types[type]+=it.second;hottest.push_back(std::make_pair(it.second,it.first));}
        std::sort(hottest.rbegin(),hottest.rend());
        printf("READ_TYPES sound=%u music=%u bitmap=%u palette=%u bytecode=%u part_shapes=%u shared_shapes=%u scene_map=%u table=%u unique_sectors=%zu\n",types[0],types[1],types[2],types[3],types[4],types[5],types[6],types[7],types[8],sectors.size());
        printf("HOT_SECTORS");for(unsigned i=0;i<10&&i<hottest.size();++i)printf(" %u:%u",hottest[i].second,hottest[i].first);puts("");
    }
};

struct AuditGame:Game {
    struct Blob {std::vector<uint8_t> bytes;};
    std::map<unsigned,Blob> blobs;
    unsigned shapeCalls=0,edgeMismatch=0,shortPolygons=0,headFlags=0,maxDepth=0,maxVertices=0,selfScroll=0;
    explicit AuditGame(Host&h):Game(h){}
    const std::vector<uint8_t>& blob(unsigned id){
        Blob&b=blobs[id];if(!b.bytes.empty())return b.bytes;
        Resource r=pack.entry(id);b.bytes.resize(r.size);
        Host&h=static_cast<Host&>(pack.device);fseek(h.file,r.offset,SEEK_SET);fread(b.bytes.data(),1,r.size,h.file);
        return b.bytes;
    }
    void audit(const std::vector<uint8_t>&b,unsigned p,unsigned depth=0){
        if(depth>maxDepth)maxDepth=depth;if(depth>32||p>=b.size())return;
        unsigned tag=b[p++];
        if(tag>=192){
            if(p+3>b.size())return;
            unsigned n=b[p+2];p+=3;
            if(n>maxVertices)maxVertices=n;if(n<4)++shortPolygons;if(n%2||p+2*n>b.size())return;
            for(unsigned i=1;i<n/2;++i){int dr=int(b[p+i*2+1])-b[p+(i-1)*2+1];int dl=int(b[p+(n-1-i)*2+1])-b[p+(n-i)*2+1];if(dr!=dl)++edgeMismatch;}
        }else if((tag&63)==2){
            if(p+3>b.size())return;
            unsigned n=b[p+2]+1;p+=3;
            while(n--){if(p+4>b.size())return;unsigned child=b[p]*256+b[p+1];p+=4;if(child&32768){if(p+2>b.size())return;if(b[p]&128)++headFlags;p+=2;}audit(b,(child*2)&65535,depth+1);}
        }
    }
    void drawShape(bool secondary,uint16_t off,int16_t x,int16_t y,uint16_t zoom)override{
        static const unsigned shapes[]={22,25,28,31,34,37,40,43,127,127};
        unsigned id=0;
        if(pack.hasSceneMap()){
            const auto&map=blob(pack.resourceCount()-1);
            for(unsigned p=0;p+10<=map.size();p+=10)if(map[p]+256*map[p+1]==part){unsigned q=p+(secondary?8:6);id=map[q]+256*map[q+1];if(!id&&secondary)id=map[p+6]+256*map[p+7];break;}
        }else id=secondary&&part!=16000&&part!=16001&&part!=16005&&part<16008?17:shapes[part-16000];
        audit(blob(id),off);++shapeCalls;Game::drawShape(secondary,off,x,y,zoom);
    }
    void copyPage(uint8_t s,uint8_t d,int16_t y)override{if(s<254&&(s&128)&&(s&3)==d&&y)++selfScroll;Game::copyPage(s,d,y);}
};

struct Stage {unsigned end, directions,action;};
int main(int argc,char**argv){
    if(argc<4){fprintf(stderr,"usage: probe PACK ticks part [script.csv|-] [output-prefix]\nscript lines: end_tick,direction_mask,action\n");return 2;}
    ProbeHost host(argv[1]);AuditGame game(host);unsigned ticks=strtoul(argv[2],0,10),part=atoi(argv[3]);
    std::vector<unsigned> readCounts;
    std::vector<Stage> stages;if(argc>4&&strcmp(argv[4],"-")){FILE*f=fopen(argv[4],"r");if(!f)return 2;Stage s;while(fscanf(f,"%u,%u,%u",&s.end,&s.directions,&s.action)==3)stages.push_back(s);fclose(f);}
    unsigned stage=0,oldPart=0;int oldScreen=-999;char path[512];FILE*vars=0;
    if(argc>5){snprintf(path,sizeof(path),"%s-vars.csv",argv[5]);vars=fopen(path,"w");}
    if(!game.begin(part)){fprintf(stderr,"BEGIN %s\n",game.fault());return 1;}
    for(unsigned t=0;t<ticks;++t){
        while(stage<stages.size()&&t>=stages[stage].end)++stage;
        VmInput input;if(stage<stages.size()){input.directions=stages[stage].directions;input.action=stages[stage].action!=0;}
        unsigned readsBefore=game.pack.reads;
        if(!game.tick(input)){fprintf(stderr,"FAULT tick=%u part=%u pc=%u: %s\n",t,game.part,game.vm.faultPc(),game.fault());return 1;}
        readCounts.push_back(game.pack.reads-readsBefore);
        int screen=game.vm.variable(0x67);
        if(game.part!=oldPart||screen!=oldScreen){printf("SCENE tick=%u part=%u screen=%d frames=%u game_ms=%u\n",t,game.part,screen,game.frames,host.elapsed);oldPart=game.part;oldScreen=screen;}
        if(vars){fprintf(vars,"%u,%u,%u,%u",t,game.part,game.frames,game.pack.reads);for(unsigned i=0;i<256;++i)fprintf(vars,",%d",game.vm.variable(i));fputc('\n',vars);}
        if(argc>5&&t%10==0)host.save(argv[5],t);
    }
    if(vars)fclose(vars);
    printf("AUDIT shape_calls=%u edge_mismatch=%u short_polygons=%u head_flags=%u max_depth=%u max_vertices=%u self_scroll=%u sectors=%u frames=%u\n",game.shapeCalls,game.edgeMismatch,game.shortPolygons,game.headFlags,game.maxDepth,game.maxVertices,game.selfScroll,game.pack.reads,game.frames);
    std::sort(readCounts.begin(),readCounts.end());if(!readCounts.empty())printf("READS_PER_TICK median=%u p95=%u max=%u\n",readCounts[readCounts.size()/2],readCounts[readCounts.size()*95/100],readCounts.back());
    host.profile();
    return 0;
}
