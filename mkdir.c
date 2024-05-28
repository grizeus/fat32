#include "bootsec.h"
#include "directory.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int create_lfn_entries(const char *lfn, uint8_t *sector_buffer,
                              uint16_t sector_size);
static void generate_short_name(const char *dir_name, char *short_name,
                                uint8_t *nt_res);
static uint8_t lfn_checksum(const uint8_t *name);
static void get_fat_time_date(uint16_t *fat_date, uint16_t *fat_time,
                              uint8_t *fat_time_tenth);
static void generate_lfn_short_name(const char *lfn, char *short_name);

static void read_boot_sector(FILE *disk, BootSec_t *boot_sec) {
  fseek(disk, 0, SEEK_SET);
  fread(boot_sec, sizeof(BootSec_t), 1, disk);
}

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
      printf("Found free cluster: %u\n", cluster);
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
  uint32_t first_sector = boot_sec->BPB_HiddSec + boot_sec->BPB_RsvdSecCnt +
                          (boot_sec->BPB_NumFATs * boot_sec->BPB_FATSz32) +
                          (cluster - 2) * boot_sec->BPB_SecPerClus;
  for (uint32_t i = 0; i < boot_sec->BPB_SecPerClus; i++) {
    write_sector(disk, first_sector + i, buffer, sector_size);
  }
  free(buffer);
}

static void create_directory_entry(FILE *disk, uint32_t parent_cluster,
                                   const char *dirname, uint32_t new_cluster,
                                   BootSec_t *boot_sec) {
  uint16_t sector_size = boot_sec->BPB_BytsPerSec;
  uint16_t rsrvd_sec = boot_sec->BPB_RsvdSecCnt;
  uint8_t *sector_buffer = malloc(sector_size);
  if (!sector_buffer) {
    fprintf(stderr, "Memory allocation failed\n");
    return;
  }
  memset(sector_buffer, 0, sector_size);
  uint32_t current_cluster = parent_cluster;
  uint32_t current_sector;
  DIRStr_t *dir_entry;

  uint16_t fat_date, fat_time;
  uint8_t fat_time_tenth;
  get_fat_time_date(&fat_date, &fat_time, &fat_time_tenth);

  while (1) {
    uint32_t fat_size = (boot_sec->BPB_FATSz16 == 0) ? boot_sec->BPB_FATSz32
                                                     : boot_sec->BPB_FATSz16;
    current_sector = boot_sec->BPB_HiddSec + rsrvd_sec +
                     (boot_sec->BPB_NumFATs * fat_size) +
                     (current_cluster - 2) * boot_sec->BPB_SecPerClus;
    for (uint32_t i = 0; i < boot_sec->BPB_SecPerClus; i++) {
      read_sector(disk, current_sector + i, sector_buffer, sector_size);
      for (uint32_t j = 0; j < sector_size; j += sizeof(DIRStr_t)) {
        dir_entry = (DIRStr_t *)(sector_buffer + j);
        if (dir_entry->DIR_Name[0] == 0x00 || dir_entry->DIR_Name[0] == 0xE5) {
          // Create LFN entries
          int lfn_entries = 0;

          if (strlen(dirname) > 8) {
            lfn_entries =
                create_lfn_entries(dirname, sector_buffer + j, sector_size);
            j += lfn_entries * sizeof(LFNStr_t);
          }
          // Create the 8.3 entry
          dir_entry =
              (DIRStr_t *)(sector_buffer + j/*  + lfn_entries * sizeof(LFNStr_t) */);
          memset(dir_entry, 0, sizeof(DIRStr_t));
          uint8_t nt_res = 0;
          char short_name[8];
          if (!lfn_entries) {
            generate_short_name(dirname, short_name, &nt_res);
          } else {
            generate_lfn_short_name(dirname, short_name);
          }
          memcpy(dir_entry->DIR_Name, short_name, 8);
          dir_entry->DIR_Attr = ATTR_DIRECTORY;
          dir_entry->DIR_FstClusLO = (uint16_t)(new_cluster & 0xFFFF);
          dir_entry->DIR_FstClusHI = (uint16_t)((new_cluster >> 16) & 0xFFFF);
          dir_entry->DIR_NTRes = nt_res;

          // Set creation time and date
          dir_entry->DIR_CrtTimeTenth = fat_time_tenth;
          dir_entry->DIR_CrtTime = fat_time;
          dir_entry->DIR_CrtDate = fat_date;
          dir_entry->DIR_WrtTime = fat_time;
          dir_entry->DIR_WrtDate = fat_date;
          dir_entry->DIR_LstAccDate = fat_date;

          write_sector(disk, current_sector + i, sector_buffer, sector_size);
          printf("Created directory entry for %s at cluster %u, sector %u\n",
                 dirname, new_cluster, current_sector + i);
          free(sector_buffer);
          return;
        }
      }
    }
    uint32_t next_cluster =
        get_next_cluster(disk, current_cluster, sector_size, rsrvd_sec);
    if (next_cluster >= EOC) {
      fprintf(stderr, "No free directory entry found\n");
      free(sector_buffer);
      return;
    }
    current_cluster = next_cluster;
  }
}

