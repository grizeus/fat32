#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bootsec.h"

extern int create_disk(FILE* disk, const char* disk_name, uint32_t disk_size, char modifier);
extern void handle_command(FILE* disk, const char* disk_name, BootSec_t* boot_sec,
                           uint8_t* is_fat32, uint32_t* current_clus, char* cwd, char* command);

int main(int argc, char** argv) {

  if (argc < 2) {

    fprintf(stderr, "Usage: %s <disk_image>\n", argv[0]);
    return -1;
  }
  const char* disk_name = argv[1];
  uint8_t is_fat32 = 0;
  FILE* disk = fopen(disk_name, "r+b");
  if (!disk) {

    if (create_disk(disk, disk_name, 20, 'M') != 0) {

      return -1;
    } else {

      disk = fopen(disk_name, "r+b");
    }
  }

  BootSec_t boot_sec;
  uint32_t current_clus;
  if (read_boot_sector(disk, &boot_sec) == 0) {

    is_fat32 = (boot_sec.BPB_FATSz16 == 0 && boot_sec.BPB_FATSz32 != 0) ? 1 : 0;
    if (is_fat32) {

      current_clus = boot_sec.BPB_RootClus;
    }
  }

  char cwd[512] = "/"; // Initialize with the root directory path
  char command[256];

  while (1) {

    printf("%s> ", cwd);
    if (fgets(command, sizeof(command), stdin) == NULL) {

      break;
    }

    command[strcspn(command, "\n")] = '\0'; // Remove the newline character
    if (strncmp(command, "exit", 4) == 0 || strncmp(command, "q", 1) == 0) {

      break;
    }
    handle_command(disk, disk_name, &boot_sec, &is_fat32, &current_clus, cwd, command);
  }

  fclose(disk);
  return 0;
}
