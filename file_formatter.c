#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define BYTS_PER_SEC 512
const uint8_t NULL_BYTE = 0x0;

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
  uint8_t BS_BootCode[420];     // Boot code (420 chars set to 0x00)
  uint8_t Signature_word[2]; // 0x55 and 0xAA // Boot sector signature
  // all remainig bytes set to 0x00 (only for media where BPB_BytsPerSec > 512)
} __attribute__((packed)) BootSec_t;

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

void printBootSector(const BootSec_t *bootSector) {
    printf("BS_jmpBoot: %02X %02X %02X\n", bootSector->BS_jmpBoot[0], bootSector->BS_jmpBoot[1], bootSector->BS_jmpBoot[2]);
    printf("BS_OEMName: %s\n", bootSector->BS_OEMName);
    printf("BPB_BytsPerSec: %u\n", bootSector->BPB_BytsPerSec);
    printf("BPB_SecPerClus: %u\n", bootSector->BPB_SecPerClus);
    printf("BPB_RsvdSecCnt: %u\n", bootSector->BPB_RsvdSecCnt);
    printf("BPB_NumFATs: %u\n", bootSector->BPB_NumFATs);
    printf("BPB_RootEntCnt: %u\n", bootSector->BPB_RootEntCnt);
    printf("BPB_TotSec16: %u\n", bootSector->BPB_TotSec16);
    printf("BPB_Media: %02X\n", bootSector->BPB_Media);
    printf("BPB_FATSz16: %u\n", bootSector->BPB_FATSz16);
    printf("BPB_SecPerTrk: %u\n", bootSector->BPB_SecPerTrk);
    printf("BPB_NumHeads: %u\n", bootSector->BPB_NumHeads);
    printf("BPB_HiddSec: %u\n", bootSector->BPB_HiddSec);
    printf("BPB_TotSec32: %u\n", bootSector->BPB_TotSec32);
    printf("BPB_FATSz32: %u\n", bootSector->BPB_FATSz32);
    printf("BPB_ExtFlags: %u\n", bootSector->BPB_ExtFlags);
    printf("BPB_FSVer: %u\n", bootSector->BPB_FSVer);
    printf("BPB_RootClus: %u\n", bootSector->BPB_RootClus);
    printf("BPB_FSInfo: %u\n", bootSector->BPB_FSInfo);
    printf("BPB_BkBootSec: %u\n", bootSector->BPB_BkBootSec);
    printf("BS_DrvNum: %02X\n", bootSector->BS_DrvNum);
    printf("BS_Reserved1: %02X\n", bootSector->BS_Reserved1);
    printf("BS_BootSig: %02X\n", bootSector->BS_BootSig);
    printf("BS_VOlId: %u\n", bootSector->BS_VOlId);
    printf("BS_VOlLab: %s\n", bootSector->BS_VOlLab);
    printf("BS_FilSysType: %s\n", bootSector->BS_FilSysType);
    printf("Signature_word: %02x %02x\n", bootSector->Signature_word[0], bootSector->Signature_word[1]);
    // Print remaining fields if needed
}

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
int write_check(const void *data, uint16_t offset, size_t data_size, size_t count, FILE *file) {
  
  if (fseek(file, offset, SEEK_SET) == -1) {
    fprintf(stderr, "Failed to find position at %d to file %d: %s\n", offset, errno, strerror(errno));
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

  printf("Disk %s is open\n", filename);
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
  memcpy(boot_sec.BS_jmpBoot, "\xEB\x58\x90", 3);
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
  boot_sec.BPB_FATSz32 = (TmpVal1 + (TmpVal2 - 1)) / TmpVal2; // NOTE: too big, need to ~1
  
  boot_sec.BPB_ExtFlags = 0;
  boot_sec.BPB_FSVer = NULL_BYTE;
  boot_sec.BPB_RootClus = 2;
  boot_sec.BPB_FSInfo = 1;
  boot_sec.BPB_BkBootSec = 6;
  boot_sec.BS_DrvNum = 0x80;
  boot_sec.BS_Reserved1 = NULL_BYTE;
  boot_sec.BS_BootSig = 0x29;

  // generate volID from current date and time
  time_t cur_time;
  struct tm *gm_time;
  time(&cur_time);
  gm_time = gmtime(&cur_time);
  boot_sec.BS_VOlId = code_volID(gm_time);

  memcpy(boot_sec.BS_VOlLab, "NO NAME   ", 11);
  memcpy(boot_sec.BS_FilSysType, "FAT32   ", 8);
  memcpy(boot_sec.Signature_word, "\x55\xAA", 2);
  printBootSector(&boot_sec);
  // init FSInfo
  FSInfo_t fsinfo;
  fsinfo.FSI_Leadsig = 0x41615252;
  memset(&fsinfo.FSI_Reserved1, NULL_BYTE, sizeof(fsinfo.FSI_Reserved1));
  fsinfo.FSI_StructSig = 0x61417272;
  fsinfo.FSI_FreeCount = (uint32_t) -1;
  fsinfo.FSI_Nxt_Free = (uint32_t) -1;
  fsinfo.FSI_Nxt_Free = 0xAA550000;

  // write bootsec and FSInfo to file in two copy (og and backiup)
  for (size_t i = 0; i < 2; ++i) {
    uint16_t start = (i == 0) ? 0 : boot_sec.BPB_BkBootSec;
    if (write_check(&boot_sec, start * BYTS_PER_SEC, sizeof(boot_sec), 1, file) == -1) {
      return -1;
    }
    if (write_check(&fsinfo, (start + boot_sec.BPB_FSInfo) * BYTS_PER_SEC, sizeof(fsinfo), 1, file) == -1) {
      return -1;
    }
  }

  // write reserved FAT entries to file
  uint32_t fat[3] = {0x0FFFFFF8, 0x0FFFFFFF, 0x0FFFFFFF};

  for (size_t i = 0; i < boot_sec.BPB_NumFATs; ++i) {
    uint16_t start = boot_sec.BPB_RsvdSecCnt + (i + boot_sec.BPB_FATSz32);
    if (write_check(&fat, start, sizeof(fat), 1, file) == -1) {
      return -1;
    }
  }

  //write NULL_BYTE to end of file
  if (write_check(&NULL_BYTE, disksize - 1, sizeof(NULL_BYTE), 1, file) == -1) {
    return -1;
  }

  fclose(file);
  printf("Disk with id %u was formatted\n", boot_sec.BS_VOlId);

  return 0;
}

int main(int argc, char *argv[]) {

  return format_disk(argv[1]);

}
