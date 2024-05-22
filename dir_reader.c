#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bootsec.h"
#include "directory.h"

#define BYTS_PER_SEC 512
#define MAX_LFN_ENTRIES 20
#define MAX_MAME_LEN 255
#define NT_RES_LOWER_CASE_BASE 0x08
#define NT_RES_LOWER_CASE_EXT 0x10
#define MAX_DIR_ENTRIES 1024

typedef struct {
  char name[MAX_MAME_LEN];
} entry_store_t;

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

  const entry_store_t *entry_a = (const entry_store_t *)a;
  const entry_store_t *entry_b = (const entry_store_t *)b;

  // skip leading dot
  const char *name_a =
      (entry_a->name[0] == '.') ? entry_a->name + 1 : entry_a->name;
  const char *name_b =
      (entry_b->name[0] == '.') ? entry_b->name + 1 : entry_b->name;

  return strcasecmp(name_a, name_b);
}

static char *reverse_concatenate(char *array[], uint8_t count) {
  // Step 1: Determine the total length of the concatenated string
  int total_length = 0;
  for (int i = 0; i < count; ++i) {
    total_length += strlen(array[i]);
  }

  // Step 2: Allocate memory for the concatenated string
  char *result = (char *)malloc(total_length + 1);
  if (result == NULL) {
    perror("malloc");
    exit(1);
  }
  result[0] = '\0'; // Initialize the result string

  // Step 3: Reverse concatenate the strings
  for (int i = count - 1; i >= 0; --i) {
    strcat(result, array[i]);
  }

  return result;
}
void read_boot_sector(FILE *disk, BootSec_t *boot_sec) {
  fseek(disk, 0, SEEK_SET);
  fread(boot_sec, sizeof(BootSec_t), 1, disk);
}

static void calculate_sector(FILE *disk, BootSec_t *boot_sec, uint32_t cluster) {
    uint32_t sector = boot_sec->BPB_RsvdSecCnt +
                      (boot_sec->BPB_NumFATs * boot_sec->BPB_FATSz32);
    sector += (cluster - 2) * boot_sec->BPB_SecPerClus;
    fseek(disk, sector * BYTS_PER_SEC, SEEK_SET);
}

