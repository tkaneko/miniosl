#include "game.h"
#include "feature.h"
#include "impl/range-parallel.h"
#include "impl/rng.h"
#include "impl/checkmate.h"
#include <iostream>

osl::GameManager::GameManager() {
  record.set_initial_state(BaseState(HIRATE));
  state.generateLegal(legal_moves);
}

osl::GameManager::~GameManager() {
}

osl::GameResult osl::GameManager::make_move(Move move) {
  if (record.result != InGame)
    throw std::logic_error("not in game");

  if (move.isSpecial())
    throw std::logic_error("win or resign is not implemented yet"); // to accept, check declaration, set final move

  state.makeMove(move);
  record.append_move(move, state.inCheck());
  auto result = table.add(record.state_size()-1, record.history.back(), record.history);
  if (result == InGame && state.inCheckmate()) {
    result = (move.player() == BLACK) ? BlackWin : WhiteWin;
    record.final_move = Move::Resign();
  }
  if (result == InGame && record.move_size() >= MiniRecord::draw_limit)
    result = Draw;
  if (result == InGame) {
    state.generateLegal(legal_moves);
    if (legal_moves.empty())
      state.generateWithFullUnpromotions(legal_moves);
    if (legal_moves.empty()) {   // wcsc (csa) 27-1
      result = loss_result(state.turn());
      record.final_move = Move::Resign();
    }
  }
  else
    legal_moves.clear();
  record.result = result;
  return result;
}

void osl::GameManager::export_heuristic_feature(nn_input_element *ptr) const {
  ml::export_features(record.initial_state, record.moves, ptr);
}

osl::GameResult osl::GameManager::export_heuristic_feature_after(Move move, nn_input_element *ptr) const {
  if (! state.isAcceptable(move))
    throw std::domain_error("move");
  return export_heuristic_feature_after(move, record.initial_state, record.moves, ptr);
}

osl::GameResult osl::GameManager::
export_heuristic_feature_after(Move latest, BaseState initial, MoveVector history,
                               nn_input_element *ptr) {
  Player side = initial.turn();
  if (history.size() % 2)
    side = alt(side);

  history.push_back(latest);
  auto [state, _]  = ml::export_features(initial, history, ptr);
  auto ret = InGame;
  // Note: states are flipped if white to move to export future
  // So, the state here is always black to move.
  if (state.inCheckmate() || state.inNoLegalMoves())
    ret = win_result(side);
  else {
    auto move = state.tryCheckmate1ply();
    if (move.isNormal() || win_if_declare(state))
      ret = loss_result(side);
  }
  return ret;
}

osl::GameManager osl::GameManager::from_record(const MiniRecord& record) {
  GameManager mgr;
  mgr.record.set_initial_state(record.initial_state);
  mgr.state = record.initial_state;
  auto prev = InGame;
  for (auto move: record.moves) {
    if (prev != InGame)
      throw std::domain_error("terminated game");
    prev = mgr.make_move(move);
  }
  if (record.moves.empty())
    mgr.state.generateLegal(mgr.legal_moves);
  return mgr;
}

osl::ParallelGameManager::ParallelGameManager(int N, bool force, bool idraw)
  : games(N), force_declare(force), ignore_draw(idraw) {
}

osl::ParallelGameManager::~ParallelGameManager() {
}

std::vector<osl::GameResult> osl::ParallelGameManager::make_move_parallel(const std::vector<osl::Move>& moves) {
  if (moves.size() != n_parallel())
    throw std::invalid_argument("size");
  // std::cerr << to_csa(moves[0]) << '\n';
  const int N = n_parallel();
  std::vector<GameResult> ret(N);
  auto add = [&](int l, int r) {
    for (int i=l; i<r; ++i) {
      ret[i] = games[i].make_move(moves[i]);
      if (force_declare && ret[i] == InGame) {
        games[i].record.guess_result(games[i].state);
        ret[i] = games[i].record.result;
      }
    }
  };
  run_range_parallel(N, add);
  for (int i=0; i<n_parallel(); ++i) {
    if (ret[i] != InGame) {
      if (! ignore_draw || ret[i] != Draw)
        completed_games.push_back(std::move(games[i].record));
      reset(i);
    }
  }
  return ret;
}

template <bool with_noise>
std::vector<std::pair<double,osl::Move>>
osl::PlayerArray::sort_moves_impl(const MoveVector& moves, const policy_logits_t& logits, int top_n, TID tid,
                                  double noise_scale) {
  std::extreme_value_distribution<> gumbel {0, 1};
  
  std::vector<std::pair<double,Move>> pmv; // probability-move vector
  pmv.reserve(moves.size()); 
  for (auto move: moves) {
    auto p = logits[ml::policy_move_label(move)];
    if constexpr (with_noise)
      p += gumbel(rngs[idx(tid)]) * noise_scale;
    pmv.emplace_back(p, move);
  }
  std::partial_sort(pmv.begin(), pmv.begin()+std::min((int)pmv.size(), top_n), pmv.end(),
                    [](auto l, auto r){ return l.first > r.first; } );
  return pmv;
}

