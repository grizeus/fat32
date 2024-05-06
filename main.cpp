#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

constexpr uint32_t BYTES_PER_SECTOR = 512;
constexpr uint32_t SECTORS_PER_CLUSTER = 8;
constexpr uint32_t RESERVED_SECTORS = 32;
constexpr uint32_t FAT_COPIES = 2;
constexpr uint32_t ROOT_ENTRIES = 512;

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

struct FatEntry {
  uint32_t value;
};

int main(int argc, char **argv) {

  const uint64_t file_size = 1024ULL * 1024 * 20; // 20 MB

  const char *file_name = argv[1];

  std::ofstream file(file_name, std::ios::binary | std::ios::trunc);
  if (!file) {
    std::cerr << "Failed to create file " << file_name << "\n";
    return 1;
  }

  BootSector boot_sector;
  std::memset(&boot_sector, 0, sizeof(BootSector));

  std::memcpy(boot_sector.jump, "\xEB\x58\x90", 3);
  std::memcpy(boot_sector.oem_name, "MYOSNAME", 8);
  std::memcpy(boot_sector.system_id, "FAT32   ", 8);
  std::memcpy(boot_sector.volume_label, "MYVOLUME  ", 11);
  boot_sector.total_sectors = file_size / BYTES_PER_SECTOR;
  boot_sector.bytes_per_sector = BYTES_PER_SECTOR;
  boot_sector.sectors_per_cluster = SECTORS_PER_CLUSTER;
  boot_sector.reserved_sectors = RESERVED_SECTORS;
  boot_sector.fat_copies = FAT_COPIES;
  boot_sector.root_entries = ROOT_ENTRIES;
  boot_sector.root_cluster = 2;
  boot_sector.sectors_per_cluster =
      (boot_sector.total_sectors / SECTORS_PER_CLUSTER) / 128;

  const uint32_t data_sectors = boot_sector.total_sectors - RESERVED_SECTORS -
                                (ROOT_ENTRIES * 32 / BYTES_PER_SECTOR) -
                                (FAT_COPIES * boot_sector.total_sectors / 200);
  const uint32_t total_clusters = data_sectors / SECTORS_PER_CLUSTER;

  file.write(reinterpret_cast<char *>(&boot_sector), sizeof(BootSector));

  std::vector<FatEntry> fat(total_clusters + 2);
  fat[0].value = 0x0FFFFFF8;
  fat[1].value = 0x0FFFFFFF;
  for (uint32_t i = 2; i < total_clusters + 2; ++i) {
    fat[i].value = 0;
  }

  for (uint32_t i = 0; i < FAT_COPIES; ++i) {
    file.seekp(RESERVED_SECTORS * BYTES_PER_SECTOR +
               i * (total_clusters + 2) * sizeof(FatEntry));
    file.write(reinterpret_cast<char *>(fat.data()),
               (total_clusters + 2) * sizeof(FatEntry));
  }

  file.seekp(RESERVED_SECTORS * BYTES_PER_SECTOR +
             FAT_COPIES * (total_clusters + 2) * sizeof(FatEntry));
  std::vector<uint8_t> root_dir(ROOT_ENTRIES * 32, 0);
  file.write(reinterpret_cast<char *>(root_dir.data()), root_dir.size());

  file.close();

  std::cout << "FAT32 file system created: " << file_name << "\n";

  return 0;
}
