#include "record.h"
#include "impl/more.h"
#include "impl/checkmate.h"
#include "impl/bitpack.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <deque>

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
  if (s == "%TORYO" || s == "%ILLEGAL_MOVE")
    return Move::Resign();
  if (s == "%PASS" || s == "%SENNICHITE" || s == "%JISHOGI") // note: pass is not in CSA protocol
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
    move = Move(fromPos,toPos,ptype, capturePtype,isPromote,pl);
  }
  if (! move.is_ordinary_valid())
    throw ParseError("move composition error in csa::to_move "+s);
  if (! state.move_is_consistent(move))
    throw ParseError("move inconsistent with state in csa::to_move "+s);
  return move;
}

osl::Move osl::csa::to_move(const std::string& s,const EffectState& state) {
  auto move = to_move_light(s, state);
  if (! state.isLegal(move))
    throw ParseError("illegal move in csa::to_move "+s);
  return move;
}


std::string osl::to_csa(const BaseState& state) {
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
void osl::MiniRecord::replay(EffectState& state, int idx) {
  if (idx > moves.size())
    throw std::domain_error("replay: index too large");
  state.copyFrom(initial_state);
  for (int i=0; i<idx; ++i)
    state.makeMove(moves[i]);
}

osl::MiniRecord osl::MiniRecord::branch_at(int idx) {
  if (idx >= moves.size())
    throw std::domain_error("index too large "+std::to_string(idx)+" v.s. "+std::to_string(moves.size()));
  MiniRecord copy {initial_state,
                   {moves.begin(), moves.begin()+idx},
                   {history.begin(), history.begin()+idx+1}};
  return copy;
}

std::vector<std::array<uint64_t,4>> osl::MiniRecord::export_all(bool flip_if_white) const {
  std::vector<std::array<uint64_t,4>> ret;
  EffectState state(initial_state);
  StateRecord256 ps;
  for (auto move: moves) {
    ps.state = state;
    ps.next = move;
    ps.result = result;
    if (flip_if_white && state.turn() == WHITE) 
      ps.flip();
    ret.push_back(ps.to_bitset());
    state.makeMove(move);
  }
  return ret;
}

std::vector<std::array<uint64_t,5>> osl::MiniRecord::export_all320(bool flip_if_white) const {
  std::vector<std::array<uint64_t,5>> ret;
  std::deque<Move> history5;
  EffectState state(initial_state);
  StateRecord320 ps;
  for (size_t i=0; i<moves.size(); ++i) {
    auto move = moves[i];
    ps.base.state = state;
    ps.base.next = move;
    ps.base.result = result;
    std::ranges::fill(ps.history, Move());
    std::copy(history5.begin(), history5.end(), ps.history.begin());
    // 
    if (flip_if_white && move.player() == WHITE) // different from state.turn() with odd-length history
      ps.flip();
    ret.push_back(ps.to_bitset());
    history5.push_back(move);
    if (history5.size() > 5) {
      auto old = history5.front();
      history5.pop_front();
      state.makeMove(old);
    }
  }
  return ret;
}

void osl::MiniRecord::guess_result(const EffectState& state) {
  if (state.inCheckmate())
    result = loss_result(state.turn());
  else if (win_if_declare(state)) {
    result = win_result(state.turn());
    final_move = Move::DeclareWin();
  }
}        

void osl::MiniRecord::add_move(Move moved, bool in_check) {
  assert(history.size()>0);
  moves.push_back(moved);
  history.push_back(history.back().new_zero_history(moved, in_check));
}

void osl::MiniRecord::settle_repetition() {
  HistoryTable table;
  int interrupt_number = history.size()-1;
  for (size_t i=0; i<history.size(); ++i) {
    auto result = table.add(i, history[i], history);
    if (result != InGame) {
      if (this->result != InGame && this->result != result) {
        std::cerr << "game result inconsistency " << this->result << ' ' << result << '\n';
        throw std::domain_error("game result inconsistency");
      }
      this->result = result;
      interrupt_number = i;
      break;
    }
  }
  if (interrupt_number < history.size()-1)
    std::cerr << "game terminated at " << interrupt_number
              << " by " << moves[interrupt_number-1]
              << " before " << history.size() << "\n";
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
    if (record.result != InGame)
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
  if (record.result == InGame)
    record.guess_result(latest);
  record.settle_repetition();
  return record;
}

osl::RecordSet osl::RecordSet::from_path(std::string path, int limit) {
  RecordSet result;
  auto folder = std::filesystem::path(path);
  if (limit < 0) {
    int count=0;
    for (auto& file: std::filesystem::directory_iterator{folder}) {
      if (! file.is_regular_file() || file.path().extension() != ".csa")
        continue;
      ++count;
    }
    limit = count;
  }
  result.records.reserve(limit);
  for (auto& file: std::filesystem::directory_iterator{folder}) {
    if (! file.is_regular_file() || file.path().extension() != ".csa")
      continue;
    if (result.records.size() >= result.records.capacity())
      break;

    result.records.push_back(csa::read_record(file));
  }
  return result;
}

osl::RecordSet osl::RecordSet::from_usi_lines(std::istream& is) {
  RecordSet result;
  std::string line;
  while (getline(is, line)){
    result.records.push_back(usi::read_record(line));
  }
  return result;
}
osl::RecordSet osl::RecordSet::from_usi_file(std::string path) {
  std::ifstream is(path);
  if (! is)
    throw std::domain_error("file not found"+path);
  return from_usi_lines(is);
}




osl::GameResult osl::csa::detail::parse_move_line(EffectState& state, MiniRecord& record, std::string s) {
  if (s.length()==0) 
    return InGame;
  switch (s.at(0)) {      
  case '+':
  case '-':{
    const Move m = csa::to_move(s,state);
    state.makeMove(m);
    record.add_move(m, state.inCheck());
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
    if (s == "%SENNICHITE" || s == "%CHUDAN")
      return Draw;
    // fall through
  }  
  case 'T':
  case '#':
  case '\'':
    break;
  default:
    if (s.starts_with("END"))
      break;
    std::cerr << "ignored " << s << '\n';
  }
  return InGame;
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
      record.set_initial_state(state);
      return true;
    }
  }
  case 'N': case '$': case 'V': case '\'':
    break;
  default:
    if (s.starts_with("BEGIN") || s.starts_with("END") || s.starts_with("Format")
        || s.starts_with("Declaration") || s.starts_with("Game_ID:") || s.starts_with("Your_Turn:")
        || s.starts_with("Rematch_On_Draw") || s.starts_with("To_Move") || s.starts_with("Max_Moves:")
        || s.starts_with("Time_Unit") || s.starts_with("Total_Time") || s.starts_with("Increment")
        )
      break;
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

