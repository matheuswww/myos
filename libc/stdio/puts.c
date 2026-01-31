#include <stdio.h>

#include <kernel/terminal.h>

int puts(const char* string) {
	return printf("%s\n", string);
}