#include <stdio.h>

#include <kernel/tty.h>

void kernel_main(void) {
  terminal_initialize();
	printf("%s", "Hello, Kernel World!\n");
}