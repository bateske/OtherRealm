// SPDX-License-Identifier: GPL-2.0-or-later
// Native regression tests for scheduling, bytecode bounds, Amiga input, and
// music synchronization. No proprietary game resources or audio device needed.
#include "aw_vm.h"
#include "aw_music.h"
#include <stdio.h>
#include <string.h>

using namespace otherrealm;

static unsigned failures;
#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr); \
    ++failures; } } while (0)

struct TestHost : VmHost, MusicReader {
    const uint8_t *code;
    size_t codeSize;
    uint8_t module[0xc0 + 2048];
    unsigned callbacks, shapes, presents, nativeTexts, events;
    uint16_t offset, zoom, frameDelay;
    int16_t x, y;
    bool secondary;
    uint8_t palette, page, event;
    TestHost() : code(0), codeSize(0), callbacks(0), shapes(0), presents(0), nativeTexts(0), events(0),
        offset(0), zoom(0), frameDelay(0), x(0), y(0), secondary(false), palette(0), page(0), event(0) {
        memset(module, 0, sizeof(module));
    }
    uint8_t readCode(uint16_t p) { CHECK(p < codeSize); return p < codeSize ? code[p] : 0; }
    void drawShape(bool s, uint16_t o, int16_t px, int16_t py, uint16_t z) {
        ++callbacks; ++shapes; secondary = s; offset = o; x = px; y = py; zoom = z;
    }
    void setPalette(uint8_t p) { ++callbacks; palette = p; }
    void selectPage(uint8_t p) { ++callbacks; page = p; }
    void fillPage(uint8_t, uint8_t) { ++callbacks; }
    void copyPage(uint8_t, uint8_t, int16_t) { ++callbacks; }
    void present(uint8_t p, uint16_t ms) { ++callbacks; ++presents; page = p; frameDelay = ms; }
    void drawString(uint16_t, uint8_t, uint8_t, uint8_t) { ++callbacks; }
    void drawNativeText(uint16_t id, uint8_t px, uint8_t py, uint8_t color) {
        ++callbacks; ++nativeTexts; offset=id; x=px; y=py; palette=color;
    }
    void systemEvent(uint8_t value) { ++callbacks; ++events; event=value; }
    void updateResource(uint16_t) { ++callbacks; }
    void sound(uint16_t, uint8_t, uint8_t, uint8_t) { ++callbacks; }
    void music(uint16_t, uint16_t, uint8_t) { ++callbacks; }
    uint32_t resourceSize(uint16_t id) { return id == 1 ? sizeof(module) : 0; }
    uint8_t readResourceByte(uint16_t id, uint32_t p) {
        CHECK(id == 1 && p < sizeof(module));
        return p < sizeof(module) ? module[p] : 0;
    }
    bool load(Vm &vm, const uint8_t *bytes, size_t size, uint16_t part = 16002) {
        code = bytes; codeSize = size; return vm.reset(size, part, false);
    }
};

static void testArithmeticAndInput() {
    TestHost host; Vm vm(host); VmInput input;
    const uint8_t code[] = {
        0x00,0,0x7f,0xff, // v0 = 32767
        0x03,0,0,1,       // v0 wraps to -32768
        0x01,1,0,         // v1 = v0
        0x00,2,0xff,0xff, // v2 = -1
        0x13,1,2,         // v1 = -32767
        0x17,0,0,15,      // logical unsigned right shift -> 1
        0x16,2,0,16,      // large shift -> 0, no C++ undefined shift
        0x00,3,0,1,
        0x16,3,0,15,      // 0x8000
        0x14,3,0xff,0xff,
        0x15,3,0,0x80,
        0x02,2,0,         // v2 = 1
        0x11
    };
    CHECK(host.load(vm, code, sizeof(code)));
    vm.variable(0xe5) = 77;
    input.directions = VmInput::Left | VmInput::Up; input.action = true;
    CHECK(vm.tick(input));
    CHECK(vm.variable(0) == 1);
    CHECK(vm.variable(1) == -32767);
    CHECK(vm.variable(2) == 1);
    CHECK(vm.variable(3) == -32640);
    CHECK(vm.variable(0xfb) == -1 && vm.variable(0xfc) == -1);
    CHECK(vm.variable(0xfd) == 10 && vm.variable(0xfe) == 138);
    CHECK(vm.variable(0xe5) == 77); // Amiga E5 is not an input register.
    CHECK(vm.taskPc(0) == Vm::InactiveTask);
}

