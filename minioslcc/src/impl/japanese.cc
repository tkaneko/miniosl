#include "record.h"
#include <algorithm>
#include <ranges>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <cstdlib>

// ki2
const std::u8string osl::kanji::suji[] = {
  u8"", u8"１", u8"２", u8"３", u8"４", u8"５", u8"６", u8"７", u8"８", u8"９", };
const std::u8string osl::kanji::dan[] = {
      u8"", u8"一", u8"二", u8"三", u8"四", u8"五", u8"六", u8"七", u8"八", u8"九", };

namespace osl
{
  namespace kanji
  {
    const std::u8string K_NARU = u8"成", K_FUNARI = u8"不成", K_ONAZI = u8"同",
      K_PASS = u8"(パス)", K_UTSU = u8"打",
      K_YORU = u8"寄", K_HIKU = u8"引", K_UE = u8"上", K_HIDARI = u8"左", K_MIGI = u8"右", K_SUGU = u8"直";
    const std::u8string K_SHITA = u8"下", K_YUKU = u8"行", K_RESIGN = u8"投了", K_SPACE = u8"　";
    const std::u8string ptype_name[] = {
      u8"",  u8"",
      u8"と", u8"成香", u8"成桂", u8"成銀", u8"馬", u8"龍",
      u8"王", u8"金", u8"歩", u8"香", u8"桂", u8"銀", u8"角", u8"飛",
    };
    // unofficial variant to show all pieces in a single letter
    const std::u8string ptype_name1[] = {
      u8"",  u8"",
      u8"と", u8"杏", u8"圭", u8"全", u8"馬", u8"龍",
      u8"王", u8"金", u8"歩", u8"香", u8"桂", u8"銀", u8"角", u8"飛",
    };
    const std::u8string K_GYOKU = u8"玉", K_RYU_alt = u8"竜";
    const std::u8string promote_flag[] = { K_FUNARI, K_NARU, };
    const std::u8string sign[] = { u8"☗", u8"☖", };
    const std::u8string sign_alt[] = { u8"▲", u8"△", };
    std::unordered_map<std::u8string, Ptype> j2ptype {
      { ptype_name[idx(PPAWN)],   PPAWN },
      { ptype_name[idx(PLANCE)],  PLANCE },
      { ptype_name[idx(PKNIGHT)], PKNIGHT },
      { ptype_name[idx(PSILVER)], PSILVER },
      { ptype_name[idx(PBISHOP)], PBISHOP },
      { ptype_name[idx(PROOK)],   PROOK },
      { ptype_name[idx(KING)],   KING },
      { ptype_name[idx(GOLD)],   GOLD },
      { ptype_name[idx(PAWN)],   PAWN },
      { ptype_name[idx(LANCE)],  LANCE },
      { ptype_name[idx(KNIGHT)], KNIGHT },
      { ptype_name[idx(SILVER)], SILVER },
      { ptype_name[idx(BISHOP)], BISHOP },
      { ptype_name[idx(ROOK)],   ROOK },
      // 
      { K_GYOKU, KING }, { K_RYU_alt, PROOK },
    };

    // for kifu
    const std::u8string K_BLACK = u8"先手", K_WHITE = u8"後手", K_COLON = u8"：",
      K_KAISHI = u8"開始", K_MOCHIGOMA = u8"持駒", K_NASHI = u8"なし", K_KISEN = u8"棋戦",
      K_K10 = u8"十", K_TESUU = u8"手数", K_TORYO = u8"投了", K_HENKA = u8"変化";
  }
}
std::u8string osl::to_ki2(Square sq) {
  if (sq.isPieceStand())
    return u8"";
  return kanji::suji[sq.x()] + kanji::dan[sq.y()];
}

std::u8string osl::to_ki2(Ptype ptype) {
  return kanji::ptype_name[idx(ptype)];
}

std::u8string osl::to_ja1(Ptype ptype) {
  return kanji::ptype_name1[idx(ptype)];
}

