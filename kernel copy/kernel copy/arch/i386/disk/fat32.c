#include <kernel/disk.h>
#include <kernel/fat32.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define FAT_PARTITION_START 2057
#define STATE_PATH_CHAR 0
#define STATE_EXT_CHAR  1


FAT32_BootSector_s* fat_boot;
FAT32_FSInfo_s* fs_info;
uint16_t clusterSizeBytes;

FAT32_BootSector_s* create_fat_boot_s() {
  fat_boot = (FAT32_BootSector_s*)alloc(sizeof(FAT32_BootSector_s));
  
  memset(fat_boot, 0, sizeof(FAT32_BootSector_s));

  // fat_boot->BS_jmpBoot - zero

  memcpy(fat_boot->BS_OEMName, "MYOSV0.0", 8);

  fat_boot->BPB_BytsPerSec = 512;
  fat_boot->BPB_SecPerClus = 8;
  fat_boot->BPB_RsvdSecCnt = 32; // FAT start sector
  fat_boot->BPB_NumFATs = 2;
  // fat_boot->BPB_RootEntCnt - zero;
  // fat_boot->BPB_TotSec16 - zero;
  fat_boot->BPB_Media = 0xF8;
  // fat_boot->BPB_FATsz16 - zero;
  // fat_boot->BPB_SecPerTrk - zero;
  // fat_boot->BPB_NumHeads - zero;
  // fat_boot->BPB_HiddSec - zero;
  fat_boot->BPB_TotSec32 = 1048576; // 512MB
  fat_boot->BPB_FATSz32 = 1026; // ceil((num_clusters × 4 bytes) / bytes_per_sector) FAT32 size in sectors for all clusters
  fat_boot->BPB_ExtFlags = 0x00;
  fat_boot->BPB_FSVer = 0x0000;
  fat_boot->BPB_RootClus = 2; 
  fat_boot->BPB_FSInfo = 1;
  fat_boot->BPB_BkBootSec = 6;
  // fat_boot->BPB_Reserved - zero
  fat_boot->BS_DrvNum = 0x80;
  // fat_boot->BS_Reserved1 - zero
  fat_boot->BS_BootSig = 0x29;
  // fat_boot->BS_VolID - zero

  memcpy(fat_boot->BS_VolLab, "MYOS       ", 11);

  memcpy(fat_boot->BS_FilSysType, "FAT32   ", 8);

  return fat_boot;
}

FAT32_FSInfo_s* create_fat32_fs_info_s() {
  fs_info = (FAT32_FSInfo_s*)alloc(512);
  
  memset(fs_info, 0, 512);

  fs_info->FSI_LeadSig = 0x41615252;
  // fs_info->FSI_Reserved1 - zero
  fs_info->FSI_StrucSig = 0x61417272;
  fs_info->FSI_Free_Count = 0xFFFFFFFF;
  fs_info->FSI_Nxt_Free = 2;
  // fs_info->FSI_Reserved2 - zero
  fs_info->FSI_TrailSig = 0xAA550000;


  return fs_info;
}

FAT32_DirEntry* create_fat32_entry(uint8_t name[11], uint8_t *buffer, uint32_t size, uint8_t attr) {
  FAT32_DirEntry fatDirEntry;

  uint32_t cluster = fs_info->FSI_Nxt_Free;

  memset(&fatDirEntry, 0, sizeof(FAT32_DirEntry));
  memcpy(fatDirEntry.DIR_name, name, 11);
  fatDirEntry.DIR_Attr = attr;
  fatDirEntry.DIR_NTRES = 0;
  fatDirEntry.DIR_CrtTimeTenth = 0;
  fatDirEntry.DIR_CrtTime = 0;
  fatDirEntry.DIR_CrtDate = 0;
  fatDirEntry.DIR_LstAccDate = 0;
  fatDirEntry.DIR_FstClusHI = (cluster >> 16) & 0xFFFF;
  fatDirEntry.DIR_WrtTime = 0;
  fatDirEntry.DIR_FstClustLO = cluster & 0x0000FFFF;
  fatDirEntry.DIR_FileSize = size;

  return (FAT32_DirEntry*)0;
}

#include <stdbool.h>

bool is_valid_fat_char(char c, bool file) {
  if (c >= 'A' && c <= 'Z') return true;
  if (c >= '0' && c <= '9') return true;

  switch (c) {
    case '$': case '%': case '\'': case '-': case '_':
    case '@': case '~': case '`': case '!': case '(':
    case ')': case '{': case '}': case '^': case '#':
    case '&':
    return true;
  }

  if (file && c == '.') {
    return true;
  }
  
  return false;
}

