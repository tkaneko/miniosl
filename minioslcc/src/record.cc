#include "record.h"
#include "more.h"
#include <fstream>
#include <iostream>
#include <algorithm>

std::string osl::to_csa(const EffectState& state) {
  std::ostringstream ss;
  ss << state;
  return ss.str();
}

std::string osl::to_csa(Player player, std::string& buf, size_t offset) {
  assert(buf.size() >= offset+1);
  buf[offset] = (player==BLACK) ? '+' : '-';
  return buf;
}

std::string osl::to_csa(Move move, std::string& buf) {
  assert(buf.capacity() >= 7);
  buf.resize(7);
  if (move == Move::DeclareWin())
    return buf = "%KACHI";
  if (move.isInvalid())
    return buf = "%TORYO";
  if (move.isPass())
    return buf = "%PASS";		// FIXME: not in CSA protocol
  to_csa(move.player(), buf);
  to_csa(move.from(), buf, 1);
  to_csa(move.to(), buf, 3);
  to_csa(move.ptype(), buf, 5);
  return buf;
}

std::string osl::to_csa(Square pos, std::string& buf, size_t offset) {
  assert(buf.size() >= offset+2);
  if (pos.isPieceStand()) 
  {
    buf[0+offset] = '0';
    buf[1+offset] = '0';
    return buf;
  }
  const int x = pos.x();
  const int y = pos.y();
  buf[offset+0] = x + '0';
  buf[offset+1] = y + '0';
  return buf;
}

std::string osl::to_csa(Ptype ptype, std::string& buf, size_t offset) {
  assert(buf.size() >= offset+2);
  const char *name = ptype_csa_names[idx(ptype)];
  buf[0+offset] = name[0];
  buf[1+offset] = name[1];
  return buf;
}

std::string osl::to_csa(Move move) {
  // NOTE: copy コピーを返すので dangling pointer ではない
  std::string buf("+7776FU");
  return to_csa(move, buf);
}

std::string osl::to_csa_extended(Move move)
{
  std::string ret = to_csa(move);
  if (move.isNormal()) {
    if (move.capturePtype() != Ptype_EMPTY)
      ret += "x" + to_csa(move.capturePtype());
    if (move.isPromotion())
      ret += '+';
  }
  return ret;
}

std::string osl::to_csa(Player player) {
  std::string buf("+");
  return to_csa(player, buf);
}

std::string osl::to_csa(Square position) {
  std::string buf("00");
  return to_csa(position, buf);
}

std::string osl::to_csa(Ptype ptype) {
  std::string buf("OU");
  return to_csa(ptype, buf);
}

std::string osl::to_csa(Piece piece) {
  if (piece.isEdge())
    return "   ";
  if (piece.isEmpty())
    return " * ";

  assert(piece.isPiece() && is_piece(piece.ptype()));
  assert(unpromote(piece.ptype()) == piece_id_ptype[piece.id()]);
  return to_csa(piece.owner()) + to_csa(piece.ptype());
}

std::string osl::to_csa(const Move *first, const Move *last) {
  std::ostringstream out;
  for (; first != last; ++first) {
    if (first->isInvalid())
      break;
    out << to_csa(*first);
  }
  return out.str();
}

/* ------------------------------------------------------------------------- */
osl::MiniRecord osl::csa::read_record(const std::filesystem::path& filename) {
  std::ifstream is(filename);
  if (! is) {
    const std::string msg = "CsaFileMinimal::CsaFileMinimal file open failed ";
    std::cerr << msg << filename << "\n";
    throw ParseError(msg + filename.string());
  }
  return read_record(is);
}

osl::MiniRecord osl::csa::read_record(std::istream& is) {
  osl::MiniRecord record;
  SimpleState work;
  work.init();
  std::string line;
  CArray<bool, 9> board_parsed = {{ false }};
  while (std::getline(is, line))  {
    // quick hack for \r
    if ((! line.empty())
	&& (line[line.size()-1] == 13))
      line.erase(line.size()-1);
#if 1
    detail::parse_line(work, record, line, board_parsed);
#else
    // todo
    std::vector<std::string> elements;
    boost::algorithm::split(elements, line, boost::algorithm::is_any_of(","));
    for (auto& e: elements) {
      boost::algorithm::trim(e);
      boost::algorithm::trim_left(e);
      parseLine(work, record, e, board_parsed);
    }
#endif
  }
  if (*std::min_element(board_parsed.begin(), board_parsed.end()) == false) {
    if (*std::max_element(board_parsed.begin(), board_parsed.end()) == false)
      throw ParseError("no position in csa game record");
    throw ParseError("incomplete position description in csa game record");
  }
  assert(record.initial_state.isConsistent());
  return record;
}

