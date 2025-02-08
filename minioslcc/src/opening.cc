#include "opening.h"
#include "feature.h"
#include <cista/serialization.h>
#include <cista/containers/hash_map.h>
#include <cista/containers/pair.h>
#include <cista/mode.h>
#include <cmath>
#include <iostream>

namespace osl {
  namespace data = cista::offset;
  constexpr auto const MODE = cista::mode::WITH_VERSION
    // | cista::mode::WITH_INTEGRITY
    ;
}

struct osl::OpeningTree::Data {
  typedef data::pair<const uint64_t, uint32_t> key_t;
  static key_t make(const BasicHash& key) { return key_t{key.first, key.second}; }
  typedef data::hash_map<key_t, Node> table_t;
  std::shared_ptr<cista::mmap> buf; // table is not writable if notnull
  cista::mmap::protection protection_mode;
  std::optional<table_t> local_table; // table is editable notnull
  table_t *table;

  table_t::iterator find(const BasicHash& key) { return table->find(make(key)); }
  table_t::iterator end() { return table->end(); }

  Data() : local_table(table_t()), table(&*local_table) { // writable to default
  }
  virtual ~Data() = default;

  /** false if resizable */
  bool read_only() const { return (bool)buf; }
};

osl::OpeningTree::OpeningTree() : data(new Data) {
}
osl::OpeningTree::~OpeningTree() {
}

size_t osl::OpeningTree::size() const {
  return data->table->size();
}

size_t osl::OpeningTree::root_count() const {
  auto key = hash_code(BaseState(HIRATE));
  return read(key).value_or(Node()).count();
}

std::optional<osl::OpeningTree::Node>
osl::OpeningTree::read(BasicHash key) const {
  auto p = data->find(key);
  if (p == data->end())
    return std::nullopt;
  return p->second;
}

std::optional<osl::OpeningTree::Node*>
osl::OpeningTree::edit(BasicHash key) const {
  if (data->protection_mode == cista::mmap::protection::READ)
    return {};
  auto p = data->find(key);
  if (p == data->end())
    return std::nullopt;
  return &(p->second);
}

std::shared_ptr<osl::OpeningTree> osl::OpeningTree::load_binary(std::string filename, int threshold, bool modify) {
  try {
    auto mode = modify ? cista::mmap::protection::MODIFY : cista::mmap::protection::READ;
    std::shared_ptr<cista::mmap> buf(new cista::mmap(filename.c_str(), mode));
    auto loaded = cista::deserialize<Data::table_t, MODE>(*buf);
    std::shared_ptr<OpeningTree> tree(new OpeningTree);
    if (threshold <= 1) {
      tree->data->buf = buf;
      tree->data->table = loaded;
      tree->data->local_table.reset();
      tree->data->protection_mode = mode;
      return tree;
    }
    for (auto& e: *loaded)
      if (e.second.count() >= threshold)
        tree->data->table->insert(e);
    return tree;
  }
  catch (std::exception& e) {
    std::cerr << "OpeningTree::load_binary failed " << e.what() << '\n';
    throw;
  }
}

void osl::OpeningTree::save_binary(std::string filename) const {
  static_assert(std::is_trivially_copyable_v<Node>);
  cista::buf mmap{cista::mmap{filename.c_str()}};
  cista::serialize<MODE>(mmap, *(data->table));
}

void osl::OpeningTree::prune(int threshold, std::string output) const {
  Data::table_t pruned;
  for (const auto& e: *(data->table))
    if (e.second.count() >= threshold)
      pruned.insert(e);

  cista::buf mmap{cista::mmap{output.c_str()}};
  cista::serialize<MODE>(mmap, pruned);  
}


osl::OpeningTree::tuple_t
osl::OpeningTree::export_all() const {
  std::vector<uint64_t> board_v;
  std::vector<uint32_t> stand_v;
  std::vector<int> count_v;
  std::vector<int> depth_v;
  std::vector<float> value_v;
  board_v.reserve(size());
  stand_v.reserve(size());
  count_v.reserve(size()*GameResultTypes);
  depth_v.reserve(size());
  value_v.reserve(size());
  for (const auto& [key, node]: *(data->table)) {
    board_v.push_back(key.first);
    stand_v.push_back(key.second);
    for (auto c: node.result_count)
      count_v.push_back(c);
    depth_v.push_back(node.depth * (1l << 16) + node.age);
    value_v.push_back(node.black_value_backup);
  }
  return { board_v, stand_v, count_v, depth_v, value_v };
}

size_t osl::OpeningTree::count_visits(const BasicHash& key, const MoveVector& arms,
                                      std::vector<size_t>& out) const {
  out.resize(arms.size());
  std::ranges::fill(out, 0);
  if (! contains(key))
    return 0;
  size_t parent_visit = 0;
  for (int i=0; i<arms.size(); ++i) {
    auto ckey = make_move(key, arms[i]);
    auto q = read(ckey);
    if (! q)
      continue;
    auto n = q->count();
    out[i] = n;
    parent_visit += n;
  }
  return parent_visit;
}

std::vector<std::pair<osl::Move,osl::OpeningTree::Node>>
osl::OpeningTree::retrieve_children(const EffectState& state) const {
  std::vector<std::pair<Move,Node>> ret;
  auto key = HashStatus(state).basic();
  if (! contains(key))
    return ret;

  MoveVector moves;
  state.generateLegal(moves);
  ret.reserve(moves.size());
  
  for (auto move: moves) {
    auto ckey = make_move(key, move);
    auto q = read(ckey);
    if (! q)
      continue;
    ret.push_back({move, *q});
  }

  std::sort(ret.begin(), ret.end(),
            [](const auto& l, const auto& r){ return l.second.count() > r.second.count();});
  return ret;
}

