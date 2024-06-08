#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bootsec.h"
#include "directory.h"

static void generate_short_name(const char *dir_name, char *short_name,
                                uint8_t *nt_res);
static uint8_t lfn_checksum(const uint8_t *name);
static void get_fat_time_date(uint16_t *fat_date, uint16_t *fat_time,
                              uint8_t *fat_time_tenth);
static void generate_lfn_short_name(const char *lfn, char *short_name,
                                    uint8_t *nt_res);

static void read_sector(FILE *disk, uint32_t sector, uint8_t *buffer,
                        uint16_t sector_size) {
  fseek(disk, sector * sector_size, SEEK_SET);
  fread(buffer, 1, sector_size, disk);
}

static void write_sector(FILE *disk, uint32_t sector, const uint8_t *buffer,
                         uint16_t sector_size) {
  fseek(disk, sector * sector_size, SEEK_SET);
  fwrite(buffer, 1, sector_size, disk);
}

static uint32_t get_next_cluster(FILE *disk, uint32_t cluster,
                                 uint16_t sector_size, uint16_t rsrvd_sec) {
  uint32_t fat_sector = rsrvd_sec + (cluster * 4) / sector_size;
  uint32_t offset = (cluster * 4) % sector_size;
  uint8_t *sector_buffer = malloc(sector_size);
  if (sector_buffer == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return 1;
  }

  read_sector(disk, fat_sector, sector_buffer, sector_size);
  uint32_t next_clus = *((uint32_t *)(sector_buffer + offset)) & 0x0FFFFFFF;
  free(sector_buffer);
  return next_clus;
}

static uint32_t get_free_cluster(FILE *disk, BootSec_t *boot_sec) {
  for (uint32_t cluster = 2;
       cluster < (boot_sec->BPB_TotSec32 / boot_sec->BPB_SecPerClus);
       cluster++) {
    if (get_next_cluster(disk, cluster, boot_sec->BPB_BytsPerSec,
                         boot_sec->BPB_RsvdSecCnt) == 0) {
      return cluster;
    }
  }
  return 0; // No free clusters
}

static void update_fat(FILE *disk, uint32_t cluster, uint32_t value,
                       uint16_t sector_size, uint16_t rsrvd_sec) {
  uint32_t fat_sector = rsrvd_sec + (cluster * 4) / sector_size;
  uint32_t offset = (cluster * 4) % sector_size;
  uint8_t *sector_buffer = malloc(sector_size);
  if (!sector_buffer) {
    fprintf(stderr, "Memory allocation failed\n");
    return;
  }

  read_sector(disk, fat_sector, sector_buffer, sector_size);
  *((uint32_t *)(sector_buffer + offset)) = value; // cast new value to buffer
  write_sector(disk, fat_sector, sector_buffer, sector_size);
  free(sector_buffer);
}

static void clear_cluster(FILE *disk, uint32_t cluster, BootSec_t *boot_sec) {
  uint16_t sector_size = boot_sec->BPB_BytsPerSec;
  uint8_t *buffer = malloc(sector_size);
  if (!buffer) {
    fprintf(stderr, "Memory allocation failed\n");
    return;
  }
  memset(buffer, 0, sector_size);

  // compute first sector of the data region
  uint32_t fat_size = (boot_sec->BPB_FATSz16 == 0) ? boot_sec->BPB_FATSz32
                                                   : boot_sec->BPB_FATSz16;
  uint32_t root_dir_sectors =
      ((boot_sec->BPB_RootEntCnt * 32) + (sector_size - 1)) / sector_size;
  uint32_t first_data_sector = boot_sec->BPB_RsvdSecCnt +
                               (boot_sec->BPB_NumFATs * fat_size) +
                               root_dir_sectors;
  uint32_t first_sector_clus =
      ((cluster - 2) * boot_sec->BPB_SecPerClus) + first_data_sector;

  for (uint32_t i = 0; i < boot_sec->BPB_SecPerClus; i++) {
    write_sector(disk, first_sector_clus + i, buffer, sector_size);
  }

  free(buffer);
}

static void fill(const char *src, int8_t src_size, uint16_t *dst,
                 size_t dst_size) {
  for (int i = 0; i < dst_size; i++) {

    if (i > src_size) {
      dst[i] = 0xFFFF;
    } else {
      dst[i] = (uint16_t)src[i];
    }
  }
}

