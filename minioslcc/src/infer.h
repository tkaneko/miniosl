#ifndef MINIOSL_INFER_H
#define MINIOSL_INFER_H

// this header is carefully separated from others so as to be compatible with C++17

#include <vector>
#include <array>

namespace osl {
  typedef std::array<float,2187> policy_logits_t;
  typedef std::array<float,60*81> nn_input_t;
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
