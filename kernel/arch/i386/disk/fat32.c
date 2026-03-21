#include <kernel/disk.h>
#include <kernel/fat32.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdbool.h>

FAT32_BootSector_s* fat_boot;
FAT32_FSInfo_s* fs_info;

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

uint32_t get_next_free_cluster() {
  uint8_t buffer[512];
  uint32_t start = fs_info->FSI_Nxt_Free;
  uint32_t fat = fs_info->FSI_Nxt_Free + 1;
  uint32_t end = max_fats_entry;

  do {
    read_ata_st_c(get_fat_sector(fat), buffer, 1, &drv);
    uint32_t offset = get_fat_offset(fat);

    if (*(uint32_t*)&buffer[offset] == FREE_FAT) {
      return fat;
    }
    if (fat >= max_fats_entry) {
      fat = FIRST_VALID_FAT;
      end = start;
      continue;
    }
    fat++;
  } while (fat != end);

  return 0;
}

bool update_next_free_cluster() {
  uint32_t next = get_next_free_cluster();
  if (next == 0) {
    return false;
  }
  fs_info->FSI_Nxt_Free = next;
  return true;
}

void update_cluster(uint32_t cluster, uint32_t offset, void* fat_entry, uint32_t size, uint32_t is_entry) {
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
  if (!is_entry) {
    update_next_free_cluster();
  }
  free(buf);
}

Entry_Addr* get_free_entry_addr(uint32_t cluster) {
  FAT32_DirEntry* entry;
  uint8_t* cluster_buff = alloc(fat_boot->BPB_BytsPerSec * fat_boot->BPB_SecPerClus);
  read_ata_st_c(get_cluster_sector(cluster), cluster_buff, fat_boot->BPB_SecPerClus, &drv);
  for (int i = 0; i < MAX_ENTRYS - 1; i++) {
    entry = (FAT32_DirEntry*)&cluster_buff[i*32];
    if (entry->DIR_name[0] == FREE_ENTRY) {
      Entry_Addr* entry_addr = (Entry_Addr*)alloc(sizeof(Entry_Addr));
      entry_addr->Cluster = cluster;
      entry_addr->Offset = i;
      free(cluster_buff);
      return entry_addr;
    }
  }
  free(cluster_buff);
  uint32_t new_cluster = fs_info->FSI_Nxt_Free;
  Entry_Addr* entry_addr = (Entry_Addr*)alloc(sizeof(Entry_Addr));
  entry_addr->Cluster = new_cluster;
  entry_addr->Offset = 0;
  return entry_addr;
}

uint32_t check_entry_exists(uint32_t cluster, uint8_t entry_name[MAX_ENTRY_NAME]) {
  uint8_t* cluster_buff = alloc(fat_boot->BPB_BytsPerSec * fat_boot->BPB_SecPerClus);
  while(1) {
    read_ata_st_c(get_cluster_sector(cluster), cluster_buff, fat_boot->BPB_SecPerClus, &drv);
    FAT32_DirEntry* entry;
    for (int i = 0; i < MAX_ENTRYS; i++) {
      entry = (FAT32_DirEntry*)&cluster_buff[i*32];
      int cmp = memcmp(entry->DIR_name, entry_name, MAX_ENTRY_NAME);
      if (cmp == 0) {
        free(cluster_buff);
        return entry->DIR_FstClusHI | entry->DIR_FstClustLO;
      }
    }
    uint32_t fat_buff[128];
    uint32_t sector = get_fat_sector(cluster);
    uint32_t offset = get_fat_offset(cluster);
    read_ata_st_c(sector, fat_buff, 1, &drv);
    int val = fat_buff[offset];
    if (val >= EOF) {
      break;
    }
    cluster = val;
  }
  free(cluster_buff);
  return 0;
}

void format_name(uint8_t name[MAX_ENTRY_NAME], uint32_t name_len, bool is_dir) {
  if (!is_dir && name_len < MAX_ENTRY_NAME) {
    for (uint8_t i = 0; i < 11; i++) {
      if (is_dir && i >= name_len - 4) {
        if (name[i] == '.') {
          name[10] = name[name_len - 1];
          name[9] = name[name_len - 2];
          name[8] = name[name_len - 3];
          name[i] = ' ';
        } else {
          name[i] = ' ';
          if (name_len - 1 - i == 0) {
            break;
          }
        }
      } else if (i >= name_len) {
        name[i] = ' ';
      }
    }
  }
}

