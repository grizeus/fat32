#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bootsec.h"
#include "directory.h"

extern void list_dir(FILE *disk, BootSec_t *boot_sec, uint32_t cluster);
extern void mkdir(FILE *disk, const char *path, uint32_t current_clus);

int change_dir(FILE *disk, BootSec_t *boot_sec, const char *path,
               uint32_t *current_clus) {

  char components[256];
  strcpy(components, path);

  char *token = strtok(components, "/");
  uint32_t cluster = *current_clus; // Start from the current directory

  if (token == NULL && path[0] == '/') {
    cluster = boot_sec->BPB_RootClus;
  }

  while (token != NULL) {
    // special cases for root directory
    if (cluster == boot_sec->BPB_RootClus) {
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

  *current_clus = cluster;
  return 0;
}
