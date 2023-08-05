#ifndef MINIOSL_INFER_H
#define MINIOSL_INFER_H

// this header is carefully separated from others so as to be compatible with C++17

#include <vector>
#include <array>

namespace osl {
  namespace ml {
    const int basic_channels = 44, heuristic_channels = 13,
      board_channels = basic_channels + heuristic_channels;
    /** board_channels + channels_per_history*history_length */
    extern const int standard_channels;
    /** moves included before current position */
    constexpr int history_length = 7; // 1 for comatibility with r0.0.10 >=
    constexpr int channels_per_history = (history_length > 1) ? 4 : 3;
    const int input_channels = board_channels + history_length*channels_per_history;
  }

  typedef std::array<float,2187> policy_logits_t;
  typedef std::array<float,ml::input_channels*81> nn_input_t;
  class InferenceModel {
  public:
    virtual ~InferenceModel();
    /** warmup and may tell standard batch size to the engine */
    virtual void test_run(std::vector<nn_input_t>& in,
                          std::vector<policy_logits_t>& policy_out,
                          std::vector<std::array<float,1>>& vout);
    virtual void batch_infer(std::vector<nn_input_t>& in,
                             std::vector<policy_logits_t>& policy_out,
                             std::vector<std::array<float,1>>& vout) = 0;
  };
}
#endif
