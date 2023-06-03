// validate-sfen.cc
#include "record.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <ranges>

void compress(std::filesystem::path sfen) {
  auto bin = sfen;
  bin.replace_extension("bin");
  std::cout << "compress = " << sfen << " to " << bin << '\n';

  std::ofstream os(bin, std::ios::binary);
  
  std::ifstream is(sfen);
  int count=0;
  std::string line;
  std::vector<uint64_t> work;
  try {
    while (getline(is, line)) {
      auto record=osl::usi::read_record(line);
      if (! osl::bitpack::append_binary_record(record, work))
        continue;
      ++count;
      for (uint64_t code: work)
        os.write(reinterpret_cast<char*>(&code), sizeof(code));
      work.clear();
    }
  }
  catch (std::exception& e) {
    std::cerr << "error at line " << count << " " << line << "\n";
    std::cerr << e.what();
  }
  std::cout << "wrote " << count << " records\n";
}

void decompress(std::filesystem::path bin) {
  auto txt = bin;
  txt.replace_extension("txt");
  std::cout << "DEcompress " << txt << " from " << bin << '\n';

  std::ofstream os(txt);

  std::vector<uint64_t> coded_record_seq;
  {
    std::ifstream is(bin, std::ios::binary);
    uint64_t code;
    while (is.read(reinterpret_cast<char*>(&code), sizeof(code)))
      coded_record_seq.push_back(code);
  }
      
  int count=0;
  try {
    osl::MiniRecord record;
    const uint64_t *first = &*coded_record_seq.begin(), *ptr = first;
    while (osl::bitpack::read_binary_record(ptr, record)) {
      os << to_usi(record) << '\n';
      ++count;
      if (ptr-first >= coded_record_seq.size())
        break;
    }
  }
  catch (std::exception& e) {
    std::cerr << "error at line " << count << "\n";
    std::cerr << e.what();
  }
  std::cout << "wrote " << count << " records\n";
}


int main(int argc, char *argv[]) {
  if (argc != 2 || argv[1][0] == '-') {
    std::cout << "usage: argv[0] sfen-file-name\n";
    return (argc >= 2 && std::string(argv[1]) == "--help") ? 0 : 1;
  }

  auto sfen = std::filesystem::path(argv[1]);
  if (! exists(sfen)) {
    std::cerr << "file not found\n";
    return 1;
  }

  auto ext = sfen.extension();
  if (ext == ".bin")
    decompress(sfen);
  else
    compress(sfen);
}
