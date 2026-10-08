#pragma once
#include "aw_pack.h"
namespace otherrealm {
class Renderer {
public:
    explicit Renderer(Pack &p):pack(p){reset();}
    void reset();
    void select(uint8_t p){work=resolve(p);}
    void fill(uint8_t p,uint8_t color);
    void copy(uint8_t src,uint8_t dst,int16_t scroll);
    void shape(const Resource&r,uint16_t offset,int16_t x,int16_t y,uint16_t zoom);
    void bitmap(const Resource&r);
    void palette(const Resource&r,uint8_t num);
    void present(uint8_t p);
    void string(uint16_t id,uint8_t x,uint8_t y,uint8_t color);
    void text(const char *s,int x,int y,uint8_t color);
    void nativeText(const Resource&r,uint8_t x,uint8_t y,uint8_t color);
    bool loadTitle(bool completed=false);
    const uint8_t *titleFrame()const{return reinterpret_cast<const uint8_t *>(pages);}
    const uint8_t *frame()const{return pages[front];}
    const uint16_t *colors()const{return pal;}
    uint32_t polygons;
    const char *error;
private:
    struct Point {int16_t x,y;};
    Pack &pack;
    uint8_t pages[4][PageBytes];
    uint16_t pal[16];
    uint8_t work,front,back;
    uint16_t nodesLeft;
    uint8_t resolve(uint8_t p)const{return p<4?p:p==0xFF?back:p==0xFE?front:0;}
    void draw(const Resource&r,uint32_t offset,int x,int y,uint16_t zoom,uint8_t color,uint8_t depth);
    void polygon(const Resource&r,uint32_t offset,int x,int y,uint16_t zoom,uint8_t color);
    void span(int x1,int x2,int y,uint8_t color);
    void pixel(int x,int y,uint8_t color);
};
}
