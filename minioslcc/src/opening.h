#ifndef MINIOSL_OPENING_H
#define MINIOSL_OPENING_H

#include "record.h"
#include "impl/hash.h"
#include <tuple>
#include <vector>

namespace osl {
  class SubRecord;
  class OpeningTreeEditable;
  class OpeningTree {
  protected:
    class Data;
    std::shared_ptr<Data> data;
  public:
    struct Node {
      std::array<int,GameResultTypes> result_count = {0};
      float black_value_backup = 0;
      int16_t depth = 32767, age = 0;

      int operator[](int idx) const { return result_count[idx]; }
      int count() const { return std::accumulate(result_count.begin(), result_count.end(), 0); }
      static constexpr double eps = 1.0/1024;
      float black_advantage_tree(float prior=eps) const {
        auto draw = result_count[Draw] + result_count[InGame];
        auto num = result_count[BlackWin] + draw * .5;
        auto den = result_count[BlackWin] + result_count[WhiteWin] + draw;
        return (num + prior) / (den + 2*prior);
      }
      float black_advantage(float prior=eps) const {
        auto value = black_advantage_tree(prior);
        if (depth == 0 || black_value_backup == 0)
          return value;
        return (depth % 2 == 1) // view from parent
          ? std::max(value, black_value_backup)
          : std::min(value, black_value_backup);
      }
    };

    OpeningTree();
    virtual ~OpeningTree();

    std::optional<Node> read(BasicHash key) const;
    bool contains(const BasicHash& key) const { return read(key).has_value(); }
    /** number of elements */
    size_t size() const;
    size_t root_count() const;

    std::optional<Node*> edit(BasicHash key) const; // effective only mapping with modify

    /** compute visit counts for each move, returning their sum */
    size_t count_visits(const BasicHash& key, const MoveVector& arms, std::vector<size_t>& out) const;
    std::vector<std::pair<Move,Node>> retrieve_children (const EffectState& state) const;

    static std::shared_ptr<OpeningTree> load_binary(std::string filename, int threshold=1, bool modify=true);
    void save_binary(std::string filename) const;
    /** prune table entries by visit counts to save */
    void prune(int threshold, std::string output) const;

    typedef std::tuple<
      std::vector<uint64_t>, std::vector<uint32_t>, std::vector<int>,
      std::vector<int>, std::vector<float>
      > tuple_t;
    /** export data suitable for storing as npz  */
    tuple_t export_all() const;

    std::optional<OpeningTreeEditable> mutable_view();
    OpeningTreeEditable mutable_clone() const;
  };

  class OpeningTreeEditable : public OpeningTree {
  public:
    OpeningTreeEditable();
    ~OpeningTreeEditable();
    
    Node& operator[](const BasicHash& key);

    /** add a result for existing node and extend one node */
    void add(const MiniRecord&);
    void add_all(const std::vector<MiniRecord>&);
    void add_subrecords(const std::vector<SubRecord>&);    

    void decay_all(double coef=.99);
    size_t dfs(int threshold=128);
    size_t _dfs(const EffectState& root, int threshold);

    static OpeningTreeEditable from_record_set(const RecordSet&, int minimum_count);
    static OpeningTreeEditable restore_from(const tuple_t&);
  };
}

#endif
// MINIOSL_OPENING_H