static int create_lfn_entries(const char *lfn, uint8_t *sector_buffer,
                              uint16_t sector_size) {
  int lfn_len = strlen(lfn);
  int num_entries = (lfn_len + 12) / 13;
  LFNStr_t *lfn_entry;

  char short_name[11];
  generate_lfn_short_name(lfn, short_name);
  uint8_t checksum = lfn_checksum((uint8_t *)short_name);

  static int is_incremented = 0;
  for (int i = num_entries; i > 0; i--) {
    lfn_entry = (LFNStr_t *)(sector_buffer + (i - 1) * sizeof(LFNStr_t));
    memset(lfn_entry, 0, sizeof(LFNStr_t));

    lfn_entry->LDIR_Ord = i;
    if (i == num_entries) {
      lfn_entry->LDIR_Ord |= 0x40; // Set the last LFN entry bit
    }
    lfn_entry->LDIR_Attr = ATTR_LFN;
    lfn_entry->LDIR_Chksum = checksum;
    for (int j = 0; j < 13; ++j) {
      int lfn_index = (i - 1) * 13 + j;
      if (lfn_index < lfn_len) {
        if (j < 5) {
          lfn_entry->LDIR_Name1[j] = lfn[lfn_index];
        } else if (j < 11) {
          lfn_entry->LDIR_Name2[j - 5] = lfn[lfn_index];
        } else {
          lfn_entry->LDIR_Name3[j - 11] = lfn[lfn_index];
        }
      } else {
        //NOTE: ugly as shit, rework ASAP
        int k = j;
        if (k < 5) {
          lfn_entry->LDIR_Name1[k++] = 0xFFFF;
        }
        if (k < 11 && k >= 5) {
          k++;
          lfn_entry->LDIR_Name2[k - 5] = 0xFFFF;
        }
        if (k >= 11) {
          lfn_entry->LDIR_Name3[k - 11] = 0xFFFF;
        }
        j = k;
      }
    }
  }
  printf("Created %d LFN entries for %s\n", num_entries, lfn);
  return num_entries;
}

static void generate_short_name(const char *dir_name, char *short_name,
                                uint8_t *nt_res) {
  memset(short_name, ' ', 8);
  *nt_res = 0;

  int i = 0, j = 0;
  while (i < 8 && dir_name[j] && dir_name[j] != '.') {
    if (islower(dir_name[j])) {
      *nt_res |= NT_RES_LOWER_CASE_BASE;
    }
    short_name[i++] = toupper(dir_name[j++]);
  }

  // if (dir_name[j] == '.') {
  //   j++;
  // }
  //
  // i = 8;
  // while (i < 11 && dir_name[j] && isalnum(dir_name[j])) {
  //   if (islower(dir_name[j])) {
  //     *nt_res |= NT_RES_LOWER_CASE_EXT;
  //   }
  //   short_name[i++] = toupper(dir_name[j++]);
  // }
}

static void generate_lfn_short_name(const char *lfn, char *short_name) {
  static int count = 0;
  generate_short_name(lfn, short_name, &(uint8_t){0});
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
  uint8_t sum = 0;
  for (int i = 0; i < 11; i++) {
    sum = (sum >> 1) + (sum << 7) + name[i];
  }
  return sum;
}

static void get_fat_time_date(uint16_t *fat_date, uint16_t *fat_time,
                              uint8_t *fat_time_tenth) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  *fat_date = ((t->tm_year - 80) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday;
  *fat_time = (t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec / 2);
  *fat_time_tenth = (uint8_t)((t->tm_sec % 2) * 100);
}

void mkdir(FILE *disk, const char *path) {
  BootSec_t boot_sec;
  read_boot_sector(disk, &boot_sec);

  uint32_t parent_cluster = boot_sec.BPB_RootClus;
  const char *dirname = path;

  uint32_t new_cluster = get_free_cluster(disk, &boot_sec);
  if (new_cluster == 0) {
    fprintf(stderr, "No free clusters available\n");
    exit(EXIT_FAILURE);
  }

  update_fat(disk, new_cluster, EOC, boot_sec.BPB_BytsPerSec,
             boot_sec.BPB_RsvdSecCnt);
  clear_cluster(disk, new_cluster, &boot_sec);
  create_directory_entry(disk, parent_cluster, dirname, new_cluster, &boot_sec);
}

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <disk_image> <path>\n", argv[0]);
    return 1;
  }

  const char *disk_name = argv[1];
  const char *path = argv[2];

  FILE *disk = fopen(disk_name, "r+b");
  if (!disk) {
    fprintf(stderr, "Failed to open disk image\n");
    return EXIT_FAILURE;
  }

  mkdir(disk, path);
  fclose(disk);

  return EXIT_SUCCESS;
}
