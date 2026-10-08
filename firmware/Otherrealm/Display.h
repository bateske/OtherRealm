#pragma once
#include <stdint.h>

// CHGame Rev0 display transport. The renderer owns all framebuffer memory.
namespace device {
void displayBegin();
// Packed 4bpp, even x in high nibble. Scales to 128x80 at y=24.
void displayPresent(const uint8_t *pixels, uint16_t width, uint16_t height,
                    const uint16_t *rgb565);
void displaySolid(uint16_t rgb565);
// Full-panel UI without touching the renderer's logical pages. The callback
// fills 128 RGB565 pixels and must not access SPI/SD while DMA is active.
typedef void (*UiScanline)(uint16_t *pixels, uint8_t y, void *context);
void displayUi(UiScanline renderRow, void *context);
// Reuses the 512B DMA scanlines while no displayPresent call is active.
uint8_t *displayScratch();
}
