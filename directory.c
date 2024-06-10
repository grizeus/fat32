#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "bootsec.h"
#include "directory.h"

#define MAX_LFN_ENTRIES 20
#define MAX_DIR_ENTRIES 1024

static void calculate_sector(FILE* disk, BootSec_t* boot_sec, uint32_t cluster) {

  uint32_t fat_size = (boot_sec->BPB_FATSz32 == 0) ? boot_sec->BPB_FATSz16 : boot_sec->BPB_FATSz32;
  uint32_t sector = boot_sec->BPB_RsvdSecCnt + (boot_sec->BPB_NumFATs * fat_size);
  sector += (cluster - 2) * boot_sec->BPB_SecPerClus;
  fseek(disk, sector * boot_sec->BPB_BytsPerSec, SEEK_SET);
}

static void process_lfn_entry(DIRStr_t* dir_entry, char** lfn_chunks, uint8_t* lfn_chunk_counter) {

  LFNStr_t* lfn_entry = (LFNStr_t*)dir_entry;

  if (lfn_entry->LDIR_Ord & LAST_LONG_ENTRY) {

    *lfn_chunk_counter = 0;
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
  char* lfn_part = malloc(sizeof(lfn_buf) + 1);
  if (!lfn_part) {

    fprintf(stderr, "Failed with allocate memory\n");
    exit(EXIT_FAILURE);
  }

  strcpy(lfn_part, lfn_buf);
  lfn_chunks[*lfn_chunk_counter] = lfn_part;
  *lfn_chunk_counter += 1;
}

static void process_dir_entry(DIRStr_t* dir_entry, char* name, char* ext) {

  strncpy(name, (char*)dir_entry->DIR_Name, 8);
  name[8] = '\0';

  strncpy(ext, (char*)dir_entry->DIR_Name + 8, 3);
  ext[3] = '\0';

  // remove trailing spaces in name and ext
  for (int i = 7; i >= 0 && name[i] == ' '; i--)
    name[i] = '\0';

  for (int i = 2; i >= 0 && ext[i] == ' '; i--)
    ext[i] = '\0';

  // "fix" true-case name
  if (dir_entry->DIR_NTRes & NT_RES_LOWER_CASE_BASE) {

    for (int i = 0; name[i]; i++) {

      name[i] = tolower(name[i]);
    }
  }

  if (dir_entry->DIR_NTRes & NT_RES_LOWER_CASE_EXT) {

    for (int i = 0; ext[i]; i++) {

      ext[i] = tolower(ext[i]);
    }
  }
}

static char* reverse_concatenate(char* array[], uint8_t count) {

  // Step 1: Determine the total length of the concatenated string
  int total_length = 0;
  for (int i = 0; i < count; ++i) {

    total_length += strlen(array[i]);
  }

  // Step 2: Allocate memory for the concatenated string
  char* result = (char*)malloc(total_length + 1);
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

int read_dir_entries(FILE* disk, BootSec_t* boot_sec, uint32_t cluster, EntrSt_t** entries,
                     uint32_t* entry_count) {

  DIRStr_t dir_entry;
  *entry_count = 0;
  char* lfn_name = NULL;
  char* lfn_chunks[MAX_LFN_ENTRIES] = {0};
  uint8_t lfn_chunk_counter = 0;

  calculate_sector(disk, boot_sec, cluster);

  while (fread(&dir_entry, sizeof(DIRStr_t), 1, disk) == 1) {

    if (dir_entry.DIR_Name[0] == 0) {

      break; // No more entries
    }
    if ((dir_entry.DIR_Attr & ATTR_LFN) == ATTR_LFN) {

      process_lfn_entry(&dir_entry, lfn_chunks, &lfn_chunk_counter);
      continue;
    }
    if (lfn_chunk_counter > 0) {

      lfn_name = reverse_concatenate(lfn_chunks, lfn_chunk_counter);
      for (int i = 0; i < lfn_chunk_counter; ++i) {

        free(lfn_chunks[i]);
      }
      lfn_chunk_counter = 0;
    }
    char name[9], ext[4];
    process_dir_entry(&dir_entry, name, ext);
    (*entries) = realloc((*entries), ((*entry_count) + 1) * sizeof(EntrSt_t));
    if (!(*entries)) {

      fprintf(stderr, "Failed to allocate memory\n");
      return 1;
    }
    if (lfn_name) {

      strcpy((*entries)[*entry_count].name, lfn_name);
      free(lfn_name);
      lfn_name = NULL;
    } else {

      if (ext[0] != '\0') {

        char final_name[13];
        snprintf(final_name, 12, "%s.%s", name, ext);
        strcpy((*entries)[*entry_count].name, final_name);
      } else {

        strcpy((*entries)[*entry_count].name, name);
      }
    }
    (*entries)[*entry_count].cluster = (dir_entry.DIR_FstClusHI << 16) | dir_entry.DIR_FstClusLO;
    (*entries)[*entry_count].size = dir_entry.DIR_FileSize;
    (*entries)[*entry_count].date = dir_entry.DIR_CrtDate;
    (*entries)[*entry_count].time = dir_entry.DIR_CrtTime;
    (*entries)[*entry_count].attr = dir_entry.DIR_Attr;
    memcpy((*entries)[*entry_count].ext, ext, 4);

    (*entry_count)++;
  }
  return 0;
}
