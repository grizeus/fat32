#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "directory.h"
#include "utility.h"

extern int format_disk(const char* filename);
extern void list_dir(FILE* disk, BootSec_t* boot_sec, uint32_t cluster);
extern void mkdir(FILE* disk, BootSec_t* boot_sec, const char* path, uint32_t current_clus);
extern int change_dir(FILE* disk, BootSec_t* boot_sec, const char* path, uint32_t* current_clus);
extern void touch(FILE* disk, BootSec_t* boot_sec, char* path, uint32_t current_clus);

void read_sector(FILE* disk, uint32_t sector, uint8_t* buffer, uint16_t sector_size) {

  fseek(disk, sector * sector_size, SEEK_SET);
  fread(buffer, 1, sector_size, disk);
}

void write_sector(FILE* disk, uint32_t sector, const uint8_t* buffer, uint16_t sector_size) {

  fseek(disk, sector * sector_size, SEEK_SET);
  fwrite(buffer, 1, sector_size, disk);
}

uint32_t get_next_cluster(FILE* disk, uint32_t cluster, uint16_t sector_size, uint16_t rsrvd_sec) {

  uint32_t fat_sector = rsrvd_sec + (cluster * 4) / sector_size;
  uint32_t offset = (cluster * 4) % sector_size;
  uint8_t* sector_buffer = malloc(sector_size);
  if (sector_buffer == NULL) {

    fprintf(stderr, "Memory allocation failed\n");
    return 1;
  }

  read_sector(disk, fat_sector, sector_buffer, sector_size);
  uint32_t next_clus = *((uint32_t*)(sector_buffer + offset)) & 0x0FFFFFFF;
  free(sector_buffer);
  return next_clus;
}

uint32_t get_free_cluster(FILE* disk, BootSec_t* boot_sec) {

  for (uint32_t cluster = 2; cluster < (boot_sec->BPB_TotSec32 / boot_sec->BPB_SecPerClus);
       cluster++) {

    if (get_next_cluster(disk, cluster, boot_sec->BPB_BytsPerSec, boot_sec->BPB_RsvdSecCnt) == 0) {

      return cluster;
    }
  }
  return 0; // No free clusters
}

void update_fat(FILE* disk, uint32_t cluster, uint32_t value, uint16_t sector_size,
                uint16_t rsrvd_sec) {

  uint32_t fat_sector = rsrvd_sec + (cluster * 4) / sector_size;
  uint32_t offset = (cluster * 4) % sector_size;
  uint8_t* sector_buffer = malloc(sector_size);
  if (!sector_buffer) {

    fprintf(stderr, "Memory allocation failed\n");
    return;
  }

  read_sector(disk, fat_sector, sector_buffer, sector_size);
  *((uint32_t*)(sector_buffer + offset)) = value; // cast new value to buffer
  write_sector(disk, fat_sector, sector_buffer, sector_size);
  free(sector_buffer);
}

void clear_cluster(FILE* disk, uint32_t cluster, BootSec_t* boot_sec) {

  uint16_t sector_size = boot_sec->BPB_BytsPerSec;
  uint8_t* buffer = malloc(sector_size);
  if (!buffer) {

    fprintf(stderr, "Memory allocation failed\n");
    return;
  }
  memset(buffer, 0, sector_size);

  // compute first sector of the data region
  uint32_t fat_size = (boot_sec->BPB_FATSz16 == 0) ? boot_sec->BPB_FATSz32 : boot_sec->BPB_FATSz16;
  uint32_t root_dir_sectors = ((boot_sec->BPB_RootEntCnt * 32) + (sector_size - 1)) / sector_size;
  uint32_t first_data_sector =
      boot_sec->BPB_RsvdSecCnt + (boot_sec->BPB_NumFATs * fat_size) + root_dir_sectors;
  uint32_t first_sector_clus = ((cluster - 2) * boot_sec->BPB_SecPerClus) + first_data_sector;

  for (uint32_t i = 0; i < boot_sec->BPB_SecPerClus; i++) {

    write_sector(disk, first_sector_clus + i, buffer, sector_size);
  }

  free(buffer);
}

static void fill_idle(const char* src, size_t src_size, uint16_t* dst, size_t dst_size) {

  for (size_t i = 0; i < dst_size; i++) {

    if (i > src_size) {

      dst[i] = 0xFFFF;
    } else {

      dst[i] = (uint16_t)src[i];
    }
  }
}

static uint8_t lfn_checksum(const uint8_t* name) {

  int8_t name_len;
  uint8_t sum;

  sum = 0;
  for (name_len = 11; name_len != 0; name_len--) {

    sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *name++;
  }
  return (sum);
}

static void generate_lfn_short_name(const char* lfn, char* short_name, uint8_t* nt_res,
                                    void (*generate_short_name)(const char*, char*, uint8_t*)) {

  int count = 1;
  generate_short_name(lfn, short_name, nt_res);

  if (count > 9) {

    short_name[6] = '~';
    short_name[7] = '1' + count % 10;
  } else {

    short_name[6] = '~';
    short_name[7] = '0' + count;
  }
  count++;
}