static int create_lfn_entries(const char *lfn, size_t lfn_len,
                              uint8_t *sector_buffer, uint16_t sector_size,
                              char *short_name, uint8_t *nt_res) {
  int num_entries = (lfn_len + 12) / 13;

  generate_lfn_short_name(lfn, short_name, nt_res);
  uint8_t checksum = lfn_checksum((uint8_t *)short_name);

  uint16_t name1[5] = {0};
  uint16_t name2[6] = {0};
  uint16_t name3[2] = {0};

  char src[num_entries * 13];
  memset(src, '\0', num_entries * 13); // populate array with size of filled lfn
                                       // entries with whitespaces
  memcpy(src, lfn, lfn_len);

  for (int i = 0; i < num_entries; i++) {
    uint8_t entry_idx = num_entries - 1 - i;
    LFNStr_t *lfn_entry =
        (LFNStr_t *)(sector_buffer + entry_idx * sizeof(LFNStr_t));
    memset(lfn_entry, 0, sizeof(LFNStr_t));

    int8_t src_size = 13;
    const char *src_ptr = src + 13 * i;
    if (i == num_entries - 1) {
      src_size = lfn_len - 13 * (num_entries - 1);
    }
    fill(src_ptr, src_size, name1, 5);
    fill(src_ptr + 5, src_size - 5, name2, 6);
    fill(src_ptr + 11, src_size - 11, name3, 2);

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

static void create_directory_entry(FILE *disk, uint8_t is_dir,
                                   uint32_t parent_cluster,
                                   const char *dir_name, uint32_t new_cluster,
                                   BootSec_t *boot_sec) {
  uint16_t sector_size = boot_sec->BPB_BytsPerSec;
  uint16_t rsrvd_sec = boot_sec->BPB_RsvdSecCnt;
  uint8_t *sector_buffer = malloc(sector_size);
  if (!sector_buffer) {
    fprintf(stderr, "Memory allocation failed\n");
    return;
  }
  memset(sector_buffer, 0, sector_size);
  uint32_t current_clus = parent_cluster;
  uint32_t current_sector;
  DIRStr_t *dir_entry;

  uint16_t fat_date, fat_time;
  uint8_t fat_time_tenth;
  get_fat_time_date(&fat_date, &fat_time, &fat_time_tenth);

  if (is_dir == 1) {
    // Initialize the new directory with "." and ".." entries
    current_sector = boot_sec->BPB_HiddSec + rsrvd_sec +
                     (boot_sec->BPB_NumFATs * ((boot_sec->BPB_FATSz16 == 0)
                                                   ? boot_sec->BPB_FATSz32
                                                   : boot_sec->BPB_FATSz16)) +
                     (new_cluster - 2) * boot_sec->BPB_SecPerClus;

    // Read the first sector of the new directory cluster
    read_sector(disk, current_sector, sector_buffer, sector_size);

    // Create the "." entry
    dir_entry = (DIRStr_t *)(sector_buffer);
    memset(dir_entry, 0, sizeof(DIRStr_t));
    memcpy(dir_entry->DIR_Name, ".          ", 11);
    dir_entry->DIR_Attr = ATTR_DIRECTORY;
    dir_entry->DIR_FstClusLO = (uint16_t)(new_cluster & 0xFFFF);
    dir_entry->DIR_FstClusHI = (uint16_t)((new_cluster >> 16) & 0xFFFF);
    dir_entry->DIR_CrtTimeTenth = fat_time_tenth;
    dir_entry->DIR_CrtTime = fat_time;
    dir_entry->DIR_CrtDate = fat_date;
    dir_entry->DIR_WrtTime = fat_time;
    dir_entry->DIR_WrtDate = fat_date;
    dir_entry->DIR_LstAccDate = fat_date;

    // Create the ".." entry
    dir_entry = (DIRStr_t *)(sector_buffer + sizeof(DIRStr_t));
    memset(dir_entry, 0, sizeof(DIRStr_t));
    memcpy(dir_entry->DIR_Name, "..         ", 11);
    dir_entry->DIR_Attr = ATTR_DIRECTORY;
    dir_entry->DIR_FstClusLO = (uint16_t)(parent_cluster & 0xFFFF);
    dir_entry->DIR_FstClusHI = (uint16_t)((parent_cluster >> 16) & 0xFFFF);
    dir_entry->DIR_CrtTimeTenth = fat_time_tenth;
    dir_entry->DIR_CrtTime = fat_time;
    dir_entry->DIR_CrtDate = fat_date;
    dir_entry->DIR_WrtTime = fat_time;
    dir_entry->DIR_WrtDate = fat_date;
    dir_entry->DIR_LstAccDate = fat_date;

    // Write the sector back to disk
    write_sector(disk, current_sector, sector_buffer, sector_size);
  }
  while (1) {

    uint32_t fat_size = (boot_sec->BPB_FATSz16 == 0) ? boot_sec->BPB_FATSz32
                                                     : boot_sec->BPB_FATSz16;
    current_sector = boot_sec->BPB_HiddSec + rsrvd_sec +
                     (boot_sec->BPB_NumFATs * fat_size) +
                     (current_clus - 2) * boot_sec->BPB_SecPerClus;

    for (uint32_t i = 0; i < boot_sec->BPB_SecPerClus; i++) {

      read_sector(disk, current_sector + i, sector_buffer, sector_size);

      for (uint32_t j = 0; j < sector_size; j += sizeof(DIRStr_t)) {

        dir_entry = (DIRStr_t *)(sector_buffer + j);

        char sln_dir_name[11];
        memset(sln_dir_name, 0x20, 11);
        size_t dir_len = strlen(dir_name);
        for (size_t i = 0; i < dir_len; ++i) {
          sln_dir_name[i] = toupper(dir_name[i]);
        }

        // if file exist - update access time
        if (strncmp((char *)dir_entry->DIR_Name, sln_dir_name, 11) == 0) {
          dir_entry->DIR_LstAccDate = fat_date;

          write_sector(disk, current_sector + i, sector_buffer, sector_size);
          free(sector_buffer);
          return;
        }
        if (dir_entry->DIR_Name[0] == 0x00 || dir_entry->DIR_Name[0] == 0xE5) {
          // Create LFN entries
          int lfn_entries = 0;
          size_t dir_len = strlen(dir_name);
          uint8_t nt_res = 0;
          char short_name[11] = {" "};

          if (dir_len > 8) {
            lfn_entries =
                create_lfn_entries(dir_name, dir_len, sector_buffer + j,
                                   sector_size, short_name, &nt_res);
            j += lfn_entries * sizeof(LFNStr_t);
          }

          // Create the 8.3 entry
          dir_entry = (DIRStr_t *)(sector_buffer + j);
          memset(dir_entry, 0, sizeof(DIRStr_t));
          if (!lfn_entries) {
            generate_short_name(dir_name, short_name, &nt_res);
          }

          memcpy(dir_entry->DIR_Name, short_name, 11);
          dir_entry->DIR_NTRes = nt_res;
          dir_entry->DIR_Attr = (is_dir == 1) ? ATTR_DIRECTORY : ATTR_ARCHIVE;
          if (is_dir) {
            dir_entry->DIR_FstClusLO = (uint16_t)(new_cluster & 0xFFFF);
            dir_entry->DIR_FstClusHI = (uint16_t)((new_cluster >> 16) & 0xFFFF);
          }
          // Set creation time and date
          dir_entry->DIR_CrtTimeTenth = fat_time_tenth;
          dir_entry->DIR_CrtTime = fat_time;
          dir_entry->DIR_CrtDate = fat_date;
          dir_entry->DIR_WrtTime = fat_time;
          dir_entry->DIR_WrtDate = fat_date;
          dir_entry->DIR_LstAccDate = fat_date;

          write_sector(disk, current_sector + i, sector_buffer, sector_size);
          free(sector_buffer);
          return;
        }
      }
    }
    uint32_t next_cluster =
        get_next_cluster(disk, current_clus, sector_size, rsrvd_sec);
    if (next_cluster >= EOC) {
      fprintf(stderr, "No free directory entry found\n");
      free(sector_buffer);
      return;
    }
    current_clus = next_cluster;
  }
}

void update_file_entry() {}
static void generate_short_name(const char *dir_name, char *short_name,
                                uint8_t *nt_res) {

  memset(short_name, 0x20, 11); // 0x20 for whitespace
  *nt_res = 0;

  int i = 0, j = 0;
  while (i < 8 && dir_name[j]) {
    if (islower(dir_name[j])) {
      *nt_res |= NT_RES_LOWER_CASE_BASE;
    }
    short_name[i++] = toupper(dir_name[j++]);
  }
}

static void generate_lfn_short_name(const char *lfn, char *short_name,
                                    uint8_t *nt_res) {

  static int count = 1;
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

static uint8_t lfn_checksum(const uint8_t *name) {
  int8_t name_len;
  uint8_t sum;

  sum = 0;
  for (name_len = 11; name_len != 0; name_len--) {
    sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *name++;
  }
  return (sum);
}

static void get_fat_time_date(uint16_t *fat_date, uint16_t *fat_time,
                              uint8_t *fat_time_tenth) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  *fat_date = ((t->tm_year - 80) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday;
  *fat_time = (t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec / 2);
  *fat_time_tenth = (uint8_t)((t->tm_sec % 2) * 100);
}

void mkentry(FILE *disk, BootSec_t *boot_sec, char *path, uint8_t is_dir,
             uint32_t current_clus) {

  uint32_t parent_cluster = current_clus;
  char *dir_name = path;
  size_t size = strlen(path);
  dir_name[size] = '\0';

  uint32_t new_cluster = get_free_cluster(disk, boot_sec);
  if (new_cluster == 0) {
    fprintf(stderr, "No free clusters available\n");
    exit(EXIT_FAILURE);
  }

  update_fat(disk, new_cluster, EOC, boot_sec->BPB_BytsPerSec,
             boot_sec->BPB_RsvdSecCnt);
  clear_cluster(disk, new_cluster, boot_sec);
  create_directory_entry(disk, is_dir, parent_cluster, dir_name, new_cluster,
                         boot_sec);
}
