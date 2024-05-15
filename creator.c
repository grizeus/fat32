#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

  if (argc != 4) {
    fprintf(stderr,
            "Program must run with %s <file_name> <size> <spec>(M for Mb, K "
            "for Kb)\n",
            argv[0]);
    return 1;
  }

  const char *filename = argv[1];
  if (!filename) {
    fprintf(stderr, "File name must be provided\n");
    return 1;
  }

  int64_t size = atol(argv[2]);
  if (size < 1) {
    fprintf(stderr, "Error: size must be greater than 1\n");
    return 1;
  }

  if (size >= 2000000) {
    fprintf(stderr, "Error: size must be smaller than 2'000'000\n");
    return 1;
  }

  FILE *file = fopen(argv[1], "w");
  if (!file) {
    printf("Can't create %s\n", argv[1]);
    return 1;
  }
  uint32_t desired_size;
  if (strcmp(argv[3], "K") == 0) {
    desired_size = 1000UL * size;
  } else if (strcmp(argv[3], "M") == 0) {
    desired_size = 1000UL * 1000 * size;
  }

  if (ftruncate(fileno(file), desired_size) != 0) {
    fprintf(stderr, "Error setting file size");
    fclose(file);
    return 1;
  }

  fclose(file);

  return 0;
}
