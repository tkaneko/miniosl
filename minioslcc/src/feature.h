#ifndef MINIOSL_FEATURE_H
#define MINIOSL_FEATURE_H

#include "state.h"
#include <unordered_map>

namespace osl {
  namespace ml {
    const int basic_channels = 44;
    std::array<int8_t,81> board_dense_feature(const BaseState& state);
    std::array<int8_t,piece_stand_order.size()*2> hand_dense_feature(const BaseState& state);

    std::array<int8_t,81*30> board_feature(const BaseState& state);
    std::array<float,81*piece_stand_order.size()*2> hand_feature(const BaseState& state);

    /** covered squares by long piece */
    std::array<int8_t,81*2> lance_cover(const EffectState& state);
    std::array<int8_t,81*2> bishop_cover(const EffectState& state);
    std::array<int8_t,81*2> rook_cover(const EffectState& state);
    std::array<int8_t,81*2> king_visibility(const EffectState& state);
    /** checkmate or threatmate */
    std::array<int8_t,81*2> mate_path(const EffectState& state);
    namespace impl  {
      /** fill 1 on the path from src (exclusive) to dst (inclusive) */
      void fill_segment(Square src, Square dst, int offset, std::array<int8_t,81*2>& out);
      inline void fill_segment(Piece p, Square dst, Player owner, std::array<int8_t,81*2>& out) {
        fill_segment(p.square(), dst, idx(owner)*81, out);
      }
      void fill_move_trajectory(Move move, int offset, std::array<int8_t,81*2>& out);
      void fill_ptypeo(const BaseState& state, Square sq, PtypeO ptypeo, std::array<int8_t,81>& out);
    }
    using impl::fill_segment;
    using impl::fill_move_trajectory;
    namespace helper {
      // 44ch
      void write_np_44ch(const BaseState& state, float *);
      // 10ch
      void write_np_additional(const EffectState& state, bool flipped, Move last_move, float *);
      // status after move
      void write_np_aftermove(EffectState state, Move move, float *);
    }
    /** move label [0,27*81] for cross entropy */
    int policy_move_label(Move move);
    inline int value_label(GameResult result) {
      if (result == BlackWin) return 1;
      if (result == WhiteWin) return -1;
      return 0;
    }
    /** decode, assume black to move */
    Move decode_move_label(int code, const BaseState& state);

    /** name of input features */
    extern const std::unordered_map<std::string, int> channel_id;
  }

  struct MiniRecord;
  /** subset of MiniRecord assuming completed game with the standard initial state */
  struct SubRecord {
    /** moves */
    std::vector<Move> moves;
    /** resign or DeclareWin if game has the winner */
    Move final_move;
    /** result of the game or `InGame` if not yet initialized */
    GameResult result = InGame;

    SubRecord() = default;
    SubRecord(const MiniRecord& record);

    /** export features and labels */
    void export_feature_labels(int idx, float *input, int& move_label, int& value_label, float *aux_label) const;
    /** randomly samle index and call export_feature_labels() */
    void sample_feature_labels(float *input, int& move_label, int& value_label, float *aux_label, int tid=0) const;
    /** @internal make state of given `index` with flip if white to move */
    std::tuple<BaseState,Move,Move,GameResult,bool> make_state_label_of_turn(int index) const;
    /** @internal make a state after the first `n` moves */
    BaseState make_state(int n) const;

    /** sample int in range [0, limit-1], with progressive 1/2 weight for the opening moves, 2^{-21} for initial position */
    static int weighted_sampling(int limit, int tid=0);
  };
}

#endif
// MINIOSL_FEATURE_H