static void testTasksAndCalls() {
    TestHost host; Vm vm(host); VmInput input;
    const uint8_t code[] = {
        0x08,1,0,14,      // task 1 becomes runnable next tick
        0x04,0,19,        // call subroutine: increments v1
        0x06,             // cooperative yield
        0x0c,1,1,2,       // request task 1 removal next tick
        0x06,
        0x11,             // task 0 ends on third tick
        0x03,0,0,1,       // [14] task 1 increments v0
        0x11,
        0x03,1,0,1,       // [19] subroutine
        0x05
    };
    CHECK(host.load(vm, code, sizeof(code)));
    CHECK(vm.tick(input));
    CHECK(vm.variable(0) == 0 && vm.variable(1) == 1);
    CHECK(vm.taskPc(1) == Vm::InactiveTask);
    CHECK(vm.tick(input));
    CHECK(vm.variable(0) == 1); // removal is deferred; task still runs this tick
    CHECK(vm.tick(input));
    CHECK(vm.taskPc(0) == Vm::InactiveTask && vm.taskPc(1) == Vm::InactiveTask);
}

static void testBranches() {
    TestHost host; Vm vm(host); VmInput input;
    const uint8_t code[] = {
        0x00,0,0xff,0xff,
        0x0a,0x44,0,0,0,0,15, // v0 < signed word 0: jump to 15
        0x00,1,0,99,
        0x00,2,0,3,           // [15] v2 = 3
        0x03,1,0,1,           // [19] v1++
        0x09,2,0,19,          // decrement + loop
        0x0a,0x80,0,0,0,37,   // v0 == v0: jump to 37
        0x00,1,0,99,
        0x11
    };
    CHECK(host.load(vm, code, sizeof(code)));
    CHECK(vm.tick(input));
    CHECK(vm.variable(1) == 3 && vm.variable(2) == 0);
}

static void testShapeModesAndDisplay() {
    TestHost host; Vm vm(host); VmInput input;
    const uint8_t compact[] = { 0x80,3,250,205,0x06,0x11 };
    CHECK(host.load(vm, compact, sizeof(compact)) && vm.tick(input));
    CHECK(host.offset == 6 && host.x == 256 && host.y == 199 && host.zoom == 64 && !host.secondary);
    // X/Y from signed variables, explicit zoom variable, primary segment.
    const uint8_t variable[] = { 0x55,0,5,0,1,2,0x11 };
    CHECK(host.load(vm, variable, sizeof(variable)));
    vm.variable(0) = -10; vm.variable(1) = 240; vm.variable(2) = 128;
    CHECK(vm.tick(input));
    CHECK(host.offset == 10 && host.x == -10 && host.y == 240 && host.zoom == 128);
    const uint8_t secondary[] = { 0x7b,0,6,20,50,0x11 };
    CHECK(host.load(vm, secondary, sizeof(secondary)) && vm.tick(input));
    CHECK(host.secondary && host.x == 276 && host.y == 50 && host.zoom == 64);
    const uint8_t full[] = { 0x40,0,7,0xff,0xfb,0xff,0xf6,0x11 };
    CHECK(host.load(vm, full, sizeof(full)) && vm.tick(input));
    CHECK(!host.secondary && host.x == -5 && host.y == -10 && host.zoom == 64);
    const uint8_t display[] = { 0x0b,4,0,0x0d,2,0x00,0xff,0,3,0x10,0xfe,0x11 };
    CHECK(host.load(vm, display, sizeof(display)) && vm.tick(input));
    CHECK(host.palette == 4 && host.page == 0xfe && host.frameDelay == 60);
}

static void testFaults() {
    TestHost host; Vm vm(host); VmInput input;
    const uint8_t invalid[] = {0x20};
    CHECK(host.load(vm, invalid, sizeof(invalid)) && !vm.tick(input));
    CHECK(vm.error() && vm.faultTask() == 0 && vm.faultPc() == 0);
    const uint8_t loop[] = {0x07,0,0};
    CHECK(host.load(vm, loop, sizeof(loop)) && !vm.tick(input, 100));
    CHECK(vm.instructionsLastTick() == 100);
    const uint8_t stack[] = {0x04,0,0};
    CHECK(host.load(vm, stack, sizeof(stack)) && !vm.tick(input));
    CHECK(strcmp(vm.error(), "call stack overflow") == 0);
    const uint8_t underflow[] = {0x05};
    CHECK(host.load(vm, underflow, sizeof(underflow)) && !vm.tick(input));
    const uint8_t badTask[] = {0x08,64,0,0};
    CHECK(host.load(vm, badTask, sizeof(badTask)) && !vm.tick(input));
    const uint8_t truncatedDisplay[] = {0x10};
    const unsigned before = host.callbacks;
    CHECK(host.load(vm, truncatedDisplay, sizeof(truncatedDisplay)) && !vm.tick(input));
    CHECK(host.callbacks == before);
    const uint8_t truncatedShape[] = {0x40,0,1,0,0};
    CHECK(host.load(vm, truncatedShape, sizeof(truncatedShape)) && !vm.tick(input));
    CHECK(host.callbacks == before);
    CHECK(!vm.reset(0, 16002));
    CHECK(!vm.reset(65537, 16002));
}

