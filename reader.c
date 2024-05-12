#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bootsec.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
    return 1;
  }

  FILE *file = fopen(argv[1], "rb");
  if (!file) {
    fprintf(stderr, "Failed to open file: %s\n", argv[1]);
    return 1;
  }

  const size_t bytes_to_read = sizeof(BootSec_t);
  printf("Size of bootsec %lu\n", bytes_to_read);

  char *buffer = malloc(bytes_to_read);
  if (!buffer) {
    fprintf(stderr, "Failed to allocate memory\n");
    fclose(file);
    return 1;
  }

  size_t bytes_read = fread(buffer, 1, bytes_to_read, file);
  if (bytes_read != bytes_to_read) {
    printf("Error reading file\n");
    free(buffer);
    fclose(file);
    return 1;
  }

  fclose(file);

  BootSec_t boot_sec;
  memcpy(&boot_sec, buffer, sizeof(BootSec_t));
  free(buffer);

  printBootSector(&boot_sec);

  return 0;
}
