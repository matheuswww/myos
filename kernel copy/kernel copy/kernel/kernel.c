#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <kernel/terminal.h>
#include <kernel/graphics.h>
#include <kernel/fat32.h>


void kernel_main(void) {
  heap_init();
  create_fat32();
  while(1);
}