std::u8string osl::to_ki2(Square cur, Square prev) {
  return (cur == prev) ? kanji::K_ONAZI : to_ki2(cur);
}
std::u8string osl::to_ki2(Move m, const EffectState& state, Square prev) {
  if (m == Move::Resign()) {
    return kanji::sign[idx(state.turn())] + kanji::K_RESIGN;
  }
  const Player player = m.player();
  std::u8string ret = kanji::sign[idx(player)];
  if (m.isPass()) {
    ret += kanji::K_PASS;
    return ret;
  }
  if (! state.isAcceptable(m))
    throw kanji::ParseError("to_ki2 invalid move"+to_csa(m));
  const Square from = m.from(), to = m.to();
  const Ptype ptype = m.oldPtype();
  mask_t pieces = state.effectAt(player, to).to_ullong() & piece_id_set(ptype);
  const mask_t promoted = state.promotedPieces().to_ullong();
  if (is_promoted(ptype))
    pieces &= promoted;
  else
    pieces &= ~promoted;
  if (from.isPieceStand()) {
    ret += to_ki2(to) + to_ki2(ptype);
    int has_effect = 0;
    for (int id: to_range(pieces))
      if (state.pieceOf(id).ptype() == ptype)
	++has_effect;

    if (has_effect)
      ret += kanji::K_UTSU;
    return ret;
  }
  ret += prev.isOnBoard() && (to == prev) ? kanji::K_ONAZI : to_ki2(to);
  ret += to_ki2(m.oldPtype());
  const int count = std::popcount(pieces);
  if (count == 0)
    throw kanji::ParseError("to_ki2 no candidate pieces");
  if (count >= 2) {
    CArray<int,3> x_count = {{ 0 }}, y_count = {{ 0 }};
    int my_x = 0, my_y = 0;
    for (int id: to_range(pieces)) {
      const Piece p = state.pieceOf(id);
      if (p.ptype() != ptype)
	continue;
      int index_x = 1, index_y = 1;
      if (p.square().x() != to.x())
	index_x = ((p.square().x() - to.x()) * sign(player) > 0)
	  ? 2 : 0;
      if (p.square().y() != to.y())
	index_y = ((p.square().y() - to.y()) * sign(player) > 0)
	  ? 2 : 0;
      if (p.square() == from)
	my_x = index_x, my_y = index_y;
      x_count[index_x]++;
      y_count[index_y]++;
    }
    if (y_count[my_y] == 1) {
      if (from.y() == to.y()) 
	ret += kanji::K_YORU;
      else if ((to.y() - from.y())*sign(player) > 0)
	ret += kanji::K_HIKU;
      else
	ret += kanji::K_UE;
    }
    else if (x_count[my_x] == 1) {
      if (from.x() == to.x()) {
	if (is_promoted(ptype) && is_major(ptype)) {
          bool left = (my_x == 2) || (my_x == 1 && x_count[0] > 0);
	  if (left)
	    ret += kanji::K_HIDARI;
	  else
	    ret += kanji::K_MIGI;
	}
	else 
	  ret += kanji::K_SUGU;
      }
      else if ((to.x() - from.x())*sign(player) > 0)
	ret += kanji::K_MIGI;
      else
	ret += kanji::K_HIDARI;
    }
    else if (from.x() == to.x()) {
      if ((to.y() - from.y())*sign(player) > 0)
	ret += kanji::K_HIKU;
      else
	ret += kanji::K_SUGU;
    }
    else {
      if ((to.x() - from.x())*sign(player) > 0)
	ret += kanji::K_MIGI;
      else if (to.x() != from.x())
	ret += kanji::K_HIDARI;
      if (to.y() == from.y())
        ret += kanji::K_YORU;
      else if ((to.y() - from.y())*sign(player) > 0)
	ret += kanji::K_HIKU;
      else
	ret += kanji::K_UE;
    }
  }
  if (can_promote(m.oldPtype()))
    if (m.isPromotion()
	|| to.isPromoteArea(player) || from.isPromoteArea(player)) {
      ret += kanji::promote_flag[m.isPromotion()];
  }
  return ret;
}

