#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "bootsec.h"
typedef struct fat_extBS_32 {
  // extended fat32 stuff
  unsigned int table_size_32;
  unsigned short extended_flags;
  unsigned short fat_version;
  unsigned int root_cluster;
  unsigned short fat_info;
  unsigned short backup_BS_sector;
  unsigned char reserved_0[12];
  unsigned char drive_number;
  unsigned char reserved_1;
  unsigned char boot_signature;
  unsigned int volume_id;
  unsigned char volume_label[11];
  unsigned char fat_type_label[8];

} __attribute__((packed)) fat_extBS_32_t;

typedef struct fat_extBS_16 {
  // extended fat12 and fat16 stuff
  unsigned char bios_drive_num;
  unsigned char reserved1;
  unsigned char boot_signature;
  unsigned int volume_id;
  unsigned char volume_label[11];
  unsigned char fat_type_label[8];

} __attribute__((packed)) fat_extBS_16_t;

typedef struct fat_BS {
  unsigned char bootjmp[3];
  unsigned char oem_name[8];
  unsigned short bytes_per_sector;
  unsigned char sectors_per_cluster;
  unsigned short reserved_sector_count;
  unsigned char table_count;
  unsigned short root_entry_count;
  unsigned short total_sectors_16;
  unsigned char media_type;
  unsigned short table_size_16;
  unsigned short sectors_per_track;
  unsigned short head_side_count;
  unsigned int hidden_sector_count;
  unsigned int total_sectors_32;

  // this will be cast to it's specific type once the driver actually knows what
  // type of FAT this is.
  unsigned char extended_section[54];

} __attribute__((packed)) fat_BS_t;

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

  const size_t bytes_to_read = sizeof(fat_BS_t);
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

  fat_BS_t boot_sec;
  memcpy(&boot_sec, buffer, sizeof(fat_BS_t));
  fat_extBS_32_t *fat_boot_ext32 = (fat_extBS_32_t *)boot_sec.extended_section;
  free(buffer);

  uint32_t tot_sectors = (boot_sec.total_sectors_16 == 0)
                             ? boot_sec.total_sectors_32
                             : boot_sec.total_sectors_16;
  uint32_t fat_size = (boot_sec.table_size_16 == 0)
                          ? fat_boot_ext32->table_size_32
                          : boot_sec.table_size_16;
  uint32_t root_dir_sec =
      ((boot_sec.root_entry_count * 32) + (boot_sec.bytes_per_sector - 1)) /
      boot_sec.bytes_per_sector;
  uint32_t first_data_sec = boot_sec.reserved_sector_count +
                            (boot_sec.table_count * fat_size) + root_dir_sec;
  uint32_t first_fat_sec = boot_sec.reserved_sector_count;
  uint32_t data_sec =
      tot_sectors - (boot_sec.reserved_sector_count +
                     (boot_sec.table_count * fat_size) + root_dir_sec);
  uint32_t tot_clus = data_sec / boot_sec.sectors_per_cluster;
  if (tot_clus < 4085) {
    puts("FAT12\n");
  } else if (tot_clus < 65525) {
    puts("FAT16\n");
  } else {
    puts("FAT32\n");
  }

  printf("sectors %d fat size %d root_dir %d\nfirst data %d total data sectors "
         "%d sec per clus %d total clusters %d\n",
         tot_sectors, fat_size, root_dir_sec, first_data_sec, data_sec,
         boot_sec.sectors_per_cluster, tot_clus);
  // printBootSector(&boot_sec);

  return 0;
}
