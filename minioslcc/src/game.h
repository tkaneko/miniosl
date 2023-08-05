#ifndef MINIOSL_GAME_H
#define MINIOSL_GAME_H

#include "record.h"
#include "infer.h"
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

    GameManager();
    ~GameManager();

    /** make a move
     * @param move a usual move
     * @return result indicating the game was completed by the move
     */
    GameResult make_move(Move move);
    void export_heuristic_feature(float*) const;
    /** export features for a state after move
     * @return InGame (usual cases) or a definite result if identified
     */
    osl::GameResult export_heuristic_feature_after(Move, float*) const;
    /** @internal this interface will subject to change along with optimization/enhancements */
    static GameResult export_heuristic_feature_after(Move latest,
                                                     BaseState initial, MoveVector history,
                                                     float*);
    static GameManager from_record(const MiniRecord& record);
  };

  /**
   * array of homogeneous players for self-play
   * start thinking by the first call of make_request, after receiving data by recv_result,
   * each player make its decision or continue thinking.
   */
  class PlayerArray {
  public:
    PlayerArray(bool greedy_) : greedy(greedy_) {}
    virtual ~PlayerArray()=default;
    void new_series(const std::vector<GameManager>& games);
    /** return request size per game */
    virtual int make_request(float *)=0;
    /** return decision made */
    virtual bool recv_result(const std::vector<policy_logits_t>& logits, const std::vector<std::array<float,1>>& values)=0;
    /** maximum number of position each player may request at a time */
    virtual int max_width() const { return 1; }
    virtual std::string name() const=0;

    const auto& decision() const { return _decision; }
    int n_parallel() const { check_ready(); return _games->size(); }

    const bool greedy;
  protected:
    const std::vector<GameManager> *_games = nullptr;
    std::vector<Move> _decision;
    void check_ready() const;
    void check_size(int n, int scale=1, std::string where="") const;
  public:
    static std::vector<std::pair<double,Move>>
    sort_moves(const osl::MoveVector& moves, const policy_logits_t& logits, int top_n) {
      return sort_moves_impl<false>(moves, logits, top_n);
    }
    static std::vector<std::pair<double,Move>>
    sort_moves_with_gumbel(const osl::MoveVector& moves, const policy_logits_t& logits, int top_n, TID tid=TID_ZERO,
                           double noise_scale=1.0) {
      return sort_moves_impl<true>(moves, logits, top_n, tid, noise_scale);
    }
    template <bool with_noise> static std::vector<std::pair<double,Move>>
    sort_moves_impl(const osl::MoveVector& moves, const policy_logits_t& logits, int top_n, TID tid=TID_ZERO,
                    double noise_scale=1.0);
  };
  
  struct PolicyPlayer : public PlayerArray {
    PolicyPlayer(bool greedy=false);
    ~PolicyPlayer();
    int make_request(float *) override;
    bool recv_result(const std::vector<policy_logits_t>& logits, const std::vector<std::array<float,1>>& values) override;
    std::string name() const override;
  };

  struct FlatGumbelPlayer : public PlayerArray {
    FlatGumbelPlayer(int width, double noise_scale=1.0, int greedy_after=999);
    ~FlatGumbelPlayer();

    int make_request(float *) override;
    bool recv_result(const std::vector<policy_logits_t>& logits, const std::vector<std::array<float,1>>& values) override;
    int max_width() const override { return root_width; }
    std::string name() const override;

    /** transform nnQ in [-1,1] following Gumbel MuZero */
    static double transformQ(double nnQ) {
      auto Q = nnQ/2.0 + 0.5;
      const auto cvisit=50.0, cscale=1.0, maxnb=1.0;
      return (cvisit + maxnb) * Q;
    }

    std::vector<std::pair<double,Move>> root_children;
    std::vector<GameResult> root_children_terminal;
    const int root_width, greedy_threshold;
    const double noise_scale;
  };

  struct SingleCPUPlayer {
    virtual ~SingleCPUPlayer();
    virtual Move think(std::string usi)=0;
    virtual std::string name()=0;
  };
  
  struct CPUPlayer : public PlayerArray {
    CPUPlayer(std::shared_ptr<SingleCPUPlayer> player, bool greedy);
    ~CPUPlayer();
    int make_request(float *) override;
    bool recv_result(const std::vector<policy_logits_t>& logits, const std::vector<std::array<float,1>>& values) override;
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
  
  struct ParallelGameManager {
    ParallelGameManager(int N, bool force_declare, bool ignore_draw=false);
    ~ParallelGameManager();

    int n_parallel() const { return games.size(); }
    
    std::vector<GameResult> make_move_parallel(const std::vector<Move>& move);
    void reset(int g) {
      games.at(g) = GameManager();
    }
    std::vector<GameManager> games;
    std::vector<MiniRecord> completed_games;
    bool force_declare, ignore_draw;
  };

  class GameArray {
  public:
    GameArray(int N, PlayerArray& a, PlayerArray& b, InferenceModel& model,
              bool ignore_draw=false);
    ~GameArray();

    void step();
    const auto& completed() const { return mgrs.completed_games; }

    void warmup(int n=4);
    static void export_root_features(const std::vector<GameManager>& games, float*);
  private:
    void resize_buffer(int width);

    ParallelGameManager mgrs;
    std::array<PlayerArray*,2> players;
    InferenceModel &model;
    bool side=0;
    std::vector<nn_input_t> input_buf;
    std::vector<policy_logits_t> policy_buf;
    std::vector<std::array<float,1>> value_buf;
    /** player_a should always play first */
    std::vector<int8_t> skip_one_turn;
    int max_width;
  };
}

#endif
// MINIOSL_GAME_H
