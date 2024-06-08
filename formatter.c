#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bootsec.h"
#include "fsinfo.h"

#define BYTS_PER_SEC 512
#define NUM_FATS 2
const uint8_t NULL_BYTE = 0x0;

static uint32_t code_volID(struct tm *time_info) {

  uint32_t VolID = 0;

  VolID |= (time_info->tm_sec & 0x1F);
  VolID |= ((time_info->tm_min & 0x3F) << 5);
  VolID |= ((time_info->tm_hour & 0x1F) << 11);
  VolID |= (((time_info->tm_mday - 1) & 0x1F) << 16);
  VolID |= (((time_info->tm_mon + 1) & 0xF) << 21);
  VolID |= (((time_info->tm_year - 80) & 0x7F) << 25);

  return VolID;
}

uint32_t get_FATSz32(BootSec_t *boot_sec) {

  uint32_t fat_size32;

  uint32_t TmpVal1 =
      FAT_ELEM_SIZE * (boot_sec->BPB_TotSec32 - boot_sec->BPB_RsvdSecCnt);
  uint32_t TmpVal2 =
      (boot_sec->BPB_SecPerClus * BYTS_PER_SEC) + (FAT_ELEM_SIZE * NUM_FATS);

  fat_size32 = TmpVal1 / TmpVal2;
  fat_size32 += 1; // round up

  return fat_size32;
}
// write to file with error checking
int write_check(const void *data, uint32_t offset, size_t data_size,
                size_t count, FILE *file) {

  if (fseek(file, offset, SEEK_SET) == -1) {
    fprintf(stderr, "Failed to find position at %d to file %d: %s\n", offset,
            errno, strerror(errno));
    return -1;
  }
  size_t res = fwrite(data, data_size, count, file);
  if (res != count) {
    fprintf(stderr, "Failed to write to file %d: %s\n", errno, strerror(errno));
    return -1;
  }
  return 0;
}

int format_disk(const char *filename) {

  FILE *file = fopen(filename, "r");
  if (!file) {
    fprintf(stderr, "File open failed %d: %s.\n", errno, strerror(errno));
    fclose(file);
    return -1;
  }

  fseek(file, 0, SEEK_END);
  const uint32_t disksize = ftell(file);
  fclose(file);

  file = fopen(filename, "wb");
  // init boot sector
  BootSec_t *boot_sec = (BootSec_t *)malloc(sizeof(BootSec_t));
  if (!boot_sec) {
    fprintf(stderr, "Failed to allocate memory for BootSector.\n");
    fclose(file);
    return -1;
  }

  memcpy(boot_sec->BS_jmpBoot, "\xEB\x58\x90", 3);
  memcpy(boot_sec->BS_OEMName, "MYOSNAME", 8);
  boot_sec->BPB_BytsPerSec = 512;
  boot_sec->BPB_SecPerClus = 1;
  boot_sec->BPB_RsvdSecCnt = 32; // "typical" value
  boot_sec->BPB_NumFATs = 2;
  boot_sec->BPB_RootEntCnt = 0;
  boot_sec->BPB_TotSec16 = 0;
  boot_sec->BPB_Media = 0xF8;
  boot_sec->BPB_FATSz16 = 0;
  boot_sec->BPB_SecPerTrk = 0x3F;
  boot_sec->BPB_NumHeads = 16;
  boot_sec->BPB_HiddSec = 0;
  boot_sec->BPB_TotSec32 = disksize / BYTS_PER_SEC;

  uint32_t fat_size = get_FATSz32(boot_sec);
  boot_sec->BPB_FATSz32 = fat_size;
  boot_sec->BPB_ExtFlags = 0;
  boot_sec->BPB_RootClus = 2;
  boot_sec->BPB_FSInfo = 1;
  boot_sec->BPB_BkBootSec = 6;
  boot_sec->BS_DrvNum = 0x80;
  boot_sec->BS_BootSig = 0x29;

  // generate volID from current date and time
  time_t cur_time;
  struct tm *gm_time;
  time(&cur_time);
  gm_time = gmtime(&cur_time);
  boot_sec->BS_VOlId = code_volID(gm_time);

  memcpy(boot_sec->BS_VOlLab, "NO NAME    ", 11);
  memcpy(boot_sec->BS_FilSysType, "FAT32   ", 8);
  memcpy(boot_sec->Signature_word, "\x55\xAA", 2);

  // init FSInfo
  FSInfo_t *fsinfo = (FSInfo_t *)malloc(sizeof(FSInfo_t));
  if (!fsinfo) {
    fprintf(stderr, "Failed to allocate memory for fsinfo.\n");
    free(boot_sec);
    fclose(file);
    return -1;
  }

  fsinfo->FSI_Leadsig = 0x41615252;
  fsinfo->FSI_StructSig = 0x61417272;
  fsinfo->FSI_FreeCount = (uint32_t)-1; // 0xFFFFFFFF
  fsinfo->FSI_Nxt_Free = (uint32_t)-1;  // 0xFFFFFFFF
  fsinfo->FSI_Nxt_Free = 0xAA550000;

  // init reserved FAT entries
  uint32_t *rsrvd_fat_sec = (uint32_t *)malloc(BYTS_PER_SEC);
  if (!rsrvd_fat_sec) {
    fprintf(stderr, "Failed to allocate memory for FAT.\n");
    free(boot_sec);
    free(fsinfo);
    fclose(file);
    return -1;
  }
  memset(rsrvd_fat_sec, 0, BYTS_PER_SEC);

  rsrvd_fat_sec[0] = 0x0FFFFFF8;
  rsrvd_fat_sec[1] = 0x0FFFFFFF;
  rsrvd_fat_sec[2] = 0x0FFFFFFF;

  uint32_t user_area =
      boot_sec->BPB_TotSec32 - boot_sec->BPB_RsvdSecCnt - (NUM_FATS * fat_size);
  uint32_t cluster_count = user_area / boot_sec->BPB_SecPerClus;

  fsinfo->FSI_FreeCount = (user_area / boot_sec->BPB_SecPerClus) - 1;
  fsinfo->FSI_Nxt_Free = 3; // first really free sector after reserved

  // write bootsec and FSInfo to file in two copy (original and backup)
  for (size_t i = 0; i < 2; ++i) {
    uint16_t start = (i == 0) ? 0 : boot_sec->BPB_BkBootSec;
    if (write_check(boot_sec, start * BYTS_PER_SEC, BYTS_PER_SEC, 1, file) ==
        -1) {
      return -1;
    }
    if (write_check(fsinfo, ((start + 1) + boot_sec->BPB_FSInfo) * BYTS_PER_SEC,
                    BYTS_PER_SEC, 1, file) == -1) {
      return -1;
    }
  }

  for (size_t i = 0; i < NUM_FATS; ++i) {
    uint16_t start = boot_sec->BPB_RsvdSecCnt + (i * fat_size);
    if (write_check(rsrvd_fat_sec, start * BYTS_PER_SEC, BYTS_PER_SEC, 1,
                    file) == -1) {
      free(boot_sec);
      free(fsinfo);
      free(rsrvd_fat_sec);
      fclose(file);
      return -1;
    }
  }

  // write NULL_BYTE to end of file
  if (write_check(&NULL_BYTE, disksize - 1, sizeof(NULL_BYTE), 1, file) == -1) {
    return -1;
  }

  fclose(file);
  printf("Disk was formatted\n");

  free(boot_sec);
  free(fsinfo);
  free(rsrvd_fat_sec);

  return 0;
}

int main(int argc, char *argv[]) {

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <disk_image>\n", argv[0]);
    return 1;
  }

  return format_disk(argv[1]);
}
