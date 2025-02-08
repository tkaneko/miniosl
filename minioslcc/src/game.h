#ifndef MINIOSL_GAME_H
#define MINIOSL_GAME_H

#include "record.h"
#include "infer.h"
#include "impl/rng.h"

namespace osl {
  /** run 1:1 game */
  struct GameManager {
    /** record */
    MiniRecord record;
    /** table to detect repetition */
    HistoryTable table;
    /** current state */
    EffectState state;
    /** legal moves in current state to detect game ends */
    MoveVector legal_moves;

    /** start a new game */
    explicit GameManager(GameVariant kind=HIRATE,
                         std::optional<int> shogi816k_id=std::nullopt);
    ~GameManager();

    /** make a move
     * @param move a usual move
     * @return result indicating the game was completed by the move
     */
    GameResult make_move(Move move);
    void export_heuristic_feature(nn_input_element*) const;
    /** export features for a state after move
     * @param ptr must be zero-filled in advance
     * @return InGame (usual cases) or a definite result if identified
     */
    osl::GameResult export_heuristic_feature_after(Move, nn_input_element *ptr) const;
    /** export features for a state after move
     * @param move considering move
     * @param reply the opponent's reply (can be illegal)
     * @param ptr must be zero-filled in advance
     * @return InGame (usual cases) or a definite result if identified
     */
    bool export_heuristic_feature_after(Move move, int reply, nn_input_element *ptr) const;
    /** @internal this interface will subject to change along with optimization/enhancements 
     * @param ptr must be zero-filled in advance
     */
    static GameResult export_heuristic_feature_after(Move latest,
                                                     BaseState initial, MoveVector history,
                                                     nn_input_element *ptr);
    static GameManager from_record(const MiniRecord& record);
  };

  class OpeningTree;
  /**
   * array of homogeneous players for self-play
   * start thinking by the first call of make_request, after receiving data by recv_result,
   * each player make its decision or continue thinking.
   */
  class PlayerArray {
  public:
    PlayerArray(bool greedy_);
    virtual ~PlayerArray()=default;
    void new_series(const std::vector<GameManager>& games);
    /** return whether need policy in addition to value */
    virtual bool make_request(int phase, nn_input_element *)=0;
    /** return decision made */
    virtual bool recv_result(int phase,
                             const std::vector<policy_logits_t>& logits,
                             const std::vector<value_vector_t>& values)=0;
    /** maximum number of position each player may request at a time */
    virtual int max_width() const { return 1; }
    virtual int width(int /* phase */) const { return max_width(); }
    virtual std::string name() const=0;

    const auto& decision() const { return _decision; }
    int n_parallel() const { check_ready(); return _games->size(); }

    const bool greedy;
  protected:
    const std::vector<GameManager> *_games = nullptr;
    std::vector<Move> _decision;
    rng::rng_array_t rngs;
    std::shared_ptr<const OpeningTree> book;
    void check_ready() const;
    void check_size(int n, int scale=1, std::string where="") const;
  public:
    static std::vector<std::pair<float,Move>>
    sort_moves(const osl::MoveVector& moves, const policy_logits_t& logits, int top_n) {
      return sort_moves_impl<false>(moves, logits, top_n);
    }
    static std::vector<std::pair<float,Move>>
    sort_moves_with_gumbel(const osl::MoveVector& moves, const policy_logits_t& logits, int top_n,
                           rng_t *rng, float noise_scale=1.0) {
      return sort_moves_impl<true>(moves, logits, top_n, rng, noise_scale);
    }
    template <bool with_noise> static std::vector<std::pair<float,Move>>
    sort_moves_impl(const osl::MoveVector& moves, const policy_logits_t& logits, int top_n,
                    rng_t *rng=nullptr, float noise_scale=1.0);
    static std::vector<std::pair<float,Move>>
    sort_moves_with_book(const OpeningTree& book, BasicHash state_key,
                         const osl::MoveVector& moves, const policy_logits_t& logits, int top_n,
                         rng_t *rng, float gumbel_noise_scale=1.0,
                         float book_weight_p=1.0, float book_weight_v=1.0
                         );
  };
  
  struct PolicyPlayer : public PlayerArray {
    explicit PolicyPlayer(bool greedy=false);
    ~PolicyPlayer() override;
    bool make_request(int phase, nn_input_element *) override;
    bool recv_result(int phase,
                     const std::vector<policy_logits_t>& logits,
                     const std::vector<value_vector_t>& values) override;
    std::string name() const override;
  };

  struct GumbelPlayerConfig {
    int root_width=8, second_width=0;
    float noise_scale=1.0, cscale=1.0;
    int greedy_after=999;
    float softalpha=0.0;
    int value_mix = 0;
    float depth_weight = 0.5;
    static constexpr int mc = 0, td = 1, ave = 2, max = 3;
    std::string book_path = "";
    int book_threshold = 16;
    float book_weight_p = 1, book_weight_v = 1;

