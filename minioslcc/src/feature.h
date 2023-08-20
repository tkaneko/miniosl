#ifndef MINIOSL_FEATURE_H
#define MINIOSL_FEATURE_H

#include "state.h"
#include "infer.h"
#include <unordered_map>

namespace osl {
  struct MiniRecord;
  namespace ml {
    std::array<int8_t,81> board_dense_feature(const BaseState& state);
    std::array<int8_t,piece_stand_order.size()*2> hand_dense_feature(const BaseState& state);

    void board_feature(const BaseState& state, nn_input_element /*30ch*/ *planes);
    void hand_feature(const BaseState& state, nn_input_element /*14ch*/ *planes);

    /** covered squares by long piece */
    void lance_cover(const EffectState& state, nn_input_element /*2ch*/ *planes);
    void bishop_cover(const EffectState& state, nn_input_element /*2ch*/ *planes);
    void rook_cover(const EffectState& state, nn_input_element /*2ch*/ *planes);
    void king_visibility(const EffectState& state, nn_input_element /*2ch*/ *planes);
    /** checkmate or threatmate */
    void mate_path(const EffectState& state, nn_input_element /*2ch*/ *planes);
    namespace impl  {
      /** fill 1 on the path from src (exclusive) to dst (inclusive) */
      void fill_segment(Square src, Square dst, nn_input_element /*1ch*/ *out);
      inline void fill_segment(Piece p, Square dst, Player owner, nn_input_element /*2ch*/ *out) {
        fill_segment(p.square(), dst, out + idx(owner)*81);
      }
      void fill_move_trajectory(Move move, nn_input_element /* 1ch */ *out);
      void fill_ptypeo(const BaseState& state, Square sq, PtypeO ptypeo, nn_input_element /*1ch*/ *out);
    }
    using impl::fill_segment;
    using impl::fill_move_trajectory;
    namespace helper {
      // 44ch
      void write_np_44ch(const BaseState& state, nn_input_element *);
      // +13 ch
      void write_np_additional(const EffectState& state, bool flipped, nn_input_element *);
      /** write state features i.e., w/o history (44+13ch) */
      void write_state_features(const EffectState& state, bool flipped, nn_input_element *);
      // 4ch
      /**
       * @param ptr must be zero-filled in advance
       */
      void write_np_history(const BaseState& state, Move last_move, nn_input_element *ptr);
      /** @internal write 4*7ch and update state applied after moves in history 
       * @param ptr must be zero-filled in advance
       */ 
      void write_np_histories(BaseState& state, const MoveVector& history, nn_input_element *ptr);
      // status after move
      void write_np_aftermove(EffectState state, Move move, nn_input_element *);
    }
    /** move label [0, 27*81-1] for cross entropy */
    int policy_move_label(Move move);
    inline int value_label(GameResult result) {
      if (result == BlackWin) return 1;
      if (result == WhiteWin) return -1;
      return 0;
    }
    /** decode, inverse of `policy_move_label`, assume black to move
     * @param state board to help decode with piece locations, black to move
     */
    Move decode_move_label(int code, const BaseState& state);

    /** name of input features */
    extern const std::unordered_map<std::string, int> channel_id;

    /** @internal export features primary for game playing
     * @param features must be zero-filled in advance
     * @return pair of the current state and the flag for flipped
     */
    std::pair<EffectState,bool> export_features(BaseState initial, const MoveVector& moves, nn_input_element *features, int idx=-1);
  }

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
    SubRecord(MiniRecord&& record);

    /** export features and labels */
    void export_feature_labels(int idx, nn_input_element *input,
                               int& move_label, int& value_label, nn_input_element *aux_label) const;
    /** randomly sample index and call export_feature_labels() */
    void sample_feature_labels(nn_input_element *input,
                               int& move_label, int& value_label, nn_input_element *aux_label,
                               int decay=default_decay, TID tid=TID_ZERO) const;
    /** randomly sample index and export features to given pointers (must be zero-filled) */
    void sample_feature_labels_to(int offset,
                                  nn_input_element *input_buf,
                                  int32_t *policy_buf, float *value_buf, nn_input_element *aux_buf,
                                  int decay=default_decay, TID tid=TID_ZERO) const;

    /** @internal make a state after the first `n` moves
     * marked as internal due to lack of the safety in make_move
     */
    BaseState make_state(int n) const;

    /** sample int in range [0, limit-1], with progressive 1/2 weight for the opening moves, 2^{-decay} for initial position */
    static int weighted_sampling(int limit, int N=default_decay, TID tid=TID_ZERO);
    static constexpr int default_decay=11;
  };
}

#endif
// MINIOSL_FEATURE_H