void list_dir(FILE *disk, BootSec_t *boot_sec, uint32_t cluster) {
  uint32_t sector = boot_sec->BPB_RsvdSecCnt +
                    (boot_sec->BPB_NumFATs * boot_sec->BPB_FATSz32);
  sector += (cluster - 2) * boot_sec->BPB_SecPerClus;
  fseek(disk, sector * BYTS_PER_SEC, SEEK_SET);

  DIRStr_t dir_entry;
  uint32_t file_count = 0;
  char lfn_buffer[256] = {0};
  int lfn_index = 0;

  char *lfn_name = NULL;
  char *lfn_chunks[MAX_LFN_ENTRIES] = {0};
  uint8_t lfn_chunk_counter = 0;
  entry_store_t *entries = malloc(MAX_DIR_ENTRIES * sizeof(entry_store_t));
  if (!entries) {
    fprintf(stderr, "Failed with allocate memory for entries\n");
    exit(EXIT_FAILURE);
  }

  while (fread(&dir_entry, sizeof(DIRStr_t), 1, disk) == 1) {
    if (dir_entry.DIR_Name[0] == 0) {
      break; // No more entries
    }
    if ((dir_entry.DIR_Attr & ATTR_LFN) == ATTR_LFN) {

      LFNStr_t *lfn_entry = (LFNStr_t *)&dir_entry;

      if (lfn_entry->LDIR_Ord & LAST_LONG_ENTRY) {
        lfn_chunk_counter = 0;
      }

      char lfn_buf[14] = {0};
      for (int j = 0; j < 5; ++j) {
        lfn_buf[j] = (char)lfn_entry->LDIR_Name1[j];
      }
      for (int j = 0; j < 6; ++j) {
        lfn_buf[5 + j] = (char)lfn_entry->LDIR_Name2[j];
      }
      for (int j = 0; j < 2; ++j) {
        lfn_buf[11 + j] = (char)lfn_entry->LDIR_Name3[j];
      }
      char *lfn_part = malloc(sizeof(lfn_buf) + 1);
      if (!lfn_part) {
        perror("malloc");
        exit(1);
      }

      strcpy(lfn_part, lfn_buf);
      lfn_chunks[lfn_chunk_counter] = lfn_part;
      lfn_chunk_counter++;
      continue;
    }
    if (lfn_chunk_counter > 0) {
      lfn_name = reverse_concatenate(lfn_chunks, lfn_chunk_counter);
      for (int i = 0; i < lfn_chunk_counter; ++i) {
        free(lfn_chunks[i]);
      }
      lfn_chunk_counter = 0;
    }

    char name[9];
    char ext[4];
    strncpy(name, (char *)dir_entry.DIR_Name, 8);
    name[8] = '\0';
    strncpy(ext, (char *)dir_entry.DIR_Name + 8, 3);
    ext[3] = '\0';

    // remove triling spaces in name and ext
    for (int i = 7; i >= 0 && name[i] == ' '; i--)
      name[i] = '\0';
    for (int i = 2; i >= 0 && ext[i] == ' '; i--)
      ext[i] = '\0';

    // "fix" true-case name
    if (dir_entry.DIR_NTRes & NT_RES_LOWER_CASE_BASE) {
      for (int i = 0; name[i]; i++) {
        name[i] = tolower(name[i]);
      }
    }

    if (dir_entry.DIR_NTRes & NT_RES_LOWER_CASE_EXT) {
      for (int i = 0; ext[i]; i++) {
        ext[i] = tolower(ext[i]);
      }
    }
    if (file_count < MAX_DIR_ENTRIES) {
      if (lfn_name) {
        strcpy(entries[file_count].name, lfn_name);
        free(lfn_name);
        lfn_name = NULL;
        ++file_count;
      } else {
        strcpy(entries[file_count].name, name);
        ++file_count;
      }
    }
  }

  qsort(entries, file_count, sizeof(entry_store_t), entries_compare);

  for (size_t i = 0; i < file_count; ++i) {
    printf("%s  ", entries[i].name);
  }

  putchar('\n');

  free(entries);
}

