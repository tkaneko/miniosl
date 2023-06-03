#include "acutest.h"
#include "state.h"
#include "more.h"
#include "record.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

#define TEST_CHECK_EQUAL(a,b) TEST_CHECK((a) == (b))
#define TEST_ASSERT_EQUAL(a,b) TEST_ASSERT((a) == (b))

namespace osl
{
  namespace move_generator
  {
    // removed from miniosl, serving as interface of move_generator::Capture
    struct GenerateCapture
    {
      static void generate(Player p, const EffectState& state,Square target,
			   Store& action)
      {
	if (p == BLACK)
	  Capture::template generate<BLACK>(state,target,action);
	else
	  Capture::template generate<WHITE>(state,target,action);
      }
      static void generate(Player P, const EffectState& state,Square target,
			   MoveVector& out)
      {
	Store store(out);
	generate(P, state, target, store);
      }
      static void generate(const EffectState& state,Square target,
			   MoveVector& out)
      {
	generate(state.turn(), state, target, out);
      }
      static void escapeByCapture(Player p, const EffectState& state,Square target,
				  Piece piece,Store& action)
      {
	if (p == BLACK)
	  Capture::template escapeByCapture<BLACK>(state,target,piece,action);
	else
	  Capture::template escapeByCapture<WHITE>(state,target,piece,action);
      }
    };
  }
  bool consistent_transition(const EffectState& now, const EffectState& prev, Move moved) {
    // test changedEffects
    const CArray<BoardMask,2> changed_squares = {{ now.changedEffects(BLACK), now.changedEffects(WHITE) }};
    const auto changed_all = changed_squares[BLACK] | changed_squares[WHITE];
    CArray<BoardMask, Piece::SIZE> each_effect, prev_effect;
    for (int i: all_piece_id()) {
      each_effect[i].clear();
      prev_effect[i].clear();
    }
    for (int y: board_y_range()) {
      for (int x: board_x_range()) {
        const Square sq(x, y);
        for (int i: all_piece_id()) {
          if (now.effectAt(sq).test(i))
            each_effect[i].set(sq);
          if (prev.effectAt(sq).test(i))
            prev_effect[i].set(sq);
        }
        if (! changed_all.test(sq))
          if (now.effectAt(sq) != prev.effectAt(sq)) 
            return false;
        for (Player pl: players) {
          if (! changed_squares[pl].test(sq))
            if ((now.effectAt(sq) & now.piecesOnBoard(pl))
                != (prev.effectAt(sq) & prev.piecesOnBoard(pl))) 
              return false;
        }
      }
    }
    // test changedSource()
    const EffectPieceMask changed_effect_pieces = now.changedSource(); 
    for (int i: all_piece_id()) {
      if (each_effect[i] == prev_effect[i])
        continue;
      if (! changed_effect_pieces.test(i)) {
        return false;
      }
    }
    // test effectedChanged(Player pl)
    for (int i: all_piece_id()) {
      for (Player pl: players) {
        if (prev.pieceOf(i).square() == moved.to())
          continue;		// captured
        if (prev.effectedPieces(pl).test(i) != now.effectedPieces(pl).test(i)) {
          if (! now.effectedChanged(pl).test(i)) {
            return false;
          }
        }
      }
    }
    return true;
  }
}

using namespace osl;

template <class Container, class T>
bool is_member(const Container& c, const T& val) {
  return std::find(c.begin(), c.end(), val) != c.end();
}

auto make_path() {
  return std::filesystem::path("csa");
}

#ifdef NDEBUG
const int limit = 65536;
#else
const int limit = 1000;
#endif

void test_path() {
  try {
    auto csa = make_path();
    int count=0;
    for (auto& file: std::filesystem::directory_iterator{csa}) {
      if (! file.is_regular_file() || file.path().extension() != ".csa")
        continue;

      if (++count > limit)
        break;

      auto record=csa::read_record(file);
      auto state=record.initial_state;
      for (auto move:record.moves) {
        TEST_CHECK(state.isLegal(move));
        state.makeMove(move);
        TEST_CHECK(! state.inCheck(alt(state.turn())));
      }
    }
    TEST_ASSERT(count > 0);
  }
  catch (std::runtime_error& e) {
    std::cerr << e.what() << "\n";
    std::cerr << "please place game records under csa folder to enable this test\n";
    throw;
  }
}

