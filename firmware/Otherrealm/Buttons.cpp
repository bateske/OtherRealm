#include "Buttons.h"
#include <Arduino.h>
extern "C" volatile uint32_t CFGHR_tmpB, CFGHR_tmpC;

namespace device {
void buttonsBegin() {
#if defined(ARDUINO_CHGAME_REV1)
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC;
    const unsigned pins[] = {0,1,6,7};
    for(unsigned n:pins) GPIOB->CFGLR=(GPIOB->CFGLR & ~(15u<<(n*4))) | (8u<<(n*4));
    CFGHR_tmpB=(CFGHR_tmpB & ~0xF00Fu) | 0x8008u; GPIOB->CFGHR=CFGHR_tmpB;
    GPIOC->CFGLR=(GPIOC->CFGLR & ~0xF00Fu) | 0x8008u;
    GPIOB->BSHR=(1u<<0)|(1u<<1)|(1u<<6)|(1u<<7)|(1u<<8)|(1u<<11);
    GPIOC->BSHR=(1u<<0)|(1u<<3);
#else
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC;
    // CHGame Rev0's eight active-low switches, configured as inputs with
    // pull-ups. CFGHR is write-only and shares the Arduino core's shadows.
    const unsigned lowPins[] = {1,3,4,6,7};
    for (unsigned i=0; i<5; ++i) {
        unsigned shift = lowPins[i]*4;
        GPIOB->CFGLR = (GPIOB->CFGLR & ~(15u << shift)) | (8u << shift);
    }
    CFGHR_tmpB = (CFGHR_tmpB & ~15u) | 8u;
    GPIOB->CFGHR = CFGHR_tmpB;
    CFGHR_tmpC = (CFGHR_tmpC & 0x00FFFFFFu) | 0x88000000u;
    GPIOC->CFGHR = CFGHR_tmpC;
    GPIOB->BSHR = (1u<<1)|(1u<<3)|(1u<<4)|(1u<<6)|(1u<<7)|(1u<<8);
    GPIOC->BSHR = (1u<<14)|(1u<<15);
#endif
}
uint8_t buttonsRead() {
    uint32_t b = ~GPIOB->INDR, c = ~GPIOC->INDR;
#if defined(ARDUINO_CHGAME_REV1)
    return ((b>>11)&1)|((b>>5)&2)|((b<<2)&4)|((b<<2)&8)|((c<<1)&16)|((c<<5)&32)|((b>>2)&64)|(b&128);
#else
    return ((b>>1)&1) | ((b>>5)&2) | ((b>>2)&4) | ((c>>11)&8) |
           ((b<<1)&16) | ((c>>10)&32) | ((b>>2)&64) | (b&128);
#endif
}
}