    float take_value(const value_vector_t& values) const {
      auto cv = values[0];      // default mc return
      // balance between mc and td1
      if (value_mix == GumbelPlayerConfig::td)
        cv = values[1];
      else if (value_mix == GumbelPlayerConfig::ave)
        cv = values[0]*0.75 + values[1]*0.25;
      else if (value_mix == GumbelPlayerConfig::max)
        cv = std::min(values[0], values[1]); // optimistic
      // compose soft value
      cv += softalpha * values[2];
      return cv;
    }
  };

  struct FlatGumbelPlayer : public PlayerArray, private GumbelPlayerConfig {
    explicit FlatGumbelPlayer(GumbelPlayerConfig config);
    ~FlatGumbelPlayer() override;

    bool make_request(int phase, nn_input_element *) override;
    bool recv_result(int phase,
                     const std::vector<policy_logits_t>& logits,
                     const std::vector<value_vector_t>& values) override;
    int max_width() const override { return root_width; }
    int width(int phase) const override;
    std::string name() const override;

    /** transform nnQ in [-1,1] to > 0 following Gumbel MuZero paper */
    static float transformQ_formula(float nnQ, float cvisit, float maxnb, float cscale=1.0) {
      auto Q = nnQ/2.0 + 0.5;
      return (cvisit + maxnb) * cscale * Q;
    }
    float transformQ(float nnQ, float cvisit=50.0) const {
      auto maxnb = (second_width > 0) ? 2.0 : 1.0;
      return transformQ_formula(nnQ, cvisit, maxnb, cscale);
    }
    float& root_gumbel_value(int idx) { return get<0>(root_children[idx]); } 
    Move& root_move(int idx) { return get<1>(root_children[idx]); } 
    int& root_reply(int idx) { return get<2>(root_children[idx]); } 
    float& root_half_value(int idx) { return get<3>(root_children[idx]); } 

    std::vector<std::tuple<float,Move,int,float>> root_children; // (logits+value, move, reply-code, half-value)
    std::vector<GameResult> root_children_terminal;
  };

  struct SingleCPUPlayer {
    virtual ~SingleCPUPlayer();
    virtual Move think(std::string usi)=0;
    virtual std::string name()=0;
  };
  
  struct CPUPlayer : public PlayerArray {
    CPUPlayer(std::shared_ptr<SingleCPUPlayer> player, bool greedy);
    ~CPUPlayer() override;
    bool make_request(int phase, nn_input_element *) override;
    bool recv_result(int phase,
                     const std::vector<policy_logits_t>& logits,
                     const std::vector<value_vector_t>& values) override;
    int max_width() const override { return 0; }
    std::string name() const override { return player->name(); }
  private:
    std::shared_ptr<SingleCPUPlayer> player;
  };

  struct RandomPlayer : public SingleCPUPlayer {
    RandomPlayer();
    ~RandomPlayer();
    Move think(std::string usi) override;
    std::string name() override;
  };
  
  struct GameConfig {
    bool force_declare = true;
    bool ignore_draw = false;
    float random_opening = 0.0;
    GameVariant variant = HIRATE;
  };
  
  struct ParallelGameManager {
    ParallelGameManager(int N, std::optional<GameConfig> config=std::nullopt);
    ~ParallelGameManager();

    int n_parallel() const { return games.size(); }
    
    std::vector<GameResult> make_move_parallel(const std::vector<Move>& move);
    GameManager make_newgame() const {
      return GameManager(config.variant);
    }
    void reset(int g) {
      games.at(g) = make_newgame();
    }
    std::vector<GameManager> games;
    std::vector<MiniRecord> completed_games;
    GameConfig config;
  };

  class GameArray {
  public:
    GameArray(int N, PlayerArray& a, PlayerArray& b,
              InferenceModel& model_a,
              InferenceModel& model_b,
              std::optional<GameConfig> config=std::nullopt);
    ~GameArray();

    void step();
    const auto& completed() const { return mgrs.completed_games; }

    void warmup(int n=4);
    /** @param ptr must be zero-filled in advance */
    static void export_root_features(const std::vector<GameManager>& games, nn_input_element *ptr);
  private:
    void resize_buffer(int width);

    ParallelGameManager mgrs;
    std::array<PlayerArray*,2> players;
    std::array<InferenceModel*,2> model;
    bool side=0;
    std::vector<nn_input_element> input_buf;
    std::vector<policy_logits_t> policy_buf;
    std::vector<value_vector_t> value_buf;
    /** player_a should always play first */
    std::vector<int8_t> skip_one_turn;
    int max_width;
    double random_opening=0.0;
  };
}

#endif
// MINIOSL_GAME_H