int create_lfn_entries(const char* lfn, size_t lfn_len, uint8_t* sector_buffer, char* short_name,
                       uint8_t* nt_res, void (*generate_short_name)(const char*, char*, uint8_t*)) {

  int num_entries = (lfn_len + 12) / 13;

  generate_lfn_short_name(lfn, short_name, nt_res, generate_short_name);
  uint8_t checksum = lfn_checksum((uint8_t*)short_name);

  uint16_t name1[5] = {0};
  uint16_t name2[6] = {0};
  uint16_t name3[2] = {0};

  char src[num_entries * 13];
  memset(src, '\0', num_entries * 13); // populate array with size of filled
                                       // lfn entries with whitespaces
  memcpy(src, lfn, lfn_len);

  for (int i = 0; i < num_entries; i++) {

    uint8_t entry_idx = num_entries - 1 - i;
    LFNStr_t* lfn_entry = (LFNStr_t*)(sector_buffer + entry_idx * sizeof(LFNStr_t));
    memset(lfn_entry, 0, sizeof(LFNStr_t));

    size_t src_size = 13;
    const char* src_ptr = src + 13 * i;

    if (i == num_entries - 1) {

      src_size = lfn_len - 13 * (num_entries - 1);
    }

    fill_idle(src_ptr, src_size, name1, 5);
    fill_idle(src_ptr + 5, src_size - 5, name2, 6);
    fill_idle(src_ptr + 11, src_size - 11, name3, 2);

    memcpy(lfn_entry->LDIR_Name1, name1, sizeof(name1));
    memcpy(lfn_entry->LDIR_Name2, name2, sizeof(name2));
    memcpy(lfn_entry->LDIR_Name3, name3, sizeof(name3));

    lfn_entry->LDIR_Ord = i + 1;
    if (i == num_entries - 1) {

      lfn_entry->LDIR_Ord |= LAST_LONG_ENTRY; // Set the last LFN entry bit
    }
    lfn_entry->LDIR_Attr = ATTR_LFN;
    lfn_entry->LDIR_Chksum = checksum;
  }
  return num_entries;
}

void get_fat_time_date(uint16_t* fat_date, uint16_t* fat_time, uint8_t* fat_time_tenth) {

  time_t now = time(NULL);
  struct tm* t = localtime(&now);

  *fat_date = ((t->tm_year - 80) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday;
  *fat_time = (t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec / 2);
  *fat_time_tenth = (uint8_t)((t->tm_sec % 2) * 100);
}

static void update_cwd(char* cwd, char* path) {

  char temp_cwd[512];
  if (path[0] == '/') {

    strcpy(temp_cwd, "/");
  } else {

    strcpy(temp_cwd, cwd);
  }

  char* token = strtok(path, "/");
  while (token != NULL) {

    if (strncmp(token, "..", 2) == 0) {

      char* last_slash = strrchr(temp_cwd, '/');
      if (last_slash != NULL && last_slash != temp_cwd) {

        *last_slash = '\0';
      } else {

        strcpy(temp_cwd, "/");
      }
    } else if (strncmp(token, ".", 1) != 0) {

      if (temp_cwd[strlen(temp_cwd) - 1] != '/') {

        strcat(temp_cwd, "/");
      }
      strcat(temp_cwd, token);
    }
    token = strtok(NULL, "/");
  }
  strcpy(cwd, temp_cwd);
}

void handle_command(FILE* disk, const char* disk_name, BootSec_t* boot_sec, uint8_t* is_fat32,
                    uint32_t* current_clus, char* cwd, char* command) {

  if (!*is_fat32) {

    if (strncmp(command, "format", 6) == 0) {

      fclose(disk);
      format_disk(disk_name);
      disk = fopen(disk_name, "r+b");
      if (!disk) {

        fprintf(stderr, "Failed to open disk image after formatting: %s\n", disk_name);
        exit(-1);
      }
      read_boot_sector(disk, boot_sec);
      *is_fat32 = 1;
      *current_clus = boot_sec->BPB_RootClus;
    } else {

      fprintf(stderr, "Unknown disk format\n");
    }
  } else if (strncmp(command, "ls", 2) == 0) {

    list_dir(disk, boot_sec, *current_clus);
  } else if (strncmp(command, "cd ", 3) == 0) {

    char* path = command + 3;
    if (change_dir(disk, boot_sec, path, current_clus) == 1) {

      fprintf(stderr, "Failed to change directory: %s\n", path);
    } else {

      update_cwd(cwd, path);
    }
  } else if (strncmp(command, "mkdir ", 6) == 0) {

    const char* path = command + 6;
    EntrSt_t* entries = NULL;
    uint32_t entry_count = 0;
    read_dir_entries(disk, boot_sec, *current_clus, &entries, &entry_count);
    uint8_t is_exist = 0;
    for (size_t i = 0; i < entry_count; ++i) {

      if (strcmp(entries[i].name, path) == 0) {

        fprintf(stderr, "Directory %s already exists\n", path);
        free(entries);
        entries = NULL;
        is_exist = 1;
        break;
      }
    }
    if (!is_exist) {

      mkdir(disk, boot_sec, path, *current_clus);
    }
    if (entries) {

      free(entries);
    }
  } else if (strncmp(command, "touch ", 6) == 0) {

    char* path = command + 6;
    touch(disk, boot_sec, path, *current_clus);
  } else {

    fprintf(stderr, "Unknown command: %s\n", command);
  }
}
