#include <stdio.h>

#include "bootsec.h"

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