std::optional<osl::OpeningTreeEditable> osl::OpeningTree::mutable_view() {
  if (data->buf)
    return std::nullopt;
  OpeningTreeEditable ret;
  ret.data = data;              // share
  return ret;
}

osl::OpeningTreeEditable osl::OpeningTree::mutable_clone() const {
  OpeningTreeEditable ret;
  *(ret.data->table) = *(data->table); // copy
  return ret;
}


osl::OpeningTreeEditable::OpeningTreeEditable() {
}

osl::OpeningTreeEditable::~OpeningTreeEditable() {
}

osl::OpeningTree::Node& osl::OpeningTreeEditable::operator[](const BasicHash& key) {
  return data->table->operator[](Data::make(key));
}


void osl::OpeningTreeEditable::add(const MiniRecord& record) {
  if (record.moves.empty()
      || record.variant == Shogi816K
      || record.variant == UnIdentifiedVariant)
    return;
  for (size_t i=0; i<record.moves.size(); ++i) {
    const auto& key = record.history[i];
    auto& node = operator[](key.basic());
    node.result_count[record.result] += 1;
    if (i > 0 && node.depth > i)
      node.depth = i;
    if (node.count() <= 1)      // new node
      break;
  }
  auto& root = operator[](record.history.front().basic());
  root.depth = 0;
}

void osl::OpeningTreeEditable::add_all(const std::vector<MiniRecord>& records) {
  for (const auto& r: records)
    add(r);
}

void osl::OpeningTreeEditable::add_subrecords(const std::vector<SubRecord>& records) {
  auto v0 = HIRATE;
  auto key0 = hash_code(BaseState(HIRATE));
  for (const auto& r: records) {
    if (! (r.is_hirate_game() || r.variant == Aozora))
      return;
    if (r.variant != v0) {
      v0 = r.variant;
      key0 = hash_code(BaseState(v0));
    }
    auto key = key0;
    for (size_t i=0; i<r.moves.size(); ++i) {
      auto move = r.moves[i];
      auto& node = operator[](key);
      node.result_count[r.result] += 1;
      if (i > 0 && (node.depth == 0 || node.depth > i))
        node.depth = i;
      if (node.count() <= 1)      // new node
        break;
      key = make_move(key, move);
    }
  }
  auto& root = operator[](key0);
  root.depth = 0;
}

osl::OpeningTreeEditable osl::OpeningTreeEditable::
from_record_set(const RecordSet& data, int minimum_count) {
  const int max_depth = 100;
  OpeningTreeEditable tree;
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
        tree[{kv.first, e.first.to_uint()}] = {e.second};
      }
    }
  }
  return tree;
}

osl::OpeningTreeEditable osl::OpeningTreeEditable::
restore_from(const tuple_t& vectors) {
  static_assert(GameResultTypes == 4);

  const auto& [key_board, key_stand, count, depth, value] = vectors;

  OpeningTreeEditable tree;
  
  int n = key_board.size();
  if (key_stand.size() != n || count.size() != n*GameResultTypes
      || depth.size() != n || value.size() != n)
    throw std::invalid_argument("unexpected size "+std::to_string(n)+" "+std::to_string(key_stand.size())
                                +" "+std::to_string(count.size())
                                +" "+std::to_string(depth.size())+" "+std::to_string(value.size())
                                );
  for (int i: std::views::iota(0, n)) {
    Node node = {
      { count[i*4], count[i*4+1], count[i*4+2], count[i*4+3] },
      value[i],
      int16_t(depth[i] / (1l << 16)),
      int16_t(depth[i] % (1l << 16))
    };
    tree[{key_board[i], key_stand[i]}] = node;
  }
  return tree;
}

void osl::OpeningTreeEditable::decay_all(double coef) {
  for (auto& e: *(data->table)) {
    for (auto& v: e.second.result_count)
      v = std::ceil(coef * v);
  }
}

size_t osl::OpeningTreeEditable::dfs(int threshold) {
  EffectState state;
  auto ret = _dfs(state, threshold);

  EffectState aozora((BaseState(Aozora)));
  ret += _dfs(aozora, threshold);

  return ret;
}

size_t osl::OpeningTreeEditable::_dfs(const EffectState& state, int threshold) {
  auto key = hash_code(state);
  if (!contains(key))
    return 0;
  auto& root = operator[](key);
  int age = std::max(1l, (root.age + 1) % (1l << 16));
  root.depth = 0;

  std::function<size_t(Node&, BasicHash, const EffectState&)>
    f = [&, this](Node& node, BasicHash key, const EffectState& state) -> size_t {
      if (node.count() < threshold || node.age == age)
        return 0;
      node.age = age;
      size_t total_bwin = 0, total = 0, ret = 0;
      MoveVector moves;
      state.generateLegal(moves);
      for (auto move: moves) {
        auto ckey = make_move(key, move);
        if (! contains(ckey))
          continue;
        auto& cnode = operator[](ckey);
        if (cnode.depth < node.depth) // ignore uplink
          continue;
        EffectState cstate(state);
        cstate.makeMove(move);
        ret += f(cnode, ckey, cstate);
        total_bwin += cnode.black_advantage() * cnode.count();
        total += cnode.count();
      }
      if (total > threshold)
        node.black_value_backup = 1. * total_bwin / total;
      return ret + 1;
    };

  return f(root, key, state);
}

