#include <stdint.h>
#include <stdio.h>
#include <kernel/terminal.h>
#include <kernel/graphics.h>

void kernel_main(void) {
  clear_screen(COLOR_RED);
  set_char_color(COLOR_BLUE);
  putchar('H');
  printf("%s", "Hello\nKernel World!!!");
  while(1);
}