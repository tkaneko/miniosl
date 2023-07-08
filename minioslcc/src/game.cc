#include "game.h"
#include "feature.h"
#include "impl/range-parallel.h"
#include <random>

osl::GameManager::GameManager() {
  record.set_initial_state(BaseState(HIRATE));
  state.generateLegal(legal_moves);
}

osl::GameManager::~GameManager() {
}

osl::GameResult osl::GameManager::add_move(Move move) {
  if (record.result != InGame)
    throw std::logic_error("not in game");

  state.makeMove(move);
  record.add_move(move, state.inCheck());
  auto result = table.add(record.state_size()-1, record.history.back(), record.history);
  if (result == InGame && state.inCheckmate())
    result = (move.player() == BLACK) ? BlackWin : WhiteWin;
  if (result == InGame && record.move_size() >= MiniRecord::draw_limit)
    result = Draw;
  if (result == InGame) {
    state.generateLegal(legal_moves);
    if (legal_moves.empty())    // wcsc (csa) 27-1
      result = loss_result(state.turn());
  }
  else
    legal_moves.clear();
  record.result = result;
  return result;
}

void osl::GameManager::export_heuristic_feature(float *ptr) const {
  export_heuristic_feature(state, record.moves.empty() ? Move() : record.moves.back(), ptr);
}

void osl::GameManager::export_heuristic_feature(const EffectState& given, Move given_move, float *ptr) {
  bool flip = given.turn() == WHITE;
  EffectState state(flip ? EffectState(given.rotate180()) : given);
  auto last_move = flip ? given_move.rotate180() : given_move;

  ml::helper::write_np_44ch(state, ptr);
  ml::helper::write_np_additional(state, flip, last_move, ptr + 9*9*44);  
}

void osl::GameManager::export_heuristic_feature_after(Move move, float *ptr) const {
  EffectState state {this->state};
  if (! state.isAcceptable(move))
    throw std::range_error("move");
  state.makeMove(move);
  export_heuristic_feature(state, move, ptr);  
}

osl::ParallelGameManager::ParallelGameManager(int N, bool force) : games(N), force_declare(force) {
}

osl::ParallelGameManager::~ParallelGameManager() {
}

std::vector<osl::GameResult> osl::ParallelGameManager::add_move_parallel(const std::vector<osl::Move>& moves) {
  if (moves.size() != n_parallel())
    throw std::invalid_argument("size");
  const int N = n_parallel();
  std::vector<GameResult> ret(N);
  auto add = [&](int l, int r) {
    for (int i=l; i<r; ++i) {
      ret[i] = games[i].add_move(moves[i]);
      if (force_declare && ret[i] == InGame) {
        games[i].record.guess_result(games[i].state);
        ret[i] = games[i].record.result;
      }
    }
  };
  run_range_parallel(N, add);
  for (int i=0; i<n_parallel(); ++i) {
    if (ret[i] != InGame) {
      completed_games.push_back(std::move(games[i].record));
      games[i] = GameManager();
    }
  }
  return ret;
}

void osl::ParallelGameManager::export_heuristic_feature_parallel(float *ptr) const {
  const auto N = n_parallel();
  const auto offset = ml::channel_id.size()*81;
  auto run = [&](int l, int r) {
    for (int i=l; i<r; ++i) {
      games[i].export_heuristic_feature(ptr + i*offset);
    }
  };
  run_range_parallel(N, run);
}

void osl::ParallelGameManager::
export_heuristic_feature_for_children_parallel(const std::vector<Move>& moves, int n_branch, float *ptr) const {
  const auto N = n_parallel();
  if (moves.size() != n_branch*N)
    throw std::invalid_argument("inconsistent n_branch");
  const auto offset = ml::channel_id.size()*81;
  auto run = [&](int l, int r) {
    for (int i=l; i<r; ++i)
      for (int j=0; j<n_branch; ++j) {
        int idx = i*n_branch+j;
        games[i].export_heuristic_feature_after(moves[idx], ptr + idx*offset);
      }
  };
  run_range_parallel(N, run);
}

std::vector<std::pair<double,osl::Move>>
osl::add_gumbel_noise(const MoveVector& moves, const std::array<float,2187>& logits, int top_n) {
  static std::default_random_engine rng; // todo thread local
  std::extreme_value_distribution<> gumbel {0, 1};
  
  std::vector<std::pair<double,Move>> pmv; // probability-move vector
  pmv.reserve(moves.size());
  for (auto move: moves) {
    auto p = logits[osl::ml::policy_move_label(move)];
    p += gumbel(rng);
    pmv.emplace_back(p, move);
  }
  std::partial_sort(pmv.begin(), pmv.begin()+std::min((int)pmv.size(), top_n), pmv.end(),
                    [](auto l, auto r){ return l.first > r.first; } );
  return pmv;
}
