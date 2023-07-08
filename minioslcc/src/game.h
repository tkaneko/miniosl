#ifndef MINIOSL_GAME_H
#define MINIOSL_GAME_H

#include "record.h"
namespace osl {
  struct GameManager {
    MiniRecord record;
    HistoryTable table;
    EffectState state;
    MoveVector legal_moves;

    GameManager();
    ~GameManager();
    
    GameResult add_move(Move move);
    void export_heuristic_feature(float*) const;
    void export_heuristic_feature_after(Move, float*) const;
    /** this interface will subject to change along with optimization/enhancements */
    static void export_heuristic_feature(const EffectState& cur, Move last_move, float *ptr);
  };

  struct ParallelGameManager {
    ParallelGameManager(int N, bool force_declare);
    ~ParallelGameManager();

    int n_parallel() const { return games.size(); }
    
    std::vector<GameResult> add_move_parallel(const std::vector<Move>& move);
    void export_heuristic_feature_parallel(float*) const;
    void export_heuristic_feature_for_children_parallel(const std::vector<Move>& moves, int n_branch, float*) const;
    
    std::vector<GameManager> games;
    std::vector<MiniRecord> completed_games;
    bool force_declare;
  };

  std::vector<std::pair<double,Move>>
  add_gumbel_noise(const osl::MoveVector& moves, const std::array<float,2187>& logits, int top_n);
}


#endif
// MINIOSL_GAME_H
