#include <memory.h>
#include <src/construct/Construct.h>
#include <src/construct/Csf.h>
#include <src/construct/entropyPermutation.h>

// Pybind11 library
#include <pybind11/cast.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace caramel::python {

template <typename T> void bindCsf(py::module &m, const char *name, const uint32_t type_id) {
  py::class_<Csf<T>, std::shared_ptr<Csf<T>>>(m, name)
      .def(py::init([](const std::vector<std::string> &keys,
                       const std::vector<T> &values, bool verbose) {
             return constructCsf<T>(keys, values, verbose);
           }),
           py::arg("keys"), py::arg("values"), py::arg("verbose") = true)
      .def("query", &Csf<T>::query, py::arg("key"))
      // Call save / load through a lambda to avoid user visibility of type_id.
      .def("save", [type_id](Csf<T> &self, const std::string &filename) {
          return self.save(filename, type_id);
       }, py::arg("filename"))
      .def_static("load", [type_id](const std::string &filename) {
          return Csf<T>::load(filename, type_id);
        }, py::arg("filename"));
}

template <typename T> void bindPermutation(py::module &m, const char *name){
  m.def(name, [](py::array_t<T, py::array::c_style | py::array::forcecast> &array) {
    // Check array dimensions and load the buffer into a pointer.
    if (array.ndim() != 2) {
      throw std::runtime_error("Input should be a 2D numpy array.");
    }
    py::buffer_info info = array.request();
    int num_rows = info.shape[0];
    int num_cols = info.shape[1];
    T* M = static_cast<T*>(info.ptr);  // must be row-major format.
    entropyPermutation<T>(M, num_rows, num_cols);
  });
}

PYBIND11_MODULE(_caramel, module) { // NOLINT
  bindCsf<uint32_t>(module, "CSFUint32", 1);
  bindCsf<std::array<char, 10>>(module, "CSFChar10", 2);
  bindCsf<std::array<char, 12>>(module, "CSFChar12", 3);
  bindCsf<std::string>(module, "CSFString", 4);
  bindPermutation<int32_t>(module, "permute_int32");
  py::register_exception<CsfDeserializationException>(module, "CsfDeserializationException");
}

} // namespace caramel::python