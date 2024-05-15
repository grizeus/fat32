#ifndef FS_INFO_H
#define FS_INFO_H

#include <stdint.h>

typedef struct FSInfo {
  uint32_t FSI_Leadsig;
  uint8_t FSI_Reserved1[480];
  uint32_t FSI_StructSig;
  uint32_t FSI_FreeCount;
  uint32_t FSI_Nxt_Free;
  uint8_t FSI_Reserved2[12];
  uint32_t FSI_TrailSig;
} __attribute__((packed)) FSInfo_t;

#endif // FS_INFO_H
