#include "record.h"
#include "more.h"
#include "checkmate.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

/* ------------------------------------------------------------------------- */

osl::Player osl::csa::to_player(char c) {
  if(c=='+') 
    return BLACK;
  if(c=='-') 
    return WHITE;
  throw ParseError("not a csa PlayerCharacter "+std::string(1,c));
}

osl::Square osl::csa::to_square(const std::string& s) {
  int x=s.at(0)-'0';
  int y=s.at(1)-'0';
  if(x==0 && y==0) 
    return Square::STAND();
  return Square(x,y);
}

osl::Ptype osl::csa::to_ptype(const std::string& s) {
  auto p = std::ranges::find(ptype_csa_names, s);
  if (p == ptype_csa_names.end())
    throw ParseError("unknown std::string in csa::to_ptype "+s);
  return Ptype(p-ptype_csa_names.begin());    
}

osl::Move osl::csa::to_move_light(const std::string& s,const BaseState& state) {
  if (s == "%KACHI")
    return Move::DeclareWin();
  if (s == "%TORYO")
    return Move::Resign();
  if (s == "%PASS")		// FIXME: not in CSA protocol
    return Move::PASS(state.turn());

  Player pl=csa::to_player(s.at(0));
  Square fromPos=csa::to_square(s.substr(1,2));
  Square toPos=csa::to_square(s.substr(3,2));
  Ptype ptype=csa::to_ptype(s.substr(5,2));
  Move move;
  if(fromPos==Square::STAND()){
    move = Move(toPos,ptype,pl);
  }
  else{
    Piece p0=state.pieceAt(fromPos);
    Piece p1=state.pieceAt(toPos);
    Ptype capturePtype=p1.ptype();
    bool isPromote=(p0.ptype()!=ptype);
    if (! ((p0.ptype()==ptype)||(p0.ptype()==unpromote(ptype))))
      throw ParseError("illegal promotion in csa::to_move "+s);
    move = Move(fromPos,toPos,ptype, capturePtype,isPromote,pl);
  }
  if (! move.isConsistent())
    throw ParseError("inconsistent move in csa::to_move "+s);
  return move;
}

osl::Move osl::csa::to_move(const std::string& s,const EffectState& state) {
  auto move = to_move_light(s, state);
  if (! state.isLegal(move))
    throw ParseError("illegal move in csa::to_move "+s);
  return move;
}


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
void osl::MiniRecord::guess_result(const EffectState& state) {
  if (state.inCheckmate())
    result = loss_result(state.turn());
  else if (win_if_declare(state)) {
    result = win_result(state.turn());
    final_move = Move::DeclareWin();
  }
  // todo Draw,
}        

osl::MiniRecord osl::csa::read_record(const std::filesystem::path& filename) {
  std::ifstream is(filename);
  if (! is) {
    const std::string msg = "csa::read_record: file open failed ";
    std::cerr << msg << filename << "\n";
    throw ParseError(msg + filename.string());
  }
  return read_record(is);
}

osl::MiniRecord osl::csa::read_record(std::istream& is) {
  osl::MiniRecord record;
  BaseState work;
  work.initEmpty();
  std::string line;
  CArray<bool, 9> board_parsed = {{ false }};
  while (std::getline(is, line))  {
    // quick hack for \r
    if ((! line.empty()) && (line[line.size()-1] == 13))
      line.erase(line.size()-1);
    bool complete = detail::parse_state_line(work, record, line, board_parsed);
    if (complete) break;
  }
  if (*std::min_element(board_parsed.begin(), board_parsed.end()) == false) {
    if (*std::max_element(board_parsed.begin(), board_parsed.end()) == false)
      throw ParseError("no position in csa game record");
    throw ParseError("incomplete position description in csa game record");
  }
  EffectState latest(record.initial_state);
  while (std::getline(is, line))  {
    if ((! line.empty()) && (line[line.size()-1] == 13))
      line.erase(line.size()-1);    
    record.result = detail::parse_move_line(latest, record, line);
    if (record.result != Interim)
      break;
#if 0
    // todo --- handle comma
    std::vector<std::string> elements;
    boost::algorithm::split(elements, line, boost::algorithm::is_any_of(","));
    for (auto& e: elements) {
      boost::algorithm::trim(e); boost::algorithm::trim_left(e);
      parseLine(work, record, e, board_parsed);
    }
#endif
  }
  if (record.result == Interim)
    record.guess_result(latest);
  return record;
}