void osl::PlayerArray::new_series(const std::vector<GameManager>& games) {
  _games = &games;
  _decision.resize(games.size());
  check_size(games.size());
}

void osl::PlayerArray::check_ready() const {
  if (_games == nullptr)
    throw std::logic_error("not initialized");
}

void osl::PlayerArray::check_size(int n, int scale, std::string where) const {
  check_ready();
  if (n != n_parallel()*scale)
    throw std::invalid_argument("size mismatch "+where+" "+std::to_string(n)
                                + " != " + std::to_string(n_parallel())
                                + " * " + std::to_string(scale));
}

osl::PolicyPlayer::PolicyPlayer(bool greedy) : PlayerArray(greedy) {
}

osl::PolicyPlayer::~PolicyPlayer() {
}

std::string osl::PolicyPlayer::name() const {
  using namespace std::string_literals;
  return "policy-"s + (greedy ? "greedy" : "stochastic");
}

std::pair<int,bool> osl::PolicyPlayer::make_request(nn_input_element *ptr) {
  check_ready();
  // request for root
  GameArray::export_root_features(*_games, ptr);
  return {1, true};
}

bool osl::PolicyPlayer::recv_result(const std::vector<policy_logits_t>& logits, const std::vector<std::array<float,1>>&) {
  // always make a decision in a single inference
  check_size(logits.size());
  auto run = [&](int l, int r, TID tid) {
    for (int g=l; g<r; ++g) {
      auto ret = greedy
        ? sort_moves((*_games)[g].legal_moves, logits[g], 1)
        : sort_moves_with_gumbel((*_games)[g].legal_moves, logits[g], 1, tid);
      _decision[g] = ret[0].second;
    }
  };
  run_range_parallel_tid(n_parallel(), run);
  return true;
}

osl::FlatGumbelPlayer::FlatGumbelPlayer(int width, double ns, int greedy_after)
  : PlayerArray(/* greedy */ ns == 0), root_width(width), greedy_threshold(greedy_after), noise_scale(ns) {
}

osl::FlatGumbelPlayer::~FlatGumbelPlayer() {
}

std::string osl::FlatGumbelPlayer::name() const {
  return "gumbel-flat-" + std::to_string(root_width);
}

std::pair<int,bool> osl::FlatGumbelPlayer::make_request(nn_input_element *ptr) {
  check_ready();
  // phase 1: sort moves by policy
  if (root_children.empty()) {
    // request for root
    GameArray::export_root_features(*_games, ptr);
    return {1, true};
  }
  // phase 2: examine top-n moves
  std::fill(ptr, ptr+n_parallel()*ml::input_unit, 0);
  auto run = [&](int l, int r) {
    for (int g=l; g<r; ++g) {
      for (int i=0; i<root_width; ++i) {
        int idx = g*root_width + i;
        auto terminated_by_move
          = (*_games)[g].export_heuristic_feature_after(root_children[idx].second, ptr + idx*ml::input_unit);
        root_children_terminal[idx] = terminated_by_move;
      }
    }
  };
  run_range_parallel(n_parallel(), run);
  return {root_width, false};
}

bool osl::FlatGumbelPlayer::recv_result(const std::vector<policy_logits_t>& logits, const std::vector<std::array<float,1>>& values) {
  // phase 1
  if (root_children.empty()) {
    check_size(logits.size(), 1, "FlatGumbelPlayer recv phase1");
    root_children.resize(root_width * n_parallel());
    root_children_terminal.resize(root_width * n_parallel());    

    auto run = [&](int l, int r, TID tid) {
      for (int g=l; g<r; ++g) {
        auto ns = noise_scale;
        if ((*_games)[g].record.move_size() >= greedy_threshold)
          ns = 0.0;
        auto ret = sort_moves_with_gumbel((*_games)[g].legal_moves, logits[g], root_width, TID(0), ns);
        while (ret.size() < root_width)
          ret.push_back(ret[0]);
        int offset = g*root_width;
        for (int i=0; i<root_width; ++i)
          root_children[offset + i] = ret[i];
      }
    };
    run_range_parallel_tid(n_parallel(), run);
    return false;
  }
  // phase 2
  check_size(values.size(), root_width, "FlatGumbelPlayer recv phase2");

  auto turn = root_children[0].second.player();
  auto run = [&](int l, int r) {
    for (int g=l; g<r; ++g) {
      int offset = g*root_width;
      for (int i=0; i<root_width; ++i) {
        auto value = /* negamax */ -values[offset + i][0];
        auto terminal = root_children_terminal[offset + i];
        if (terminal != InGame) {
          if (! has_winner(terminal))
            value = 0;
          else
            value = (terminal == win_result(turn)) ? 1.0 : -1.0;
        }
        root_children[offset + i].first += transformQ(value);
      }
      auto best = std::max_element(&root_children[offset], &root_children[offset+root_width]);
      _decision[g] = best->second;      
    }
  };
  run_range_parallel(n_parallel(), run);
  root_children.clear();
  root_children_terminal.clear(); // 
  return true;
}

