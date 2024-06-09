#ifndef UTILITY_H
#define UTILITY_H

#include <stdint.h>
#include <stdio.h>

#include "bootsec.h"

void read_sector(FILE *disk, uint32_t sector, uint8_t *buffer,
                 uint16_t sector_size);
void write_sector(FILE *disk, uint32_t sector, const uint8_t *buffer,
                  uint16_t sector_size);
uint32_t get_next_cluster(FILE *disk, uint32_t cluster, uint16_t sector_size,
                          uint16_t rsrvd_sec);
uint32_t get_free_cluster(FILE *disk, BootSec_t *boot_sec);
void update_fat(FILE *disk, uint32_t cluster, uint32_t value,
                uint16_t sector_size, uint16_t rsrvd_sec);
void clear_cluster(FILE *disk, uint32_t cluster, BootSec_t *boot_sec);
int create_lfn_entries(const char *lfn, size_t lfn_len, uint8_t *sector_buffer,
                       uint16_t sector_size, char *short_name, uint8_t *nt_res,
                       void (*generate_short_name)(const char *, char *, uint8_t *));
void generate_lfn_short_name(const char *lfn, char *short_name, uint8_t *nt_res,
                             void (*generate_short_name)(const char *, char *, uint8_t *));
void get_fat_time_date(uint16_t *fat_date, uint16_t *fat_time,
                       uint8_t *fat_time_tenth);
#endif // UTILITY_H
