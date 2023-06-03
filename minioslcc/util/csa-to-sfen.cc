#include "record.h"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc > 1) {
    std::cout << "search csa files in the current directory and write to sfen.txt\n";
    return std::string(argv[1]) == "--help" ? 0 : 1;
  }
  
  auto csa_folder = std::filesystem::path(".");
  std::ofstream os("sfen.txt");
  int count=0, zero_skip_count=0;
  for (auto& file: std::filesystem::directory_iterator{csa_folder}) {
    if (! file.is_regular_file() || file.path().extension() != ".csa")
      continue;
    try {
      auto record=osl::csa::read_record(file);
      if (record.moves.empty()) {
        ++zero_skip_count;
        continue;
      }
      os << to_usi(record) << '\n';
      ++count;
    }
    catch (std::exception& e) {
      std::cerr << "skip " << file << '\n' << e.what() << '\n';
    }
  }
  std::cout << "wrote " << count << " records\n";
  std::cerr << "skip " << zero_skip_count << " records with zero moves" << '\n';  
}
