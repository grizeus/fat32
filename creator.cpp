#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

// using namespace std;

// Define FAT32 boot sector structure
// NOTE: live it for now
#pragma pack(push, 1)
struct BootSector {
  uint8_t jmpBoot[3];        // JMP instruction
  char oemName[8];           // OEM name
  uint16_t bytesPerSector;   // Bytes per sector
  uint8_t sectorsPerCluster; // Sectors per cluster
  uint16_t reservedSectors;  // Reserved sectors count
  uint8_t numFATs;           // Number of FATs
  uint16_t rootDirEntries;   // Number of root directory entries
  uint16_t totalSectors16;   // Total sectors (16-bit)
  uint8_t media;             // Media descriptor
  uint16_t sectorsPerFAT16;  // Sectors per FAT (16-bit)
  uint16_t sectorsPerTrack;  // Sectors per track
  uint16_t numHeads;         // Number of heads
  uint32_t hiddenSectors;    // Hidden sectors count
  uint32_t totalSectors32;   // Total sectors (32-bit)
  uint32_t sectorsPerFAT32;  // Sectors per FAT (32-bit)
  uint16_t flags;            // Flags
  uint16_t fsVersion;        // File system version
  uint32_t rootCluster;      // Root directory cluster
  uint16_t fsInfo;           // File system information sector
  uint16_t backupBootSector; // Backup boot sector location
  uint8_t reserved[12];      // Reserved
  uint8_t driveNumber;       // Drive number
  uint8_t reserved1;         // Reserved
  uint8_t bootSig;           // Extended boot signature
  uint32_t volumeID;         // Volume serial number
  char volumeLabel[11];      // Volume label
  char fsType[8];            // File system type
  char bootCode[420];        // Boot code
  uint16_t bootSignature;    // Boot sector signature
};
#pragma pack(pop)

int main(int argc, char **argv) {

  if (argc != 2) {
    std::cerr << "Program usage is " << argv[0] << " <size>(in Mb)\n";
    return 1;
  }
  // Define disk image file name
  long size = std::atol(argv[1]);
  if (size < 1) {
    std::cerr << "Error: size must be greater than 1\n";
    return 1;
  }
  if (size >= 2'000'000) {
    std::cerr << "Error: size must be smaller than 2'000'000\n";
    return 1;
  }

  std::string diskImageFileName = "my_disk.img";

  int diskSize = size * 1024 * 1024;

  // Create a new file stream for writing the disk image
  std::ofstream diskImage(diskImageFileName, std::ios::binary);

  // Check if the file stream is open
  if (!diskImage.is_open()) {
    std::cerr << "Error: Unable to create FAT32 disk image\n";
    return 1; // Return error code
  }

  // Initialize boot sector structure
  BootSector bootSector;
  std::memset(&bootSector, 0, sizeof(BootSector));

  // Fill boot sector fields
  bootSector.bytesPerSector = 512;
  bootSector.sectorsPerCluster = 4;
  bootSector.reservedSectors = 1;
  bootSector.numFATs = 2;
  bootSector.rootDirEntries = 512;
  bootSector.totalSectors16 = 0;
  bootSector.media = 0xF8;
  bootSector.sectorsPerFAT16 = 0;
  bootSector.sectorsPerTrack = 32;
  bootSector.numHeads = 2;
  bootSector.hiddenSectors = 0;
  bootSector.totalSectors32 = diskSize / bootSector.bytesPerSector;
  bootSector.sectorsPerFAT32 = 10;
  bootSector.flags = 0;
  bootSector.fsVersion = 0;
  bootSector.rootCluster = 2;
  bootSector.fsInfo = 1;
  bootSector.backupBootSector = 6;
  bootSector.driveNumber = 0x80;
  bootSector.bootSig = 0x29;
  bootSector.volumeID = 12345678;
  strncpy(bootSector.volumeLabel, "MY_DISK", sizeof(bootSector.volumeLabel));
  strncpy(bootSector.fsType, "FAT32", sizeof(bootSector.fsType));
  bootSector.bootSignature = 0xAA55;

  // Write boot sector to disk image
  diskImage.write(reinterpret_cast<char *>(&bootSector), sizeof(BootSector));
  const uint32_t data_sectors = boot_sector.total_sectors - RESERVED_SECTORS -
                                (ROOT_ENTRIES * 32 / BYTES_PER_SECTOR) -
                                (FAT_COPIES * boot_sector.total_sectors / 200);
  const uint32_t total_clusters = data_sectors / SECTORS_PER_CLUSTER;
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

  // Close the file stream
  diskImage.close();

  // Output success message
  std::cout << "FAT32 disk image created successfully: " << diskImageFileName
            << "\n";

  return 0; // Return success
}
