#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bootsec.h"
#include "directory.h"

static void format_with_spaces(char *buffer, uint32_t num) {
  char temp[30];
  sprintf(temp, "%u", num);

  int len = strlen(temp);
  int ws_count = (len - 1) / 3;
  int new_len = len + ws_count;

  buffer[new_len] = '\0';

  int j = new_len - 1;
  for (int i = len - 1, k = 0; i >= 0; i--, j--, k++) {
    if (k == 3) {
      buffer[j--] = ' ';
      k = 0;
    }
    buffer[j] = temp[i];
  }
}

static int entries_compare(const void *a, const void *b) {

  const EntrSt_t *entry_a = (const EntrSt_t *)a;
  const EntrSt_t *entry_b = (const EntrSt_t *)b;

  // skip leading dot
  const char *name_a =
      (entry_a->name[0] == '.') ? entry_a->name + 1 : entry_a->name;
  const char *name_b =
      (entry_b->name[0] == '.') ? entry_b->name + 1 : entry_b->name;

  return strcasecmp(name_a, name_b);
}

void list_dir(FILE *disk, BootSec_t *boot_sec, uint32_t cluster) {
  EntrSt_t *entries = NULL;
  uint32_t entry_count = 0;

  if (read_dir_entries(disk, boot_sec, cluster, &entries, &entry_count) == 1) {
    fprintf(stderr, "Failed to read directory entries\n");
    return;
  }

  // Check if the current cluster is the root directory
  if (cluster == 2) {
    printf(".  ..  ");
  }

  qsort(entries, entry_count, sizeof(EntrSt_t), entries_compare);
  for (size_t i = 0; i < entry_count; ++i) {
    if (entries[i].attr & ATTR_DIRECTORY) {
      if (strncmp(entries[i].name, ".", 1) == 0 ||
          strncmp(entries[i].name, "..", 2) == 0) {
        printf("%s  ", entries[i].name);
      } else {
        printf("%s/  ", entries[i].name);
      }
    } else {
      printf("%s  ", entries[i].name);
    }
  }

  putchar('\n');

  free(entries);
}

void list_dir_long(FILE *disk, BootSec_t *boot_sec, uint32_t cluster) {

  EntrSt_t *entries = NULL;
  uint32_t local_entry_count = 0;

  if (read_dir_entries(disk, boot_sec, cluster, &entries, &local_entry_count) ==
      1) {
    fprintf(stderr, "Failed to read directory entries\n");
    return;
  }

  printf("Directory for ::/\n\n");
  uint32_t byts_in_use = 0;
  uint32_t actual_files_size = 0;
  uint32_t entry_count = 0;

  for (uint32_t i = 0; i < local_entry_count; i++) {
    // Convert date and time
    uint16_t date = entries[i].date;
    uint16_t time = entries[i].time;
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    tm.tm_year = ((date >> 9) & 0x7F) + 80;
    tm.tm_mon = ((date >> 5) & 0x0F) - 1;
    tm.tm_mday = date & 0x1F;
    tm.tm_hour = (time >> 11) & 0x1F;
    tm.tm_min = (time >> 5) & 0x3F;
    tm.tm_sec = (time & 0x1F) * 2;
    char date_str[11];
    char time_str[6];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", &tm);
    strftime(time_str, sizeof(time_str), "%H:%M", &tm);

    if (entries[i].attr & ATTR_DIRECTORY) {
      char dir_label[] = "<DIR>";
      printf("%-8s  %-3s %19s %s  %s\n", entries[i].name, dir_label, date_str,
             time_str, entries[i].name);
      byts_in_use += boot_sec->BPB_SecPerClus * boot_sec->BPB_BytsPerSec;
    } else {
      uint32_t file_size = entries[i].size;
      uint32_t integer_part = file_size / boot_sec->BPB_BytsPerSec;
      uint32_t float_part = file_size % boot_sec->BPB_BytsPerSec;
      uint32_t sectors = (float_part == 0) ? integer_part : integer_part + 1;
      byts_in_use += sectors * boot_sec->BPB_BytsPerSec;
      printf("%-8s %-3s %10u %s %s  %s\n", entries[i].name, entries[i].ext,
             file_size, date_str, time_str, entries[i].name);
      actual_files_size += file_size;
    }
    entry_count++;
  }

  free(entries);

  uint32_t fat_size = (boot_sec->BPB_FATSz32 == 0) ? boot_sec->BPB_FATSz16
                                                   : boot_sec->BPB_FATSz32;
  uint32_t tot_sec = (boot_sec->BPB_TotSec32 == 0) ? boot_sec->BPB_TotSec16
                                                   : boot_sec->BPB_TotSec32;
  uint32_t user_area =
      tot_sec - boot_sec->BPB_RsvdSecCnt - (boot_sec->BPB_NumFATs * fat_size);
  uint32_t free_byts = ((user_area - 1) / boot_sec->BPB_SecPerClus) *
                       boot_sec->BPB_BytsPerSec; // initial free space

  free_byts -= byts_in_use; // substract space occupied by files
  char formatted_free_byts[30];
  char formatted_files_size[30];
  format_with_spaces(formatted_free_byts, free_byts);
  format_with_spaces(formatted_files_size, actual_files_size);

  printf("%8u files %21s bytes\n", entry_count, formatted_files_size);
  printf("%36s bytes free\n", formatted_free_byts);
}