uint32_t check_path(uint8_t* path, bool is_dir, bool is_insert, uint8_t last_entry[MAX_ENTRY_NAME]) {
  uint8_t* ptr = path;
  uint8_t c = *ptr;
  uint8_t current_path[MAX_ENTRY_NAME] = "";
  uint8_t path_len = 0;
  uint8_t current_ext_len = 0;
  uint16_t paths = 1;
  uint32_t cluster = ROOT_DIR_CLUSTER;

  if (path[0] != '/' || (path[0] == '/' && path[1] == '/')) {
    printf("invalid path: %s", path);
    return 0;
  }

  uint8_t state = STATE_ENTRY_CHAR;

  while (1) {
    if (state == STATE_EXT_CHAR && current_ext_len >= 4 && c != '\0') {
      printf("the ext must be 3 characters\n");
      return 0;
    }

    if (c == '/' && path_len == 0 && paths > 1) {
      printf("invalid path: %s", current_path);
      return 0;
    }

    if ((c == '/' && path_len > 0) || c == '\0') {
      ptr--;
      c = *ptr;
      ptr++;
      if (c == '.') {
        printf("the path cannot end with '.'\n");
        return 0;
      }
      if (c == ' '){
        printf("the path cannot end with ' '\n");
        return 0;
      }
      c = *ptr;
      if ((c == '/' ) || (c == '\0' && !is_insert)) {
        current_path[path_len] = '\0';
        format_name(current_path, path_len, true);
        cluster = check_entry_exists(cluster, current_path);
        if (cluster == 0) {
          printf("dir not found: %s\n", current_path);
          return 0;
        }
      }
      if (c == '\0') {
        if (state == STATE_EXT_CHAR && current_ext_len != 3) {
          printf("the ext must be 3 characters\n");
          return 0;
        }
        break;
      }
      memset(current_path, 0, MAX_ENTRY_NAME);
      path_len = 0;
      paths++;
    }
    
    switch (state) {
      case STATE_EXT_CHAR:
      if (c == '.') {
        printf("the name cannot have two points\n");
        return 0;
      }
      current_ext_len++;
      if (current_ext_len > 4) {
        printf("the ext cannot have a len greater than 3\n");
        return 0;
      }
      
      case STATE_ENTRY_CHAR:
      if (c != '/') {
        if (!is_valid_fat_char(c, is_dir)) {
          printf("invalid char: %c", c);
          return 0;
        }
        if (path_len == 0 && c == '.'){
          printf("the name cannot start with '.'\n");
          return 0;
        }
        if (path_len == 0 && (c == 0x00 || c == 0xE5)) {
          printf("the name cannot start with 0x00 or OxE5\n");
          return 0;
        }

        current_path[path_len] = c; 
        path_len++;
        
        if (path_len > MAX_ENTRY_NAME && c != '\0') {
          printf("too large path: %s", path);
          return 0;
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

  format_name(current_path, path_len, is_dir);

  memcpy(last_entry, current_path, MAX_ENTRY_NAME);

  return cluster;
}

bool create_fat32_directory(uint8_t* path) {
uint8_t new_entry[MAX_ENTRY_NAME];
  uint32_t dir_cluster;
  dir_cluster = check_path(path, true, true, new_entry);
  if (dir_cluster == 0) {
    return false;
  }
  
  uint32_t cluster = get_next_free_cluster();

  uint8_t attr = ATTR_DIRECTORY;
  FAT32_DirEntry fat_entry = create_fat32_entry(cluster, new_entry, 0, attr);
  
  Entry_Addr* entry_addr = get_free_entry_addr(dir_cluster);
  if (entry_addr->Offset == 0 && entry_addr->Cluster != ROOT_DIR_CLUSTER) {
    update_fat_entry(dir_cluster, entry_addr->Cluster);
    update_fat_entry(entry_addr->Cluster, EOF);
  }
  update_cluster(entry_addr->Cluster, get_entry_offset_in_cluster(entry_addr->Offset), &fat_entry, sizeof(FAT32_DirEntry), true);
  update_next_free_cluster();
}

bool create_fat32_file(uint8_t *buffer, uint8_t* path, uint32_t size) {  
  if (size > MAX_FILE_SIZE) {
    printf("too large size, max: %d", MAX_FILE_SIZE);
    return false;
  }

  uint8_t new_entry[MAX_ENTRY_NAME];
  uint32_t dir_cluster;
  dir_cluster = check_path(path, false, true, new_entry);
  if (dir_cluster == 0) {
    return false;
  }

  uint32_t cluster = get_next_free_cluster();

  uint8_t attr = ATTR_ARCHIVE;
  FAT32_DirEntry fat_entry = create_fat32_entry(cluster, new_entry, size, attr);
  
  Entry_Addr* entry_addr = get_free_entry_addr(dir_cluster);
  if (entry_addr->Offset == 0 && entry_addr->Cluster != ROOT_DIR_CLUSTER) {
    update_fat_entry(dir_cluster, entry_addr->Cluster);
    update_fat_entry(entry_addr->Cluster, EOF);
  }
  update_cluster(entry_addr->Cluster, get_entry_offset_in_cluster(entry_addr->Offset), &fat_entry, sizeof(FAT32_DirEntry), true);
  
  uint32_t entry_val;
  uint32_t clusters = (size + clusterSizeBytes - 1) / clusterSizeBytes;
  uint8_t* write = buffer;
  
  for (uint32_t i = 0; i < clusters;  i++) {
    if (i != clusters - 1) {
      update_cluster(cluster, 0, write, clusterSizeBytes, false);
      write+=fat_boot->BPB_BytsPerSec*fat_boot->BPB_SecPerClus;
      entry_val = fs_info->FSI_Nxt_Free;
    } else {
      update_cluster(cluster, 0, write, size - (write - buffer), false);
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



    
    // ==============================================
  // Test section - Verifying FAT32 file and directory creation
  // ==============================================

  // Test 1: Create file in root - /TEST.TXT
  uint8_t file1_path[30];
  memcpy(file1_path, "/TEST.TXT\0", 30);

  uint8_t file1_content[7];
  memcpy(file1_content, "Test1\0", 7);

  create_fat32_file(file1_content, file1_path, 6);

  // Read root directory (cluster 2)
  uint8_t root_buffer[512];
  read_ata_st_c(get_cluster_sector(2), root_buffer, 1, &drv);

  FAT32_DirEntry* entry_file1 = (FAT32_DirEntry*)root_buffer;
  printf("File 1 name: %s\n", entry_file1->DIR_name);

  uint32_t file1_cluster = (entry_file1->DIR_FstClusHI << 16) | entry_file1->DIR_FstClustLO;

  uint8_t content_buffer[512];
  read_ata_st_c(get_cluster_sector(file1_cluster), content_buffer, 1, &drv);
  printf("File 1 content: %s\n", content_buffer);

  uint32_t fat_buffer[128];
  read_ata_st_c(get_fat_sector(file1_cluster), fat_buffer, 1, &drv);
  if ((int)fat_buffer[get_fat_offset(file1_cluster) / 4] >= 0x0FFFFFF8) {
      printf("FAT file 1: end\n");
  }

  // Test 2: Create second file in root - /TESTT.TXT
  uint8_t file2_path[30];
  memcpy(file2_path, "/TESTT.TXT\0", 30);

  uint8_t file2_content[8];
  memcpy(file2_content, "Test2\0", 8);

  create_fat32_file(file2_content, file2_path, 6);

  read_ata_st_c(get_cluster_sector(2), root_buffer, 1, &drv);

  FAT32_DirEntry* entry_file2 = (FAT32_DirEntry*)&root_buffer[sizeof(FAT32_DirEntry)];
  printf("File 2 name: %s\n", entry_file2->DIR_name);

  uint32_t file2_cluster = (entry_file2->DIR_FstClusHI << 16) | entry_file2->DIR_FstClustLO;

  read_ata_st_c(get_cluster_sector(file2_cluster), content_buffer, 1, &drv);
  printf("File 2 content: %s\n", content_buffer);

  read_ata_st_c(get_fat_sector(file2_cluster), fat_buffer, 1, &drv);
  if ((int)fat_buffer[get_fat_offset(file2_cluster) / 4] >= 0x0FFFFFF8) {
      printf("FAT file 2: end\n");
  }

  // Test 3: Create directories
  create_fat32_directory("/TEST\0");
  create_fat32_directory("/TEST/TESTL\0");

  // Test 4: Create file inside subdirectory - /TEST/TESTL/TESTT.TXT
  uint8_t file3_path[30];
  memcpy(file3_path, "/TEST/TESTL/TESTT.TXT\0", 30);

  uint8_t file3_content[8];
  memcpy(file3_content, "Test3\0", 8);

  create_fat32_file(file3_content, file3_path, 6);

  // Read root again
  read_ata_st_c(get_cluster_sector(2), root_buffer, 1, &drv);

  // Get TEST directory entry (third entry in root)
  FAT32_DirEntry* entry_test = (FAT32_DirEntry*)&root_buffer[sizeof(FAT32_DirEntry) * 2];
  uint32_t test_cluster = (entry_test->DIR_FstClusHI << 16) | entry_test->DIR_FstClustLO;

  // Read TEST directory cluster
  uint8_t test_dir_buffer[512];
  read_ata_st_c(get_cluster_sector(test_cluster), test_dir_buffer, 1, &drv);

  // Get TESTL entry (first entry in TEST directory)
  FAT32_DirEntry* entry_testl = (FAT32_DirEntry*)test_dir_buffer;
  uint32_t testl_cluster = (entry_testl->DIR_FstClusHI << 16) | entry_testl->DIR_FstClustLO;

  // Read TESTL directory cluster
  uint8_t testl_buffer[512];
  read_ata_st_c(get_cluster_sector(testl_cluster), testl_buffer, 1, &drv);

  // Get file entry (first entry in TESTL directory)
  FAT32_DirEntry* entry_file3 = (FAT32_DirEntry*)testl_buffer;
  printf("File 3 name: %s\n", entry_file3->DIR_name);

  uint32_t file3_cluster = (entry_file3->DIR_FstClusHI << 16) | entry_file3->DIR_FstClustLO;

  read_ata_st_c(get_cluster_sector(file3_cluster), content_buffer, 1, &drv);
  printf("File 3 content: %s\n", content_buffer);

  read_ata_st_c(get_fat_sector(file3_cluster), fat_buffer, 1, &drv);
  if ((int)fat_buffer[get_fat_offset(file3_cluster) / 4] >= 0x0FFFFFF8) {
      printf("FAT file 3: end\n");
  }

  printf("All tests completed.\n");
}