bool osl::csa::detail::parse_line(SimpleState& state, MiniRecord& record, std::string s,
                                  CArray<bool,9>& board_parsed) {
  while (! s.empty() && isspace(s[s.size()-1])) // ignore trailing garbage
    s.resize(s.size()-1);
  if (s.length()==0) 
    return true;
  switch(s.at(0)){
  case 'P': /* 開始盤面 */
    switch(s.at(1)){
    case 'I': /* 平手初期配置 */
      board_parsed.fill(true);
      state.init(HIRATE);
      break;
    case '+': /* 先手の駒 */
    case '-':{ /* 後手の駒 */
      Player pl=csa::to_player(s.at(1));
      for(int i=2;i<=(int)s.length()-4;i+=4){
	Square pos=csa::to_square(s.substr(i,2));
	if(s.substr(i+2,2) == "AL"){
	  state.setPieceAll(pl);
	}
	else{
	  Ptype ptype=csa::to_ptype(s.substr(i+2,2));
	  state.setPiece(pl,pos,ptype);
	}
      }
      break;
    }
    default:
      if(isdigit(s.at(1))){
	const int y=s.at(1)-'0';
	board_parsed[y-1] = true;
	for(unsigned int x=9,i=2;i<s.length();i+=3,x--){
	  if (s.at(i) != '+' && s.at(i) != '-' && s.find(" *",i)!=i) {
            std::cerr << "possible typo for empty square " << s << "\n";
            throw ParseError("parse board error " + s);
	  }   
	  if (s.at(i) != '+' && s.at(i) != '-') continue;
	  Player pl=csa::to_player(s.at(i));
	  Square pos(x,y);
	  Ptype ptype=csa::to_ptype(s.substr(i+1,2));
	  state.setPiece(pl,pos,ptype);
	}
      }
    }
    break;
  case '+':
  case '-':{
    Player pl=csa::to_player(s.at(0));
    if(s.length()==1){
      state.setTurn(pl);
      state.initPawnMask();
      record.initial_state = EffectState(state);
    }
    else{ // actual moves
      const Move m = csa::to_move(s,state);
      EffectState copy(state);
      if (! copy.isLegal(m))
      {
	std::cerr << "Illegal move " << m << std::endl;
	throw ParseError("illegal move "+s);
      }
      record.moves.push_back(m);
      copy.makeMove(m);
      state = copy;
    }
    break;
  }
  default:
    return false;		// there are unhandled contents
  }
  return true;
}

/* ------------------------------------------------------------------------- */



// usi.cc
std::string osl::to_psn(Square pos) {
  const int x = pos.x();
  const int y = pos.y();
  std::string result = "XX";
  result[0] = x + '0';
  result[1] = y + 'a' - 1;
  return result;
}

char osl::to_psn(Ptype ptype) {
  switch (ptype) {
  case PAWN:	return 'P';
  case LANCE:	return 'L';
  case KNIGHT:	return 'N';
  case SILVER:	return 'S';
  case GOLD:	return 'G';
  case BISHOP:	return 'B';
  case ROOK:	return 'R';
  case KING:	return 'K';
  default:
    assert("unsupported ptype" == 0);
    return '!';
  }
}

std::string osl::to_psn(Move m) {
  const Square from = m.from();
  const Square to = m.to();
  if (from.isPieceStand())
  {
    std::string result = "X*";
    result[0] = to_psn(m.ptype());
    result += to_psn(to);
    return result;
  }
  std::string result = to_psn(from);
  result += to_psn(to);
  if (m.promoteMask())
    result += '+';
  return result;
}

std::string osl::to_psn_extended(Move m) {
  if (m.isInvalid())
    return "resign";  
  if (m.isPass())
    return "pass";
  const Square from = m.from();
  const Square to = m.to();
  if (from.isPieceStand())
  {
    std::string result = "X*";
    result[0] = to_psn(m.ptype());
    result += to_psn(to);
    return result;
  }
  std::string result = to_psn(from);
  if (m.capturePtype() != Ptype_EMPTY)
    result += 'x';
  result += to_psn(to);
  if (m.isPromotion())
    result += '+';
  else if (can_promote(m.ptype())
	   && (from.isPromoteArea(m.player()) || to.isPromoteArea(m.player())))
    result += '=';
  return result;
}


osl::Move osl::psn::to_move(const std::string& str, const SimpleState& s) {
  if (str.size() < 4)
    throw ParseError("Invalid move string: " + str);

  const Square to = to_square(str.substr(2,2));
  if (str[1] == '*')
  {
    const Ptype ptype = to_ptype(str[0]);
    return Move(to, ptype, s.turn());
  }

  const Square from = to_square(str.substr(0,2));
  const Ptype ptype = s.pieceOnBoard(from).ptype();
  const Ptype captured = s.pieceOnBoard(to).ptype();
  if (! is_piece(ptype))
    throw ParseError("No piece on square: " + str);    
  bool promotion = false;
  if (str.size() > 4)
  {
    assert(str[4] == '+');
    promotion = true;
  }
  return Move(from, to, (promotion ? promote(ptype) : ptype), 
	      captured, promotion, s.turn());
}

