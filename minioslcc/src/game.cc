#include "game.h"
#include "feature.h"
#include "impl/range-parallel.h"
#include "impl/rng.h"
#include "impl/checkmate.h"
#include <iostream>

osl::GameManager::GameManager(std::optional<int> shogi816k_id) {
  if (! shogi816k_id)
    record.set_initial_state(BaseState(HIRATE));
  else {
    int id = shogi816k_id.value();
    if (id < 0)
      id = rngs[0]() % Shogi816K_Size;
    state = EffectState(BaseState(Shogi816K, id));
    record.set_initial_state(state, id);
  }
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
  auto ret = export_heuristic_feature_after(move, record.initial_state, record.moves, ptr);
  if (ret == InGame && !state.inCheck() && ! state.isCheck(move)) {
    if (table.has_entry(record.history.back().basic(), move))
      ret = Draw;
  }
  return ret;
}

bool osl::GameManager::export_heuristic_feature_after(Move move, int reply_code,
                                                      nn_input_element *ptr) const {
  if (! state.isAcceptable(move))
    throw std::domain_error("move");
  BaseState copy(state);
  copy.make_move_unsafe(move);
  try {
    auto reply = ml::decode_move_label(reply_code, copy);
    if (reply.is_ordinary_valid() && copy.move_is_consistent(reply)) {
      // reply is a roughly valid move
      auto history = record.moves;
      history.push_back(move);
      export_heuristic_feature_after(reply, record.initial_state, std::move(history), ptr);
      return true;
    }
  }
  catch (std::domain_error& e) {
    ;                           // fall through
  }
  return false;
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
  mgr.record.set_initial_state(record.initial_state, record.shogi816k_id);
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

osl::ParallelGameManager::ParallelGameManager(int N, std::optional<GameConfig> cfg)
  : games(),
    config(cfg.value_or(GameConfig())) {
  games.reserve(N);
  for (int i=0; i<N; ++i)
    games.emplace_back(make_newgame());
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
      if (config.force_declare && ret[i] == InGame) {
        games[i].record.guess_result(games[i].state);
        ret[i] = games[i].record.result;
      }
    }
  };
  run_range_parallel(N, add);
  for (int i=0; i<n_parallel(); ++i) {
    if (ret[i] != InGame) {
      if (! config.ignore_draw || ret[i] != Draw)
        completed_games.push_back(std::move(games[i].record));
      reset(i);
    }
  }
  return ret;
}

osl::PlayerArray::PlayerArray(bool greedy_) : greedy(greedy_), rngs(rng::make_rng_array()) {
}

