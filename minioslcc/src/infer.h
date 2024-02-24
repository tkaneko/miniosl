#ifndef MINIOSL_INFER_H
#define MINIOSL_INFER_H

// this header is carefully separated from others so as to be compatible with C++17

#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>

namespace osl {
  namespace ml {
    /** minimum coefficient to obtain integer value.
     * the current value is for hand features as lcm(18,4,2)
     */
    constexpr int quantize_scale = 36;
    const int basic_channels = 44, heuristic_channels = 20,
      board_channels = basic_channels + heuristic_channels;
    /** board_channels + channels_per_history*history_length */
    extern const int standard_channels;
    /** moves included before current position */
    constexpr int history_length = 7; // for AZ;
    constexpr int channels_per_history = 16;
    constexpr int input_channels = board_channels + history_length*channels_per_history;
    constexpr int aux_channels = 22;
    constexpr int input_unit = input_channels*81, policy_unit = 2187, aux_unit = 9*9*aux_channels;

    typedef std::array<float,ml::policy_unit> policy_logits_t;
    /** 
     * note this objects will be poorly aligned unless the number of input_channels is a good multiples.
     */
    typedef std::array<float,ml::input_unit> nn_input_t;
    constexpr int One = ml::quantize_scale;
    typedef int8_t nn_input_element;
    inline float to_float(nn_input_element v) { return 1.0*v/ml::One; }
    template <class Container>
    inline void transform(const Container& container, float *ptr) {
      std::transform(container.begin(), container.end(), ptr, to_float); // ranges needs C++20
    }
    struct transform_when_leave {
      std::vector<nn_input_element> work;
      float *dst;
      transform_when_leave(float *ptr, int size) : work(size, 0), dst(ptr) {};
      ~transform_when_leave() { transform(work, dst); }
      nn_input_element *proxy() { return &work[0]; }
    };
    template <class Function>
    auto write_float_feature(Function f, int sz, float *ptr) {
      transform_when_leave work(ptr, sz);
      return f(work.proxy());
    }
  }
  using ml::nn_input_t;
  using ml::policy_logits_t;
  using ml::nn_input_element;

  class InferenceModel {
  public:
    virtual ~InferenceModel();
    /** warmup and may tell standard batch size to the engine */
    virtual void test_run(std::vector<nn_input_element>& /* size = batch_size * input_unit */ in,
                          std::vector<policy_logits_t>& /* size = batch_size */ policy_out,
                          std::vector<std::array<float,1>>& /* size = batch_size */ vout);
    /** primary inference function 
     * @param policy_out can be zero if only vout is needed
     */
    virtual void batch_infer(std::vector<nn_input_element>& in,
                             std::vector<policy_logits_t>& policy_out,
                             std::vector<std::array<float,1>>& vout) = 0;
  };
}
#endif
