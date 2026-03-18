#ifndef _KERNEL_GRAPHICS_H
#define _KERNEL_GRAPHICS_H

#include <stdint.h>

#define VBE_ADDRESS 0x00100000
#define FRAME_BUFFER_ADDRESS 0x00101000

#define COLOR_BLACK   0x00000000
#define COLOR_WHITE   0x00FFFFFF

#define COLOR_RED     0x00FF0000
#define COLOR_GREEN   0x0000FF00
#define COLOR_BLUE    0x000000FF

#define COLOR_YELLOW  0x00FFFF00
#define COLOR_CYAN    0x0000FFFF
#define COLOR_MAGENTA 0x00FF00FF

#define COLOR_GRAY    0x00808080
#define COLOR_DGRAY   0x00404040
#define COLOR_LGRAY   0x00C0C0C0

typedef struct {
  uint16_t width;
  uint16_t height;
  uint8_t  bpp;
  uint32_t pitch;
  uint32_t physical_buffer;
} __attribute__((packed)) vbe_info_t;

void put_pixel(uint32_t x, uint32_t y, uint32_t color);
void clear(uint32_t color);

#endif