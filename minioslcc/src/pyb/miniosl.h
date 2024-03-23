#ifndef MINIOSL_H
#define MINIOSL_H

// #define PYBIND11_DETAILED_ERROR_MESSAGES 1

#include "game.h"
#include "feature.h"
#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace pyosl {
  void init_basic(py::module_ &);
  void init_state_np(py::module_ &);
  void init_game(py::module_ &);
}

PYBIND11_MAKE_OPAQUE(std::vector<osl::Move>);
PYBIND11_MAKE_OPAQUE(std::vector<osl::HashStatus>);
PYBIND11_MAKE_OPAQUE(std::vector<osl::MiniRecord>);
PYBIND11_MAKE_OPAQUE(std::vector<osl::SubRecord>);
PYBIND11_MAKE_OPAQUE(std::vector<std::vector<osl::SubRecord>>);
PYBIND11_MAKE_OPAQUE(std::vector<osl::GameManager>);

#endif