osl::GameResult osl::csa::detail::parse_move_line(EffectState& state, MiniRecord& record, std::string s) {
  if (s.length()==0) 
    return Interim;
  switch (s.at(0)) {      
  case '+':
  case '-':{
    const Move m = csa::to_move(s,state);
    record.moves.push_back(m);
    state.makeMove(m);
    break;
  }
  case '%': {
    if (s == "%TORYO") {
      record.final_move = Move::Resign();
      return loss_result(state.turn());
    }
    if (s == "%KACHI") {
      bool legal = win_if_declare(state);
      record.final_move = legal ? Move::DeclareWin() : Move::Resign();
      return legal ? win_result(state.turn()) : loss_result(state.turn());
    }
    if (s == "%CHUDAN")
      return Draw;
    // fall through
  }  
  case 'T':
  case '\'':
    break;
  default: 
    std::cerr << "ignored " << s << '\n';
  }
  return Interim;
}

bool osl::csa::detail::parse_state_line(BaseState& state, MiniRecord& record, std::string s,
                                        CArray<bool,9>& board_parsed) {
  while (! s.empty() && isspace(s[s.size()-1])) // ignore trailing garbage
    s.resize(s.size()-1);
  if (s.length()==0) 
    return false;
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
      state.initFinalize();
      record.initial_state = EffectState(state);
      return true;
    }
  }
  case 'N': case '$': case 'V': case '\'':
    break;
  default:
    std::cerr << "ignored " << s << '\n';
  }
  return false;
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

