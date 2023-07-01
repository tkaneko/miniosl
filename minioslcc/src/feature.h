#ifndef MINIOSL_FEATURE_H
#define MINIOSL_FEATURE_H

#include "state.h"
#include <unordered_map>

namespace osl {
  namespace ml {
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
}

#endif
// MINIOSL_FEATURE_H