osl::Move osl::psn::to_move_light(const std::string& str, const BaseState& state) {
  if (str.size() < 4)
    throw ParseError("move syntax error in usi::to_move " + str);

  const Square to = to_square(str.substr(2,2));
  Move move;
  if (str[1] == '*') {
    move = Move(to, to_ptype(str[0]), state.turn());
  }
  else {
    const Square from = to_square(str.substr(0,2));
    const Ptype ptype = state.pieceOnBoard(from).ptype();
    const Ptype captured = state.pieceOnBoard(to).ptype();
    bool promotion = false;
    if (str.size() > 4) {
      assert(str[4] == '+');
      promotion = true;
    }
    move = Move(from, to, (promotion ? promote(ptype) : ptype), 
                captured, promotion, state.turn());
  }
  if (! move.is_ordinary_valid())
    throw ParseError("move composition error in usi::to_move " + str);
  if (! state.move_is_consistent(move))
    throw ParseError("move inconsistent with state in usi::to_move "+str);
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
      int move_number; // will not be used
      is >> move_number;
    }
    state.initFinalize();
    record.set_initial_state(state);
  }
  if (! (is >> word))
    return record;
  if (word != "moves")
    throw ParseError("moves not found "+word);
  EffectState uptodate(record.initial_state);
  while (is >> word) {
    Move m = to_move(word, uptodate);
    if (! m.isNormal()) {
      record.moves.push_back(m); // typically poped back later
      break;
    }
    uptodate.makeMove(m);
    record.add_move(m, uptodate.inCheck());
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
  if (record.result == InGame)
    record.guess_result(uptodate);
  record.settle_repetition();
  return record;
} 

// ;;; Local Variables:
// ;;; mode:c++
// ;;; c-basic-offset:2
// ;;; End:
