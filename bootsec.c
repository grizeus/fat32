#include "bootsec.h"
#include <stdio.h>

int read_boot_sector(FILE* disk, BootSec_t* boot_sec) {

  fseek(disk, 0, SEEK_SET);
  size_t res = fread(boot_sec, sizeof(BootSec_t), 1, disk);
  if (res != 1) {

    fprintf(stderr, "Failed to read boot sector\n");
    return -1;
  }
  return 0;
}
