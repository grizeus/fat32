#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>

constexpr int SIGNATURE_OFFSET = 510;
constexpr uint16_t SIGNATURE_VALUE = 0xAA55;

struct BootSector {
  uint8_t volume_label[11];
  uint8_t oem_name[8];
  uint8_t system_id[8];
  uint8_t jump[3];
  uint32_t total_sectors;
  uint32_t sectors_per_fat;
  uint32_t root_cluster;
  uint32_t serial_number;
  uint16_t fsinfo_sector;
  uint16_t reserved_sectors;
  uint16_t bytes_per_sector;
  uint16_t root_entries;
  uint16_t backup_boot_sector;
  uint8_t sectors_per_cluster;
  uint8_t fat_copies;
  uint8_t drive_number;
  uint8_t extended_signature;
};

bool is_fat32(const char *file_name) {

  std::ifstream file(file_name, std::ios::binary);
  if (!file) {
    std::cerr << "Failed to open file " << file_name << "\n";
    return false;
  }

  BootSector boot_sector;
  file.seekg(0);
  file.read(reinterpret_cast<char *>(&boot_sector), sizeof(BootSector));

  file.seekg(SIGNATURE_OFFSET);
  uint16_t signature;
  file.read(reinterpret_cast<char *>(&signature), sizeof(signature));

  if (signature != SIGNATURE_VALUE) {
    std::cerr << "Invalid signature in Boot Sector.\n";
    return false;
  }

  if (boot_sector.bytes_per_sector != 512) {
    std::cerr << "Invalid bytes per sector.\n";
    return false;
  }

  if (boot_sector.sectors_per_cluster != 8) {
    std::cerr << "Invalid sectors per cluster.\n";
    return false;
  }

  if (boot_sector.reserved_sectors != 0) {
    std::cerr << "Invalid reserved sectors.\n";
    return false;
  }

  return true;
}

int main(int argc, char **argv) {

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <file_name>\n";
    return 1;
  }

  const char *file_name = argv[1];
  if (is_fat32(file_name)) {
    std::cout << "The file is FAT32 file system.\n";
  } else {
    std::cout << "The file is not FAT32 file system.\n";
  }

  return 0;
}
