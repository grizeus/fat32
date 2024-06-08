#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bootsec.h"
#include "directory.h"

extern void list_dir(FILE *disk, BootSec_t *boot_sec, uint32_t cluster);
extern void mkentry(FILE *disk, BootSec_t *boot_sec, const char *path,
                    uint8_t is_dir, uint32_t current_clus);
extern int change_dir(FILE *disk, BootSec_t *boot_sec, const char *path,
                      uint32_t *current_clus);
extern int read_dir_entries(FILE *disk, BootSec_t *boot_sec, uint32_t cluster,
                            EntrSt_t **entries, uint32_t *entry_count);
int main(int argc, char **argv) {

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <disk_image>\n", argv[0]);
    return 1;
  }

  FILE *disk = fopen(argv[1], "r+b");
  if (!disk) {
    fprintf(stderr, "Failed to open disk image: %s\n", argv[1]);
    return 1;
  }

  BootSec_t boot_sec;
  read_boot_sector(disk, &boot_sec);
  uint32_t current_clus = boot_sec.BPB_RootClus;

  char cwd[512] = "/"; // Initialize with the root directory path
  while (1) {

    printf("%s> ", cwd);
    char command[256];
    if (fgets(command, sizeof(command), stdin) == NULL) {
      break;
    }

    command[strcspn(command, "\n")] = '\0'; // Remove the newline character

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
      mkentry(disk, &boot_sec, path, 1, current_clus);
    } else if (strncmp(command, "touch ", 6) == 0) {
      const char *path = command + 6;
      mkentry(disk, &boot_sec, path, 0, current_clus);
    } else if (strncmp(command, "exit", 4) == 0) {
      break;
    } else {
      fprintf(stderr, "Unknown command: %s\n", command);
    }
  }

  fclose(disk);
  return 0;
}
