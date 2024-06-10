#include "bootsec.h"
#include <stdint.h>
#include <stdio.h>

int is_fat32(const char* disk_image_path) {
  FILE* disk_image = fopen(disk_image_path, "rb");
  if (!disk_image) {
    perror("Failed to open disk image");
    return -1;
  }

  BootSec_t boot_sector;
  size_t read_size = fread(&boot_sector, sizeof(BootSec_t), 1, disk_image);
  fclose(disk_image);

  if (read_size != 1) {
    fprintf(stderr, "Failed to read boot sector\n");
    return -1;
  }

  // Check for FAT32 signature
  if (boot_sector.BPB_FATSz16 == 0 && boot_sector.BPB_FATSz32 != 0) {
    printf("This disk image is formatted as FAT32.\n");
    return 1;
  } else {
    printf("This disk image is not formatted as FAT32.\n");
    return 0;
  }
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <disk_image_path>\n", argv[0]);
    return 1;
  }

  int result = is_fat32(argv[1]);
  return (result == -1) ? 1 : 0;
}
