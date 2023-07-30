#include "pyb/miniosl.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include "game.h"
#include "infer.h"
#include "feature.h"
#include <iostream>

namespace pyosl {
  using namespace osl;
  py::array_t<float> export_heuristic_feature(const GameManager& mgr);
  py::array_t<float> export_heuristic_feature_parallel(const ParallelGameManager& mgr);

  // allow implementation of virtual methods in python
  // https://pybind11.readthedocs.io/en/stable/advanced/classes.html
  typedef py::array_t<float, py::array::c_style | py::array::forcecast> np_float_t;
  class InferenceModelStub : public InferenceModel {
  public:
    using InferenceModel::InferenceModel;
    void test_run(std::vector<nn_input_t>& in,
                  std::vector<policy_logits_t>& policy_out,
                  std::vector<std::array<float,1>>& vout) override {
      batch_infer(in, policy_out, vout);
    }
    
    void batch_infer(std::vector<nn_input_t>& in,
                     std::vector<policy_logits_t>& policy_out,
                     std::vector<std::array<float,1>>& vout) override {
      py::array_t<float> feature(in.size()*in[0].size());
      auto buffer = feature.request();
      auto ptr = static_cast<float_t*>(buffer.ptr);
      std::copy(&in[0][0], &in[0][0]+in.size()*in[0].size(), ptr);
      
      auto [policy, value, aux] = py_infer(feature);
      auto pb = policy.request();
      auto pptr = static_cast<float*>(pb.ptr);
      for (size_t i=0; i<in.size(); ++i)
        for (size_t j=0; j<policy_out[0].size(); ++j)
          policy_out[i][j] = pptr[i*policy_out[0].size() + j];
      
      auto vb = value.request();
      auto vptr = static_cast<float*>(vb.ptr);
      for (size_t i=0; i<in.size(); ++i)
        for (size_t j=0; j<vout[0].size(); ++j)
          vout[i][j] = vptr[i*vout[0].size() + j];
    }
    virtual std::tuple<np_float_t, np_float_t, np_float_t> py_infer(py::array_t<float>) =0;
  };

  class PyInferenceModelStub : public InferenceModelStub {
  public:
    using InferenceModelStub::InferenceModelStub;
    typedef std::tuple<np_float_t, np_float_t, np_float_t> tuple_t;
    tuple_t py_infer(py::array_t<float> inputs) override {
      PYBIND11_OVERRIDE_PURE
        (
         tuple_t, /* Return type */
         InferenceModelStub,      /* Parent class */
         py_infer,          /* Name of function in C++ (must match Python name) */
         inputs      /* Argument(s) */
        );
    }
  };

  class PySingleCPUPlaer : public SingleCPUPlayer {
  public:
    using SingleCPUPlayer::SingleCPUPlayer;
    Move think(std::string usi_line) override {
      PYBIND11_OVERRIDE_PURE
        (
         Move, /* Return type */
         SingleCPUPlayer,      /* Parent class */
         think,          /* Name of function in C++ (must match Python name) */
         usi_line      /* Argument(s) */
         );
    }
    std::string name() override {
      PYBIND11_OVERRIDE_PURE
        (
         std::string, /* Return type */
         SingleCPUPlayer,      /* Parent class */
         name          /* Name of function in C++ (must match Python name) */
               /* Argument(s) */
         );
    }
  };
  
}