osl::Square osl::psn::to_square(const std::string& str) {
  assert(str.size() == 2);
  const int x = str[0] - '0';
  const int y = str[1] - 'a' + 1;
  if (x <= 0 || x > 9 || y <= 0 || y > 9)
    throw ParseError("Invalid square character: " + str);
  return Square(x, y);
}

osl::Ptype osl::psn::to_ptype(char c) {
  switch (c) 
  {
  case 'P': return PAWN;
  case 'L': return LANCE;
  case 'N': return KNIGHT;
  case 'S': return SILVER;
  case 'G': return GOLD;
  case 'B': return BISHOP;
  case 'R': return ROOK;
  case 'K': return KING;
  default:
    return Ptype_EMPTY;
  }
}

/* ------------------------------------------------------------------------- */

std::string osl::to_usi(Move m) {
  if (m.isPass())
    return "pass";
  if (m == Move::DeclareWin())
    return "win";
  if (! m.isNormal())
    return "resign";
  return to_psn(m);
}

std::string osl::to_usi(PtypeO ptypeo) {
  if (! is_piece(ptypeo))
    return "";

  char c = to_psn(unpromote(ptype(ptypeo)));
  if (owner(ptypeo) == WHITE)
    c = tolower(c);
  std::string ret(1,c);
  if (is_promoted(ptypeo))
    ret = "+" + ret;
  return ret;
}

std::string osl::to_usi(const EffectState& state) {
  std::ostringstream ret;
  if (state == SimpleState(HIRATE)) {
    ret << "startpos";
    return ret.str();
  }
  ret << "sfen ";
  for (int y: board_y_range()) {
    int empty_count = 0;
    for (int x: board_x_range()) {
      const Piece p = state.pieceOnBoard(Square(x,y));
      if (p.isEmpty()) {
	++empty_count;
	continue;
      }
      if (empty_count) {
	ret << empty_count;
	empty_count = 0;
      }
      ret << to_usi(p);
    }
    if (empty_count)
      ret << empty_count;
    if (y < 9) ret << "/";
  }
  ret << " " << "bw"[state.turn() == WHITE] << " ";
  bool has_any = false;
  for (auto player: players) {
    for (Ptype ptype: PieceStand::order) {
      const int count = state.countPiecesOnStand(player, ptype);
      if (count == 0)
	continue;
      if (count > 1)
	ret << count;
      ret << to_usi(newPtypeO(player, ptype));
      has_any = true;
    }
  }
  if (! has_any)
    ret << "-";
  ret << " 1";
  return ret.str();
}

std::string osl::to_usi(const MiniRecord& record) {
  std::string ret = to_usi(record.initial_state);
  ret += " moves";
  for (auto move: record.moves)
    ret += " " + to_usi(move);
  return ret;
}

osl::Move osl::usi::to_move(const std::string& str, const EffectState& s) {
  if (str == "win")
    return Move::DeclareWin();
  if (str == "pass")
    return Move::PASS(s.turn());
  if (str == "resign")
    return Move::INVALID();
  try {
    return psn::to_move(str, s);
  }
  catch (std::exception& e) {
    throw ParseError("usi::to_move failed for " + str + " by "+ e.what());
  }
  catch (...) {
    throw ParseError("usi::to_move failed for " + str);
  }
}

osl::PtypeO osl::usi::to_ptypeo(char c) {
  const Ptype ptype = psn::to_ptype(toupper(c));
  if (ptype == Ptype_EMPTY)
    throw ParseError("Invalid piece character: " + std::string(1,c));
  const Player pl = isupper(c) ? BLACK : WHITE;
  return newPtypeO(pl, ptype);
}

void osl::usi::parse_board(const std::string& word, EffectState& out) {
  if (word.empty())
    throw ParseError(word);

  SimpleState state;
  state.init();
  int x=9, y=1;
  for (size_t i=0; i<word.size(); ++i) {
    const char c = word[i];
    if (isalpha(c)) {
      const PtypeO ptypeo = to_ptypeo(c);
      state.setPiece(owner(ptypeo), Square(x,y), ptype(ptypeo));
      --x;
    } else if (c == '+') {
      if ( (i+1) >= word.size() )
        throw ParseError(word);
      const char next = word[i+1];
      if (!isalpha(next))
        throw ParseError(word);
      const PtypeO ptypeo = to_ptypeo(next);
      if (!can_promote(ptypeo))
        throw ParseError(word);
      const PtypeO promoted = promote(ptypeo);
      state.setPiece(owner(promoted), Square(x,y), ptype(promoted));
      --x;
      ++i;
    } else if (c == '/') {
      if (x != 0)
        throw ParseError(word);
      x = 9;
      ++y;
    } else if (isdigit(c)) {
      const int n = c - '0';
      if (n == 0)
        throw ParseError(word);
      x -= n;
    } else {
      throw ParseError("usi: unknown input " + std::string(1,c));
    }
    if (x < 0 || x > 9 || y < 0 || y > 9)
      throw ParseError(word);
  }
  out = EffectState(state);
}

