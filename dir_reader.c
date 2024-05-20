#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bootsec.h"
#include "directory.h"

#define BYTS_PER_SEC 512

void format_with_spaces(char *buffer, uint32_t num) {
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

void read_boot_sector(FILE *disk, BootSec_t *boot_sec) {
  fseek(disk, 0, SEEK_SET);
  fread(boot_sec, sizeof(BootSec_t), 1, disk);
}

uint32_t get_fat_entry(FILE *disk, BootSec_t *boot_sec, uint32_t cluster) {
  uint32_t fat_offset = cluster * 4;
  uint32_t fat_sector =
      boot_sec->BPB_RsvdSecCnt + (fat_offset / boot_sec->BPB_BytsPerSec);
  uint32_t entry_offset = fat_offset % boot_sec->BPB_BytsPerSec;
  uint32_t fat_entry;

  fseek(disk, fat_sector * boot_sec->BPB_BytsPerSec + entry_offset, SEEK_SET);
  fread(&fat_entry, sizeof(uint32_t), 1, disk);
  return fat_entry & 0x0FFFFFFF;
}

void read_directory(FILE *disk, BootSec_t *boot_sec, uint32_t cluster) {
  uint32_t sector = boot_sec->BPB_RsvdSecCnt +
                    (boot_sec->BPB_NumFATs * boot_sec->BPB_FATSz32);
  sector += (cluster - 2) * boot_sec->BPB_SecPerClus;
  fseek(disk, sector * BYTS_PER_SEC, SEEK_SET);

  DIRStr_t dir_entry;
  uint32_t byts_in_use = 0;
  uint32_t actual_files_size = 0;
  uint32_t file_count = 0;

  printf("Directory for ::/\n\n");

  while (fread(&dir_entry, sizeof(DIRStr_t), 1, disk) == 1) {
    if (dir_entry.DIR_Name[0] == 0) {
      break; // No more entries
    }
    if ((dir_entry.DIR_Attr & 0x0F) == 0x0F)
      continue; // Skip long name entries

    char name[9];
    char ext[4];
    strncpy(name, (char *)dir_entry.DIR_Name, 8);
    name[8] = '\0';
    strncpy(ext, (char *)dir_entry.DIR_Name + 8, 3);
    ext[3] = '\0';

    for (int i = 7; i >= 0 && name[i] == ' '; i--)
      name[i] = '\0';
    for (int i = 2; i >= 0 && ext[i] == ' '; i--)
      ext[i] = '\0';

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
    if (dir_entry.DIR_Attr & 0x10) {
      char dir_label[] = "<DIR>";
      printf("%-8s %-3s %19s %s\n", name, dir_label, date_str, time_str);
      byts_in_use += boot_sec->BPB_SecPerClus * BYTS_PER_SEC;
    } else {
      uint32_t file_size = dir_entry.DIR_FileSize;
      // if (file_size > 0) {
      uint32_t integer_part = file_size / BYTS_PER_SEC;
      uint32_t float_part = file_size % BYTS_PER_SEC;
      uint32_t sectors = (float_part == 0) ? integer_part : integer_part + 1;
      byts_in_use += sectors * BYTS_PER_SEC;
      printf("%-8s %-3s %10u %s %s\n", name, ext, file_size, date_str,
             time_str);
      actual_files_size += file_size;
      // }
    }
    file_count++;
  }
  uint32_t user_area = boot_sec->BPB_TotSec32 - boot_sec->BPB_RsvdSecCnt -
                       (boot_sec->BPB_NumFATs * boot_sec->BPB_FATSz32);
  uint32_t free_byts = ((user_area - 1) / boot_sec->BPB_SecPerClus) *
                       BYTS_PER_SEC; // initial free space
  free_byts -= byts_in_use;          // substract space occupied by files
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

  read_directory(disk, &boot_sec, boot_sec.BPB_RootClus);

  fclose(disk);
  return 0;
}
