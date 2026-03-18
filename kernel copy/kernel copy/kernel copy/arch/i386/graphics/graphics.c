#include <kernel/graphics.h>

vbe_info_t *vbe = (vbe_info_t *)VBE_ADDRESS;
uint32_t *fb  = (uint32_t *)FRAME_BUFFER_ADDRESS;

void clear(uint32_t color) {
  for (uint16_t y = 0; y < vbe->height; y++) {
    for (uint16_t x = 0; x < vbe->width; x++) {
      uint32_t offset = y * vbe->width + x;
      fb[offset] = color;
    }
  }
}

void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
  if (x >= vbe->width || y >= vbe->height) {
    return;
  }

  uint32_t offset = y * vbe->width + x;
  fb[offset] = color;
}