osl::Square osl::kanji::to_square(std::u8string s) {
  if (s.size() != 6)
    return Square();
  auto p = std::ranges::find(suji, s.substr(0,3));
  auto q = std::ranges::find(dan, s.substr(3,6));
  if (p == end(suji) || q == end(dan))
    return Square();
  return Square(p - begin(suji), q - begin(dan));
}
osl::Ptype osl::kanji::to_ptype(std::u8string s) {
  auto p = j2ptype.find(s);
  return (p == j2ptype.end()) ? Ptype() : p->second;
}

osl::Player osl::kanji::to_player(std::u8string s) {
  if (s.starts_with(sign[0]) || s.starts_with(sign_alt[0])) return BLACK;
  if (s.starts_with(sign[1]) || s.starts_with(sign_alt[1])) return WHITE;
  throw ParseError(debugu8(u8"kanji::to_player "+s));
}

namespace osl {
  namespace kanji {    
    void remove_moves(MoveVector& vec, auto pred) {
      auto [f, l] = std::ranges::remove_if(vec, pred);
      vec.erase(f, l);
    }
    /** choose 1 move and remove others from found, return unread strings expected to be empty */
    std::u8string select_candidates(MoveVector& found, std::u8string str,
                                    Square to_pos, Player player) {
      if (str.empty())
        throw ParseError(debugu8(u8"select_candidates "+str));
      if (found.size() < 2)
        throw std::logic_error("size? "+std::to_string(found.size()));

      auto sgn = osl::sign(player);
      while (found.size() >= 2) {
        if (str.empty())
          throw ParseError(debugu8(u8"insufficient specification "+str));
        bool right=str.starts_with(K_MIGI), left=str.starts_with(K_HIDARI),
          down=str.starts_with(K_HIKU) || str.starts_with(K_SHITA), up=str.starts_with(K_UE);
        if (right || left) {
          std::ranges::sort(found, [](Move l, Move r){ return l.from().x() < r.from().x(); });
          if ((right && player == BLACK) || (left && player == WHITE)) {
            const auto th = found.front().from().x(); // min
            remove_moves(found, [=](auto move){ return move.from().x() > th; }); 
          }
          else {
            const auto th = found.back().from().x(); // max
            remove_moves(found, [=](auto move){ return move.from().x() < th; }); 
          }
        } 
        else if (down || up) {
          std::ranges::sort(found, [](Move l, Move r){ return l.from().y() < r.from().y(); });
          if ((down && player == BLACK) || (up && player == WHITE)) {
            const auto th = found.front().from().y(); // min
            remove_moves(found, [=](auto move){ return move.from().y() > th; }); 
          }
          else {
            const auto th = found.back().from().y(); // max
            remove_moves(found, [=](auto move){ return move.from().y() < th; }); 
          }
        }
        else if (str.starts_with(K_YORU)) {
          remove_moves(found, [=](auto move){ return move.from().y() != to_pos.y(); }); 
        }
        else if (str.starts_with(K_SUGU)) {
          auto target_y = to_pos.y() + sgn;
          remove_moves(found, [=](auto move){
            return move.from().x() != to_pos.x() || move.from().y() != target_y;
          });
        }
        else if (str.starts_with(K_YUKU)) {
          remove_moves(found, [=](auto move) { return move.from().y()*sgn <= to_pos.y()*sgn; }); 
        }
        str = str.substr(3);
        if (found.empty())
          throw ParseError("no candidate moves");
      } // while
      return str;
    }
    constexpr int unit = 3;        // in utf-8, most Japanese characters consumes 3 bytes each
  } // kanji
}   // osl

