#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bootsec.h"
#include "directory.h"

extern void list_dir(FILE *disk, BootSec_t *boot_sec, uint32_t cluster);
extern void mkdir(FILE *disk, BootSec_t *boot_sec, const char *path,
                  uint32_t current_clus);
extern int change_dir(FILE *disk, BootSec_t *boot_sec, const char *path,
                      uint32_t *current_clus);
extern int read_dir_entries(FILE *disk, BootSec_t *boot_sec, uint32_t cluster,
                            EntrSt_t **entries, uint32_t *entry_count);
extern void touch(FILE *disk, BootSec_t *boot_sec, char *path,
                  uint32_t current_clus);
extern int create_disk(FILE *disk, const char *disk_name, uint32_t disk_size,
                       char modifier);
extern int format_disk(const char *filename);

int main(int argc, char **argv) {

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <disk_image>\n", argv[0]);
    return -1;
  }
  const char *disk_name = argv[1];
  uint8_t is_fat32 = 0;
  FILE *disk = fopen(disk_name, "r+b");
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
  while (1) {

    printf("%s> ", cwd);
    char command[256];
    if (fgets(command, sizeof(command), stdin) == NULL) {
      break;
    }

    command[strcspn(command, "\n")] = '\0'; // Remove the newline character

    if (!is_fat32 && strncmp(command, "format", 6) == 0) {
      fclose(disk);
      format_disk(disk_name);
      disk = fopen(disk_name, "r+b");
      if (!disk) {
        fprintf(stderr, "Failed to open disk image after formatting: %s\n",
                disk_name);
        return -1;
      }

      read_boot_sector(disk, &boot_sec);
      is_fat32 = 1;
      current_clus = boot_sec.BPB_RootClus;
      continue;
    } else if (!is_fat32) {
       fprintf(stderr, "Unknown disk format\n");
       continue;
    }

    if (strncmp(command, "ls", 2) == 0) {
      list_dir(disk, &boot_sec, current_clus);
    } else if (strncmp(command, "cd ", 3) == 0) {
      char *path = command + 3;
      uint32_t parent_clus = current_clus;
      if (change_dir(disk, &boot_sec, path, &current_clus) == 1) {
        fprintf(stderr, "Failed to change directory: %s\n", path);
      } else {
        // Update the current working directory
        char temp_cwd[512];
        strcpy(temp_cwd, cwd);
        char *token = strtok(path, "/");

        // handle "cd /" command to display root correctly
        if (token == NULL && path[0] == '/') {
          strcpy(temp_cwd, "/");
        }

        while (token != NULL) {
          if (strncmp(token, "..", 2) == 0) {
            // Go to the parent directory
            char *last_slash = strrchr(temp_cwd, '/');
            if (last_slash != NULL && last_slash != temp_cwd) {
              *last_slash = '\0'; // Truncate the string at the last slash
            } else {
              strcpy(temp_cwd, "/"); // root directory case
            }
          } else if (strncmp(token, ".", 1) == 0) {
            strcpy(cwd, temp_cwd);
          } else {
            // Append the directory component to temp_cwd
            if (parent_clus >
                boot_sec.BPB_RootClus) { // avoid trailing slash for root dir
              strcat(temp_cwd, "/");
            }
            strcat(temp_cwd, token);
          }
          token = strtok(NULL, "/");
        }
        strcpy(cwd, temp_cwd);
      }
    } else if (strncmp(command, "mkdir ", 6) == 0) {
      // handle similar dir name creating
      const char *path = command + 6;
      EntrSt_t *entries = NULL;
      uint32_t entry_count = 0;
      read_dir_entries(disk, &boot_sec, current_clus, &entries, &entry_count);
      uint8_t is_exist = 0;
      for (size_t i = 0; i < entry_count; ++i) {
        if (strcmp(entries[i].name, path) == 0) {
          fprintf(stderr, "Directory %s already exists\n", path);
          free(entries);
          entries = NULL;
          is_exist = 1;
        }
      }
      if (is_exist) {
        continue;
      }
      if (entries) {
        free(entries);
      }
      mkdir(disk, &boot_sec, path, current_clus);
    } else if (strncmp(command, "touch ", 6) == 0) {
      char *path = command + 6;
      touch(disk, &boot_sec, path, current_clus);
    } else if (strncmp(command, "format", 6) == 0) {
      fclose(disk);
      format_disk(disk_name);
      disk = fopen(disk_name, "r+b");
      if (!disk) {
        fprintf(stderr, "Failed to open disk image after formatting: %s\n",
                disk_name);
        return -1;
      }

      read_boot_sector(disk, &boot_sec);
      is_fat32 = 1;
      current_clus = boot_sec.BPB_RootClus;
    } else if (strncmp(command, "exit", 4) == 0 ||
               strncmp(command, "q", 1) == 0) {
      break;
    } else {
      fprintf(stderr, "Unknown command: %s\n", command);
    }
  }

  fclose(disk);
  return 0;
}
