#ifndef MINIOSL_RECORD_H
#define MINIOSL_RECORD_H
#include "impl/hash.h"
#include "impl/more.h"
#include <filesystem>
#include <sstream>
#include <tuple>

namespace osl
{
  /**
   * A game record.
   *
   * @internal
   * Incrementally constructed by the following sequence of method calls:
   * - set_initial_state(), 
   * - append_move() (x #moves)
   * - guess_result() (optional)
   * - settle_repetition()
   */
  struct MiniRecord {
    /** initial state */
    EffectState initial_state;
    /** moves */
    std::vector<Move> moves;
    /** history status of moves.size()+1 to detect repetition */
    std::vector<HashStatus> history;
    /** resign or DeclareWin if game has the winner, PASS in InGame or end by repetition  */
    Move final_move = Move::PASS(BLACK);
    /** result of the game or `InGame` unless completed */
    GameResult result = InGame;

    int state_size() const { return history.size(); }
    /** numebr of moves */
    int move_size() const { return moves.size(); }
    /** test game is completed */
    bool has_winner() const { return osl::has_winner(result); }
    std::vector<std::array<uint64_t,4>> export_all(bool flip_if_white_to_move=true) const;
    std::vector<std::array<uint64_t,5>> export_all320(bool flip_if_white_to_move=true) const;
    /** @internal export latest state */
    std::array<uint64_t,5> export320(bool flip_if_white_to_move=true) const;
    /**
     * return number of occurrence of the specified state. 
     * @param id = index if positive otherwise rollback from current (the last item), i.e., 0 for current
     */
    int repeat_count(int id=0) const {
      int now = resolve_id(id);
      const auto& cur = history.at(now);
      return cur.history.count;
    }
    bool has_repeat_state(int id=0) const { return repeat_count(id) > 0; }
    /** state (not move) index of repeating, only meaningful if has_repeat_state()
     * @param id = index if positive otherwise rollback from current (the last item), i.e., 0 for current
     */
    int previous_repeat_index(int id=0) const {
      int now = resolve_id(id);
      return now - history.at(now).history.prev_dist*2;
    }

    /** number of consecutive in-check states
     * @param id = index if positive otherwise rollback from current (the last item), i.e., 0 for current
     */
    int consecutive_in_check(int id=0) const { return osl::consecutive_in_check(history, resolve_id(id)); }
    
    void guess_result(const EffectState& final);
    void settle_repetition();

    void set_initial_state(const BaseState& state) {
      *this = MiniRecord { EffectState(state) };
      history.emplace_back(HashStatus(initial_state));
    }
    /**
     * @internal
     * append a new move to the record
     * @param moved move to append
     * @param in_check status after make_move
     */
    void append_move(Move moved, bool in_check);
    MiniRecord branch_at(int idx);
    /** set `state` as `idx`-th state */
    void replay(EffectState& state, int idx);
    
    friend inline bool operator==(const MiniRecord&, const MiniRecord&) = default;
    friend inline bool operator!=(const MiniRecord&, const MiniRecord&) = default;
    int resolve_id(int id) const {
      int now = (id <= 0) ? moves.size()+id : id;  // history[moves.size()] == history.back()
      assert(0 <= now && now < history.size());
      return now;
    }

    static constexpr int draw_limit = 320;
  };
  /** a set of `MiniRecord` s */
  struct RecordSet {
    RecordSet() {}
    RecordSet(const std::vector<MiniRecord>& v) : records(v) {}

    std::vector<MiniRecord> records;

    /** read csa files in folder */
    static RecordSet from_path(std::string folder_path, int limit=-1);
    /** read usi lines */
    static RecordSet from_usi_lines(std::istream&);
    static RecordSet from_usi_file(std::string);
  };

  std::string to_csa(const BaseState&);
  std::string to_csa(Move);
  std::string to_csa_extended(Move);
  std::string to_csa(Square);
  std::string to_csa(Ptype);
  std::string to_csa(Piece);
  std::string to_csa(Player);
  std::string to_csa(const Move *first, const Move *last);
  std::string to_csa(Move, std::string& buf);
  std::string to_csa(Square, std::string& buf, size_t offset=0);
  std::string to_csa(Ptype, std::string& buf, size_t offset=0);
  std::string to_csa(Player, std::string& buf, size_t offset=0);

