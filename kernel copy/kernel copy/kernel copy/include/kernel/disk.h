#ifndef _KERNEL_DISK_H
#define _KERNEL_DISK_H

#include <stdint.h>

#define DISK_SIZE 1024 * 1024

typedef struct __attribute__((packed)) {
    uint16_t    dd_tf;          // Task File base I/O port
    uint16_t    dd_dcr;         // Device Control / Alternate Status
    uint32_t    dd_stLBA;       // Starting LBA
    uint32_t    dd_prtlen;      // Partition size
    uint8_t     dd_sbits;       // Drive/Head bits (0xE0 ou 0xF0)
} ata_driver_data;

void read_ata_st_c(uint32_t lba, void* buffer, uint32_t sectors, ata_driver_data* drv);

void write_ata_st_c(uint32_t lba, void* buffer, uint32_t sectors, ata_driver_data* drv);

#endif