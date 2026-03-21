#ifndef _KERNEL_FAT32_H
#define _KERNEL_FAT32_H

#include <stdint.h>
#include <stdint.h>

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID

#define FAT_PARTITION_START 2057
#define MAX_ENTRY_NAME      11
#define FIRST_VALID_FAT     2
#define MAX_FILE_SIZE       0xFFFFFFFF
#define MAX_ENTRYS          128
#define FREE_FAT            0x00000000
#define EOF_FAT             0x0FFFFFF8
#define ROOT_DIR_CLUSTER    2

#define FREE_ENTRY    0x00
#define DELETED_ENTRY 0xE5

#define STATE_ENTRY_CHAR    0
#define STATE_EXT_CHAR      1


typedef struct __attribute__((packed)) {

  uint8_t  BS_jmpBoot[3];       // Jump instruction to boot code
  uint8_t  BS_OEMName[8];       // any string;

  uint16_t BPB_BytsPerSec;      // bytes per sector
  uint8_t  BPB_SecPerClus;      // bytes per cluster
  uint16_t BPB_RsvdSecCnt;      // Number of reserved sectors in the Reserved region of the volume starting at the first sector of the volume
  uint8_t  BPB_NumFATs;         // The count of FAT data structures on the volume
  uint16_t BPB_RootEntCnt;      // For FAT12 and FAT16 volumes, this field contains the count of 32-byte directory entries in the root directory. For FAT32 volumes, this field must be set to 0
  uint16_t BPB_TotSec16;        // This field is the old 16-bit total count of sectors on the volume
  uint8_t  BPB_Media;           // 0xF8 is the standard value for “fixed” (non-removable) media. For removable media, 0xF0 is frequently use
  uint16_t BPB_FATsz16;         // This field is the FAT12/FAT16 16-bit count of sectors occupied by ONE FAT. On FAT32 volumes this field must be 0, and BPB_FATSz32 contains the FAT size count
  uint16_t BPB_SecPerTrk;       // Sectors per track for interrupt 0x13. This field is only relevant for media that have a geometry
  uint16_t BPB_NumHeads;        // Number of heads for interrupt 0x13. This field is relevant as discussed earlier for BPB_SecPerTrk
  uint32_t BPB_HiddSec;         // Count of hidden sectors preceding the partition that contains this FAT volume
  uint32_t BPB_TotSec32;        // This field is the new 32-bit total count of sectors on the volume. This count includes the count of all sectors in all four regions of the volume

  uint32_t BPB_FATSz32;         // This field is the FAT32 32-bit count of sectors occupied by ONE FAT. BPB_FATSz16 must be 0.
  uint16_t BPB_ExtFlags;        // Bits 0-3 -- Zero-based number of active FAT. Only valid if mirroring is disabled
                                // Bits 4-6 -- Reserved.
                                // Bit 7 -- 0 means the FAT is mirrored at runtime into all FATs.
                                //         1 means only one FAT is active; it is the one referenced in bits 0-3.
                                // Bits 8-15 -- Reserved 

  uint16_t BPB_FSVer;           // High byte is major revision number.
                                // Low byte is minor revision number. This is the version number of the FAT32 volume.

  uint32_t BPB_RootClus;        // This is set to the cluster number of the first cluster of the root directory, usually 2 but not required to be 2
  uint16_t BPB_FSInfo;          // Sector number of FSINFO structure in the reserved area of the FAT32 volume. Usually 1
  uint16_t BPB_BkBootSec;       // If non-zero, indicates the sector number in the reserved area of the volume of a copy of the boot record. Usually 6
  uint8_t  BPB_Reserved[12];    // Reserved for future expansion. Code that formats FAT32 volumes should always set all of the bytes of this field to 0

  uint8_t  BS_DrvNum;           // Int 0x13 drive number (e.g. 0x80). This field supports MS-DOS bootstrap and is set to the INT 0x13 drive number of the media (0x00 for floppy disks, 0x80 for hard disks)
  uint8_t  BS_Reserved1;        // Reserved (used by Windows NT). Code that formats FAT volumes should always set this byte to 0
  uint8_t  BS_BootSig;          // Extended boot signature (0x29). This is a signature byte that indicates that the following three fields in the boot sector are present.
  uint32_t BS_VolId;            // Volume ID (serial number)
  uint8_t  BS_VolLab[11];       // Volume label. This field matches the 11-byte volume label recorded in the root directory. 
  uint8_t  BS_FilSysType[8];    // "FAT32   "

} FAT32_BootSector_s;