static void testNativeExtensions() {
    TestHost host; Vm vm(host); VmInput input;
    const uint8_t text[]={0x1b,0x12,0x34,7,9,15,0x00,0,0,42,0x11};
    CHECK(host.load(vm,text,sizeof(text))&&vm.tick(input));
    CHECK(host.nativeTexts==1&&host.offset==0x1234&&host.x==7&&host.y==9&&host.palette==15);
    CHECK(vm.variable(0)==42); // six-byte extension preserves the following opcode
    for(unsigned n=1;n<6;++n){
        const unsigned before=host.nativeTexts;
        CHECK(host.load(vm,text,n)&&!vm.tick(input));
        CHECK(host.nativeTexts==before); // never dispatch a partially decoded call
    }
    const uint8_t nop[]={0x1c,0,0,0,0,0,0x00,0,0,55,0x11};
    CHECK(host.load(vm,nop,sizeof(nop))&&vm.tick(input));
    CHECK(host.events==0&&vm.variable(0)==55);
    for(uint8_t event=1;event<=3;++event){
        uint8_t code[]={
            0x08,1,0,17,0x06, // first tick enables task one
            0x1c,event,0,0,0,0,
            0x03,0,0,1,0x06,0x11,
            0x03,1,0,1,0x11
        };
        CHECK(host.load(vm,code,sizeof(code))&&vm.tick(input));
        const unsigned before=host.events;
        CHECK(vm.tick(input)&&host.events==before+1&&host.event==event);
        CHECK(vm.variable(0)==0&&vm.variable(1)==0); // current and later tasks both suspend
        CHECK(vm.taskPc(0)==11&&vm.taskPc(1)==17);
        CHECK(vm.tick(input)&&vm.variable(0)==1&&vm.variable(1)==1);
    }
    uint8_t invalid[]={0x1c,4,0,0,0,0};
    unsigned before=host.events;
    CHECK(host.load(vm,invalid,sizeof(invalid))&&!vm.tick(input)&&host.events==before);
    for(unsigned i=2;i<6;++i){
        memset(invalid+1,0,5);invalid[1]=1;invalid[i]=1;
        CHECK(host.load(vm,invalid,sizeof(invalid))&&!vm.tick(input)&&host.events==before);
    }
    memset(invalid+1,0,5);invalid[1]=1;
    for(unsigned n=1;n<6;++n){
        CHECK(host.load(vm,invalid,n)&&!vm.tick(input)&&host.events==before);
    }
}

static void testSilentMusic() {
    TestHost host; SilentMusic music(host); int16_t sync = -1;
    host.module[0] = 0x0b; host.module[1] = 0xa7; // 2983 -> 25 ms per row
    host.module[0x3f] = 2;
    host.module[0x40] = 0; host.module[0x41] = 1;
    host.module[0xc0] = 0xff; host.module[0xc1] = 0xfd;
    host.module[0xc3] = 42;
    host.module[0xd0] = 0xff; host.module[0xd1] = 0xfd;
    host.module[0xd3] = 43;
    host.module[0x4c0] = 0xff; host.module[0x4c1] = 0xfd;
    host.module[0x4c3] = 77;
    CHECK(music.play(1));
    music.advance(0, sync); CHECK(sync == 42 && music.eventsProcessed() == 1);
    music.advance(24, sync); CHECK(sync == 42);
    music.advance(1, sync); CHECK(sync == 43);
    music.advance(63 * 25, sync); CHECK(sync == 77 && music.eventsProcessed() == 65);
    music.advance(100000, sync); CHECK(!music.playing() && music.eventsProcessed() == 128);
    CHECK(!music.play(2) && music.error());
    CHECK(!music.play(1, 0, 2));
    host.module[0x40] = 255;
    CHECK(music.play(1)); music.advance(0, sync); CHECK(music.error() && !music.playing());
}

int main() {
    testArithmeticAndInput(); testTasksAndCalls(); testBranches();
    testShapeModesAndDisplay(); testFaults(); testNativeExtensions(); testSilentMusic();
    if (failures) { fprintf(stderr, "%u failures\n", failures); return 1; }
    printf("VM and silent music tests passed (VM=%u bytes, tracker=%u bytes).\n",
           static_cast<unsigned>(sizeof(Vm)), static_cast<unsigned>(sizeof(SilentMusic)));
    return 0;
}
