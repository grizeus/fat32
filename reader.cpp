#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>

int main(int argc, char **argv) {

  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <file_name> <output_name>\n";
    return 1;
  }

  std::ifstream file(argv[1], std::ios::binary);
  if (!file) {
    std::cerr << "Failed to open file\n";
    return 1;
  }

  char byte;
  std::ofstream out_file(argv[2], std::ios::out);
  if (!out_file.is_open()) {
    std::cerr << "Unable to open file";
    return 1;
  }

  while (file.get(byte)) {
    out_file << std::hex << std::setw(2) << std::setfill('0')
             << (int)(unsigned char)byte << " ";
  }

  out_file.close();
  file.close();

  return 0;
}
