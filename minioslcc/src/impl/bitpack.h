#ifndef MINIOSL_BITPACK_H
#define MINIOSL_BITPACK_H

#include "state.h"
#include "record.h"
#include "infer.h"

namespace osl
{
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
     * - uint32_t flip: 1;
     */
    typedef std::array<uint64_t,4> B256;
    /** compress move to 12bit (depending on a current state) */
    uint32_t encode12(const BaseState& state, Move move);
    Move decode_move12(const BaseState& state, uint32_t code);
    constexpr uint32_t move12_resign = 0, move12_win_declare = 127, move12_pass = 126;

    typedef std::array<uint64_t,5> B320;

    /** to save a set of (pure) game records in npz.
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
  using bitpack::B320;
}

#endif
// MINIOSL_BITPACK_H
