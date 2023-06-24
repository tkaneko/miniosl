#ifndef MINIOSL_OPENING_H
#define MINIOSL_OPENING_H

#include "record.h"
#include "impl/hash.h"
#include <tuple>
#include <vector>

namespace osl {
  struct OpeningTree {
    struct Node {
      std::array<int,GameResultTypes> result_count = {0};
      int operator[](int idx) const { return result_count[idx]; }
      int count() const { return std::accumulate(result_count.begin(), result_count.end(), 0); }
      static constexpr double eps = 1.0/1024;
      float black_advantage() const { return (result_count[0]+eps)/(result_count[0]+result_count[1]+2*eps); }
    };
    HashTable<Node> table;
    Node& operator[](const std::pair<uint64_t, uint32_t>& key) { return table[key]; }    

    bool contains(const std::pair<uint64_t, uint32_t>& key) const { return table.contains(key); }
    /** number of elements ignoring differences in pieces in hand */
    size_t board_size() const { return table.board_size(); }
    /** number of elements */
    size_t size() const { return table.size(); }

    /** export data suitable for storing as npz  */
    std::tuple<std::vector<uint64_t>, std::vector<uint32_t>, std::vector<int>> export_all() const;
    static OpeningTree from_record_set(const RecordSet&, int minimum_count);
    static OpeningTree restore_from(const std::vector<uint64_t>&, const std::vector<uint32_t>&,
                                    const std::vector<int>&);
  };
}

#endif
// MINIOSL_OPENING_H