void osl::usi::parse(const std::string& line, EffectState& state) {
  EffectState board;
  std::vector<Move> moves;
  parse(line, board, moves);
  state.copyFrom(board);
  for (Move move: moves) {
    state.makeMove(move);
  }
}

osl::EffectState osl::usi::to_state(const std::string& line) {
  EffectState state;
  parse(line,state);
  return state;
}

void osl::usi::parse(const std::string& line, EffectState& state, std::vector<Move>& moves) {
  moves.clear();
  std::istringstream is(line);
  std::string word;
  is >> word;
  if (word == "position")
    is >> word;
  if (word == "startpos") 
    state.init(HIRATE);
  else {
    if (word != "sfen")
        throw ParseError("sfen not found "+word);
    is >> word;
    parse_board(word, state);
    is >> word;
    if (word != "b" && word != "w")
        throw ParseError(" turn error "+word);
    state.setTurn((word == "b") ? BLACK : WHITE);
    is >> word;
    if (word != "-") {
      int prefix = 0;
      for (char c: word) {
	if (isalpha(c)) {
	  PtypeO ptypeo = to_ptypeo(c);
	  for (int j=0; j<std::max(1, prefix); ++j)
	    state.setPiece(owner(ptypeo), Square::STAND(), ptype(ptypeo));
	  prefix = 0;
	}
	else {
	  if (!isdigit(c))
            throw ParseError(word);
	  prefix = (c - '0') + prefix*10;
          if (prefix == 0)
            throw ParseError(word);
	}
      }
    }
    state.initPawnMask();
    int move_number; // will not be used
    if (! (is >> move_number))
      return;
    assert(is);
  }
  if (! (is >> word))
    return;
  if (word != "moves")
    throw ParseError("moves not found "+word);
  EffectState state_copy(state);
  while (is >> word) {
    Move m = to_move(word, state_copy);
    moves.push_back(m);
    if (! m.isNormal() || ! state_copy.isLegal(m))
      throw ParseError("invalid move "+word);
    state_copy.makeMove(m);
  }
} 

// ki2
namespace osl
{
  namespace kanji
  {
    const std::u8string suji[] = {
      u8"", u8"１", u8"２", u8"３", u8"４", u8"５", u8"６", u8"７", u8"８", u8"９", };
    const std::u8string dan[] = {
      u8"", u8"一", u8"二", u8"三", u8"四", u8"五", u8"六", u8"七", u8"八", u8"九", };
    const std::u8string K_NARU = u8"成", K_ONAZI = u8"同", K_PASS = u8"(パス)", K_UTSU = u8"打",
      K_YORU = u8"寄", K_HIKU = u8"引", K_UE = u8"上", K_HIDARI = u8"左", K_MIGI = u8"右", K_SUGU = u8"直";
    const std::u8string ptype_name[] = {
      u8"",  u8"",
      u8"と", u8"成香", u8"成桂", u8"成銀", u8"馬", u8"龍",
      u8"王", u8"金", u8"歩", u8"香", u8"桂", u8"銀", u8"角", u8"飛",
    };
    const std::u8string promote_flag[] = { u8"不成", u8"成", };
    const std::u8string sign[] = { u8"☗", u8"☖", };
  }
}
std::u8string osl::to_ki2(osl::Square sq) {
  if (sq.isPieceStand())
    return u8"";
  return kanji::suji[sq.x()] + kanji::dan[sq.y()];
}

std::u8string osl::to_ki2(Ptype ptype) {
  return kanji::ptype_name[idx(ptype)];
}

std::u8string osl::to_ki2(Square cur, Square prev) {
  return (cur == prev) ? kanji::K_ONAZI : to_ki2(cur);
}
std::u8string osl::to_ki2(Move m, const EffectState& state, Square prev) {
  const Player player = m.player();
  std::u8string ret = kanji::sign[idx(player)];
  if (m.isPass()) {
    ret += kanji::K_PASS;
    return ret;
  }
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
	  const Piece l = state.pieceAt
	    (Square(from.x() - sign(player), from.y()));
	  if (l.isOnBoardByOwner(player) && l.ptype() == ptype)
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
      else
	ret += kanji::K_HIDARI;
      if ((to.y() - from.y())*sign(player) > 0)
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
