#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <sys/cdefs.h>
#include <stdint.h>

__attribute__((__noreturn__))
void  abort(void);
void* alloc(uint32_t size);
void  free(void* ptr);
void heap_init();

#endif