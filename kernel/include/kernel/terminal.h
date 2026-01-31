#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H

#include <stddef.h>
#include <stdint.h>

void terminal_putchar(char c);
void terminal_writestring(const char* s);
void clear_screen(uint32_t color);
void set_char_color(uint32_t color);

#endif