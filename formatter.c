#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bootsec.h"

#define BYTS_PER_SEC 512
const uint8_t NULL_BYTE = 0x0;

typedef struct FSInfo {
  uint32_t FSI_Leadsig;
  uint8_t FSI_Reserved1[480];
  uint32_t FSI_StructSig;
  uint32_t FSI_FreeCount;
  uint32_t FSI_Nxt_Free;
  uint8_t FSI_Reserved2[12];
  uint32_t FSI_TrailSig;
} __attribute__((packed)) FSInfo_t;

typedef struct DIRStr {
  uint8_t DIR_Name[11];
  uint8_t DIR_Attr;
  uint8_t DIR_NTRes;
  uint8_t DIR_CrtTimeTenth;
  uint16_t DIR_CrtTime;
  uint16_t DIR_CrtDate;
  uint16_t DIR_LstAccDate;
  uint16_t DIR_FstClusHI;
  uint16_t DIR_WrtTime;
  uint16_t DIR_WrtDate;
  uint16_t DIR_FstClusLO;
  uint32_t DIR_FileSize;
} __attribute__((packed)) DIRStr_t;

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
  printf("Disk %s is open\n", filename);
  fseek(file, 0, SEEK_END);
  const uint32_t disksize = ftell(file);
  printf("Disksize %d\n", disksize);
  fclose(file);

  file = fopen(filename, "wb");
  // init boot sector
  BootSec_t *boot_sec = (BootSec_t *)malloc(sizeof(BootSec_t));
  memcpy(boot_sec->BS_jmpBoot, "\xEB\xF8\x90", 3);
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
  boot_sec->BPB_TotSec32 = disksize / boot_sec->BPB_BytsPerSec;

  // calculate FAT size
  // FIX: this need to repair
  uint32_t RootDirSectors =
      ((boot_sec->BPB_RootEntCnt * 32) + (boot_sec->BPB_BytsPerSec - 1)) /
      boot_sec->BPB_BytsPerSec;
  uint32_t TmpVal1 = disksize - (boot_sec->BPB_RsvdSecCnt + RootDirSectors);
  uint32_t TmpVal2 =
      ((256 * boot_sec->BPB_SecPerClus) + boot_sec->BPB_NumFATs) / 2;
  printf("rootdir %d tm1 %d tmp2 %d\n", RootDirSectors, TmpVal1, TmpVal2);
  boot_sec->BPB_FATSz32 =
      (TmpVal1 + (TmpVal2 - 1)) / TmpVal2; // NOTE: too big, need to ~1
  boot_sec->BPB_ExtFlags = 0;
  boot_sec->BPB_FSVer = NULL_BYTE;
  boot_sec->BPB_RootClus = 2;
  boot_sec->BPB_FSInfo = 1;
  boot_sec->BPB_BkBootSec = 6;
  boot_sec->BS_DrvNum = 0x80;
  boot_sec->BS_Reserved1 = NULL_BYTE;
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
  printBootSector(boot_sec);

  // init FSInfo
  FSInfo_t *fsinfo = (FSInfo_t *)malloc(sizeof(FSInfo_t));
  fsinfo->FSI_Leadsig = 0x41615252;
  memset(fsinfo->FSI_Reserved1, NULL_BYTE, sizeof(fsinfo->FSI_Reserved1));
  fsinfo->FSI_StructSig = 0x61417272;
  fsinfo->FSI_FreeCount = 0xFFFFFFFF;
  fsinfo->FSI_Nxt_Free = 2;
  fsinfo->FSI_Nxt_Free = 0xAA550000;

  // write bootsec and FSInfo to file in two copy (og and backiup)
  for (size_t i = 0; i < 2; ++i) {
    uint16_t start = (i == 0) ? 0 : boot_sec->BPB_BkBootSec;
    if (write_check(boot_sec, start * BYTS_PER_SEC, sizeof(*boot_sec), 1,
                    file) == -1) {
      return -1;
    }
    if (write_check(fsinfo, (start + boot_sec->BPB_FSInfo) * BYTS_PER_SEC,
                    sizeof(*fsinfo), 1, file) == -1) {
      return -1;
    }
  }

  // write reserved FAT entries to file
  uint32_t fat[3] = {0x0FFFFFF8, 0x0FFFFFFF, 0x0FFFFFFF};

  for (size_t i = 0; i < boot_sec->BPB_NumFATs; ++i) {
    uint16_t start = boot_sec->BPB_RsvdSecCnt + (i + boot_sec->BPB_FATSz32);
    if (write_check(&fat, start, sizeof(fat), 1, file) == -1) {
      return -1;
    }
  }

  uint32_t current = ftell(file);
  printf("Current posafter write %d\n", current);
  // write NULL_BYTE to end of file
  if (write_check(&NULL_BYTE, disksize - 1, sizeof(NULL_BYTE), 1, file) == -1) {
    return -1;
  }

  fclose(file);

  file = fopen(filename, "r");
  if (!file) {
    fprintf(stderr, "File open failed %d: %s.\n", errno, strerror(errno));
    fclose(file);
    return -1;
  }

  fseek(file, 0, SEEK_END);
  uint32_t disksize1 = ftell(file);
  printf("Disksize after reopen %d\n", disksize1);
  fclose(file);
  printf("Disk with id %u was formatted\n", boot_sec->BS_VOlId);
  free(boot_sec);
  free(fsinfo);
  return 0;
}

int main(int argc, char *argv[]) {
  return format_disk(argv[1]);
}