  /**
   * CSA形式.
   * CSA形式の定義 http://www.computer-shogi.org/wcsc12/record.html
   */
  namespace csa {
    Move to_move_light(const std::string& s,const BaseState& st);
    Move to_move(const std::string& s,const EffectState& st);
    Player to_player(char c);
    Square to_square(const std::string& s);
    Ptype to_ptype(const std::string& s);

    MiniRecord read_record(const std::filesystem::path& filename);
    /** read record from csa file */
    MiniRecord read_record(std::istream& is);
    namespace detail {
      bool parse_state_line(BaseState&, std::string element, CArray<bool,9>&);
      GameResult parse_move_line(EffectState&, MiniRecord&, std::string element);
    }
    inline MiniRecord read_record(std::string str)  {
      std::istringstream is(str);
      return read_record(is);
    }
    /** read state from csa file */
    inline EffectState read_board(const std::string& str) { return read_record(str).initial_state; }
    struct ParseError : public std::domain_error {
      ParseError(const std::string& w) : std::domain_error(w) {}
    };
  } // namespace csa
} // namespace osl

// usi.h
namespace osl
{
  std::string to_usi(Move);
  std::string to_usi(PtypeO);
  inline std::string to_usi(Piece p) { return to_usi(p.ptypeO()); }
  std::string to_usi(const BaseState&);
  std::string to_usi(const MiniRecord&);
  namespace usi {
    Move to_move(const std::string&, const EffectState&);
    PtypeO to_ptypeo(char);
    
    /**
     * @internal
     * 盤面のみを解析する. 持ち駒や手番は読まない
     * 
     * @param board USIの文字列
     * @param state boardの解析結果が出力される
     */
    void parse_board(const std::string& board, BaseState& state);
    /**  parse string with usi syntax `[sfen <sfenstring> | startpos ] moves <move1> ... <movei>` */
    void parse(const std::string& line, EffectState&);

    /** read usi record */
    MiniRecord read_record(std::string line);
    /** read state in usi */
    EffectState to_state(const std::string& line);

    class ParseError : public std::domain_error {
    public:
      ParseError(const std::string& msg = "") : domain_error(msg) { }
    };
  }

  /**
   * gnushogi で使われるフォーマット.
   * 何種類かある．
   */
  namespace psn {
    Move to_move_light(const std::string&, const BaseState&);
    Move to_move(const std::string&, const EffectState&);
    Square to_square(const std::string&);
    Ptype to_ptype(char);

    class ParseError : public std::domain_error {
    public:
      ParseError(const std::string& msg = "") : domain_error(msg) {}
    };
  }
  std::string to_psn(Move);
  std::string to_psn(Square);
  char to_psn(Ptype);
  /** decorate capture by 'x', promote by '+', and unpromote by '=' */
  std::string to_psn_extended(Move);

  // ki2
  /** return japanese representation of `move` */
  std::u8string to_ki2(Move move, const EffectState&, Square prev=Square());
  std::u8string to_ki2(Square);
  std::u8string to_ki2(Square cur, Square prev);
  std::u8string to_ki2(Player);
  std::u8string to_ki2(Ptype);
  namespace kanji {
    /** read japanese representation of move */
    Move to_move(std::u8string, const EffectState&, Square last_to=Square());
    Square to_square(std::u8string);
    Ptype to_ptype(std::u8string);
    Player to_player(std::u8string);

    extern const std::u8string suji[], dan[], ptype_name[], promote_flag[], sign[], sign_alt[];
    extern const std::u8string K_ONAZI, K_MIGI, K_HIDARI, K_SPACE;

    class ParseError : public std::domain_error {
    public:
      ParseError(const std::string& msg = "") : domain_error(msg) {}
    };
    // hopefully addressed in c++23
    inline auto debugu8(const std::u8string& s) { return reinterpret_cast<const char*>(s.c_str()); }
  }

  // kif
  namespace kifu {
    Move to_move(const std::u8string&, const BaseState&, Square last_to=Square());
    std::pair<Player,Ptype> to_piece(const std::u8string&);
    MiniRecord read_record(const std::filesystem::path& filename);
    MiniRecord read_record(std::istream& is);

    namespace detail {
      /** @internal */
      void parse_line(BaseState& state, MiniRecord& record, std::u8string s, CArray<bool,9>& board_parsed);
    }
  }
} // osl

#endif
// MINIOSL_RECORD_H
