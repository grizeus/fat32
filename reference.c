#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fsinfo.h"
#include "bootsec.h"

#define BYTS_PER_SEC 512
const uint8_t NULL_BYTE = 0x0;

typedef struct Node {
  char name[256];
  struct Node *next;
  struct Node *child;
} Node;

Node *root;
Node *current;

Node *create_node(const char *name) {
  Node *node = (Node *)malloc(sizeof(Node));
  strcpy(node->name, name);
  node->next = NULL;
  node->child = NULL;
  return node;
}

void format_disk() {
  root = create_node("/");
  current = root;
  printf("Ok\n");
}

void list_directory(Node *dir) {
  Node *node = dir->child;
  while (node != NULL) {
    printf("%s\n", node->name);
    node = node->next;
  }
}

void make_directory(const char *name) {
  Node *node = create_node(name);
  node->next = current->child;
  current->child = node;
  printf("Ok\n");
}

void change_directory(const char *path) {
  if (strcmp(path, "/") == 0) {
    current = root;
  } else {
    Node *node = current->child;
    while (node != NULL) {
      if (strcmp(node->name, path) == 0) {
        current = node;
        printf("Ok\n");
        return;
      }
      node = node->next;
    }
    printf("Directory not found\n");
  }
}

void execute_command(char *command) {
  if (strcmp(command, "ls") == 0) {
    list_directory(current);
  } else if (strncmp(command, "mkdir ", 6) == 0) {
    make_directory(command + 6);
  } else if (strncmp(command, "cd ", 3) == 0) {
    change_directory(command + 3);
  } else if (strcmp(command, "format") == 0) {
    format_disk();
  } else {
    printf("Unknown command\n");
  }
}

int main(int argc, char *argv[]) {
  char command[256];
  format_disk(); // Automatically format the disk on start

  while (1) {
    printf("/%s> ", current->name);
    if (fgets(command, sizeof(command), stdin) == NULL) {
      break;
    }
    command[strcspn(command, "\n")] = '\0'; // Remove the newline character
    execute_command(command);
  }

  return 0;
}
