#include <kernel/disk.h>
#include <kernel/fat32.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdbool.h>

FAT32_BootSector_s* fat_boot;
FAT32_FSInfo_s* fs_info;
Cluster_entry* cluster_entry;

uint16_t clusterSizeBytes;
uint32_t max_fats_entry;

ata_driver_data drv;

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

FAT32_DirEntry create_fat32_entry(uint32_t data_cluster, uint8_t name[MAX_ENTRY_NAME], uint32_t size, uint8_t attr) {
  FAT32_DirEntry fatDirEntry;
  
  memcpy(fatDirEntry.DIR_name, name, MAX_ENTRY_NAME);
  fatDirEntry.DIR_Attr = attr;
  fatDirEntry.DIR_NTRES = 0;
  fatDirEntry.DIR_CrtTimeTenth = 0;
  fatDirEntry.DIR_CrtTime = 0;
  fatDirEntry.DIR_CrtDate = 0;
  fatDirEntry.DIR_LstAccDate = 0;
  fatDirEntry.DIR_FstClusHI = (data_cluster >> 16) & 0xFFFF;
  fatDirEntry.DIR_WrtTime = 0;
  fatDirEntry.DIR_WrtDate = 0;
  fatDirEntry.DIR_FstClustLO = data_cluster & 0x0000FFFF;
  fatDirEntry.DIR_FileSize = size;

  return fatDirEntry;
}

bool is_valid_fat_char(char c, bool dir) {
  if (c >= 'A' && c <= 'Z') return true;
  if (c >= '0' && c <= '9') return true;

  switch (c) {
    case '$': case '%': case '\'': case '-': case '_':
    case '@': case '~': case '`': case '!': case '(':
    case ')': case '{': case '}': case '^': case '#':
    case '&':
    return true;
  }

  if (!dir && c == '.') {
    return true;
  }
  
  return false;
}

uint32_t get_entry_offset_in_cluster(uint32_t entry) {
  uint32_t offset = entry * 32;
  return offset % (fat_boot->BPB_SecPerClus * fat_boot->BPB_BytsPerSec);
}

uint32_t get_cluster_sector(uint32_t cluster) {
  uint32_t first_data_sector = fat_boot->BPB_RsvdSecCnt + (fat_boot->BPB_NumFATs * fat_boot->BPB_FATSz32);
  uint32_t sector = ((cluster - 2) * fat_boot->BPB_SecPerClus + first_data_sector);
  return sector;
}

uint32_t get_fat_sector(uint32_t entry) {
  uint32_t fat_offset = entry * 4;
  return fat_boot->BPB_RsvdSecCnt + (fat_offset / fat_boot->BPB_BytsPerSec);
}

uint32_t get_fat_offset(uint32_t entry) {
  uint32_t fat_offset = entry * 4;
  return fat_offset % fat_boot->BPB_BytsPerSec;
}

void update_fat_entry(uint32_t entry, uint32_t val) {
  uint32_t buffer[128];
  memset(buffer, 0, 512);
  uint32_t sector = get_fat_sector(entry);
  read_ata_st_c(sector, buffer, 1, &drv);
  uint32_t offset = get_fat_offset(entry) / 4;
  buffer[offset] = val;
  write_ata_st_c(sector, buffer, 1, &drv);
}

bool update_next_free_cluster() {
  uint8_t buffer[512];
  uint32_t start = fs_info->FSI_Nxt_Free;
  uint32_t fat = fs_info->FSI_Nxt_Free + 1;
  uint32_t end = max_fats_entry;

  do {
    read_ata_st_c(get_fat_sector(fat), buffer, 1, &drv);
    uint32_t offset = get_fat_offset(fat);

    if (*(uint32_t*)&buffer[offset] == FREE_FAT) {
      fs_info->FSI_Nxt_Free = fat;
      return true;
    }
    if (fat >= max_fats_entry) {
      fat = FIRST_VALID_FAT;
      end = start;
      continue;
    }
    fat++;
  } while (fat != end);

  return false;
}

void update_cluster(uint32_t cluster, uint32_t offset, void* fat_entry, uint32_t size) {
  uint32_t sectors = (size + fat_boot->BPB_BytsPerSec - 1) / fat_boot->BPB_BytsPerSec;
  if (sectors > fat_boot->BPB_SecPerClus || offset > MAX_ENTRYS - 1) {
    return;
  }
  uint32_t bytes = fat_boot->BPB_BytsPerSec * fat_boot->BPB_SecPerClus;
  uint8_t *buf = (uint8_t*)alloc(bytes);
  memset(buf, 0, bytes);
  read_ata_st_c(get_cluster_sector(cluster), buf, fat_boot->BPB_SecPerClus, &drv);
  memcpy(&buf[offset], fat_entry, size);
  write_ata_st_c(get_cluster_sector(cluster), buf, fat_boot->BPB_SecPerClus, &drv);
  update_next_free_cluster();
  free(buf);
}

