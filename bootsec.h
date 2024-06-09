#ifndef BOOT_SECTOR_H
#define BOOT_SECTOR_H

#include <stdint.h>
#include <stdio.h>

#define FAT_ELEM_SIZE  4 // 4 Bytes in uint32_t
typedef struct BootSec {
  uint8_t BS_jmpBoot[3];     // JMP instruction
  uint8_t BS_OEMName[8];     // OEM name
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
  uint8_t BS_VOlLab[11];     // Volume label
  uint8_t BS_FilSysType[8];  // File system type
  uint8_t BS_BootCode[420];  // Boot code (420 chars set to 0x00)
  uint8_t Signature_word[2]; // 0x55 and 0xAA // Boot sector signature
  // all remainig bytes set to 0x00 (only for media where BPB_BytsPerSec > 512)
} __attribute__((packed, aligned(1))) BootSec_t;

int read_boot_sector(FILE *disk, BootSec_t *boot_sec);
#endif // BOOT_SECTOR_H
