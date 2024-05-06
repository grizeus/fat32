#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// #include <sys/stat.h>
#include <fcntl.h>

#define DISK_SIZE (20 * 1024 * 1024)
#define BLOCK_SIZE 512
#define RESERVED_BLOCKS 32
#define FAT_BLOCKS 16
#define ROOT_BLOCKS 32
#define DATA_BLOCKS                                                            \
  (DISK_SIZE / BLOCK_SIZE - RESERVED_BLOCKS - FAT_BLOCKS - ROOT_BLOCKS)

typedef struct {
  char name[8 + 3 + 1];
  uint8_t attrs;
  uint8_t reserved[10];
  uint16_t first_block;
  uint32_t size;
} __attribute__((packed)) directory_entry_t;

typedef struct {
  uint8_t boot_sector[BLOCK_SIZE];
  uint32_t fat[FAT_BLOCKS * BLOCK_SIZE / sizeof(uint32_t)];
  directory_entry_t
      root_dir[ROOT_BLOCKS * BLOCK_SIZE / sizeof(directory_entry_t)];
  uint8_t data_blocks[DATA_BLOCKS * BLOCK_SIZE];
} disk_t;

disk_t *disk;
char cwd[256] = "/";

void read_disk(const char *path) {
  int fd = open(path, O_RDWR | O_CREAT, 0644);
  if (fd == -1) {
    perror("open");
    exit(1);
  }

  disk = malloc(DISK_SIZE);
  if (disk == NULL) {
    perror("malloc");
    exit(1);
  }

  if (read(fd, disk, DISK_SIZE) != DISK_SIZE) {
    perror("read");
    exit(1);
  }

  close(fd);
}

void write_disk(const char *path) {
  int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    perror("open");
    exit(1);
  }

  if (write(fd, disk, DISK_SIZE) != DISK_SIZE) {
    perror("write");
    exit(1);
  }

  close(fd);
}

void format_disk() {
  memset(disk, 0, DISK_SIZE);
  memset(disk->fat, 0xFF, sizeof(disk->fat));
  disk->fat[0] = 0xFFFFFFF8;
  disk->fat[1] = 0xFFFFFFFF;
}

void ls() {
  directory_entry_t *dir = disk->root_dir;
  int count = 0;
  for (int i = 0; i < ROOT_BLOCKS * BLOCK_SIZE / sizeof(directory_entry_t);
       i++) {
    if (dir[i].name[0] != 0) {
      printf("%s\n", dir[i].name);
      count++;
    }
  }
  if (count == 0) {
    printf("Directory is empty\n");
  }
}

void cd(const char *path) {
  if (path[0] != '/') {
    printf("Error: Only absolute paths are supported\n");
    return;
  }

  strcpy(cwd, path);
}

void mkdir(const char *name) {
  directory_entry_t *dir = disk->root_dir;
  for (int i = 0; i < ROOT_BLOCKS * BLOCK_SIZE / sizeof(directory_entry_t);
       i++) {
    if (dir[i].name[0] == 0) {
      strncpy(dir[i].name, name, 11);
      dir[i].attrs = 0x10;    // Directory attribute
      dir[i].first_block = 0; // No data blocks for directories
      dir[i].size = 0;
      break;
    }
  }
}

void touch(const char *name) {
  directory_entry_t *dir = disk->root_dir;
  for (int i = 0; i < ROOT_BLOCKS * BLOCK_SIZE / sizeof(directory_entry_t);
       i++) {
    if (dir[i].name[0] == 0) {
      strncpy(dir[i].name, name, 11);
      dir[i].attrs = 0;       // Regular file attribute
      dir[i].first_block = 0; // No data blocks allocated yet
      dir[i].size = 0;
      break;
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <disk_file>\n", argv[0]);
    return 1;
  }

  read_disk(argv[1]);

  char command[256];
  while (1) {
    printf("%s> ", cwd);
    if (fgets(command, sizeof(command), stdin) == NULL) {
      break;
    }

    char *cmd = strtok(command, " \n");
    if (cmd == NULL) {
      continue;
    }

    if (strcmp(cmd, "format") == 0) {
      format_disk();
    } else if (strcmp(cmd, "ls") == 0) {
      ls();
    } else if (strcmp(cmd, "cd") == 0) {
      char *path = strtok(NULL, " \n");
      if (path == NULL) {
        printf("Error: Path required\n");
      } else {
        cd(path);
      }
    } else if (strcmp(cmd, "mkdir") == 0) {
      char *name = strtok(NULL, " \n");
      if (name == NULL) {
        printf("Error: Name required\n");
      } else {
        mkdir(name);
      }
    } else if (strcmp(cmd, "touch") == 0) {
      char *name = strtok(NULL, " \n");
      if (name == NULL) {
        printf("Error: Name required\n");
      } else {
        touch(name);
      }
    } else {
      printf("Unknown command: %s\n", cmd);
    }
  }

  write_disk(argv[1]);
  free(disk);

  return 0;
}
