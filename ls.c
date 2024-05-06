#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define MAX_ENTRIES 1024
#define MAX_MAME_LEN 256

typedef struct entry_store entry_store_t;
typedef struct direntry dirent_t;

struct entry_store {
  char name[MAX_MAME_LEN];
};

struct direntry {
  unsigned long d_ino;
  off_t d_off;
  unsigned short d_reclen;
  char d_name[];
};

int entries_compare(const void *a, const void *b) {

  const entry_store_t *entry_a = (const entry_store_t *)a;
  const entry_store_t *entry_b = (const entry_store_t *)b;
  
  // skip leading dot
  const char* name_a = (entry_a->name[0] == '.') ? entry_a->name + 1 : entry_a->name;
  const char* name_b = (entry_b->name[0] == '.') ? entry_b->name + 1 : entry_b->name;

  return strcasecmp(name_a, name_b);
}

void list_dir(const char *path) {

  dirent_t *entry;
  char buf[BUF_SIZE];
  int fd;
  ssize_t count;
  entry_store_t entries[MAX_ENTRIES];
  int num_entries;

  fd = open(path, O_RDONLY | O_DIRECTORY);

  if (fd == -1) {
    perror("open");
    return;
  }

  while (1) {
    count = syscall(SYS_getdents, fd, buf, BUF_SIZE);
    if (count == -1) {
      perror("getdents");
      break;
    }

    if (count == 0) {
      break;
    }

    for (size_t pos = 0; pos < count;) {
      entry = (dirent_t *)(buf + pos);
      size_t next_pos = pos + entry->d_reclen;

      if (num_entries < MAX_ENTRIES) {
        strcpy(entries[num_entries].name, entry->d_name);
        ++num_entries;
      }

      pos = next_pos;
    }
  }

  qsort(entries, num_entries, sizeof(entry_store_t), entries_compare);

  for (size_t i = 0; i < num_entries; ++i) {
    printf("%s  ", entries[i].name);
  }
  putchar('\n');

  close(fd);
}

int main(int argc, char **argv) {

  if (argc < 2) {
    list_dir(".");
  } else {
    for (size_t i = 1; i < argc; ++i) {
      list_dir(argv[1]);
    }
  }

  return 0;
}