typedef struct __attribute__((packed)) {
  uint32_t FSI_LeadSig; // Value 0x41615252. This lead signature is used to validate that this is in fact an FSInfo sector
  uint32_t FSI_Reserved1[120]; // This field is currently reserved for future expansion
  uint32_t FSI_StrucSig; // Value 0x61417272. Another signature that is more localized in the sector to the location of the fields that are used
  uint32_t FSI_Free_Count; // Contains the last known free cluster count on the volume. If the value is 0xFFFFFFFF, then the free count is unknown and must be computed
  uint32_t FSI_Nxt_Free; // This is a hint for the FAT driver. It indicates the cluster number at which the driver should start looking for free clusters
  uint8_t  FSI_Reserved[12]; // This field is currently reserved for future expansion.
  uint32_t FSI_TrailSig; // Value 0xAA550000. This trail signature is used to validate that this is in fact an FSInfo sector
} FAT32_FSInfo_s;

typedef struct __attribute__((packed)) {
  uint8_t  DIR_name[11]; // Short name.
  uint8_t  DIR_Attr;     // ATTR_READ_ONLY 0x01
                         // ATTR_HIDDEN 0x02
                         // ATTR_SYSTEM 0x04
                         // ATTR_VOLUME_ID 0x08
                         // ATTR_DIRECTORY 0x10
                         // ATTR_ARCHIVE 0x20
                         // ATTR_LONG_NAME ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID

  uint8_t  DIR_NTRES;
 // The upper two bits of the attribute byte are reserved and should always be set to 0 when a file is created and never modified or looked at after that
  uint8_t  DIR_CrtTimeTenth; // Millisecond stamp at file creation time
  uint16_t DIR_CrtTime;      // Time file was created
  uint16_t DIR_CrtDate;      // Date file was created
  uint16_t DIR_LstAccDate;   // Last access date
  uint16_t DIR_FstClusHI;    // High word of this entry’s first cluster number
  uint16_t DIR_WrtTime;      // Time of last write
  uint16_t DIR_WrtDate;      // Date of last write
  uint16_t DIR_FstClustLO;   // Low word of this entry’s first cluster number
  uint32_t DIR_FileSize;     // 32-bit DWORD holding this file’s size in bytes
} FAT32_DirEntry;

typedef struct __attribute__((packed)) {
  uint8_t  LDIR_Ord;          // The order of this entry in the sequence of long dir entries associated with the short dir entry at the end of the long dir set.
  uint16_t LDIR_Nome1[5];     // Characters 1-5 of the long-name sub-component in this dir entry.
  uint8_t  LDIR_Attr;         // Attributes - must be ATTR_LONG_NAME
  uint8_t  LDIR_Type;         // reserved for future extensions
  uint8_t  LDIR_Chksum;       // Checksum of name in the short dir entry at the end of the long dir set.
  uint16_t LDIR_Name2[6];     // Characters 6-11 of the long-name sub-component in this dir entry
  uint16_t LDIR_FstClustLO;   // Must be ZERO
  uint16_t LDIR_Name3[2];     // Characters 12-13 of the long-name sub-component in this dir entry.
} LongFat32_DirEntry;

typedef struct {
  uint32_t Cluster;
  uint32_t Offset;
} Entry_Addr;

void create_fat32();

#endif