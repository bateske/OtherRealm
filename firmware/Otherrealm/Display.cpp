// ST7735/SPI setup adapted from CHGfx, Copyright (c) 2026 bateske, MIT.
// See THIRD_PARTY_CHGFX_LICENSE.txt. The renderer/DMA pipeline is independent:
// two scanlines, no display framebuffer and no 1KB palette expansion table.
#include "Display.h"
#include <Arduino.h>

extern "C" volatile uint32_t CFGHR_tmpA, CFGHR_tmpB, CFGHR_tmpC;
namespace device {
namespace {
const uint32_t SPE = 1u << 6, TXE = 1u << 1, BSY = 1u << 7;
const uint32_t DFF16 = 1u << 11;
alignas(4) uint16_t scanline[2][128];

void pin(GPIO_TypeDef *port, unsigned n, uint32_t mode) {
    unsigned shift = (n & 7) * 4;
    if (n < 8) port->CFGLR = (port->CFGLR & ~(15u << shift)) | (mode << shift);
    else {
        volatile uint32_t *shadow = port == GPIOA ? &CFGHR_tmpA :
                                   port == GPIOB ? &CFGHR_tmpB : &CFGHR_tmpC;
        *shadow = (*shadow & ~(15u << shift)) | (mode << shift);
        port->CFGHR = *shadow;
    }
}
void drain() {
    while (!(SPI1->STATR & TXE)) {}
    while (SPI1->STATR & BSY) {}
}
void bits(bool wide) {
    drain();
    SPI1->CTLR1 &= ~SPE;
    if (wide) SPI1->CTLR1 |= DFF16;
    else SPI1->CTLR1 &= ~DFF16;
    SPI1->CTLR1 |= SPE;
}
void byte(uint8_t value) {
    while (!(SPI1->STATR & TXE)) {}
    SPI1->DATAR = value;
}
void cmd(uint8_t value) {
    bits(false);
    GPIOB->BCR = 1u;
    byte(value);
    drain();
    GPIOB->BSHR = 1u;
}
void data(const uint8_t *p, unsigned n) { while (n--) byte(*p++); }
void window(unsigned y, unsigned height) {
    GPIOB->BSHR = 1u << 12;
    pin(GPIOB, 12, 3);
    GPIOA->BCR = 1u << 4;
    cmd(0x2A); byte(0); byte(2); byte(0); byte(129);
    cmd(0x2B); byte(0); byte(y + 3); byte(0); byte(y + height + 2);
    cmd(0x2C);
    bits(true);
}
void dmaStart(const uint16_t *p, unsigned count, bool increment = true) {
    DMA1_Channel3->CFGR = 0;
    DMA1->INTFCR = 15u << 8;
    DMA1_Channel3->PADDR = (uint32_t)&SPI1->DATAR;
    DMA1_Channel3->MADDR = (uint32_t)p;
    DMA1_Channel3->CNTR = count;
    const uint32_t config = (1u << 4) | (1u << 8) | (1u << 10) | (3u << 12);
    DMA1_Channel3->CFGR = config | (increment ? 1u << 7 : 0);
    SPI1->CTLR2 |= 1u << 1;
    DMA1_Channel3->CFGR |= 1;
}
void dmaFinish() {
    while (!(DMA1->INTFR & (1u << 9))) {}
    DMA1_Channel3->CFGR = 0;
    DMA1->INTFCR = 15u << 8;
    SPI1->CTLR2 &= ~(1u << 1);
    drain();
}
// No division per pixel: fixed point nearest-neighbour expansion. The
// scanline can run while the previous 256 bytes are transmitted by DMA.
__attribute__((section(".gnu.linkonce.r.otherrealm.scanline"), noinline))
void convert(uint16_t *dst, const uint8_t *src, unsigned width, const uint16_t *pal) {
    unsigned pos = 0;
    for (unsigned x = 0; x != 128; ++x, pos += width) {
        unsigned sx = pos >> 7;
        unsigned packed = src[sx >> 1];
        dst[x] = pal[(packed >> ((1 - (sx & 1)) * 4)) & 15];
    }
}
} // namespace

uint8_t *displayScratch() { return reinterpret_cast<uint8_t *>(scanline); }

void displaySolid(uint16_t color) {
    window(0, 128);
    scanline[0][0] = color;
    dmaStart(scanline[0], 128 * 128, false);
    dmaFinish();
    GPIOA->BSHR = 1u << 4;
}

void displayBegin() {
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                     RCC_APB2Periph_GPIOC | RCC_APB2Periph_SPI1;
    RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;
    GPIOA->BSHR = 1u << 4;
    GPIOB->BSHR = 1u | (1u << 11) | (1u << 12);
    pin(GPIOA, 5, 11); pin(GPIOA, 7, 11); pin(GPIOA, 4, 3);
    pin(GPIOB, 0, 3); pin(GPIOB, 11, 3); pin(GPIOB, 12, 3);
    // Audio owns PB10; preserve its pin configuration through the GPIO shadow.
    SPI1->CTLR1 = 0;
    SPI1->CTLR2 = 0;
    SPI1->CTLR1 = (1u << 2) | (1u << 8) | (1u << 9) | SPE;
    delay(5); GPIOB->BCR = 1u << 12;
    delay(20); GPIOB->BSHR = 1u << 12; delay(150);
    GPIOA->BCR = 1u << 4;
    cmd(0x01); delay(150); cmd(0x11); delay(255);
    static const uint8_t frame[] = {5, 58, 58, 5, 58, 58};
    cmd(0xB1); data(frame, 3); cmd(0xB2); data(frame, 3);
    cmd(0xB3); data(frame, 6);
    cmd(0xB4); byte(7);
    cmd(0xC0); byte(0xA2); byte(2); byte(0x84);
    cmd(0xC1); byte(0xC5); cmd(0xC2); byte(0x0A); byte(0);
    cmd(0xC3); byte(0x8A); byte(0x2A); cmd(0xC4); byte(0x8A); byte(0xEE);
    cmd(0xC5); byte(0x0E); cmd(0x20); cmd(0x36); byte(0xC8);
    static const uint8_t gp[16] = {2,28,7,18,55,50,41,45,41,37,43,57,0,1,3,16};
    static const uint8_t gn[16] = {3,29,7,6,46,44,41,45,46,46,55,63,0,0,2,16};
    cmd(0xE0); data(gp,16); cmd(0xE1); data(gn,16);
    cmd(0x3A); byte(5); cmd(0x13); delay(10); cmd(0x29); delay(100);
    drain(); GPIOA->BSHR = 1u << 4;
    displaySolid(0);
}

void displayPresent(const uint8_t *pixels, uint16_t width, uint16_t height,
                    const uint16_t *rgb565) {
    if (!pixels || !width || !height || (width & 1)) return;
    window(24, 80);
    unsigned cur = 0;
    convert(scanline[cur], pixels, width, rgb565);
    for (unsigned y = 0; y != 80; ++y) {
        dmaStart(scanline[cur], 128);
        unsigned next = cur ^ 1;
        if (y + 1 != 80) {
            unsigned sy = ((y + 1) * height) / 80;
            convert(scanline[next], pixels + sy * (width / 2), width, rgb565);
        }
        dmaFinish();
        cur = next;
    }
    GPIOA->BSHR = 1u << 4;
}

void displayUi(UiScanline renderRow, void *context) {
    if (!renderRow) return;
    window(0, 128);
    unsigned cur = 0;
    renderRow(scanline[cur], 0, context);
    for (unsigned y = 0; y != 128; ++y) {
        dmaStart(scanline[cur], 128);
        unsigned next = cur ^ 1;
        if (y + 1 != 128) renderRow(scanline[next], uint8_t(y + 1), context);
        dmaFinish();
        cur = next;
    }
    GPIOA->BSHR = 1u << 4;
}
} // namespace device
