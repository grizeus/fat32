#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

int create_disk(FILE* disk, const char* disk_name, uint32_t disk_size, char modifier) {

  disk = fopen(disk_name, "w+b");
  if (!disk) {
    printf("Can't create %s\n", disk_name);
    return -1;
  }

  uint32_t desired_size;
  if (toupper(modifier) == 'K') {
    desired_size = 1000UL * disk_size;
  } else if (toupper(modifier) == 'M') {
    desired_size = 1000UL * 1000 * disk_size;
  }

  if (ftruncate(fileno(disk), desired_size) != 0) {
    fprintf(stderr, "Error setting file size");
    fclose(disk);
    return -1;
  }
  return 0;
}