osl::Move osl::kanji::to_move(std::u8string str, const osl::EffectState& state, Square last_to) {
  auto orig = str;
  if (str.find(K_RESIGN) != str.npos)
    return Move();
  if (! (str.size() >= 4*unit
	 || (str.size() >= 3*unit
	     && (str.substr(unit,unit) == K_ONAZI
		 || (isdigit(str[unit]) && isdigit(str[unit+1]))))))
    throw ParseError(debugu8(u8"length too short" + str));
  const auto player = to_player(str);
  if (player != state.turn())
    throw ParseError(debugu8(u8"turn in to_move " + str));
  str = str.substr(unit);

  Square to_pos;
  if (str.starts_with(K_ONAZI)) {
    to_pos = last_to;
    if (to_pos.isPieceStand())
      throw ParseError("K_ONAZI needs last_to onboard");
    str = str.substr(unit);
    if (str.starts_with(K_SPACE))
      str = str.substr(unit);
  }
  else if (isdigit(str[0]) && isdigit(str[1])) {
    to_pos = Square(str[0]-'0', str[1]-'0');
    str = str.substr(2);
  }
  else {
    to_pos = to_square(str.substr(0,unit*2));
    str = str.substr(unit*2);
  }

  Ptype ptype;
  if (str.starts_with(K_NARU)) {  // PLANCE, PKIGHT, PSILVER
    ptype = to_ptype(str.substr(0,unit*2));
    str = str.substr(unit);
  }
  else {
    ptype = to_ptype(str.substr(0,unit));
    str = str.substr(unit);
  }

  // promote or not
  bool is_promote = false;
  if (str.starts_with(K_FUNARI))
    str = str.substr(unit*2);
  else if (str.ends_with(K_FUNARI))
    str = str.substr(0, str.size()-unit*2);
  else if (str.starts_with(K_NARU)) {
    is_promote = true;
    str = str.substr(unit);
  }
  else if (str.ends_with(K_NARU)) {
    is_promote = true;
    str = str.substr(0, str.size()-unit);
  }

  MoveVector moves;
  move_generator::Capture::generateOfTurn(state, to_pos, moves);
  if (is_basic(ptype) && state.pieceAt(to_pos).isEmpty() && state.hasPieceOnStand(player, ptype))
    moves.emplace_back(to_pos, ptype, player);

  MoveVector found;
  for (Move move: moves) {
    if (move.oldPtype() == ptype /* &&  move.to() == to_pos*/) {
      if (move.isPromotion() == is_promote) 
        found.push_back(move);
      else if (move.hasIgnoredUnpromote())
        found.push_back(move.unpromote());
    } 
  }
  if (found.empty()) {
    // there is no leagal move
    return Move::Resign();
  }
  assert(!found.empty());

  // Single candidate
  if (found.size() == 1)
    return found.front();

  // Multiple candidates
  assert(found.size() >= 2);

  // drop
  if (str.starts_with(K_UTSU)) {
    auto it = std::find_if(found.begin(), found.end(), [](Move m){ return m.isDrop(); });
    str = str.substr(unit);
    assert(str.empty());
    if (it == found.end())
      throw ParseError("no legal drop");
    return *it;
  }
  else {
    remove_moves(found, [](Move m){ return m.isDrop(); });
    if (found.size() == 1)
      return found.front();
  }

  // Multiple candidates
  assert(found.size() >= 2);
  if (str.empty())
    throw ParseError("insufficient representation for multiple candidates");
  auto unused = select_candidates(found, str, to_pos, player);
  if (!unused.empty()) {
    bool has_reason = (state.pinOrOpen(player) & state.effectAt(player, to_pos)).to_ullong()
      & piece_id_set(ptype);
    if (! has_reason)
      std::cerr << "ignored somechars " << debugu8(unused) << " of " << debugu8(orig) << std::endl
                << state << last_to << '\n';
  }
  assert(found.size() == 1);
  return found.front();
}

// .kif
std::pair<osl::Player,osl::Ptype> osl::
kifu::to_piece(const std::u8string& s) {
  if (s.size() != kanji::unit+1 || (s[0] != 'v' && s[0] != ' '))
    throw std::domain_error(kanji::debugu8(u8"unexpected piece in kifu" + s));
  const Player pl = s[0] == ' ' ? BLACK : WHITE;
  const Ptype ptype = kanji::to_ptype(s.substr(1));
  return std::make_pair(pl, ptype);
}