bool validate_path(uint8_t* path, bool dir, uint8_t new_entry[MAX_ENTRY_NAME]) {
  uint8_t* ptr = path;
  uint8_t c = *ptr;
  uint8_t current_path[MAX_ENTRY_NAME] = "";
  uint8_t path_len = 0;
  uint8_t current_ext_len = 0;
  uint16_t paths = 1;

  if (path[0] != '/' || (path[0] == '/' && path[1] == '/')) {
    printf("invalid path: %s", path);
    return (FAT32_DirEntry*)0;
  }

  uint8_t state = STATE_ENTRY_CHAR;

  while (1) {
    if (state == STATE_EXT_CHAR && current_ext_len >= 4 && c != '\0') {
      printf("the ext must be 3 characters\n");
      return false;
    }

    if (c == '/' && path_len == 0 && paths > 1) {
      printf("invalid path: %s", current_path);
      return false;
    }

    if ((c == '/' && path_len > 0) || c == '\0') {
      ptr--;
      c = *ptr;
      ptr++;
      if (c == '.') {
        printf("the path cannot end with '.'\n");
        return false;
      }
      if (c == ' '){
        printf("the path cannot end with ' '\n");
        return false;
      }
      c = *ptr;
      if (c == '\0') {
        if (state == STATE_EXT_CHAR && current_ext_len != 3) {
          printf("the ext must be 3 characters\n");
          return false;
        }
        current_path[path_len] = '\0';
        break;
      }
      current_path[path_len] = '\0';
      memset(current_path, 0, MAX_ENTRY_NAME);
      path_len = 0;
      paths++;
    }
    
    switch (state) {
      case STATE_EXT_CHAR:
      if (c == '.') {
        printf("the name cannot have two points\n");
        return false;
      }
      current_ext_len++;
      if (current_ext_len > 4) {
        printf("the ext cannot have a len greater than 3\n");
        return false;
      }
      
      case STATE_ENTRY_CHAR:
      if (c != '/') {
        if (!is_valid_fat_char(c, dir)) {
          printf("invalid char: %c", c);
          return false;
        }
        if (path_len == 0 && c == '.'){
          printf("the name cannot start with '.'\n");
          return false;
        }
        if (path_len == 0 && (c == 0x00 || c == 0xE5)) {
          printf("the name cannot start with 0x00 or OxE5\n");
          return false;
        }
        current_path[path_len] = c; 
        path_len++;
        
        if (path_len > MAX_ENTRY_NAME && c != '\0') {
          printf("too large path: %s", path);
          return false;
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

  if (!dir && path_len < MAX_ENTRY_NAME) {
    for (uint8_t i = 0; i < 11; i++) {
      if (state == STATE_EXT_CHAR && i >= path_len - 4) {
        if (current_path[i] == '.') {
          current_path[10] = current_path[path_len - 1];
          current_path[9] = current_path[path_len - 2];
          current_path[8] = current_path[path_len - 3];
          current_path[i] = ' ';
        } else {
          current_path[i] = ' ';
          if (path_len - 1 - i == 0) {
            break;
          }
        }
      } else if (i >= path_len) {
        current_path[i] = ' ';
      }
    }
  }

  memcpy(new_entry, current_path, MAX_ENTRY_NAME);

  return true;
}

bool create_fat32_dir(uint8_t* path) {
  uint8_t new_entry[MAX_ENTRY_NAME];
  if (!validate_path(path, true, new_entry)) {
    return false;
  }
 
  uint8_t attr = ATTR_DIRECTORY;
  
  uint32_t fat_entry_cluster = fs_info->FSI_Nxt_Free;

  update_next_free_cluster();
  
  uint32_t cluster = fs_info->FSI_Nxt_Free;
  
  FAT32_DirEntry fat_entry = create_fat32_entry(cluster_entry->cluster, cluster_entry->offset, 0, attr);

  update_cluster(cluster_entry->cluster, cluster_entry->offset, &fat_entry, sizeof(FAT32_DirEntry));
  cluster_entry->offset++;

  uint32_t entry_val = EOF;
  update_fat_entry(fat_entry_cluster, entry_val);

  return true;
}

bool create_fat32_file(uint8_t *buffer, uint32_t size, uint8_t* path) {
  uint8_t new_entry[MAX_ENTRY_NAME];
  if (!validate_path(path, false, new_entry)) {
    return false;
  }
 
  if (size > MAX_FILE_SIZE) {
    printf("too large size, max: %d", MAX_FILE_SIZE);
    return false;
  }
  
  uint8_t attr = ATTR_ARCHIVE;
  
  uint32_t cluster = fs_info->FSI_Nxt_Free;
  
  FAT32_DirEntry fat_entry = create_fat32_entry(cluster, new_entry, size, attr);

  update_cluster(cluster_entry->cluster, get_entry_offset_in_cluster(cluster_entry->offset), &fat_entry, sizeof(FAT32_DirEntry));
  cluster_entry->offset++;

  uint32_t entry_val = EOF;
  update_fat_entry(cluster_entry->cluster, entry_val);
  
  uint32_t clusters = (size + clusterSizeBytes - 1) / clusterSizeBytes;
  uint8_t* write = buffer;

  for (uint32_t i = 0; i < clusters;  i++) {
    if (i != clusters - 1) {
      update_cluster(cluster, 0, write, clusterSizeBytes);
      write+=fat_boot->BPB_BytsPerSec*fat_boot->BPB_SecPerClus;
      entry_val = fs_info->FSI_Nxt_Free;
    } else {
      update_cluster(cluster, 0, write, size - (write - buffer));
      entry_val = EOF;
    }
    update_fat_entry(cluster, entry_val);
    cluster = fs_info->FSI_Nxt_Free;
  }

  return true;
}

void create_fat32() {
	static const ata_driver_data configs[1] = {
    {0x1F0, 0x3F6, FAT_PARTITION_START, DISK_SIZE, 0xE0},
  };
  drv = configs[0];
  create_fat_boot_s();

  max_fats_entry = (fat_boot->BPB_BytsPerSec * fat_boot->BPB_FATSz32) / 4;

  clusterSizeBytes = fat_boot->BPB_BytsPerSec * fat_boot->BPB_SecPerClus;
 
  write_ata_st_c(0, fat_boot, 1, &drv);

  create_fat32_fs_info_s();

  cluster_entry = alloc(sizeof(Cluster_entry));
  cluster_entry->cluster = fs_info->FSI_Nxt_Free;
  cluster_entry->offset = 0;

  update_next_free_cluster();

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

  uint8_t size = 30;
  uint8_t *entry = (uint8_t*)alloc(size);

  memcpy(entry, "/TEST/TEST.TXT\0", size);

  uint8_t buf[7];

  memcpy(buf, "peidei\0", 7);
  
  create_fat32_file(buf, 7, entry);

  uint8_t test[512];
  read_ata_st_c(get_cluster_sector(2), test, 1, &drv);

  FAT32_DirEntry* pao = (FAT32_DirEntry*)test;
  printf("%s\n", pao->DIR_name);

  uint8_t test2[512];
  read_ata_st_c(get_cluster_sector(pao->DIR_FstClusHI | pao->DIR_FstClustLO), test2, 1, &drv);
  printf("%s\n", test2);

  uint32_t test3[128];
  read_ata_st_c(get_fat_sector(3), test3, 1, &drv);
  if (test3[get_fat_offset(3) / 4] == EOF) {
    printf("end\n");
  }

  

  uint8_t *entryy = (uint8_t*)alloc(size);
  memcpy(entryy, "/TEST/TESTT.TXT\0", size);
  uint8_t buff[7];

  memcpy(buff, "peidei2\0", 7);
  
  create_fat32_file(buff, 7, entry);
  uint8_t testt[512];
  read_ata_st_c(get_cluster_sector(2), testt, 1, &drv);

  FAT32_DirEntry* paoo = (FAT32_DirEntry*)&testt[sizeof(FAT32_DirEntry)];
  printf("%s\n", paoo->DIR_name);

  uint8_t testt2[512];
  read_ata_st_c(get_cluster_sector(paoo->DIR_FstClusHI | paoo->DIR_FstClustLO), testt2, 1, &drv);
  printf("%s\n", testt2);
  
  uint32_t testt3[128];
  read_ata_st_c(get_fat_sector(3), testt3, 1, &drv);
  if (testt3[get_fat_offset(3) / 4] == EOF) {
    printf("end\n");
  }

}