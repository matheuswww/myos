DEFAULT_HOST != ../default-host.sh
HOST_ ?= $(DEFAULT_HOST)
HOSTARCH != ../target-triplet-to-arch.sh $(HOST_)

CC ?= $(HOST_)-gcc

CFLAGS ?= -O2 -g -mno-sse -mno-sse2 -mno-sse3 -mno-ssse3 -mno-sse4 -mno-sse4.1 -mno-sse4.2 -mno-avx -mno-avx2 -mno-fma -mno-mmx -fno-omit-frame-pointer -march=i386 -mno-red-zone -fno-stack-protector
LDFLAGS ?=
LIBS ?=

DESTDIR ?=
PREFIX ?= /usr/local
EXEC_PREFIX ?= $(PREFIX)
BOOTDIR ?= $(EXEC_PREFIX)/boot
INCLUDEDIR ?= $(PREFIX)/include

SYSROOT ?= $(DESTDIR)

CFLAGS := $(CFLAGS) -ffreestanding -Wall -Wextra \
          -Iinclude -I$(SYSROOT)/usr/include
LDFLAGS := $(LDFLAGS)
LIBS := $(LIBS) -nostdlib -lk

ARCHDIR = arch/$(HOSTARCH)

include $(ARCHDIR)/make.config

CFLAGS := $(CFLAGS) $(KERNEL_ARCH_CFLAGS)
LDFLAGS := $(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS := $(LIBS) $(KERNEL_ARCH_LIBS)

KERNEL_OBJS = \
$(KERNEL_ARCH_OBJS) \
kernel/kernel.o

OBJS = $(KERNEL_OBJS)

LINK_LIST = \
$(LDFLAGS) \
$(KERNEL_OBJS) \
$(LIBS)

.PHONY: all clean install install-headers install-kernel

all: myos.kernel

myos.kernel: $(OBJS) $(ARCHDIR)/linker.ld
	$(CC) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LINK_LIST)

%.o: %.c
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS)

%.o: %.S
	$(CC) -MD -c $< -o $@ $(CFLAGS)

clean:
	rm -f myos.kernel
	rm -f $(OBJS)
	rm -f $(OBJS:.o=.d)

install: install-headers install-kernel

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.

install-kernel: myos.kernel
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp myos.kernel $(DESTDIR)$(BOOTDIR)

-include $(OBJS:.o=.d)
