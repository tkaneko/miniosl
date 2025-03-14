#ifndef MINIOSL_FEATURE_H
#define MINIOSL_FEATURE_H

#include "state.h"
#include "infer.h"
#include <unordered_map>
#include <optional>

namespace osl {
  struct MiniRecord;
  namespace ml {
    std::array<int8_t,81> board_dense_feature(const BaseState& state);
    std::array<int8_t,piece_stand_order.size()*2> hand_dense_feature(const BaseState& state);

    void board_feature(const BaseState& state, nn_input_element /*30ch*/ *planes);
    void hand_feature(const BaseState& state, nn_input_element /*14ch*/ *planes);

    /** covered squares by long piece */
    void lance_cover(const EffectState& state, nn_input_element /*4ch*/ *planes);
    void bishop_cover(const EffectState& state, nn_input_element /*4ch*/ *planes);
    void rook_cover(const EffectState& state, nn_input_element /*4ch*/ *planes);
    void king_visibility(const EffectState& state, nn_input_element /*2ch*/ *planes);
    void check_piece(const EffectState& state, nn_input_element /*1ch*/ *plane);
    /** checkmate or threatmate */
    void mate_path(const EffectState& state, nn_input_element /*2ch*/ *planes);
    void checkmate_if_capture(const EffectState& state, Square sq, nn_input_element /*3ch*/ *planes);

    void color_of_piece(const BaseState& state, nn_input_element /* 2ch */ *planes);
    void piece_changed_cover(const EffectState& state, nn_input_element /* 2ch */ *planes);
    void cover_count(const EffectState& state, nn_input_element /* 2ch */ *planes);
    
    namespace impl  {
      /** fill 1 on the path from src (exclusive) to dst (inclusive) */
      void fill_segment(Square src, Square dst, nn_input_element /*1ch*/ *out);
      inline void fill_segment(Piece p, Square dst, Player owner, nn_input_element /*2ch*/ *out) {
        fill_segment(p.square(), dst, out + idx(owner)*81);
      }
      void fill_move_trajectory(Move move, nn_input_element /* 1ch */ *out);
      void fill_ptypeo(const BaseState& state, Square sq, PtypeO ptypeo, nn_input_element /*1ch*/ *out);
      void fill_empty(const BaseState& state, Square src, Offset diff, nn_input_element /*1ch*/ *out);
    }
    using impl::fill_segment;
    using impl::fill_empty;
    using impl::fill_move_trajectory;
    using impl::fill_ptypeo;
    namespace helper {
      // 44ch
      void write_np_44ch(const BaseState& state, nn_input_element *);
      // +13 ch
      void write_np_additional(const EffectState& state, bool flipped, nn_input_element *);
      /** write state features i.e., w/o history (44+{heuristic_channels}ch) */
      void write_state_features(const EffectState& state, bool flipped, nn_input_element *);
      // 4ch
      /**
       * @internal write history features and update state applying `last_move`
       * @param ptr must be zero-filled in advance
       */
      void write_np_history(EffectState& state, Move last_move, nn_input_element *ptr);
      /** @internal write history features and update state applying moves in the history 
       * @param ptr must be zero-filled in advance
       */ 
      void write_np_histories(EffectState& state, const MoveVector& history, nn_input_element *ptr);
      // status after move
      void write_np_aftermove(EffectState state, Move move, nn_input_element *aux_label);
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

    constexpr int legalmove_bs_sz = (policy_unit+7)/8;
    /** @internal export features primary for game playing */
    inline void set_in_uint8bit_vector(uint8_t *buf, int id) {
      int q = id / 8, r = id % 8;
      buf[q] |= (1u << r);
    }
    void set_legalmove_bits(const MoveVector&, uint8_t *buf);
  }

  /** subset of MiniRecord assuming completed game with the standard initial state */
  struct SubRecord {
    /** moves */
    std::vector<Move> moves;
    GameVariant variant=HIRATE;
    int shogi816k_id=0;
    /** resign or DeclareWin if game has the winner */
    Move final_move;
    /** result of the game or `InGame` if not yet initialized */
    GameResult result = InGame;

    SubRecord() = default;
    SubRecord(const MiniRecord& record);
    SubRecord(MiniRecord&& record);

    BaseState initial_state() const;

    bool is_hirate_game() const {
      return variant == HIRATE;
    }
    /** export features and labels */
    void export_feature_labels(int idx, nn_input_element *input,
                               int& move_label, int& value_label, nn_input_element *aux_label,
                               MoveVector& legal_moves) const;
    /** randomly sample index and call export_feature_labels() */
    void sample_feature_labels(nn_input_element *input,
                               int& move_label, int& value_label, nn_input_element *aux_label,
                               uint8_t *legalmove_buf=nullptr,
                               int decay=default_decay, TID tid=TID_ZERO) const;
    /** randomly sample index and export features to given pointers (must be zero-filled) */
    void sample_feature_labels_to(int offset,
                                  nn_input_element *input_buf,
                                  int32_t *policy_buf, float *value_buf, nn_input_element *aux_buf,
                                  nn_input_element *input2_buf,
                                  uint8_t *legalmove_buf,
                                  uint16_t *sampled_id_buf,
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