void test_piece_stand()
{
  auto csa = make_path();
  int count = 0;
  for (auto& file: std::filesystem::directory_iterator{csa}) {
    if (! file.is_regular_file() || file.path().extension() != ".csa")
      continue;
    if (++count > limit)
      break;

    auto record=csa::read_record(file);
    auto state = record.initial_state;
      
    PieceStand black(BLACK, state);
    PieceStand white(WHITE, state);
    
    for (Move m: record.moves) {
      state.makeMove(m);

      PieceStand black_new(BLACK, state);
      PieceStand white_new(WHITE, state);

      TEST_CHECK_EQUAL(black_new, black.nextStand(BLACK, m));
      TEST_CHECK_EQUAL(white_new, white.nextStand(WHITE, m));

      TEST_CHECK_EQUAL(black, black_new.previousStand(BLACK, m));
      TEST_CHECK_EQUAL(white, white_new.previousStand(WHITE, m));

      black = black_new;
      white = white_new;
    }
  }
}

void test_copy()
{
  auto csa = make_path();
  int count = 0;
  
  for (auto& file: std::filesystem::directory_iterator{csa}) {
    if (! file.is_regular_file() || file.path().extension() != ".csa")
      continue;
    if (++count > limit/4)
      break;

    auto record=csa::read_record(file);
    auto state(record.initial_state);
    EffectState state2 = state;

    TEST_CHECK(state.isConsistent());
    TEST_CHECK(state2.isConsistent());
    for (Move move: record.moves) {
      state.makeMove(move);
      TEST_CHECK(consistent_transition(state, state2, move));
      state2.copyFrom(state);

      TEST_CHECK(state.isConsistent());
      TEST_ASSERT(state2.isConsistent());
      TEST_CHECK_EQUAL(state, state2);
      TEST_CHECK_EQUAL(state.changedEffects(BLACK), state2.changedEffects(BLACK));
      TEST_CHECK_EQUAL(state.changedEffects(WHITE), state2.changedEffects(WHITE));
      TEST_CHECK_EQUAL(state.changedSource(), state2.changedSource());
      TEST_CHECK_EQUAL(state.effectedChanged(BLACK), state2.effectedChanged(BLACK));
      TEST_CHECK_EQUAL(state.effectedChanged(WHITE), state2.effectedChanged(WHITE));
    }
  }
}

