#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bootsec.h"
#include "directory.h"

static void get_fat_time_date(uint16_t *fat_date, uint16_t *fat_time,
                              uint8_t *fat_time_tenth) {
  time_t t = time(NULL);
  struct tm *tm_info = localtime(&t);

  // FAT date: Bits 0–4: Day of month (1–31), Bits 5–8: Month (1–12), Bits 9–15:
  // Year (since 1980)
  *fat_date = ((tm_info->tm_year - 80) << 9) | ((tm_info->tm_mon + 1) << 5) |
              tm_info->tm_mday;

  // FAT time: Bits 0–4: 2-second count (0–29), Bits 5–10: Minutes (0–59), Bits
  // 11–15: Hours (0–23)
  *fat_time =
      (tm_info->tm_hour << 11) | (tm_info->tm_min << 5) | (tm_info->tm_sec / 2);

  // FAT time tenth: Milliseconds divided by 10 (0-199)
  *fat_time_tenth = (uint8_t)((t % 1000) / 10);
}

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
  uint32_t fat_sector = rsrvd_sec + (cluster * FAT_ELEM_SIZE) / sector_size;
  uint32_t offset = (cluster * FAT_ELEM_SIZE) % sector_size;
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
  uint32_t fat_sector = rsrvd_sec + (cluster * FAT_ELEM_SIZE) / sector_size;
  uint32_t offset = (cluster * FAT_ELEM_SIZE) % sector_size;
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

  uint32_t current_cluster = parent_cluster;
  uint32_t current_sector;
  DIRStr_t *dir_entry;

  uint16_t fat_date, fat_time;
  uint8_t fat_time_tenth;
  get_fat_time_date(&fat_date, &fat_time, &fat_time_tenth);

  while (1) {
    current_sector = boot_sec->BPB_HiddSec + rsrvd_sec +
                     (boot_sec->BPB_NumFATs * boot_sec->BPB_FATSz32) +
                     (current_cluster - 2) * boot_sec->BPB_SecPerClus;
    for (uint32_t i = 0; i < boot_sec->BPB_SecPerClus; i++) {
      read_sector(disk, current_sector + i, sector_buffer, sector_size);
      for (uint32_t j = 0; j < sector_size; j += rsrvd_sec) {
        dir_entry = (DIRStr_t *)(sector_buffer + j);
        if (dir_entry->DIR_Name[0] == 0x00 || dir_entry->DIR_Name[0] == 0xE5) {
          memset(dir_entry, 0, sizeof(DIRStr_t));
          strncpy((char *)dir_entry->DIR_Name, dirname, 11);
          dir_entry->DIR_Attr = ATTR_DIRECTORY;
          dir_entry->DIR_FstClusLO = (uint16_t)(new_cluster & 0xFFFF);
          dir_entry->DIR_FstClusHI = (uint16_t)((new_cluster >> 16) & 0xFFFF);

          // Set creation time and date
          dir_entry->DIR_CrtTimeTenth = fat_time_tenth;
          dir_entry->DIR_CrtTime = fat_time;
          dir_entry->DIR_CrtDate = fat_date;
          dir_entry->DIR_LstAccDate = fat_date; // Optional: Last access date
          dir_entry->DIR_WrtTime = fat_time;    // Optional: Last write time
          dir_entry->DIR_WrtDate = fat_date;    // Optional: Last write date

          write_sector(disk, current_sector + i, sector_buffer, sector_size);
          free(sector_buffer);
          return;
        }
      }
    }
    uint32_t next_cluster =
        get_next_cluster(disk, current_cluster, sector_size, rsrvd_sec);
    if (next_cluster >= EOC) {
      uint32_t new_cluster = get_free_cluster(disk, boot_sec);
      update_fat(disk, current_cluster, new_cluster, sector_size, rsrvd_sec);
      update_fat(disk, new_cluster, EOC, sector_size, rsrvd_sec);
      clear_cluster(disk, new_cluster, boot_sec);
      current_cluster = new_cluster;
    } else {
      current_cluster = next_cluster;
    }
  }
  free(sector_buffer);
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
  return EXIT_SUCCESS;
}
