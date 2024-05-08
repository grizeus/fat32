#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct BootSec {
  uint8_t BS_jmpBoot[3];     // JMP instruction
  char BS_OEMName[8];        // OEM name
  uint16_t BPB_BytsPerSec;   // Bytes per sector
  uint8_t BPB_SecPerClus;    // Sectors per cluster
  uint16_t BPB_RsvdSecCnt;   // Reserved sectors count
  uint8_t BPB_NumFATs;       // Number of FATs
  uint16_t BPB_RootEntCnt;   // Number of root directory entries
  uint16_t BPB_TotSec16;     // Total sectors (16-bit)
  uint8_t BPB_Media;         // Media descriptor
  uint16_t BPB_FATSz16;      // Sectors per FAT (16-bit)
  uint16_t BPB_SecPerTrk;    // Sectors per track
  uint16_t BPB_NumHeads;     // Number of heads
  uint32_t BPB_HiddSec;      // Hidden sectors count
  uint32_t BPB_TotSec32;     // Total sectors (32-bit)
  uint32_t BPB_FATSz32;      // Sectors per FAT (32-bit)
  uint16_t BPB_ExtFlags;     // Flags
  uint16_t BPB_FSVer;        // File system version
  uint32_t BPB_RootClus;     // Root directory cluster
  uint16_t BPB_FSInfo;       // File system information sector
  uint16_t BPB_BkBootSec;    // Backup boot sector location
  uint8_t BPB_Reserved[12];  // Reserved
  uint8_t BS_DrvNum;         // Drive number
  uint8_t BS_Reserved1;      // Reserved
  uint8_t BS_BootSig;        // Extended boot signature
  uint32_t BS_VOlId;         // Volume serial number
  char BS_VOlLab[11];        // Volume label
  char BS_FilSysType[8];     // File system type
  char BS_BootCode[420];     // Boot code (420 chars set to 0x00)
  uint8_t Signature_word[2]; // 0x55 and 0xAA // Boot sector signature
  // all remainig bytes set to 0x00 (only for media where BPB_BytsPerSec > 512)
}__attribute__((packed)) BootSec_t;

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
int write_check(const void *data, size_t size, size_t count, FILE *file) {

  size_t res = fwrite(data, size, count, file);
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
  uint32_t disksize = ftell(file);
  printf("Disksize %d\n", disksize);
  fclose(file);

  file = fopen(filename, "wb");
  // init boot sector
  BootSec_t boot_sec;
  memcpy(boot_sec.BS_jmpBoot, "\xEB\x7F\x90", 3);
  memcpy(boot_sec.BS_OEMName, "MYOSNAME", 8);
  boot_sec.BPB_BytsPerSec = 512;
  boot_sec.BPB_SecPerClus = 8;
  boot_sec.BPB_RsvdSecCnt = 4096; // sec size * sec per cluster
  boot_sec.BPB_NumFATs = 2;
  boot_sec.BPB_RootEntCnt = 0;
  boot_sec.BPB_TotSec16 = 0;
  boot_sec.BPB_Media = 0xF8;
  boot_sec.BPB_FATSz16 = 0;
  boot_sec.BPB_SecPerTrk = 0x3F;
  boot_sec.BPB_NumHeads = 2;
  boot_sec.BPB_HiddSec = 0;
  boot_sec.BPB_TotSec32 = disksize / boot_sec.BPB_BytsPerSec;

  // calculate FAT size
  uint32_t RootDirSectors =
      ((boot_sec.BPB_RootEntCnt * 32) + (boot_sec.BPB_BytsPerSec - 1)) /
      boot_sec.BPB_BytsPerSec;
  uint32_t TmpVal1 = disksize - (boot_sec.BPB_RsvdSecCnt + RootDirSectors);
  uint32_t TmpVal2 =
      ((256 * boot_sec.BPB_SecPerClus) + boot_sec.BPB_NumFATs) / 2;
  boot_sec.BPB_FATSz32 = (TmpVal1 + (TmpVal2 - 1)) / TmpVal2;

  boot_sec.BPB_ExtFlags = 0;
  boot_sec.BPB_FSVer = 0x0;
  boot_sec.BPB_RootClus = 2;
  boot_sec.BPB_FSInfo = 1;
  boot_sec.BPB_BkBootSec = 6;
  memset(boot_sec.BPB_Reserved, 0x0, 12);
  boot_sec.BS_DrvNum = 0x80;
  boot_sec.BS_Reserved1 = 0x0;
  boot_sec.BS_BootSig = 0x29;

  // generate volID from current date and time
  time_t cur_time;
  struct tm *gm_time;
  time(&cur_time);
  gm_time = gmtime(&cur_time);
  boot_sec.BS_VOlId = code_volID(gm_time);

  memcpy(boot_sec.BS_VOlLab, "NO NAME    ", 11);
  memcpy(boot_sec.BS_FilSysType, "FAT32   ", 8);
  memset(boot_sec.BS_BootCode, 0x00, 420);
  memcpy(boot_sec.Signature_word, "\x55\xAA", 2);
  //write bootsec to file
  if (write_check(&boot_sec, sizeof(boot_sec), 1, file) == -1) {
    return -1;
  }

  //write reserved FAT entries to file
  uint32_t fat[2] = {0x0FFFFFF8, 0x0FFFFFFF};
  fseek(file, boot_sec.BPB_RsvdSecCnt, SEEK_SET);
  if (write_check(&fat, sizeof(fat), 1, file) == -1) {
    return -1;
  }

  uint32_t cur = ftell(file);
  fseek(file, disksize - cur - 1, SEEK_CUR);
  if (write_check(&boot_sec.BS_Reserved1, sizeof(boot_sec.BS_Reserved1), 1, file) == -1) {
    return -1;
  }
  fclose(file);
  printf("Disk with id %u was formatted\n", boot_sec.BS_VOlId);

  return 0;
}

int main(int argc, char *argv[]) {
  return format_disk(argv[1]);
}
