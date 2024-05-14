#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "bootsec.h"
typedef struct fat_extBS_32 {
  // extended fat32 stuff
  uint32_t table_size_32;
  uint16_t extended_flags;
  uint16_t fat_version;
  uint32_t root_cluster;
  uint16_t fat_info;
  uint16_t backup_BS_sector;
  uint8_t reserved_0[12];
  uint8_t drive_number;
  uint8_t reserved_1;
  uint8_t boot_signature;
  uint32_t volume_id;
  uint8_t volume_label[11];
  uint8_t fat_type_label[8];

} __attribute__((packed)) fat_extBS_32_t;

typedef struct fat_extBS_16 {
  // extended fat12 and fat16 stuff
  uint8_t bios_drive_num;
  uint8_t reserved1;
  uint8_t boot_signature;
  uint32_t volume_id;
  uint8_t volume_label[11];
  uint8_t fat_type_label[8];

} __attribute__((packed)) fat_extBS_16_t;

typedef struct fat_BS {
  uint8_t bootjmp[3];
  uint8_t oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sector_count;
  uint8_t table_count;
  uint16_t root_entry_count;
  uint16_t total_sectors_16;
  uint8_t media_type;
  uint16_t table_size_16;
  uint16_t sectors_per_track;
  uint16_t head_side_count;
  uint32_t hidden_sector_count;
  uint32_t total_sectors_32;

  // this will be cast to it's specific type once the driver actually knows what
  // type of FAT this is.
  uint8_t extended_section[54];

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

  fat_BS_t *fat_boot = malloc(sizeof(fat_BS_t));
  memcpy(fat_boot, buffer, sizeof(fat_BS_t));
  uint8_t isFat32 = 0;
  uint8_t isFat16 = 0;
  fat_extBS_32_t *fat_boot_ext32 = NULL;
  fat_extBS_16_t *fat_boot_ext16 = NULL;

  if (fat_boot->table_size_16 == 0 && fat_boot->total_sectors_32 != 0) {
    isFat32 = 1;
    fat_boot_ext32 =
        (fat_extBS_32_t *)fat_boot->extended_section;
  } else {
    isFat16 = 1;
    fat_boot_ext16 =
        (fat_extBS_16_t *)fat_boot->extended_section;
  }
  free(buffer);

  uint32_t tot_sectors =
      (isFat16 == 0) ? fat_boot->total_sectors_32 : fat_boot->total_sectors_16;
  uint32_t fat_size =
      (isFat16 == 0) ? fat_boot_ext32->table_size_32 : fat_boot->table_size_16;
  uint32_t root_dir_sec =
      ((fat_boot->root_entry_count * 32) + (fat_boot->bytes_per_sector - 1)) /
      fat_boot->bytes_per_sector;
  uint32_t first_data_sec = fat_boot->reserved_sector_count +
                            (fat_boot->table_count * fat_size) + root_dir_sec;
  uint32_t first_fat_sec = fat_boot->reserved_sector_count;
  uint32_t data_sec =
      tot_sectors - (fat_boot->reserved_sector_count +
                     (fat_boot->table_count * fat_size) + root_dir_sec);
  uint32_t tot_clus = data_sec / fat_boot->sectors_per_cluster;
  if (tot_clus < 4085) {
    puts("FAT12");
  } else if (tot_clus < 65525) {
    puts("FAT16");
  } else {
    puts("FAT32");
  }

  printf("sectors %d\n", tot_sectors);
  printf("fat size %d \n", fat_size);
  printf("root_dir %d\n", root_dir_sec);
  printf("first data sec %d\n", first_data_sec);
  printf("tot data sec %d\n", data_sec);
  printf("total clusters %d\n\n", tot_clus);

  printf("bootjmp: %02x %02x %02x\n", fat_boot->bootjmp[0],
         fat_boot->bootjmp[1], fat_boot->bootjmp[2]);
  printf("oem_name: %.8s\n", fat_boot->oem_name);
  printf("bytes_per_sector: %u\n", fat_boot->bytes_per_sector);
  printf("sectors_per_cluster: %u\n", fat_boot->sectors_per_cluster);
  printf("reserved_sector_count: %u\n", fat_boot->reserved_sector_count);
  printf("table_count: %u\n", fat_boot->table_count);
  printf("root_entry_count: %u\n", fat_boot->root_entry_count);
  printf("total_sectors_16: %u\n", fat_boot->total_sectors_16);
  printf("media_type: %u\n", fat_boot->media_type);
  printf("table_size_16: %u\n", fat_boot->table_size_16);
  printf("sectors_per_track: %u\n", fat_boot->sectors_per_track);
  printf("head_side_count: %u\n", fat_boot->head_side_count);
  printf("hidden_sector_count: %u\n", fat_boot->hidden_sector_count);
  printf("total_sectors_32: %u\n", fat_boot->total_sectors_32);
  puts("");

  if (isFat32 == 1) {
    printf("table_size_32: %u\n", fat_boot_ext32->table_size_32);
    printf("extended_flags: %u\n", fat_boot_ext32->extended_flags);
    printf("fat_version: %u\n", fat_boot_ext32->fat_version);
    printf("root_cluster: %u\n", fat_boot_ext32->root_cluster);
    printf("fat_info: %u\n", fat_boot_ext32->fat_info);
    printf("backup_BS_sector: %u\n", fat_boot_ext32->backup_BS_sector);
    // Reserved fields can be printed if necessary
    printf("drive_number: %u\n", fat_boot_ext32->drive_number);
    printf("boot_signature: %u\n", fat_boot_ext32->boot_signature);
    printf("volume_id: %u\n", fat_boot_ext32->volume_id);
    printf("volume_label: %.11s\n", fat_boot_ext32->volume_label);
    printf("fat_type_label: %.8s\n", fat_boot_ext32->fat_type_label);
  }
  if (isFat16 == 1) {
    printf("bios_drive_num: %u\n", fat_boot_ext16->bios_drive_num);
    printf("reserved1: %u\n", fat_boot_ext16->reserved1);
    printf("boot_signature: %u\n", fat_boot_ext16->boot_signature);
    printf("volume_id: %u\n", fat_boot_ext16->volume_id);
    printf("volume_label: %.11s\n", fat_boot_ext16->volume_label);
    printf("fat_type_label: %.8s\n", fat_boot_ext16->fat_type_label);
  }

  free(fat_boot);

  return 0;
}