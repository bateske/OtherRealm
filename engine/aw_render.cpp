// Polygon format/page semantics follow Gregory Montoir's RAW and Fabother World.
// This adaptation is GPL-2.0-or-later; see LICENSE and THIRD_PARTY.md.
#include "aw_render.h"
#include "aw_font.h"
#include "aw_text.h"
namespace otherrealm {
static int scaleX(int v){int p=v*Width;return p>=0?p/320:-((-p+319)/320);}
static int scaleY(int v){int p=v*Height;return p>=0?p/200:-((-p+199)/200);}
// Read signed 16.16 coordinates without implementation-defined signed shifts.
static int fixedInteger(uint32_t v){int high=int(v>>16);return high<32768?high:high-65536;}
void Renderer::reset(){memset(pages,0,sizeof(pages));memset(pal,0,sizeof(pal));work=front=2;back=1;nodesLeft=0;polygons=0;error=0;}
bool Renderer::loadTitle(bool completed){
    // Title borrows all four pages; Game::begin clears them before VM playback.
    if(sizeof(pages)<8192)return false;
    Resource r={};
    for(uint16_t i=0;i<pack.resourceCount();++i){
        Resource candidate=pack.entry(i);
        if(candidate.type==10&&!r.size)r=candidate;
        if(completed&&candidate.type==11){r=candidate;break;}
    }
    if(r.size){
        if(r.size!=8224){error="Invalid title size";return false;}
        for(unsigned c=0;c<16;++c)pal[c]=pack.byte(r,c*2)|(uint16_t(pack.byte(r,c*2+1))<<8);
        return pack.copy(r,32,reinterpret_cast<uint8_t *>(pages),8192);
    }
    return false;
}
void Renderer::fill(uint8_t p,uint8_t c){memset(pages[resolve(p)],(c&15)*17,PageBytes);}
void Renderer::copy(uint8_t s,uint8_t d,int16_t scroll){
    if(s==d)return;
    if(s>=0xFE||!((s&0xBF)&0x80)){memmove(pages[resolve(d)],pages[resolve(s>=0xFE?s:s&0xBF)],PageBytes);return;}
    if(scroll<=-200||scroll>=200)return;
    if((s&3)==resolve(d))return;
    int sy=scaleY(scroll),n=Height-(sy<0?-sy:sy);
    uint8_t*dst=pages[resolve(d)];const uint8_t*src=pages[s&3];
    if(sy<0)src+=-sy*(Width/2);else dst+=sy*(Width/2);
    memmove(dst,src,n*(Width/2));
}
void Renderer::pixel(int x,int y,uint8_t c){
    if(x<0||x>=Width||y<0||y>=Height)return;
    int off=y*(Width/2)+x/2,shift=(x&1)?0:4;
    uint8_t &v=pages[work][off],mask=15<<shift;
    if(c==16)v|=8<<shift;
    else if(c==17)v=(v&~mask)|(pages[0][off]&mask);
    else v=(v&~mask)|((c&15)<<shift);
}
void Renderer::span(int a,int b,int y,uint8_t c){
    if(y<0||y>=Height)return;
    if(a>b){int t=a;a=b;b=t;}if(a<0)a=0;if(b>=Width)b=Width-1;if(a>b)return;
    if(a&1)pixel(a++,y,c);if(!(b&1))pixel(b--,y,c);if(a>b)return;
    int off=y*(Width/2)+a/2,n=(b-a+1)/2;uint8_t*dst=pages[work]+off;
    if(c==16){while(n--)*dst++|=0x88;}
    else if(c==17){if(work!=0)memcpy(dst,pages[0]+off,n);}
    else memset(dst,(c&15)*17,n);
}
void Renderer::shape(const Resource&r,uint16_t off,int16_t x,int16_t y,uint16_t zoom){
    if(zoom>4096){error="Shape zoom out of range";return;}
    nodesLeft=8192;draw(r,off,x,y,zoom,255,0);
}
void Renderer::draw(const Resource&r,uint32_t off,int x,int y,uint16_t z,uint8_t color,uint8_t depth){
    if(error||pack.error)return;
    if(depth>8){error="Shape nesting too deep";return;}
    if(!nodesLeft){error="Shape node budget exhausted";return;}--nodesLeft;
    uint8_t tag=pack.byte(r,off++);
    if(tag>=0xC0){polygon(r,off,x,y,z,(color&0x80)?tag&63:color);return;}
    if((tag&63)!=2){error="Invalid shape opcode";return;}
    x-=pack.byte(r,off++)*z/64;y-=pack.byte(r,off++)*z/64;
    unsigned count=unsigned(pack.byte(r,off++))+1;
    while(count--&&!pack.error&&!error){
        uint16_t child=uint16_t(pack.byte(r,off++))<<8;child|=pack.byte(r,off++);
        int cx=x+pack.byte(r,off++)*z/64,cy=y+pack.byte(r,off++)*z/64;
        uint8_t cc=255;
        if(child&0x8000){cc=pack.byte(r,off++)&127;++off;}
        draw(r,uint16_t(child*2),cx,cy,z,cc,depth+1);
    }
}
// Keep the vertex array out of every recursive group frame on the 2 KiB
// embedded stack. LTO would otherwise inline this 256-byte scratch array.
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__)
__attribute__((noinline))
#endif
void Renderer::polygon(const Resource&r,uint32_t off,int x,int y,uint16_t z,uint8_t c){
    int w=pack.byte(r,off++)*z/64,h=pack.byte(r,off++)*z/64;
    unsigned n=pack.byte(r,off++);
    if(n<4||n>64||(n&1)){error="Invalid polygon vertex count";return;}
    if(x-w/2>319||x+w/2<0||y-h/2>199||y+h/2<0)return;
    if(n==4&&w==0&&h<=1){pixel(scaleX(x),scaleY(y),c);return;}
    Point pts[64];x-=w/2;y-=h/2;
    for(unsigned i=0;i<n;++i){
        int px=scaleX(x+pack.byte(r,off++)*z/64),py=scaleY(y+pack.byte(r,off++)*z/64);
        if(px<-32768||px>32767||py<-32768||py>32767){error="Shape coordinate out of range";return;}
        pts[i].x=int16_t(px);pts[i].y=int16_t(py);
    }
    ++polygons;
    // Original shapes are paired, monotonic quad strips, not arbitrary polygons.
    unsigned i=1,j=n-2;int yy=pts[0].y<pts[n-1].y?pts[0].y:pts[n-1].y;
    uint32_t left=uint32_t(int32_t(pts[n-1].x))*65536u,right=uint32_t(int32_t(pts[0].x))*65536u;
    for(unsigned remaining=n;remaining>2;remaining-=2,++i,--j){
        int dy=pts[i].y-pts[i-1].y;
        if(dy<0){error="Non-monotonic polygon";return;}
        int leftDy=pts[j].y-pts[j+1].y;
        if(leftDy<0){error="Non-monotonic polygon";return;}
        // The original rasterizer uses modulo-32-bit fixed-point arithmetic.
        // Unsigned accumulators preserve its output without signed overflow.
        uint32_t dl=uint32_t(int32_t(pts[j].x)-pts[j+1].x)*(0x4000u/(leftDy>1?leftDy:1))*4u;
        uint32_t dr=uint32_t(int32_t(pts[i].x)-pts[i-1].x)*(0x4000u/(dy>1?dy:1))*4u;
        left=(left&0xFFFF0000u)|0x7FFF;right=(right&0xFFFF0000u)|0x8000;
        if(!dy){left+=dl;right+=dr;continue;}
        if(yy<0){int skip=(-yy<dy)?-yy:dy;left+=dl*uint32_t(skip);right+=dr*uint32_t(skip);yy+=skip;dy-=skip;}
        while(dy--){if(yy>=Height)return;span(fixedInteger(left),fixedInteger(right),yy++,c);left+=dl;right+=dr;}
    }
}
void Renderer::bitmap(const Resource&r){if(!r.size)return;if(r.size!=PageBytes||!(r.flags&2)){error="Bitmap dimensions mismatch";return;}pack.copy(r,0,pages[0],PageBytes);}
void Renderer::palette(const Resource&r,uint8_t n){
    if(n>=32)return;
    for(int i=0;i<16;++i){uint32_t p=uint32_t(n)*32+i*2;uint16_t c=pack.byte(r,p)*256+pack.byte(r,p+1);
        unsigned red=(c>>8)&15,green=(c>>4)&15,blue=c&15;
        pal[i]=((red*31/15)<<11)|((green*63/15)<<5)|(blue*31/15);
    }
}
void Renderer::present(uint8_t p){if(p==255){uint8_t t=front;front=back;back=t;}else if(p!=254)front=resolve(p);pack.device.display(pages[front],pal);}
void Renderer::text(const char*s,int x,int y,uint8_t c){
    int origin=x;
    for(;*s;++s){if(*s=='\n'||*s=='\r'){y+=8;x=origin;continue;}
        unsigned ch=uint8_t(*s);if(ch>=32&&ch<128){
            int x0=scaleX(x),x1=scaleX(x+8),y0=scaleY(y),y1=scaleY(y+8);
            for(int py=y0;py<y1;++py)for(int px=x0;px<x1;++px){
                int fx=(px-x0)*8/(x1-x0),fy=(py-y0)*8/(y1-y0);
                if(awFont[(ch-32)*8+fy]&(128>>fx))pixel(px,py,c);
            }
        }x+=8;
    }
}
void Renderer::string(uint16_t id,uint8_t x,uint8_t y,uint8_t c){for(unsigned i=0;awStrings[i].text;++i)if(awStrings[i].id==id){text(awStrings[i].text,x*8,y,c);return;}}
void Renderer::nativeText(const Resource&r,uint8_t x,uint8_t y,uint8_t c){
    uint8_t h[8];
    if(r.type!=8||r.size<9||!pack.copy(r,0,h,8)||h[0]!='T'||h[1]!='X'||h[2]!=1||(h[3]&~1)||!h[4]||!h[5]||h[7]||h[6]>15||c>15){error="Invalid native text";return;}
    const int right=int(x)+h[4],bottom=int(y)+h[5];
    if(right>Width||bottom>Height){error="Text box outside frame";return;}
    if(h[3]&1)for(int py=y;py<bottom;++py)span(x,right-1,py,h[6]);
    int px=x,py=y;
    for(uint32_t off=8;off<r.size&&!pack.error;++off){
        uint8_t ch=pack.byte(r,off);
        if(!ch)return;
        if(ch=='\n'){px=x;py+=6;continue;}
        if(ch<32||ch>126){error="Invalid native character";return;}
        if(ch!=' '&&(px+3>right||py+5>bottom)){error="Native text exceeds box";return;}
        for(int fy=0;fy<5;++fy){uint8_t bits=fontRow(ch,fy);for(int fx=0;fx<3;++fx)if(bits&(4>>fx))pixel(px+fx,py+fy,c);}
        px+=4;
    }
    if(!pack.error)error="Unterminated native text";
}
}
