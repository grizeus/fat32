#ifndef DIR_STR_H
#define DIR_STR_H

#include <stdint.h>
#include <stdio.h>

#include "bootsec.h"

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_LFN 0x0F
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define LAST_LONG_ENTRY 0x40
#define EOC 0x0FFFFFF8 // end of cluster
#define NT_RES_LOWER_CASE_BASE 0x08
#define NT_RES_LOWER_CASE_EXT 0x10
#define MAX_MAME_LEN 255

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

typedef struct LFNStr {

  uint8_t LDIR_Ord;
  uint16_t LDIR_Name1[5];
  uint8_t LDIR_Attr;
  uint8_t LDIR_Type;
  uint8_t LDIR_Chksum;
  uint16_t LDIR_Name2[6];
  uint16_t LDIR_FstClusLO;
  uint16_t LDIR_Name3[2];
} __attribute__((packed)) LFNStr_t;

typedef struct {

  char name[MAX_MAME_LEN];
  uint32_t cluster;
  uint32_t size;
  uint16_t date;
  uint16_t time;
  uint8_t attr;
  char ext[4];
} EntrSt_t;

int read_dir_entries(FILE* disk, BootSec_t* boot_sec, uint32_t cluster, EntrSt_t** entries,
                     uint32_t* entry_count);

#endif // DDIR_STR_H
