#include <kernel/terminal.h>
#include <kernel/font.h>
#include <kernel/graphics.h>

uint32_t color = COLOR_WHITE;

void clear_screen(uint32_t color) {
  clear(color);
}

void set_char_color(uint32_t color) {
  color = color;
}

void terminal_putchar(char c) {
  static uint16_t x = 0;
  static uint16_t y = 0;

  if (c == '\n') {
    y+=12;
    x = 0;
    return;
  }

  draw_char(c, x, y, color);

  x += 8;
}

void terminal_writestring(const char* s) {
  while (*s) {
    terminal_putchar(*s);
    s++;
  }
}