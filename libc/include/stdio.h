#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stdint.h>

#define EOF (-1)

int printf(const char* __restrict, ...);
int putchar(int);
int puts(const char*);
char* itoa(int64_t num);

#endif