void test_changed_effect()
{
  auto csa = make_path();
  int count = 0;
  
  for (auto& file: std::filesystem::directory_iterator{csa}) {
    if (! file.is_regular_file() || file.path().extension() != ".csa")
      continue;
    if (++count > limit)
      break;

    auto record=csa::read_record(file);
    auto state(record.initial_state);
    for (auto move:record.moves) {
      PieceMask before[9][9][2];
      {
        for (int x=1;x<=9;x++)
          for (int y=1;y<=9;y++) {
            const Square pos(x,y);
            TEST_ASSERT_EQUAL(state.hasLongEffectAt<LANCE>(BLACK, pos),
                              state.longEffectAt<LANCE>(pos, BLACK) != 0);
            TEST_CHECK_EQUAL(state.hasLongEffectAt<LANCE>(WHITE, pos),
                             state.longEffectAt<LANCE>(pos, WHITE) != 0);
            TEST_CHECK_EQUAL(state.hasLongEffectAt<BISHOP>(BLACK, pos),
                             state.longEffectAt<BISHOP>(pos, BLACK) != 0);
            TEST_ASSERT_EQUAL(state.hasLongEffectAt<BISHOP>(WHITE, pos),
                              state.longEffectAt<BISHOP>(pos, WHITE) != 0);
            TEST_CHECK_EQUAL(state.hasLongEffectAt<ROOK>(BLACK, pos),
                             state.longEffectAt<ROOK>(pos, BLACK) != 0);
            TEST_CHECK_EQUAL(state.hasLongEffectAt<ROOK>(WHITE, pos),
                             state.longEffectAt<ROOK>(pos, WHITE) != 0);
            for (int z=0; z<2; ++z) {
              const Player pl = players[z];
              before[x-1][y-1][z]=
                state.effectAt(pos)&state.piecesOnBoard(pl);
            }
          }
      }
      state.makeMove(move);
      PieceMask after[9][9][2];

      for (int x=1;x<=9;x++)
        for (int y=1;y<=9;y++) {
          const Square pos(x,y);
          for (int z=0; z<2; ++z) {
            const Player pl = players[z];
            after[x-1][y-1][z]=
              state.effectAt(pos)&state.piecesOnBoard(pl);
          }
        }
      for (int x=1;x<=9;x++)
        for (int y=1;y<=9;y++) {
          const Square pos(x,y);
          for (int z=0; z<2; ++z) {
            const Player pl = players[z];
            PieceMask b=before[x-1][y-1][z];
            PieceMask a=after[x-1][y-1][z];
            if (a!=b || (pos==move.to() && move.capturePtype()!=Ptype_EMPTY)) {
              for (int num=0;num<48;num++) {
                if (b.test(num)!=a.test(num) || 
                   (pos==move.to() && move.capturePtype()!=Ptype_EMPTY && (b.test(num) || a.test(num)))) {
                  TEST_CHECK(state.changedSource().test(num));
                }
              }
            }
            if (state.changedEffects(pl).test(pos)) {
              Piece p=state.pieceAt(move.to());
              if (b==a) {
                if (!a.test(p.id())) {
                  std::cerr << std::endl << state << ",move=" << move << ",x=" << x << ",y=" << y << std::endl;
                  std::cerr << "changed[" << pl << "]=" << std::endl << state.changedEffects(pl);
                }
              }
            }
            else {
              if (b!=a) {
                std::cerr << state << ",move=" << move << ",x=" << x << ",y=" << y << std::endl;
                std::cerr << "before " << b << "\nafter  " << a << "\n";
                std::cerr << "changed[ " << pl << "]=" << std::endl << state.changedEffects(pl);
              }
              TEST_ASSERT_EQUAL(b, a);
            }
          }
        }
    }
  }
}

