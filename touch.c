#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bootsec.h"
#include "directory.h"
#include "utility.h"

static void generate_short_filename(const char *file_name, char *short_name,
                                    uint8_t *nt_res) {
  memset(short_name, 0x20, 11); // 0x20 for whitespace
  *nt_res = 0;

  int i = 0, j = 0;
  while (i < 8 && file_name[j]) {
    if (islower(file_name[j])) {
      *nt_res |= NT_RES_LOWER_CASE_BASE;
    }
    short_name[i++] = toupper(file_name[j++]);
  }

  char *last_dot = strrchr(file_name, '.');
  if (last_dot) {
    size_t dot_pos = last_dot - file_name;
    j = dot_pos + 1;
    i = 8; // pos of ext start

    while (i < 11 && file_name[j]) {
      if (islower(file_name[j])) {
        *nt_res |= NT_RES_LOWER_CASE_EXT;
      }
      short_name[i++] = toupper(file_name[j++]);
    }
  }
}

static void create_file_entry(FILE *disk, uint32_t parent_cluster,
                              const char *file_name, uint32_t new_cluster,
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

  // check for existing file to update time
  uint32_t entry_count = 0;
  EntrSt_t *entries = NULL;
  uint8_t is_exist = 0;
  read_dir_entries(disk, boot_sec, parent_cluster, &entries, &entry_count);

  for (size_t i = 0; i < entry_count; ++i) {
    if (strcmp(entries[i].name, file_name) == 0) {
      is_exist = 1;
      free(entries);
      entries = NULL;
      break;
    }
  }
  if (entries) {
    free(entries);
    entries = NULL;
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

        if (is_exist) {
          // Update the timestamps
          dir_entry->DIR_LstAccDate = fat_date;

          // Write the updated sector back to the disk
          write_sector(disk, current_sector + i, sector_buffer, sector_size);
          free(sector_buffer);
          return;
        }
        if (dir_entry->DIR_Name[0] == 0x00 || dir_entry->DIR_Name[0] == 0xE5) {
          // Create LFN entries
          int lfn_entries = 0;
          size_t dir_len = strlen(file_name);
          uint8_t nt_res = 0;
          char short_name[11];

          if (dir_len > 8) {
            lfn_entries = create_lfn_entries(
                file_name, dir_len, sector_buffer + j, sector_size, short_name,
                &nt_res, generate_short_filename);
            j += lfn_entries * sizeof(LFNStr_t);
          }

          // Create the 8.3 entry
          dir_entry = (DIRStr_t *)(sector_buffer + j);
          memset(dir_entry, 0, sizeof(DIRStr_t));
          if (!lfn_entries) {
            generate_short_filename(file_name, short_name, &nt_res);
          }

          memcpy(dir_entry->DIR_Name, short_name, 11);
          dir_entry->DIR_NTRes = nt_res;
          dir_entry->DIR_Attr = ATTR_ARCHIVE;
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

void touch(FILE *disk, BootSec_t *boot_sec, char *path, uint32_t current_clus) {

  uint32_t parent_cluster = current_clus;
  char *file_name = path;
  size_t size = strlen(path);
  file_name[size] = '\0';

  uint32_t new_cluster = get_free_cluster(disk, boot_sec);
  if (new_cluster == 0) {
    fprintf(stderr, "No free clusters available\n");
    exit(EXIT_FAILURE);
  }

  update_fat(disk, new_cluster, EOC, boot_sec->BPB_BytsPerSec,
             boot_sec->BPB_RsvdSecCnt);
  clear_cluster(disk, new_cluster, boot_sec);
  create_file_entry(disk, parent_cluster, file_name, new_cluster, boot_sec);
}
