#include "opening.h"

osl::OpeningTree osl::OpeningTree::from_record_set(const RecordSet& data, int minimum_count) {
  const int max_depth = 100;
  OpeningTree tree;
  int found = minimum_count;
  for (int n=0; n<max_depth && found >= minimum_count; ++n) {
    found = 0;
    HashTable<Node> fresh;
    for (const auto& record: data.records){
      if (n >= record.state_size())
        continue;
      auto hash = record.history[n];
      fresh[{hash.board_hash, hash.black_stand}].result_count[record.result] += 1;
    }
    for (const auto& kv: fresh) {
      for (const auto& e: kv.second) {
        if (e.second.count() < minimum_count)
          continue;
        found = std::max(found, e.second.count());
        tree.table[{kv.first, e.first}] = {e.second};
      }
    }
  }
  return tree;
}

std::tuple<std::vector<uint64_t>, std::vector<uint32_t>, std::vector<int>>
osl::OpeningTree::export_all() const {
  std::vector<uint64_t> board_v;
  std::vector<uint32_t> stand_v;
  std::vector<int> node_v;
  board_v.reserve(table.size());
  stand_v.reserve(table.size());
  node_v.reserve(table.size()*GameResultTypes);
  for (const auto& kv: table) {
    for (const auto& e: kv.second) {
      board_v.push_back(kv.first);
      stand_v.push_back(e.first.to_uint());
      for (auto c: e.second.result_count)
        node_v.push_back(c);
    }
  }
  return { board_v, stand_v, node_v };
}

osl::OpeningTree osl::OpeningTree::
restore_from(const std::vector<uint64_t>& key_board, const std::vector<uint32_t>& key_stand,
             const std::vector<int>& node_values) {
  static_assert(GameResultTypes == 4);
  OpeningTree tree;
  
  int n = key_board.size();
  if (key_stand.size() != n || node_values.size() != n*GameResultTypes)
    throw std::invalid_argument("unexpected size "+std::to_string(n)+" "+std::to_string(key_stand.size())
                                +" "+std::to_string(node_values.size()));
  for (int i: std::views::iota(0, n)) {
    BasicHash key {key_board[i], key_stand[i]};
    Node node = {{ node_values[i*4], node_values[i*4+1], node_values[i*4+2], node_values[i*4+3] }};
    tree.table[key] = node;
  }
  return tree;
}