void test_capture()
{
  using osl::move_generator::GenerateCapture;
  {
    EffectState state(csa::read_board(
                                   "P1+NY+TO * +HI *  * -OU-KE-KY\n"
                                   "P2 *  *  *  *  * -GI-KI *  *\n"
                                   "P3 *  * +GI * +UM * -KI-FU-FU\n"
                                   "P4 *  * +FU-FU *  *  *  *  *\n"
                                   "P5 *  * -KE+FU+FU *  * +FU *\n"
                                   "P6+KE *  *  *  * -FU *  * +FU\n"
                                   "P7 *  * -UM *  *  *  *  *  *\n"
                                   "P8 *  *  *  *  *  *  *  *  * \n"
                                   "P9 * +OU * -GI *  *  *  * -NG\n"
                                   "P+00HI00KI00KE00KY00FU00FU00FU00FU00FU00FU\n"
                                   "P-00KI00KY00FU00FU\n"
                                   "P-00AL\n"
                                   "+\n"
                                   ));
    MoveVector moves;
    {
      MoveStore store(moves);
      GenerateCapture::generate(BLACK,state,Square(6,4),store);
    }
    // moves.unique();
    TEST_CHECK(is_member(moves, Move(Square(6,1),Square(6,4),PROOK,PAWN,true,BLACK)));
    // the next move is not generated bacause the rook should promote
    TEST_CHECK(is_member(moves, Move(Square(7,3),Square(6,4),PSILVER,PAWN,true,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(7,3),Square(6,4),SILVER,PAWN,false,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(6,5),Square(6,4),PAWN,PAWN,false,BLACK)));
    TEST_CHECK(is_member(moves, Move(Square(5,3),Square(6,4),PBISHOP,PAWN,false,BLACK)));
    TEST_CHECK(moves.size()==5);

    for(auto move: moves)
      TEST_CHECK(state.isLegal(move));
  }
  auto csa = make_path();
  int count = 0;
  
  for (auto& file: std::filesystem::directory_iterator{csa}) {
    if (! file.is_regular_file() || file.path().extension() != ".csa")
      continue;
    if (++count > limit)
      break;

    auto record=csa::read_record(file);
    auto state(record.initial_state);
    for (auto move:record.moves) {
      // 王手がかかっているときはcaptureを呼ばない
      if (state.inCheck())
        continue;
      MoveVector all;
      state.generateLegal(all);
      for (int y=1;y<=9;y++)
        for (int x=9;x>0;x--) {
          Square pos(x,y);
          Piece p=state.pieceAt(pos);
          if (! p.isEmpty() && p.owner()==alt(state.turn())) {
            MoveVector capture;
            {
              MoveStore store(capture);
              GenerateCapture::generate(state.turn(),state,pos,store);
            }
            // capture.unique();
            for (Move m:capture) {
              TEST_CHECK(state.isLegal(m) && m.to()==pos);
              TEST_CHECK(!m.ignoreUnpromote());
              TEST_CHECK(state.isSafeMove(m));
            }
            for (Move m:all) {
              if (m.to()==pos) {
                if (!state.isSafeMove(m)) continue;
                TEST_CHECK(is_member(capture, m));
              }
            }
          }
        }
      state.makeMove(move);
    }
  }
}

void test_check()
{
  auto csa = make_path();
  int count = 0;
  
  for (auto& file: std::filesystem::directory_iterator{csa}) {
    if (! file.is_regular_file() || file.path().extension() != ".csa")
      continue;
    if (++count > limit)
      break;

    auto record=csa::read_record(file);
    auto state(record.initial_state);
    int cnt = 0;
    for (auto move:record.moves) {
      MoveVector all, check;
      state.generateLegal(all);
      state.generateCheck(check);
      for (auto m: all) {
        bool check_in_all = state.isCheck(m);
        bool check_generated = std::ranges::find(check, m) != check.end();
        if (check_in_all != check_generated) {
          std::cerr << '\n' << state << ' ' << m << ' ' << check_in_all << ' ' << check_generated << '\n';
          std::cerr << "check generated " << check.size() << ' ' << check << '\n';
        }
        TEST_ASSERT(check_in_all == check_generated);
        ++cnt;
      }
      // TEST_CHECK(state.isConsistent());
      state.makeMove(move);
    }
  }
}

void test_pack_position()
{
  auto csa = make_path();
  int count = 0;
  
  for (auto& file: std::filesystem::directory_iterator{csa}) {
    if (! file.is_regular_file() || file.path().extension() != ".csa")
      continue;
    if (++count > limit/2)
      break;

    auto record=csa::read_record(file);
    auto state(record.initial_state);
    PackedState ps2;
    int cnt = 0;
    for (auto move:record.moves) {      
      PackedState ps{state, move, record.result};
      auto bs = ps.to_bitset();
      ps2.restore(bs);
      
      TEST_ASSERT(to_usi(ps.state) == to_usi(ps2.state));
      TEST_ASSERT(move == ps2.next);
      TEST_MSG("%s v.s. %s", to_usi(move).c_str(), to_usi(ps2.next).c_str());
      TEST_ASSERT(ps2.result == record.result);
      state.makeMove(move);
    }
  }
}


TEST_LIST = {
  { "path", test_path },
  { "check", test_check },
  { "capture", test_capture },
  { "copy", test_copy },
  { "piece_stand", test_piece_stand },
  { "changed_effect", test_changed_effect },
  { "pack_position", test_pack_position },
  { nullptr, nullptr }
};