osl::Move osl::
kifu::to_move(const std::u8string& s, const BaseState& state, Square last_to) {
  using namespace kanji;

  auto sep0 = s.find(' ');
  if (sep0 == s.npos)
    throw std::domain_error(debugu8(u8"kifu::to_move: unexpected line0" + s));
  
  auto sep1 = s.find('(', sep0);
  if (sep1 == s.npos)
    throw std::domain_error(debugu8(u8"kifu::to_move: unexpected line1" + s));
  int move_number = atoi(debugu8(s.substr(0, sep0)));
  std::u8string move_string = s.substr(sep0, sep1-sep0);
  move_string = move_string.substr(move_string.find_first_not_of(' '));

  Square to, from;
  if (move_string.substr(0, kanji::unit) == K_ONAZI)
    to = last_to;
  else
    to = kanji::to_square(move_string.substr(0, kanji::unit*2));
  //std::cerr << move_number << ' ' << debugu8(move_string) << ' ' << to << '\n';
  
  if (to == Square())		// resign?
    return Move();
  
  Ptype ptype;
  size_t cur = kanji::unit*2;
  if (move_string.substr(cur, kanji::unit) == K_NARU) { // PLANCE, PKIGHT, PSILVER
    assert(move_string.size() >= cur+kanji::unit*2);
    ptype = kanji::to_ptype(move_string.substr(cur,kanji::unit*2));
    cur += kanji::unit*2;
  }
  else {
    ptype = kanji::to_ptype(move_string.substr(cur,kanji::unit));
    cur += kanji::unit;
  }
  if (move_string.size() >= cur+kanji::unit && move_string.substr(cur,kanji::unit)
      == K_UTSU)
    from = Square();
  else {
    auto sep2 = s.find(')', sep1);
    if (sep2 == s.npos)
      throw std::domain_error(debugu8(u8"kifu::to_move: unexpected line2" + s));
    int from_number = atoi(debugu8(s.substr(sep1+1, sep2-sep1-1)));
    // std::cerr << "atoi " << from_number << " for " << debugu8(s.substr(sep1, sep2-sep1)) << '\n';
    from = Square(from_number / 10, from_number % 10);
  }
  // std::cerr << ptype << " from" << ' ' << from << '\n';
  
  bool is_promote = false;
  if (move_string.size() >= cur+kanji::unit && move_string.substr(cur,kanji::unit) == K_NARU)
    is_promote = true;

  if (from.isPieceStand())
    return Move(to, ptype, state.turn());
  Ptype captured = state.pieceOnBoard(to).ptype();
  return Move(from, to, is_promote ? promote(ptype) : ptype,
	      captured, is_promote, state.turn());
}