template <bool with_noise>
std::vector<std::pair<float,osl::Move>>
osl::PlayerArray::sort_moves_impl(const MoveVector& moves, const policy_logits_t& logits, int top_n,
                                  rng_t *rng, float noise_scale) {
  std::extreme_value_distribution<> gumbel {0, 1};
  
  std::vector<std::pair<float,Move>> pmv; // probability-move vector
  pmv.reserve(moves.size()); 
  for (auto move: moves) {
    auto p = logits[ml::policy_move_label(move)];
    if constexpr (with_noise)
      p += gumbel(*rng) * noise_scale;
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

bool osl::PolicyPlayer::make_request(int, nn_input_element *ptr) {
  check_ready();
  // request for root
  GameArray::export_root_features(*_games, ptr);
  return true;
}

bool osl::PolicyPlayer::recv_result(int,
                                    const std::vector<policy_logits_t>& logits,
                                    const std::vector<value_vector_t>&) {
  // always make a decision in a single inference
  check_size(logits.size());
  auto run = [&](int l, int r, TID tid) {
    for (int g=l; g<r; ++g) {
      auto ret = greedy
        ? sort_moves((*_games)[g].legal_moves, logits[g], 1)
        : sort_moves_with_gumbel((*_games)[g].legal_moves, logits[g], 1, &rngs[idx(tid)]);
      _decision[g] = ret[0].second;
    }
  };
  run_range_parallel_tid(n_parallel(), run);
  return true;
}

osl::FlatGumbelPlayer::FlatGumbelPlayer(GumbelPlayerConfig config)
  : PlayerArray(/* greedy */ config.noise_scale == 0),
    root_width(config.root_width), second_width(config.second_width),
    greedy_threshold(config.greedy_after),
    noise_scale(config.noise_scale),
    softalpha(config.softalpha), value_mix(config.value_mix),
    depth_weight(config.depth_weight) {
}

osl::FlatGumbelPlayer::~FlatGumbelPlayer() {
}

std::string osl::FlatGumbelPlayer::name() const {
  using namespace std::string_literals;
  auto ret = "gumbel-"s + (softalpha ? "soft-" : "flat-") + std::to_string(root_width);
  if (second_width > 0)
    ret += "-" + std::to_string(second_width);
  if (value_mix == GumbelPlayerConfig::td)
    ret += "td";
  else if (value_mix == GumbelPlayerConfig::ave)
    ret += "ave";
  else if (value_mix == GumbelPlayerConfig::max)
    ret += "max";
  return ret;
}

int osl::FlatGumbelPlayer::width(int phase) const {
  if (phase == 0)
    return 1;                   // root
  if (phase == 1)
    return root_width;
  if (phase == 2)
    return second_width;
  throw std::invalid_argument("phase error " + std::to_string(phase));
}

bool osl::FlatGumbelPlayer::make_request(int phase, nn_input_element *ptr) {
  check_ready();
  // phase 0: sort moves by policy
  if (phase == 0) {
    // request for root
    GameArray::export_root_features(*_games, ptr);
    return true;
  }
  // phase 1: examine top-n moves
  if (phase == 1) {
    auto run = [&](int l, int r) {
      for (int g=l; g<r; ++g) {
        for (int i=0; i<root_width; ++i) {
          int idx = g*root_width + i;
          auto terminated_by_move
            = (*_games)[g].export_heuristic_feature_after(get<1>(root_children[idx]),
                                                          ptr + idx*ml::input_unit);
          root_children_terminal[idx] = terminated_by_move;
        }
      }
    };
    run_range_parallel(n_parallel(), run);
    return second_width > 0; // policy is only needed with phase2
  }
  // phase 2:
  assert (phase == 2);
  auto run = [&](int l, int r) {
    for (int g=l; g<r; ++g) {
      for (int i=0; i<second_width; ++i) {
        int idx_child = g*root_width + i;
        int idx_input = g*second_width + i;
        bool ok = 
          (*_games)[g].export_heuristic_feature_after(get<1>(root_children[idx_child]),
                                                      get<2>(root_children[idx_child]),
                                                      ptr + idx_input*ml::input_unit);
        if (! ok)
          get<2>(root_children[idx_child]) = 0;
      }
    }
  };
  run_range_parallel(n_parallel(), run);
  return false;   // need value only
}

bool osl::FlatGumbelPlayer::recv_result(int phase,
                                        const std::vector<policy_logits_t>& logits,
                                        const std::vector<value_vector_t>& values) {
  // phase 0
  if (phase == 0) {
    check_size(logits.size(), 1, "FlatGumbelPlayer recv phase1");
    if (root_children.empty() != root_width * n_parallel()) {
      root_children.resize(root_width * n_parallel());
      root_children_terminal.resize(root_width * n_parallel());    
    }
    auto run = [&](int l, int r, TID tid) {
      for (int g=l; g<r; ++g) {
        auto ns = noise_scale;
        if ((*_games)[g].record.move_size() >= greedy_threshold)
          ns = 0.0;
        auto ret = sort_moves_with_gumbel((*_games)[g].legal_moves, logits[g], root_width,
                                          &rngs[idx(tid)], ns);
        while (ret.size() < root_width)
          ret.push_back(ret[0]);
        int offset = g*root_width;
        for (int i=0; i<root_width; ++i)
          root_children[offset + i] = std::make_tuple(ret[i].first, ret[i].second, 0, 0.);
      }
    };
    run_range_parallel_tid(n_parallel(), run);
    return false;
  }
  auto turn = get<1>(root_children[0]).player();
  // phase 1
  if (phase == 1) {
    check_size(values.size(), root_width, "FlatGumbelPlayer recv phase1");
    auto run = [&](int l, int r) {
      for (int g=l; g<r; ++g) {
        int offset = g*root_width;
        int c_visit = std::max(50, (*_games)[g].record.move_size());
        for (int i=0; i<root_width; ++i) {
          const auto& vec = values[offset + i];
          auto cv = vec[0];
          if (value_mix == GumbelPlayerConfig::td)
            cv = vec[1];
          else if (value_mix == GumbelPlayerConfig::ave)
            cv = vec[0]*0.75 + vec[1]*0.25;
          else if (value_mix == GumbelPlayerConfig::max)
            cv = std::min(vec[0], vec[1]); // optimistic
          auto value = /* negamax */ - (cv + softalpha*vec[2]);
          auto terminal = root_children_terminal[offset + i];
          if (terminal != InGame) {
            if (! has_winner(terminal))
              value = 0;
            else
              value = (terminal == win_result(turn)) ? 1.0 : -1.0;
          }
          get<0>(root_children[offset + i]) += transformQ(value, c_visit);
          if (second_width > 0) {
            // check policy for the opponent
            auto best_reply = std::ranges::max_element(logits[offset + i]) - logits[offset + i].begin();
            get<2>(root_children[offset + i]) = best_reply;
            // store child value for later adustment
            get<3>(root_children[offset + i]) = value * depth_weight;
          }
        }
        if (second_width == 0) {
          auto best = std::max_element(&root_children[offset], &root_children[offset+root_width]);
          _decision[g] = get<1>(*best);
        }
        else {
          // sort according to logits + transformedQ-of-depth1
          std::partial_sort(root_children.begin()+offset,
                            root_children.begin()+offset+second_width,
                            root_children.begin()+offset+root_width,
                            [](auto l, auto r){ return std::get<0>(l) > std::get<0>(r); } );
          // prepare for final decision by logits + transformQ(average(depth1_value, depth2_value))
          for (int c=0; c<second_width; ++c)
            get<0>(root_children[offset+c]) -= get<3>(root_children[offset+c]);
        }
      }
    };
    run_range_parallel(n_parallel(), run);
    if (second_width == 0)
      return true;
    return false;               // has phase2
  }
  // phase 2
  check_size(values.size(), second_width, "FlatGumbelPlayer recv phase2");
  auto run = [&](int l, int r) {
    for (int g=l; g<r; ++g) {
      int offset = g*root_width, index_offset = g*second_width;
      int c_visit = std::max(50, (*_games)[g].record.move_size());
      for (int i=0; i<second_width; ++i) {
        if (get<2>(root_children[offset + i]) == 0 // invalid id for reply
            || root_children_terminal[offset + i] != InGame)
          continue;
        const auto& vec = values[index_offset + i];
        auto cv = vec[0];
        if (value_mix == GumbelPlayerConfig::td)
          cv = vec[1];
        else if (value_mix == GumbelPlayerConfig::ave)
          cv = vec[0]*0.75 + vec[1]*0.25;
        else if (value_mix == GumbelPlayerConfig::max)
          cv = std::min(vec[0], vec[1]); // optimistic
        auto value = /* negamax*2 */ (cv + softalpha*vec[2]);
        // the final value consists of logits + transformQ((value-of-depth1 + value-of-depth2)/2.0);
        get<0>(root_children[offset + i]) += transformQ(value, c_visit) * depth_weight;
      }
      auto best = std::max_element(&root_children[offset], &root_children[offset+second_width]);
      _decision[g] = get<1>(*best);
    }
  };
  run_range_parallel(n_parallel(), run);
  return true;
}

osl::SingleCPUPlayer::~SingleCPUPlayer() {
}

osl::CPUPlayer::CPUPlayer(std::shared_ptr<SingleCPUPlayer> pl, bool greedy) : PlayerArray(greedy), player(pl) {
}

osl::CPUPlayer::~CPUPlayer() {
}

bool osl::CPUPlayer::make_request(int, nn_input_element *) {
  check_ready();
  for (int g=0; g<n_parallel(); ++g) {
    std::string usi = to_usi((*_games)[g].record);
    _decision[g] = player->think(usi); // future
  }
  return false;
}

bool osl::CPUPlayer::recv_result(int,
                                 const std::vector<policy_logits_t>& logits,
                                 const std::vector<value_vector_t>& values) {
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
  auto run = [&](int l, int r) {
    for (int i=l; i<r; ++i) {
      games[i].export_heuristic_feature(ptr + i*ml::input_unit);
    }
  };
  run_range_parallel(N, run);
}

osl::GameArray::GameArray(int N, PlayerArray& a, PlayerArray& b,
                          InferenceModel& model_a,
                          InferenceModel& model_b,
                          std::optional<GameConfig> config)
  : mgrs(N, config),
    players{&a, &b},
    input_buf(), policy_buf(), value_buf(), skip_one_turn(N),
    max_width(std::max(players[0]->max_width(), players[1]->max_width())),
    random_opening(config.value_or(GameConfig()).random_opening) {
  model[0] = &model_a;
  model[1] = &model_b;
  
  players[0]->new_series(mgrs.games);
  if (players[0] != players[1])
    players[1]->new_series(mgrs.games);

  resize_buffer(std::max(1, max_width));
}

osl::GameArray::~GameArray() {
}

void osl::GameArray::warmup(int n) {
  resize_buffer(1);
  std::ranges::fill(input_buf, 0);
  for (int i=0; i<n; ++i) {
    model[0]->test_run(input_buf, policy_buf, value_buf);
    if (model[0] != model[1])
      model[1]->test_run(input_buf, policy_buf, value_buf);
  }
}

void osl::GameArray::resize_buffer(int width) {
  int sz = width * mgrs.n_parallel();
  input_buf.resize(sz * ml::input_unit); // fill 0 for newly added elements
  policy_buf.resize(sz);
  value_buf.resize(sz);
}

void osl::GameArray::step() {
  // (1) thinking
  int safety_limit = 16, cnt=0;
  bool ready = false;
  do {
    // std::cerr << mgrs.games[0].state;

    int req_size = players[side]->width(cnt);
    // zero-clear all input_buf before the next make_request
    // resize() will partially do so for the extended items
    // so we only need to clear dirty part
    auto limit = std::min(req_size*ml::input_unit*mgrs.n_parallel(),
                          (int)input_buf.size());
    std::fill(input_buf.begin(), input_buf.begin()+limit, 0);
    resize_buffer(req_size);

    auto need_policy = players[side]->make_request(cnt, &input_buf[0]);

    if (req_size > 0) {
      if (! need_policy)
        policy_buf.resize(0);
      model[side]->batch_infer(input_buf, policy_buf, value_buf);
    }
    
    ready = players[side]->recv_result(cnt, policy_buf, value_buf);
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
                                   std::vector<value_vector_t>& vout) {
  batch_infer(in, out, vout);
}

#ifdef ENABLE_RANGE_PARALLEL
const int osl::range_parallel_threads = std::min(std::max(1, (int)std::thread::hardware_concurrency()/2),
                                                 rng::available_instances);
#endif