osl::Move osl::psn::to_move_light(const std::string& str, const BaseState& s) {
  if (str.size() < 4)
    throw ParseError("Invalid move string: " + str);

  const Square to = to_square(str.substr(2,2));
  Move move;
  if (str[1] == '*') {
    move = Move(to, to_ptype(str[0]), s.turn());
  }
  else {
    const Square from = to_square(str.substr(0,2));
    const Ptype ptype = s.pieceOnBoard(from).ptype();
    const Ptype captured = s.pieceOnBoard(to).ptype();
    bool promotion = false;
    if (str.size() > 4) {
      assert(str[4] == '+');
      promotion = true;
    }
    move = Move(from, to, (promotion ? promote(ptype) : ptype), 
                captured, promotion, s.turn());
  }
  if (! move.isConsistent())
    throw ParseError("in consistent move " + str);
  return move;
}
osl::Move osl::psn::to_move(const std::string& str, const EffectState& s) {
  auto move = to_move_light(str, s);
  if (! s.isLegal(move))
    throw ParseError("illegal move " + str);    
  return move;
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

std::string osl::to_usi(const BaseState& state) {
  std::ostringstream ret;
  if (state == BaseState(HIRATE)) {
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
    for (Ptype ptype: piece_stand_order) {
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
  if (record.has_winner())
    ret += " " + to_usi(record.final_move);
  return ret;
}

osl::Move osl::usi::to_move(const std::string& str, const EffectState& s) {
  if (str == "win")
    return Move::DeclareWin();
  if (str == "pass")
    return Move::PASS(s.turn());
  if (str == "resign")
    return Move::Resign();
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

void osl::usi::parse_board(const std::string& word, BaseState& state) {
  if (word.empty())
    throw ParseError(word);

  state.initEmpty();
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
  state.initFinalize();
}

void osl::usi::parse(const std::string& line, EffectState& state) {
  MiniRecord record = read_record(line);
  state.copyFrom(record.initial_state);
  for (Move move: record.moves) 
    state.makeMove(move);
}

osl::EffectState osl::usi::to_state(const std::string& line) {
  EffectState state;
  parse(line,state);
  return state;
}

osl::MiniRecord osl::usi::read_record(std::string line) {
  MiniRecord record;
  std::istringstream is(line);
  std::string word;
  {
    BaseState state;
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
      state.initFinalize();
      record.initial_state = EffectState(state);
      int move_number; // will not be used
      if (! (is >> move_number))
        return record;
      assert(is);
    }
  }
  if (! (is >> word))
    return record;
  if (word != "moves")
    throw ParseError("moves not found "+word);
  EffectState uptodate(record.initial_state);
  while (is >> word) {
    Move m = to_move(word, uptodate);
    if (! m.isNormal()) {
      record.moves.push_back(m);
      break;
    }
    record.moves.push_back(m);
    uptodate.makeMove(m);
  }
  if (record.moves.size()>0) {
    auto turn = uptodate.turn();
    auto last_move = record.moves.back();
    if (last_move == Move::Resign()) {
      record.result = loss_result(turn);
    }
    else if (last_move == Move::DeclareWin()) {
      bool legal = win_if_declare(uptodate);
      record.result = legal ? win_result(turn) : loss_result(turn);
    }
    if (! last_move.isNormal()) {
      record.final_move = last_move;
      record.moves.pop_back();
    }
  }
  if (record.result == Interim)
    record.guess_result(uptodate);
  return record;
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

uint64_t osl::bitpack::detail::combination_id(int first, int second) {
  assert(0 <= first && first < second);
  return first + second*(second-1)/2;
}
uint64_t osl::bitpack::detail::combination_id(int first, int second, int third) {
  assert(0 <= first && first < second && second < third);
  return combination_id(first, second) + third*(third-1)*(third-2)/6;
}
uint64_t osl::bitpack::detail::combination_id(int first, int second, int third, int fourth) {
  assert(0 <= first && first < second && second < third && third < fourth);
  return combination_id(first, second, third) + fourth*(fourth-1)*(fourth-2)*(fourth-3)/24;
}

namespace osl
{
  constexpr uint64_t comb2(int n) { return n*(n-1)/2; }
  constexpr uint64_t comb4(int n) { return n*(n-1)*(n-2)*(n-3)/24; }
  std::array<uint64_t,7> encode(const int ptype_id[8][4]) {
    uint64_t done=0;
    auto renumber = [&](int id) { return uint64_t(id - std::popcount(done & (one_hot(id)-1))); };
    auto add1 = [&](int id) { auto ret = renumber(id); done |= one_hot(id); return ret; };
    auto king = add1(ptype_id[basic_idx(KING)][0])*39 + add1(ptype_id[basic_idx(KING)][1]);
    auto add2 = [&](const int ids[]) {
      int a = renumber(ids[0]);
      int b = renumber(ids[1]);
      auto ret = bitpack::detail::combination_id(a, b);
      done |= one_hot(ids[0]); done |= one_hot(ids[1]);
      return ret;
    };
    auto rook   = add2(ptype_id[basic_idx(ROOK)]);
    auto bishop = add2(ptype_id[basic_idx(BISHOP)]);

    auto add4 = [&](const int ids[]) {
      int a = renumber(ids[0]), b = renumber(ids[1]), c = renumber(ids[2]), d = renumber(ids[3]);
      assert(a>=0); 
      auto ret = bitpack::detail::combination_id(a, b, c, d);
      done |= one_hot(ids[0]); done |= one_hot(ids[1]); done |= one_hot(ids[2]); done |= one_hot(ids[3]);
      return ret;
    };
    auto gold   = add4(ptype_id[basic_idx(GOLD)]);
    auto silver = add4(ptype_id[basic_idx(SILVER)]);
    auto knight = add4(ptype_id[basic_idx(KNIGHT)]);
    auto lance  = add4(ptype_id[basic_idx(LANCE)]);
  
    if (std::popcount(done) != 40-18)
      std::runtime_error("PackedState encode");
    return {king, rook, bishop, gold, silver, knight, lance };
  }
  struct B256Extended {
    uint128_t board = 0; // 81 (64+17)
    uint64_t order_lo = 0; // 57 (47+10) : 138
    uint32_t order_hi = 0; // 30 : 168
    uint64_t color = 0; // 38 (24+14) : 206
    uint64_t promote = 0; // 34 : 240
    uint32_t turn = 0; // 1
    uint32_t move = 0; // 12
    uint32_t game_result = 0; // 2
    uint32_t reserved = 0; // 1
  };
  inline B256 pack(const B256Extended& code) {
    std::array<uint64_t,4> binary;
    binary[0] = code.board >> 17;
    binary[1] = (code.board & (one_hot(17)-1))<<47;
    binary[1] += code.order_lo >> 10;
    binary[2] = (code.order_lo & (one_hot(10)-1)) <<30;
    binary[2] += code.order_hi;
    binary[2] <<= 24;
    binary[2] += code.color >> 14;
    binary[3] = (code.color & (one_hot(14)-1))<<34;
    binary[3] += code.promote;
    binary[3] <<= 16;
    binary[3] += (code.turn << 15) + (code.move << 3) + (code.game_result << 1) + code.reserved;
    return {binary};
  }
  inline B256Extended unpack(const std::array<uint64_t,4>& binary) {
    B256Extended code;
    code.board = binary[0];
    code.board <<= 17;
    code.board += binary[1]>>47;
    code.order_lo = ((binary[1] & (one_hot(47)-1)) << 10) + (binary[2]>>54);
    code.order_hi = (binary[2] >> 24) & (one_hot(30)-1);
    code.color = ((binary[2] & (one_hot(24)-1))<<14) + (binary[3]>>50);
    code.promote = (binary[3] >> 16) & (one_hot(34)-1);
    code.turn = (binary[3] >> 15) & 1;
    code.move = (binary[3] >> 3) & (one_hot(12)-1);
    code.game_result = (binary[3] >> 1) & 3;
    code.reserved = binary[3] & 1;
    return code;
  }
  namespace
  {
    inline int code_color_id(Ptype ptype, int piece_id) {
      return ptype_has_long_move(unpromote(ptype)) ? piece_id-2 : piece_id; // skip two kings
    }
    inline int code_promote_id(Ptype ptype, int piece_id) {
      return ptype_has_long_move(unpromote(ptype)) ? piece_id-6 : piece_id; // skip two kings and four golds
    }
    inline auto one_hot128(int n) { return ((uint128_t)1) << n; }    
  }
  const int move12_dir_size = 13, move12_unpromote_offset = 5;
}

uint32_t osl::bitpack::encode12(const BaseState& state, Move move) {
  if (move == Move::Resign())      
    return move12_resign; // 0 --- no conflict --- to (1,1) by moving UL .. outside from the board
  if (move == Move::DeclareWin())
    return move12_win_declare;  // 127 --- no conflict --- outside booard
  
  auto to = move.to().blackView(state.turn());
  auto code_to = (to.x()-1)+(to.y()-1)*9; // 7bit
  if (to.y() <= 4 && move.isPromotion())
    code_to += 81; // 81 + 32 < 128
  auto code_dir_or_ptype = 0; // [0, 21)
  constexpr int dir_size = 13; // [0,UUR+3]
  if (move.isDrop()) {
    auto drop_id = basic_idx(move.ptype())-1;
    code_dir_or_ptype = drop_id + move12_dir_size;
  }
  else {
    auto from = move.from().blackView(state.turn());
    Direction dir;
    if (move.oldPtype() == KNIGHT)
      dir = to.x() > from.x() ? UUL : UUR;
    else {
      dir = base8_dir<BLACK>(from, to);
      if (to.y() > 4 && move.hasIgnoredUnpromote()) {
        assert(dir == DL || dir == D || dir == DR);
        dir = Direction(Int(dir)+move12_unpromote_offset);
      }
    }
    code_dir_or_ptype = Int(dir);
  }
  return code_dir_or_ptype*128 + code_to;
}

osl::Move osl::bitpack::decode_move12(const BaseState& state, uint32_t code) {
  if (code == move12_resign)
    return Move::Resign();
  if (code == move12_win_declare)
    return Move::DeclareWin();

  auto code_to = code%128, code_dir_or_ptype = code/128;
  auto x = (code_to%9)+1, y = code_to/9 + 1;
  bool promote = false;
  if (y > 9) {
    promote = true;
    y -= 9;
  } 
  Square to(x,y);
  if (state.turn() == WHITE)
    to = to.blackView(WHITE);      // change view
    
  if (code_dir_or_ptype >= move12_dir_size) { // drop
    auto ptype = basic_ptype[code_dir_or_ptype-move12_dir_size+1]; // 0 is for KING
    assert(state.pieceAt(to).isEmpty());
    return Move(to, ptype, state.turn());
  }
  // move on board
  if (! state.pieceAt(to).canMoveOn(state.turn()))
    throw std::runtime_error("decode inconsistent to" + std::to_string(code));
  Direction dir = Direction(code_dir_or_ptype);
  if (! is_basic(dir)) {
    dir = Direction(code_dir_or_ptype-move12_unpromote_offset);
    promote = true;
  }
  Offset step = to_offset(state.turn(), dir);
  Square from = to - step;
  while (state.pieceAt(from).isEmpty())
    from -= step;
  if (! state.pieceAt(from).isOnBoardByOwner(state.turn()))
    throw std::runtime_error("decode inconsistent from" + std::to_string(code));
  auto ptype = state.pieceAt(from).ptype();
  if (promote)
    ptype = osl::promote(ptype);
  return Move(from, to, ptype, state.pieceAt(to).ptype(), promote, state.turn());
}

std::pair<int,int> osl::bitpack::detail::unpack2(uint32_t code) {
  int n2 = std::floor(std::sqrt(2.0*code))+1;
#if 0
  if (n2 == 0 || combination_id(0, n2+1) <= code)
    ++n2; 
  else
#endif
    if (combination_id(0, n2) > code)
      --n2;
  int n1 = code - combination_id(0, n2);
  return std::make_pair(n1, n2);
}

std::tuple<int,int,int,int> osl::bitpack::detail::unpack4(uint64_t code) {
  int n4 = std::floor(std::sqrt(std::sqrt(24.0*code)))+2;
  if (n4 == 0 || combination_id(0, 1, 2, n4+1) <= code)
    ++n4; 
  else if (combination_id(0, 1, 2, n4) > code)
    --n4;
  code -= combination_id(0, 1, 2, n4);
  int n3 = std::floor(std::cbrt(6.0*code))+1;
  if (n3 == 0 || combination_id(0, 1, n3+1) <= code)
    ++n3; 
#if 0
  else if (combination_id(0, 1, n3) > code)
    --n3;
#endif
  code -= combination_id(0, 1, n3);
  auto [n1, n2] = unpack2(code);
  return std::make_tuple(n1, n2, n3, n4);
}


osl::bitpack::B256 osl::bitpack::PackedState::to_bitset() const {
  B256Extended code;
  std::vector<Piece> board_pieces; board_pieces.reserve(40);
  for (int x: board_y_range()) // 1..9
    for (int y: board_y_range()) {
      auto p = state.pieceAt(Square(x,y));
      if (! p.isPiece())
        continue;
      code.board |= one_hot128((x-1)*9+(y-1));
      board_pieces.push_back(p);
    }
  int ptype_count[8] = { 0 };  // number of completed pieces
  int ptype_bid[8][4] = { 0 }; // (ptype,n-th) -> owned square id
  for (auto bi: std::views::iota(0,int(board_pieces.size()))) {
    auto p = board_pieces[bi];
    auto ptype = unpromote(p.ptype());
    // renumber
    int nth = ptype_count[basic_idx(ptype)]++;
    if (ptype == KING)
      ptype_bid[basic_idx(ptype)][idx(p.owner())] = bi;
    else if (ptype != PAWN)
      ptype_bid[basic_idx(ptype)][nth] = bi;
    
    int piece_id = nth + ptype_piece_id[idx(ptype)].first;
    if (p.ptype() == KING)
      continue;
    if (idx(p.owner()))
      code.color |= one_hot(code_color_id(p.ptype(), piece_id)); // skip two kings    
    if (p.ptype() == GOLD)
      continue;
    if (p.isPromoted())
      code.promote |= one_hot(code_promote_id(p.ptype(), piece_id)); // skip two kings and four golds
  }
  // stand
  int hp = board_pieces.size();
  auto encode_stand = [&](Player pl) {
    for (auto ptype: piece_stand_order) {
      int cnt = state.countPiecesOnStand(pl, ptype);
      for (int i: std::views::iota(0, cnt)) {
        int nth = ptype_count[basic_idx(ptype)]++;
        int piece_id = nth + ptype_piece_id[idx(ptype)].first;
        if (idx(pl))
          code.color |= one_hot(code_color_id(ptype, piece_id));
        if (ptype != PAWN) 
          ptype_bid[basic_idx(ptype)][nth] = hp++;
      }
    }
  };
  encode_stand(BLACK);
  encode_stand(WHITE);

  for (auto ptype: basic_ptype) {
    if (ptype == PAWN) continue;
    if (ptype_count[basic_idx(ptype)] != ptype_piece_count(ptype))
      throw std::runtime_error("PackedState::to_bitset unexpected piece number");
  }
  auto [king_code, rook_code, bishop_code, gold_code, silver_code, knight_code, lance_code] = encode(ptype_bid);
  code.order_hi= (king_code * comb2(38) + rook_code) * comb2(36) + bishop_code;
  code.order_lo= (((gold_code * comb4(30)) + silver_code) * comb4(26) + knight_code) * comb4(22) + lance_code;
  assert(code.order_lo < (1ull<<57));
  code.turn = idx(state.turn());
  code.game_result = (unsigned int)this->result;

  code.move = encode12(state, this->next);
  return pack(code);
}
void osl::bitpack::PackedState::restore(B256 packed) {
  B256Extended code = unpack(packed.binary);
  state.initEmpty();

  std::vector<Square> on_board;  
  for (int x: board_y_range())
    for (int y: board_y_range())
      if (code.board & one_hot128((x-1)*9+(y-1)))
        on_board.emplace_back(x,y);

  std::vector<Ptype> ptype_placement(on_board.size(), Ptype_EMPTY);
  auto nth_empty = [&](int n) {
    for (int i=0; i<on_board.size(); ++i) {
      if (ptype_placement[i] == Ptype_EMPTY && n-- == 0)
        return i;
    }
    return (int)on_board.size() + n;
  };

  // inverse of
  // uint32_t hi= (king_code * comb2(38) + rook_code) * comb2(36) + bishop_code;
  uint32_t code_hi = code.order_hi;
  auto king_code = code_hi / comb2(38) / comb2(36);
  state.setPiece(BLACK, on_board[king_code / 39], KING);
  ptype_placement[king_code / 39] = KING;
  int wking_bid = nth_empty(king_code % 39);
  state.setPiece(WHITE, on_board[wking_bid], KING);
  ptype_placement[wking_bid] = KING;

  int ptype_bid[8][4] = { 0 };
  auto rook = ptype_bid[basic_idx(ROOK)];
  std::tie(rook[0], rook[1]) = detail::unpack2((code_hi / comb2(36)) % comb2(38));
  auto bishop = ptype_bid[basic_idx(BISHOP)];
  std::tie(bishop[0], bishop[1]) = detail::unpack2(code_hi % comb2(36));
  
  typedef std::pair<Ptype,int> pair_t;
  uint64_t code_lo = code.order_lo;
  for (auto [ptype, n]: std::array<pair_t,4>{{{LANCE,22}, {KNIGHT,26}, {SILVER,30}, {GOLD,34}}}) {
    auto data = ptype_bid[basic_idx(ptype)];
    auto c = code_lo % comb4(n);
    std::tie(data[0], data[1], data[2], data[3]) = detail::unpack4(c);
    code_lo /= comb4(n);
  }
  
  for (auto ptype: std::array<Ptype,6>{{ROOK, BISHOP, GOLD, SILVER, KNIGHT, LANCE}}) {
    uint64_t bids = 0;
    for (int i:std::views::iota(0, ptype_piece_count(ptype))) {
      int bid = nth_empty(ptype_bid[basic_idx(ptype)][i]);
      int piece_id = i + ptype_piece_id[idx(ptype)].first;
      Player owner = players[bittest(code.color, code_color_id(ptype, piece_id))];
      if (bid >= on_board.size()) {
        state.setPiece(owner, Square::STAND(), ptype);
      }
      else {
        bool promoted = can_promote(ptype) && bittest(code.promote, code_promote_id(ptype, piece_id));
        auto pptype = promoted ? promote(ptype) : ptype;
        state.setPiece(owner, on_board[bid], pptype);
        bids |= one_hot(bid);
      }
    }
    for (auto n: to_range(bids))
      ptype_placement[n] = ptype;
  }
  // pawn
  int pawn_cnt=0;
  for (size_t i=0; i<ptype_placement.size(); ++i) {
    if (ptype_placement[i] != Ptype_EMPTY)
      continue;
    int piece_id = pawn_cnt++ + ptype_piece_id[idx(PAWN)].first;
    Player owner = players[bittest(code.color, code_color_id(PAWN, piece_id))];
    bool promoted = bittest(code.promote, code_promote_id(PAWN, piece_id));
    state.setPiece(owner, on_board[i], promoted ? PPAWN : PAWN);
  }
  // stand pawn
  while (pawn_cnt < ptype_piece_count(PAWN)) {
    int piece_id = pawn_cnt++ + ptype_piece_id[idx(PAWN)].first;
    Player owner = players[bittest(code.color, code_color_id(PAWN, piece_id))];    
    state.setPiece(owner, Square::STAND(), PAWN);
  }
  state.setTurn(players[code.turn]);
  state.initFinalize();

  this->result = GameResult(code.game_result);
  this->next = decode_move12(this->state, code.move);
}

int osl::bitpack::append_binary_record(const MiniRecord& record, std::vector<uint64_t>& out) {
  constexpr int move_length_limit = 1<<10;
  if (record.initial_state != BaseState(HIRATE))
    throw std::runtime_error("append_binary_record initial state != startpos");
  if (record.moves.size() >= move_length_limit)
    throw std::runtime_error("append_binary_record length limit over "+std::to_string(record.moves.size()));
  if (record.moves.size() == 0)
    return 0;
  size_t size_at_beginning = out.size();
  std::vector<uint16_t> code_moves; code_moves.reserve(256);
  uint16_t header = (record.moves.size() << 2) + record.result;
  // std::cerr << "header " << header << ' ' << std::bitset<12>(header) << '\n';
  code_moves.push_back(header);
  EffectState state(record.initial_state);
  for (auto move: record.moves) {
    code_moves.push_back(encode12(state, move));
    state.makeMove(move);
  }
  if (record.has_winner())
    code_moves.push_back(encode12(state, record.final_move));
  
  int used = 0;
  uint64_t work = 0;
  for (auto code: code_moves) {
    if (used +12 <= 64) {
      work += uint64_t(code) << used;
      used += 12;
    }
    else {
      int avail = 64-used;
      work += (code&(one_hot(avail)-1)) << used;
      out.push_back(work);
      work = code >> avail;
      used = 12-avail;
    }
  }
  if (used > 0)
    out.push_back(work);
  
  return out.size() - size_at_beginning;
}

int osl::bitpack::read_binary_record(const uint64_t *&in, MiniRecord& record) {
  auto ptr_at_beginning = in;
  uint64_t work = *in++;
  int remain = 64;
  auto retrieve = [&]() {
    if (remain >= 12) {
      auto value = work & (one_hot(12)-1); work >>= 12; remain -= 12;
      return value;
    }
    uint64_t value = work;
    work = *in++;
    value += (work & (one_hot(12-remain)-1)) << remain;
    work >>= 12-remain;
    remain = 64 - 12 + remain;
    return value;
  };
  uint16_t header = retrieve();
  // std::cerr << "header " << header << ' ' << std::bitset<12>(header) << '\n';
  record.initial_state = EffectState();
  record.moves.resize(header >> 2);
  record.result = GameResult(header & 3);

  EffectState state;
  int cnt = 0;
  while (cnt < record.moves.size()) {
    uint16_t code = retrieve();
    auto move = decode_move12(state, code);
    record.moves[cnt++] = move;
    state.makeMove(move);
  }
  if (record.has_winner())
    record.final_move = decode_move12(state, retrieve());
  
  return in - ptr_at_beginning;
}
