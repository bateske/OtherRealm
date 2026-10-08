#include "Buttons.h"
#include <Arduino.h>
extern "C" volatile uint32_t CFGHR_tmpB, CFGHR_tmpC;

namespace device {
void buttonsBegin() {
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
}
uint8_t buttonsRead() {
    uint32_t b = ~GPIOB->INDR, c = ~GPIOC->INDR;
    return ((b>>1)&1) | ((b>>5)&2) | ((b>>2)&4) | ((c>>11)&8) |
           ((b<<1)&16) | ((c>>10)&32) | ((b>>2)&64) | (b&128);
}
}