void osl::kifu::detail::parse_line(BaseState& state, MiniRecord& record, 
                                   std::u8string s, CArray<bool,9>& board_parsed) {
  using namespace kanji;
  //   std::cerr << "reading " << debugu8(s) << '\n';
  // header
  if (s[0] == '|') {
    if (s.size() < 1+(1+kanji::unit)*9+1+kanji::unit)
      throw std::domain_error("board shorter than expected in kifu ");
    const int y = std::find(begin(dan), end(dan), s.substr(s.size()-kanji::unit)) - begin(dan);
    if (! (1 <= y && y <= 9))
      throw std::domain_error("unexpected y in kifu ");
    board_parsed[y-1] = true;
    for (unsigned int x=9,i=1;i<s.length()&&x>0;i+=kanji::unit+1,x--) {
      std::pair<Player,Ptype> pp=to_piece(s.substr(i,kanji::unit+1));
      if (! is_piece(pp.second))
	continue;
      state.setPiece(pp.first, Square(x,y), pp.second);
    }
  }
  if (s.size() > kanji::unit*3) {
    if (s.find(K_BLACK + K_COLON) == 0) {
      // record.player[BLACK] = s.substr(6);
      return;
    }
    if (s.find(K_WHITE + K_COLON) == 0) {
      // record.player[WHITE] = s.substr(6);
      return;
    }
    if (s.find(K_KISEN + K_COLON) == 0)
    {
      // record.tournament_name = s.substr(6);
      return;
    }
    if (s.find(K_KAISHI) == 0) { //  + K_NICHIJI + K_COLON
      // record.start_date = date;
      return;
    }
    if (s.find(K_MOCHIGOMA + K_COLON) != s.npos
	&& s.find(K_NASHI) == s.npos) {
      auto piece_str = s.substr(s.find(K_COLON)+kanji::unit);
      // boost::algorithm::replace_all(piece_str, K_SPACE, " ");
      std::vector<std::u8string> pieces;
      int cur = 0;
      while (cur < piece_str.size()) {
        int next = piece_str.find(cur, ' ');
        if (next == std::string::npos)
          break;
        pieces.push_back(piece_str.substr(cur, next-cur));
        cur = next;
      }
      Player player;
      if (s.find(K_BLACK) == 0) player = BLACK;
      else if (s.find(K_WHITE) == 0) player = WHITE;
      else throw std::domain_error(debugu8(u8"error in stand "+ s));

      for (const auto& e: pieces) {
	if (e.empty()) continue;
	if (e.size() < kanji::unit) throw std::domain_error(debugu8(u8"error in stand "+ e));
	const Ptype ptype = kanji::to_ptype(e.substr(0,2));
	int n = 1;
	if (e.size() >= kanji::unit*2)
	  n = std::find(begin(dan), end(dan), e.substr(kanji::unit, kanji::unit)) - begin(dan);
	if (e.size() >= kanji::unit*3)
	  n = n * ((e.substr(kanji::unit,kanji::unit) == K_K10) ? 1 : 10)
	    + (std::find(begin(dan), end(dan), e.substr(kanji::unit*2,kanji::unit)) - begin(dan));
	for (int i=0; i<n; ++i)
	  state.setPiece(player, Square::STAND(), ptype);
      }
    }
  }

  // moves start
  if (s.find(K_TESUU + u8"--") == 0) {
    if (std::find(board_parsed.begin(), board_parsed.end(), true) == board_parsed.end()) {
      state.init(HIRATE);
      board_parsed.fill(true);
    }
    if (*std::min_element(board_parsed.begin(), board_parsed.end()) == false)
      throw std::domain_error("incomplete position description in kifu::parse_line");
    state.initFinalize();
    record.set_initial_state(state);
    return;
  }

  // moves
  if (s[0] == '*') {
    // comment
    return;
  }

  if (s[0] != ' ' && ! isdigit(s[0])) {
    // the original OSL expects that each move line startst with an white space. 
    // however it seems that it does not always hold, (e.g., ones exported from 81Dojo?)
    return;
  }
  if (s.find(K_TORYO) != s.npos) {
    record.result = ((state.turn() == BLACK) ? WhiteWin : BlackWin);
    return;
  }

  auto last_to = record.moves.empty() ? Square() : record.moves.back().to();
  const Move m = kifu::to_move(s, state, last_to);
  // std::cerr << "move " << m << '\n';
  if (m.isNormal()) {
    EffectState copy(state);
    if (! copy.isAcceptable(m)) 
      throw std::domain_error("invalid move in kifu");

    record.append_move(m, copy.inCheck());
    state.make_move_unsafe(m);
  }
}

osl::MiniRecord osl::kifu::read_record(const std::filesystem::path& path) {
  std::ifstream is(path);
  if (! is) 
    throw std::runtime_error("kifu::read_record file open failed "+path.string());
  return read_record(is);
}

osl::MiniRecord osl::kifu::read_record(std::istream& is) {
  osl::MiniRecord record;
  BaseState work;
  work.initEmpty();
  std::string line;
  CArray<bool, 9> board_parsed = {{ false }};

  while (std::getline(is, line)) {
    // quick aid for \r
    if ((! line.empty()) && (line[line.size()-1] == 13))
      line.erase(line.size()-1);
    if (line.length()==0) 
      continue;
    // skip variations
    if (line.find(kanji::debugu8(kanji::K_HENKA)) == 0)
      break;
    if (! line.empty() && line[0] == '#' 
	&& line.find("separator") != line.npos)
      break;			// tanase shogi
    detail::parse_line(work, record, std::u8string(reinterpret_cast<const char8_t*>(line.c_str())), board_parsed);
  }
  if (! work.check_internal_consistency())
    throw std::domain_error("kifu::read_record failed");
  return record;
}

// ;;; Local Variables:
// ;;; mode:c++
// ;;; c-basic-offset:2
// ;;; End:
