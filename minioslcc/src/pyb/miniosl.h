#ifndef MINIOSL_H
#define MINIOSL_H

#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace pyosl {
  void init_basic(py::module_ &);
  void init_state_np(py::module_ &);
  void init_game(py::module_ &);
}

#endif