FAT32_DirEntry* create_fat32_file(uint8_t *buffer, uint32_t size, uint8_t* path) {
  uint8_t* ptr = path;
  char c = *ptr;
  uint8_t current_path[12];
  uint8_t current_path_len = 0;
  uint8_t current_ext_len = 0;
  uint16_t paths = 1;

  if (path[0] != '/' || (path[0] == '/' && path[1] == '/')) {
    printf("invalid path: %s", path);
    return (FAT32_DirEntry*)0;
  }

  uint8_t state = STATE_PATH_CHAR;

  while (1) {
    printf("%c\n", c);
    if (state == STATE_EXT_CHAR && current_ext_len >= 4 && c != '\0') {
      printf("the path cannot contain '.'");
      return (FAT32_DirEntry*)0;
    }

    if (c == '/' && current_path_len == 0 && paths > 1) {
      printf("invalid path: %s", path);
      return (FAT32_DirEntry*)0;
    }

    if ((c == '/' && current_path_len > 0) || c == '\0') {
      ptr--;
      c = *ptr;
      *ptr++;
      if (c == '.') {
        printf("the path cannot end with '.'");
        return (FAT32_DirEntry*)0;
      }
      if (c == ' '){
        printf("the path cannot start with ' '");
        return (FAT32_DirEntry*)0;
      }
      if (c == '\0') {
        current_path[current_path_len] = '\0';
        break;
      }
      current_path[current_path_len] = '\0';
      memset(current_path, 0, 12);
      current_path_len = 0;
      paths++;
    }

    switch (state) {
    case STATE_EXT_CHAR:
      if (c == '.') {
        printf("the name cannot have two points");
        return (FAT32_DirEntry*)0;
      }
      current_ext_len++;
      if (current_ext_len > 4) {
        printf("the ext cannot have a len greater than 3");
        return (FAT32_DirEntry*)0;
      }

      case STATE_PATH_CHAR:
      if (c != '/') {
          printf("%d - %d\n", paths, current_path_len);
          if (!is_valid_fat_char(c, true)) {
            printf("invalid char: %c", c);
            return (FAT32_DirEntry*)0;
          }
          if (current_path_len == 0 && c == '.'){
            printf("the name cannot start with '.'");
            return (FAT32_DirEntry*)0;
          }
          if (current_path_len == 0 && c == 0X00 || c == 0xE5) {
            printf("the name cannot start with 0x00 or OxE5");
            return (FAT32_DirEntry*)0;
          }
          current_path[current_path_len] = c; 
          current_path_len++;

          if (current_path_len >= 12 && c != '/' && c != '\0') {
            printf("too large path: %s", path);
            return (FAT32_DirEntry*)0;
          }

          if (c == '.') {
            state = STATE_EXT_CHAR;
          }
        }
        break;
    }

    ptr++;
    c = *ptr;
  }

  uint32_t nextFreeCluster = (size + clusterSizeBytes - 1) / clusterSizeBytes + 1;
  fs_info->FSI_Nxt_Free = nextFreeCluster;

  return (FAT32_DirEntry*)0;
}

void create_fat32() {
	static const ata_driver_data configs[1] = {
    {0x1F0, 0x3F6, FAT_PARTITION_START, DISK_SIZE, 0xE0},
  };
  ata_driver_data drv = configs[0];
  create_fat_boot_s();

  clusterSizeBytes = fat_boot->BPB_BytsPerSec * fat_boot->BPB_SecPerClus;
 
  write_ata_st_c(0, fat_boot, 1, &drv);

  create_fat32_fs_info_s();

  write_ata_st_c(fat_boot->BPB_FSInfo, fs_info, 1, &drv);
  
  uint8_t sectors_backup = 3;

  uint8_t* backup = (uint8_t*)alloc(fat_boot->BPB_BytsPerSec * sectors_backup);

  read_ata_st_c(0, backup, sectors_backup, &drv);

  write_ata_st_c(fat_boot->BPB_BkBootSec, backup, sectors_backup, &drv);

  uint8_t zero[512];
  memset(zero, 0, 512);

  for (uint32_t i = 0; i < fat_boot->BPB_FATSz32 * fat_boot->BPB_NumFATs; i++) {
    write_ata_st_c(fat_boot->BPB_RsvdSecCnt + i, zero, 1, &drv);
  }

  uint32_t fat[128];

  memset(fat, 0, sizeof(fat));

  fat[0] = 0x0FFFFFF8;
  fat[1] = 0x0FFFFFFF;
  fat[2] = 0x0FFFFFFF;

  for (int i = 0; i < fat_boot->BPB_NumFATs; i++) {
    write_ata_st_c(fat_boot->BPB_RsvdSecCnt + i * fat_boot->BPB_FATSz32, fat, 1, &drv);
  }

  uint32_t firstDataSector = fat_boot->BPB_RsvdSecCnt + (fat_boot->BPB_NumFATs * fat_boot->BPB_FATSz32);

  uint8_t size = 30;
  uint8_t *path = (uint8_t*)alloc(size);

  memcpy(path, "/TEST//TEST\0", size);
  
  create_fat32_file((void*)0, 0, path);
}