osl::SingleCPUPlayer::~SingleCPUPlayer() {
}

osl::CPUPlayer::CPUPlayer(std::shared_ptr<SingleCPUPlayer> pl, bool greedy) : PlayerArray(greedy), player(pl) {
}

osl::CPUPlayer::~CPUPlayer() {
}

std::pair<int,bool> osl::CPUPlayer::make_request(nn_input_element *) {
  check_ready();
  for (int g=0; g<n_parallel(); ++g) {
    std::string usi = to_usi((*_games)[g].record);
    _decision[g] = player->think(usi); // future
  }
  return {0, false};
}

bool osl::CPUPlayer::recv_result(const std::vector<policy_logits_t>& logits, const std::vector<std::array<float,1>>& values) {
  return true;
}

osl::RandomPlayer::RandomPlayer() : SingleCPUPlayer() {
}

osl::RandomPlayer::~RandomPlayer() {
}

osl::Move osl::RandomPlayer::think(std::string line) {
  EffectState state;
  usi::parse(line, state);
  MoveVector moves;
  state.generateLegal(moves);
  int id = rngs[0]() % moves.size();
  return moves.at(id);
}

std::string osl::RandomPlayer::name() {
  return "random-player";
}


void osl::GameArray::export_root_features(const std::vector<GameManager>& games, nn_input_element *ptr) {
  const auto N = games.size();
  std::fill(ptr, ptr+N*ml::input_unit, 0);
  auto run = [&](int l, int r) {
    for (int i=l; i<r; ++i) {
      games[i].export_heuristic_feature(ptr + i*ml::input_unit);
    }
  };
  run_range_parallel(N, run);
}

osl::GameArray::GameArray(int N, PlayerArray& a, PlayerArray& b, InferenceModel& m,
                          bool ignore_draw, double ropening)
  : mgrs(N, true, ignore_draw), players{&a, &b}, model(m),
    input_buf(), policy_buf(), value_buf(), skip_one_turn(N),
    max_width(std::max(players[0]->max_width(), players[1]->max_width())),
    random_opening(ropening) {
  players[0]->new_series(mgrs.games);
  if (players[0] != players[1])
    players[1]->new_series(mgrs.games);

  resize_buffer(max_width);
}

osl::GameArray::~GameArray() {
}

void osl::GameArray::warmup(int n) {
  resize_buffer(1);
  std::ranges::fill(input_buf, 0);
  for (int i=0; i<n; ++i)
    model.test_run(input_buf, policy_buf, value_buf);
}

void osl::GameArray::resize_buffer(int width) {
  int sz = width * mgrs.n_parallel();
  input_buf.resize(sz * ml::input_unit);
  policy_buf.resize(sz);
  value_buf.resize(sz);
}

void osl::GameArray::step() {
  // (1) thinking
  int safety_limit = 16, cnt=0;
  bool ready = false;
  do {
    // std::cerr << mgrs.games[0].state;
    
    resize_buffer(players[side]->max_width());
    auto [req_size, need_policy] = players[side]->make_request(&input_buf[0]);
    resize_buffer(req_size);

    if (req_size > 0) {
      if (! need_policy)
        policy_buf.resize(0);
      model.batch_infer(input_buf, policy_buf, value_buf);
    }
    
    ready = players[side]->recv_result(policy_buf, value_buf);
    if (++cnt > safety_limit)
      throw std::runtime_error("step too long");
  } while (! ready);

  // (2) make move
  auto moves = players[side]->decision();
  if (random_opening > 0) {
    std::uniform_real_distribution<> r01(0, 1);
    for (int g=0; g<mgrs.n_parallel(); ++g) {
      if (mgrs.games[g].record.move_size() >= 2 || r01(rngs[0]) > random_opening)
        continue;
      std::ranges::sample(mgrs.games[g].legal_moves, &moves[g], 1, rngs[0]);
    }
  }
  auto ret = mgrs.make_move_parallel(moves);
  //std::cerr << to_csa(players[side]->decision()[0]) << '\n';

  // (2') misc to force player_a as the first player after completing odd-length game
  for (int g=0; g<mgrs.n_parallel(); ++g) {
    if (skip_one_turn[g]) {
      mgrs.reset(g);
      skip_one_turn[g] = 0;
    }
    else if (ret[g] != InGame && side == 0)
      skip_one_turn[g] = 1;     // will reset at next step
  }

  // (3) switch side to move
  side ^= 1;
}

osl::InferenceModel::~InferenceModel() {
}

void osl::InferenceModel::test_run(std::vector<nn_input_element>& in,
                                   std::vector<policy_logits_t>& out,
                                   std::vector<std::array<float,1>>& vout) {
  batch_infer(in, out, vout);
}

#ifdef ENABLE_RANGE_PARALLEL
const int osl::range_parallel_threads = std::min(std::max(1, (int)std::thread::hardware_concurrency()/2),
                                                 rng::available_instances);
#endif