void list_dir_long(FILE *disk, BootSec_t *boot_sec, uint32_t cluster) {
  uint32_t sector = boot_sec->BPB_RsvdSecCnt +
                    (boot_sec->BPB_NumFATs * boot_sec->BPB_FATSz32);
  sector += (cluster - 2) * boot_sec->BPB_SecPerClus;
  fseek(disk, sector * BYTS_PER_SEC, SEEK_SET);

  DIRStr_t dir_entry;
  uint32_t byts_in_use = 0;
  uint32_t actual_files_size = 0;
  uint32_t file_count = 0;

  char *lfn_name = NULL;
  char *lfn_chunks[MAX_LFN_ENTRIES] = {0};
  uint8_t lfn_chunk_counter = 0;

  printf("Directory for ::/\n\n");

  while (fread(&dir_entry, sizeof(DIRStr_t), 1, disk) == 1) {
    if (dir_entry.DIR_Name[0] == 0) {
      break; // No more entries
    }

    if ((dir_entry.DIR_Attr & ATTR_LFN) == ATTR_LFN) {
      LFNStr_t *lfn_entry = (LFNStr_t *)&dir_entry;

      if (lfn_entry->LDIR_Ord & LAST_LONG_ENTRY) {
        lfn_chunk_counter = 0;
      }

      char lfn_buf[14] = {0};
      for (int j = 0; j < 5; ++j) {
        lfn_buf[j] = (char)lfn_entry->LDIR_Name1[j];
      }
      for (int j = 0; j < 6; ++j) {
        lfn_buf[5 + j] = (char)lfn_entry->LDIR_Name2[j];
      }
      for (int j = 0; j < 2; ++j) {
        lfn_buf[11 + j] = (char)lfn_entry->LDIR_Name3[j];
      }
      char *lfn_part = malloc(sizeof(lfn_buf) + 1);
      if (!lfn_part) {
        perror("malloc");
        exit(1);
      }
      strcpy(lfn_part, lfn_buf);
      lfn_chunks[lfn_chunk_counter] = lfn_part;

      lfn_chunk_counter++;
      continue;
    }

    if (lfn_chunk_counter > 0) {
      lfn_name = reverse_concatenate(lfn_chunks, lfn_chunk_counter);
      for (int i = 0; i < lfn_chunk_counter; ++i) {
        free(lfn_chunks[i]);
      }
      lfn_chunk_counter = 0;
    }

    char name[9];
    char ext[4];
    strncpy(name, (char *)dir_entry.DIR_Name, 8);
    name[8] = '\0';
    strncpy(ext, (char *)dir_entry.DIR_Name + 8, 3);
    ext[3] = '\0';

    // remove triling spaces in name and ext
    for (int i = 7; i >= 0 && name[i] == ' '; i--)
      name[i] = '\0';
    for (int i = 2; i >= 0 && ext[i] == ' '; i--)
      ext[i] = '\0';

    // "fix" true-case name
    if (dir_entry.DIR_NTRes & NT_RES_LOWER_CASE_BASE) {
      for (int i = 0; name[i]; i++) {
        name[i] = tolower(name[i]);
      }
    }

    if (dir_entry.DIR_NTRes & NT_RES_LOWER_CASE_EXT) {
      for (int i = 0; ext[i]; i++) {
        ext[i] = tolower(ext[i]);
      }
    }

    // Convert date and time
    uint16_t date = dir_entry.DIR_CrtDate;
    uint16_t time = dir_entry.DIR_CrtTime;
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

    if (dir_entry.DIR_Attr & ATTR_DIRECTORY) {
      char dir_label[] = "<DIR>";
      printf("%-8s %-3s %19s %s\n", name, dir_label, date_str, time_str);
      byts_in_use += boot_sec->BPB_SecPerClus * BYTS_PER_SEC;
    } else {
      uint32_t file_size = dir_entry.DIR_FileSize;
      uint32_t integer_part = file_size / BYTS_PER_SEC;
      uint32_t float_part = file_size % BYTS_PER_SEC;
      uint32_t sectors = (float_part == 0) ? integer_part : integer_part + 1;
      byts_in_use += sectors * BYTS_PER_SEC;
      if (lfn_name) {
        printf("%-8s %-3s %10u %s %s %s\n", name, ext, file_size, date_str,
               time_str, lfn_name);
        free(lfn_name);
        lfn_name = NULL;
      } else {
        printf("%-8s %-3s %10u %s %s\n", name, ext, file_size, date_str,
               time_str);
      }
      actual_files_size += file_size;
    }
    file_count++;
  }

  uint32_t user_area = boot_sec->BPB_TotSec32 - boot_sec->BPB_RsvdSecCnt -
                       (boot_sec->BPB_NumFATs * boot_sec->BPB_FATSz32);
  uint32_t free_byts = ((user_area - 1) / boot_sec->BPB_SecPerClus) *
                       BYTS_PER_SEC; // initial free space

  free_byts -= byts_in_use; // substract space occupied by files
  char formatted_free_byts[30];
  char formatted_files_size[30];
  format_with_spaces(formatted_free_byts, free_byts);
  format_with_spaces(formatted_files_size, actual_files_size);

  printf("%8u files %21s bytes\n", file_count, formatted_files_size);
  printf("%36s bytes free\n", formatted_free_byts);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <disk_image>\n", argv[0]);
    return 1;
  }

  FILE *disk = fopen(argv[1], "rb");
  if (!disk) {
    perror("Failed to open disk image");
    return 1;
  }

  BootSec_t boot_sec;
  read_boot_sector(disk, &boot_sec);
  if (strncmp((char *)boot_sec.BS_VOlLab, "NO NAME    ", 11) == 0) {
    printf("Volume in drive : has no label\n");
  } else {
    printf("Volume in drive : %s\n", boot_sec.BS_VOlLab);
  }
  uint32_t vol_id = boot_sec.BS_VOlId;
  uint32_t higher_order = (vol_id >> 16) & 0xFFFF;
  uint32_t lower_order = vol_id & 0xFFFF;
  printf("Volume Serial Number is %04X-%04X\n", higher_order, lower_order);

  list_dir_long(disk, &boot_sec, boot_sec.BPB_RootClus);
  list_dir(disk, &boot_sec, boot_sec.BPB_RootClus);

  fclose(disk);
  return 0;
}
