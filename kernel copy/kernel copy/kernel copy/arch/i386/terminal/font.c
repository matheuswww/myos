#include <kernel/font.h>
#include <kernel/graphics.h>
#include <stdint.h>

void draw_char(char c, uint16_t x, uint16_t y, uint32_t color) {
  const uint8_t* glyph = 0;

  for (uint32_t i = 0; i < sizeof(font)/sizeof(font[0]); i++) {
    if (font[i].c == c) {
      glyph = font[i].glyph;
      break;
    }
  }

  if (!glyph) return;
  
  for (int row = 0; row < 8; row++) {
    uint8_t bits = glyph[row];
    for (int col = 0; col < 8; col++) {
      if (bits & (0x80 >> col)) {
        put_pixel(x + col, y + row, color);
      }
    }
  }
}