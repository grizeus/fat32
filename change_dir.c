#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bootsec.h"
#include "directory.h"

#define SECTOR_SIZE 512
#define CLUSTER_SIZE 8 // Example cluster size in sectors

extern void list_dir(FILE *disk, BootSec_t *boot_sec, uint32_t cluster);

int change_dir(FILE *disk, BootSec_t *boot_sec, const char *path,
               uint32_t *currentCluster) {

  char components[256];
  strcpy(components, path);

  char *token = strtok(components, "/");
  uint32_t cluster = *currentCluster; // Start from the current directory

  while (token != NULL) {

    //special cases for root directory
    if (cluster == 2) {
      if (strncmp(token, "..", 2) == 0) {

        printf("No entry above root directory\n");
        token = strtok(NULL, "/");
        continue;
      } else if (strncmp(token, ".", 1) == 0) {

        token = strtok(NULL, "/");
        continue;
      }
    }

    EntrSt_t *entries = NULL;
    uint32_t entry_count = 0;
    int found = 0;

    if (read_dir_entries(disk, boot_sec, cluster, &entries, &entry_count) ==
        1) {
      fprintf(stderr, "Failed to read directory entries\n");
      return 1;
    }

    for (uint32_t i = 0; i < entry_count; i++) {
      if (strcmp(entries[i].name, token) == 0) {
        // Found the next component
        cluster = entries[i].cluster;
        found = 1;
        break;
      }
    }

    free(entries);

    if (!found) {
      // Directory component not found
      fprintf(stderr, "Directory for %s is not found\n", path);
      return 1;
    }

    token = strtok(NULL, "/");
  }

  *currentCluster = cluster;
  return 0;
}

int main(int argc, char **argv) {

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <disk_image>\n", argv[0]);
    return 1;
  }

  FILE *disk = fopen(argv[1], "rb");
  if (!disk) {
    fprintf(stderr, "Failed to open disk image: %s\n", argv[1]);
    return 1;
  }

  BootSec_t boot_sec;
  read_boot_sector(disk, &boot_sec);
  uint32_t current_clus = boot_sec.BPB_RootClus;

  while (1) {
    printf("/> ");
    char command[256];
    if (fgets(command, sizeof(command), stdin) == NULL) {
      break;
    }

    command[strcspn(command, "\n")] = '\0'; // Remove the newline character

    if (strcmp(command, "ls") == 0) {
      list_dir(disk, &boot_sec, current_clus);
    } else if (strncmp(command, "cd ", 3) == 0) {
      const char *path = command + 3;
      if (change_dir(disk, &boot_sec, path, &current_clus) == 1) {
        fprintf(stderr, "Failed to change directory: %s\n", path);
      }
    } else if (strcmp(command, "exit") == 0) {
      break;
    } else {
      fprintf(stderr, "Unknown command: %s\n", command);
    }
  }

  fclose(disk);
  return 0;
}
