#ifndef MINIOSL_RECORD_H
#define MINIOSL_RECORD_H
#include "state.h"
#include <filesystem>
#include <sstream>
#include <tuple>

// csa.h
namespace osl
{
  enum GameResult { BlackWin, WhiteWin, Draw, InGame };
  constexpr GameResult win_result(Player P) { return P == BLACK ? BlackWin : WhiteWin; }
  constexpr GameResult loss_result(Player P) { return P == BLACK ? WhiteWin : BlackWin; }
  constexpr bool has_winner(GameResult r) { return r == BlackWin || r == WhiteWin; }
  struct MiniRecord {
    EffectState initial_state;
    std::vector<Move> moves;
    /** to distinguish resign or DeclareWin if game has the winner */
    Move final_move;
    GameResult result = InGame;

    bool has_winner() const { return osl::has_winner(result); }
    std::vector<std::array<uint64_t,4>> export_all() const;
    
    void guess_result(const EffectState& final);
    
    friend inline bool operator==(const MiniRecord&, const MiniRecord&) = default;
    friend inline bool operator!=(const MiniRecord&, const MiniRecord&) = default;
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
  namespace csa
  {
    struct ParseError : public std::runtime_error {
      ParseError(const std::string& w) : std::runtime_error(w) {}
    };

    Move to_move_light(const std::string& s,const BaseState& st);
    Move to_move(const std::string& s,const EffectState& st);
    Player to_player(char c);
    Square to_square(const std::string& s);
    Ptype to_ptype(const std::string& s);

    MiniRecord read_record(const std::filesystem::path& filename);
    MiniRecord read_record(std::istream& is);
    namespace detail {
      bool parse_state_line(BaseState&, MiniRecord&, std::string element, CArray<bool,9>&);
      GameResult parse_move_line(EffectState&, MiniRecord&, std::string element);
    }
    inline MiniRecord read_record(std::string str)  {
      std::istringstream is(str);
      return read_record(is);
    }
    inline EffectState read_board(const std::string& str) { return read_record(str).initial_state; }
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
    
    class ParseError : public std::invalid_argument {
    public:
      ParseError(const std::string& msg = "") : invalid_argument(msg) { }
    };

    /** 
     * 盤面を取得する. 
     * board文字列が不正なときは、ParseErrorがthrowされる. 
     * @param board USIの文字列
     * @param state boardの解析結果が出力される
     */
    void parse_board(const std::string& board, BaseState&);
    /**  [sfen <sfenstring> | startpos ] moves <move1> ... <movei> */
    void parse(const std::string& line, EffectState&);

    MiniRecord read_record(std::string line);

    EffectState to_state(const std::string& line);
  }

  /**
   * gnushogi で使われるフォーマット.
   * 何種類かある．
   */
  namespace psn
  {
    class ParseError : public std::invalid_argument {
    public:
      ParseError(const std::string& msg = "") : invalid_argument(msg) {}
    };
    Move to_move_light(const std::string&, const BaseState&);
    Move to_move(const std::string&, const EffectState&);
    Square to_square(const std::string&);
    Ptype to_ptype(char);
  }
  std::string to_psn(Move);
  std::string to_psn(Square);
  char to_psn(Ptype);
  /** decorate capture by 'x', promote by '+', and unpromote by '=' */
  std::string to_psn_extended(Move);

  // ki2
  std::u8string to_ki2(Move, const EffectState&, Square prev=Square());
  std::u8string to_ki2(Square);
  std::u8string to_ki2(Square cur, Square prev);
  std::u8string to_ki2(Player);
  std::u8string to_ki2(Ptype);

  typedef __uint128_t uint128_t;
  namespace bitpack
  {
    /** packed position for ML
     * - uint128_t board: 81;
     * - uint32_t order_hi: 30;
     * - uint64_t order_lo: 57;
     * - uint64_t color: 38;
     * - uint64_t promote: 34;
     * - uint32_t turn: 1;
     * - uint32_t move: 12;
     * - uint32_t game_result: 2;
     * - uint32_t reserved: 1;
     */
    typedef std::array<uint64_t,4> B256;    
    /** an instance of training data for prediction of the next move or game reselt given state */
    struct StateLabelTuple {
      BaseState state = BaseState(HIRATE);
      Move next;
      GameResult result = InGame;

      B256 to_bitset() const;
      void restore(B256 binary);
    };
    /** compress move to 12bit (depending on a current state) */
    uint32_t encode12(const BaseState& state, Move move);
    Move decode_move12(const BaseState& state, uint32_t code);
    constexpr uint32_t move12_resign = 0, move12_win_declare = 127;

    /** to save a set of game records in npz.
     * @return number of uint64s appended
     */
    int append_binary_record(const MiniRecord&, std::vector<uint64_t>&);
    /** read a record and advance ptr
     * @return number of uint64s read
     */
    int read_binary_record(const uint64_t*& ptr, MiniRecord&);
    
    namespace detail {
      uint64_t combination_id(int first, int second);
      uint64_t combination_id(int first, int second, int third);
      uint64_t combination_id(int first, int second, int third, int fourth);
      std::pair<int,int> unpack2(uint32_t code);
      std::tuple<int,int,int,int> unpack4(uint64_t code);
    }
  } // bitpack
  using bitpack::B256;
  using bitpack::StateLabelTuple;  
} // osl


#endif
// MINIOSL_RECORD_H