void pyosl::init_game(py::module_& m) {
  using namespace pybind11::literals;
  
  py::class_<osl::GameManager>(m, "GameManager", py::dynamic_attr())
    .def(py::init<>())
    .def_readonly("record", &osl::GameManager::record)
    .def_readonly("state", &osl::GameManager::state)
    .def_readonly("legal_moves", &osl::GameManager::legal_moves)
    .def("make_move", &osl::GameManager::make_move)
    .def("export_heuristic_feature", &pyosl::export_heuristic_feature)
    .def_static("from_record", &osl::GameManager::from_record)
    .def("__copy__",  [](const osl::GameManager& g) { return osl::GameManager(g);})
    .def("__deepcopy__",  [](const osl::GameManager& g) { return osl::GameManager(g);})
    ;
  py::class_<osl::ParallelGameManager>(m, "ParallelGameManager", py::dynamic_attr())
    .def(py::init<int,bool,bool>(), "N"_a, "force_declare"_a, "ignore_draw"_a=false)
    .def_readonly("games", &osl::ParallelGameManager::games)
    .def_readonly("completed_games", &osl::ParallelGameManager::completed_games)
    .def("make_move_parallel", &osl::ParallelGameManager::make_move_parallel)
    .def("export_heuristic_feature_parallel", &pyosl::export_heuristic_feature_parallel)
    .def("n_parallel", &osl::ParallelGameManager::n_parallel)
    ;

  py::class_<osl::PlayerArray>(m, "PlayerArray", py::dynamic_attr())
    .def_readonly("greedy", &osl::PlayerArray::greedy)
    .def("n_parallel", &osl::PlayerArray::n_parallel)
    .def("new_series", &osl::PlayerArray::new_series)
    .def("decision", &osl::PlayerArray::decision)
    ;

  py::class_<osl::PolicyPlayer, osl::PlayerArray>(m, "PolicyPlayer", py::dynamic_attr())
    .def(py::init<bool>(), "greedy"_a=false)
    .def("name", &osl::PolicyPlayer::name)
    ;

  py::class_<osl::FlatGumbelPlayer, osl::PlayerArray>(m, "FlatGumbelPlayer", py::dynamic_attr())
    .def(py::init<int,double>(), "width"_a, "noise_scale"_a=1.0)
    .def("name", &osl::FlatGumbelPlayer::name)
    ;

  py::class_<osl::CPUPlayer, osl::PlayerArray>(m, "CPUPlayer", py::dynamic_attr(), "adaptor for PlayerArray\n\n"
                                               ":param player: object descendant of `SingleCPUPlayer`\n"
                                               ":param greedy: indicating greedy behavior\n\n"
                                               ".. note:: if you give a `player` implemented in Python, please make sure its lifetime")
    .def(py::init<std::shared_ptr<osl::SingleCPUPlayer>, bool>(), "player"_a, "greedy"_a)
    .def("name", &osl::CPUPlayer::name)
    ;

  py::class_<osl::SingleCPUPlayer, pyosl::PySingleCPUPlaer, std::shared_ptr<osl::SingleCPUPlayer>>(m, "SingleCPUPlayer", py::dynamic_attr())
    .def(py::init<>())
    .def("think", &osl::SingleCPUPlayer::think)
    .def("name", &osl::SingleCPUPlayer::name)
    ;

  py::class_<osl::RandomPlayer, osl::SingleCPUPlayer, std::shared_ptr<osl::RandomPlayer>>(m, "RandomPlayer", py::dynamic_attr())
    .def(py::init<>())
    .def("name", &osl::RandomPlayer::name)
    .def("think", &osl::RandomPlayer::think)
    ;

  py::class_<osl::GameArray>(m, "GameArray", py::dynamic_attr())
    .def(py::init<int, PlayerArray&, PlayerArray&, InferenceModel&, bool>(),
         "N"_a, "player_a"_a, "player_b"_a, "model"_a, "ignore_draw"_a=false)
    .def("step", &osl::GameArray::step)
    .def("completed", &osl::GameArray::completed)
    .def("warmup", &osl::GameArray::warmup, "n"_a=4)
    ;
  
  py::class_<osl::InferenceModel>(m, "InferenceModel")
    ;
  
  py::class_<pyosl::InferenceModelStub, pyosl::PyInferenceModelStub, osl::InferenceModel>(m, "InferenceModelStub")
    .def(py::init<>())
    .def("py_infer", &pyosl::InferenceModelStub::py_infer)
    ;

  // function
  m.def("transformQ", &osl::FlatGumbelPlayer::transformQ, "q_by_nn"_a);
}

py::array_t<float> pyosl::export_heuristic_feature(const osl::GameManager& mgr) {
  auto feature = py::array_t<float>(9*9*ml::channel_id.size());
  auto buffer = feature.request();
  auto ptr = static_cast<float_t*>(buffer.ptr);
  mgr.export_heuristic_feature(ptr);
  return feature.reshape({-1, 9, 9});
}

py::array_t<float> pyosl::export_heuristic_feature_parallel(const osl::ParallelGameManager& mgrs) {
  auto feature = py::array_t<float>(9*9*ml::channel_id.size()*mgrs.n_parallel());
  auto buffer = feature.request();
  auto ptr = static_cast<float_t*>(buffer.ptr);
  GameArray::export_root_features(mgrs.games, ptr);
  return feature.reshape({-1, (int)ml::channel_id.size(), 9, 9});
}

