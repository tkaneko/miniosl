#include <pybind11/numpy.h>
namespace pyosl {
  template <typename T>
  struct nparray {
    py::array_t<T> array;
    py::buffer_info buffer;
    nparray(int size) : array(size), buffer(array.request()) {
    }
    T* ptr() { return static_cast<T*>(buffer.ptr); }
  };
}
