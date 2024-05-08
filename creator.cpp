#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

// Define FAT32 boot sector structure
// NOTE: live it for now
#pragma pack(push, 1)
struct BootSector {
  uint8_t jmp_boot[3];
  char oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t fat_copies;
  uint16_t root_dir_entries;
  uint16_t total_sectors_FAT16;
  uint8_t media;
  uint16_t sectors_per_FAT16;
  uint16_t sectors_per_track;
  uint16_t num_heads;
  uint32_t hidden_sectors;
  uint32_t total_sectors_FAT32;
  uint32_t sectors_per_FAT32;
  uint16_t flags;
  uint16_t fs_version;
  uint32_t root_cluster;
  uint16_t fs_info;
  uint16_t backup_boot_sector;
  uint8_t reserved[12];
  uint8_t drive_number;
  uint8_t reserved1;
  uint8_t extended_signature;
  uint32_t volume_ID;
  char volume_label[11];
  char fs_type[8];
  char boot_code[420];
  uint16_t boot_signature;
};

struct BootSecMS {
  uint8_t BS_jmpBoot[3];    // JMP instruction
  char BS_OEMName[8];       // OEM name
  uint16_t BPB_BytsPerSec;  // Bytes per sector
  uint8_t BPB_SecPerClus;   // Sectors per cluster
  uint16_t BPB_RsvdSecCnt;  // Reserved sectors count
  uint8_t BPB_NumFATs;      // Number of FATs
  uint16_t BPB_RootEntCnt;  // Number of root directory entries
  uint16_t BPB_TotSec16;    // Total sectors (16-bit)
  uint8_t BPB_Media;        // Media descriptor
  uint16_t BPB_FATSz16;     // Sectors per FAT (16-bit)
  uint16_t BPB_SecPerTrk;   // Sectors per track
  uint16_t BPB_NumHeads;    // Number of heads
  uint32_t BPB_HiddSec;     // Hidden sectors count
  uint32_t BPB_TotSec32;    // Total sectors (32-bit)
  uint32_t BPB_FATSz32;     // Sectors per FAT (32-bit)
  uint16_t BPB_ExtFlags;    // Flags
  uint16_t BPB_FSVer;       // File system version
  uint32_t BPB_RootClus;    // Root directory cluster
  uint16_t BPB_FSInfo;      // File system information sector
  uint16_t BPB_BkBootSec;   // Backup boot sector location
  uint8_t BPB_Reserved[12]; // Reserved
  uint8_t BS_DrvNum;        // Drive number
  uint8_t BS_Reserved1;     // Reserved
  uint8_t BS_BootSig;       // Extended boot signature
  uint32_t BS_VOlId;        // Volume serial number
  char BS_VOlLab[11];       // Volume label
  char BS_FilSysType[8];    // File system type
  char BS_BootCode[420];    // Boot code (420 chars set to 0x00)
  uint16_t Signature_word;  // 0x55 and 0xAA // Boot sector signature
  // all remainig bytes set to 0x00 (only for media where BPB_BytsPerSec > 512)
};
#pragma pack(pop)

int main(int argc, char **argv) {

  if (argc != 3) {
    std::cerr << "Program usage is " << argv[0] << " <size>(in Mb) <name>\n";
    return 1;
  }
  // Define disk image file name
  int64_t size = std::atol(argv[1]);
  if (size < 1) {
    std::cerr << "Error: size must be greater than 1\n";
    return 1;
  }
  if (size >= 2'000'000) {
    std::cerr << "Error: size must be smaller than 2'000'000\n";
    return 1;
  }
  const char *disk_name = argv[2];

  const uint64_t disk_size = 1024ULL * 1024 * size;

  // Create a new file stream for writing the disk image
  std::ofstream disk_image(disk_name, std::ios::binary | std::ios::trunc);

  // Check if the file stream is open
  if (!disk_image.is_open()) {
    std::cerr << "Error: Unable to create FAT32 disk image\n";
    return 1; // Return error code
  }

  // Initialize boot sector structure
  BootSector boot_sector;
  std::memset(&boot_sector, 0, sizeof(BootSector));

  // Fill boot sector fields
  std::memcpy(boot_sector.jmp_boot, "\xEB\xFF\x90", 3);
  std::memcpy(boot_sector.oem_name, "MYOSNAME", 8);
  std::memcpy(boot_sector.fs_type, "FAT32   ", 8);
  std::memcpy(boot_sector.volume_label, "MYVOLUME  ", 11);
  boot_sector.bytes_per_sector = 512;
  boot_sector.sectors_per_cluster = 8;
  boot_sector.reserved_sectors = 32;
  boot_sector.fat_copies = 2;
  boot_sector.root_dir_entries = 512;
  boot_sector.total_sectors_FAT16 = 0;
  boot_sector.media = 0xF8;
  boot_sector.sectors_per_FAT16 = 0;
  boot_sector.sectors_per_track = 0x13;
  boot_sector.num_heads = 2;
  boot_sector.hidden_sectors = 0;
  boot_sector.total_sectors_FAT32 = disk_size / boot_sector.bytes_per_sector;
  boot_sector.sectors_per_FAT32 = 10;
  boot_sector.flags = 0;
  boot_sector.fs_version = 0;
  boot_sector.root_cluster = 2;
  boot_sector.fs_info = 1;
  boot_sector.backup_boot_sector = 6;
  boot_sector.drive_number = 0x80;
  boot_sector.extended_signature = 0x29;
  boot_sector.volume_ID = 12345678;
  boot_sector.boot_signature = 0xAA55;

  // Write boot sector to disk image
  disk_image.write(reinterpret_cast<char *>(&boot_sector), sizeof(BootSector));

  const uint32_t data_sectors =
      boot_sector.total_sectors_FAT32 - boot_sector.reserved_sectors -
      (boot_sector.root_dir_entries * 32 / boot_sector.bytes_per_sector) -
      (boot_sector.fat_copies * boot_sector.total_sectors_FAT32 / 200);
  const uint32_t total_clusters =
      data_sectors / boot_sector.sectors_per_cluster;

  std::vector<uint32_t> fat(total_clusters + 2);
  fat[0] = 0x0FFFFFF8;
  fat[1] = 0x0FFFFFFF;
  for (uint32_t i = 2; i < total_clusters + 2; ++i) {
    fat[i] = 0;
  }

  for (uint32_t i = 0; i < boot_sector.fat_copies; ++i) {
    disk_image.seekp(boot_sector.reserved_sectors *
                         boot_sector.bytes_per_sector +
                     i * (total_clusters + 2) * sizeof(uint32_t));
    disk_image.write(reinterpret_cast<char *>(fat.data()),
                     (total_clusters + 2) * sizeof(uint32_t));
  }

  disk_image.seekp(boot_sector.reserved_sectors * boot_sector.bytes_per_sector +
                   boot_sector.fat_copies * (total_clusters + 2) *
                       sizeof(uint32_t));
  std::vector<uint8_t> root_dir(boot_sector.root_dir_entries * 32, 0);
  disk_image.write(reinterpret_cast<char *>(root_dir.data()), root_dir.size());

  // Close the file stream
  disk_image.close();

  // Output success message
  std::cout << "FAT32 disk image created successfully: " << disk_name << "\n";

  return 0; // Return success
}
