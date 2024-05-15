#include <memory.h>
#include <src/construct/Construct.h>
#include <src/construct/ConstructMultiset.h>
#include <src/construct/Csf.h>
#include <src/construct/EntropyPermutation.h>
#include <src/construct/MultisetCsf.h>

// Pybind11 library
#include <pybind11/cast.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace caramel::python {

template <typename T>
void bindCsf(py::module &module, const char *name, const uint32_t type_id) {
  py::class_<Csf<T>, std::shared_ptr<Csf<T>>>(module, name)
      .def(py::init([](const std::vector<std::string> &keys,
                       const std::vector<T> &values, bool use_bloom_filter,
                       bool verbose) {
             return constructCsf<T>(keys, values, use_bloom_filter, verbose);
           }),
           py::arg("keys"), py::arg("values"),
           py::arg("use_bloom_filter") = true, py::arg("verbose") = true)
      .def("query", &Csf<T>::query, py::arg("key"))
      // Call save / load through a lambda to avoid user visibility of type_id.
      .def(
          "save",
          [type_id](Csf<T> &self, const std::string &filename) {
            return self.save(filename, type_id);
          },
          py::arg("filename"))
      .def_static(
          "load",
          [type_id](const std::string &filename) {
            return Csf<T>::load(filename, type_id);
          },
          py::arg("filename"))
      .def_static("is_multiset", []() { return false; });
}

template <typename T>
void bindMultisetCsf(py::module &module, const char *name,
                     const uint32_t type_id) {
  py::class_<MultisetCsf<T>, std::shared_ptr<MultisetCsf<T>>>(module, name)
      .def(py::init([](const std::vector<std::string> &keys,
                       const std::vector<std::vector<T>> &values,
                       bool use_bloom_filter, bool verbose) {
             return constructMultisetCsf<T>(keys, values, use_bloom_filter,
                                            verbose);
           }),
           py::arg("keys"), py::arg("values"),
           py::arg("use_bloom_filter") = true, py::arg("verbose") = true)
      .def("query", &MultisetCsf<T>::query, py::arg("key"),
           py::arg("parallel") = true)
      // Call save / load through a lambda to avoid user visibility of
      // type_id.
      .def(
          "save",
          [type_id](MultisetCsf<T> &self, const std::string &filename) {
            return self.save(filename, type_id);
          },
          py::arg("filename"))
      .def_static(
          "load",
          [type_id](const std::string &filename) {
            return MultisetCsf<T>::load(filename, type_id);
          },
          py::arg("filename"))
      .def_static("is_multiset", []() { return true; });
}

template <typename T> void bindPermutation(py::module &m, const char *name) {
  m.def(name,
        [](py::array_t<T, py::array::c_style | py::array::forcecast> &array) {
          // Check array dimensions and load the buffer into a pointer.
          if (array.ndim() != 2) {
            throw std::runtime_error("Input should be a 2D numpy array.");
          }
          py::buffer_info info = array.request();
          int num_rows = info.shape[0];
          int num_cols = info.shape[1];
          T *M = static_cast<T *>(info.ptr); // must be row-major format.
          entropyPermutation<T>(M, num_rows, num_cols);
        });
}

PYBIND11_MODULE(_caramel, module) { // NOLINT
  bindCsf<uint32_t>(module, "CSFUint32", 1);
  bindCsf<uint64_t>(module, "CSFUint64", 2);
  bindCsf<std::array<char, 10>>(module, "CSFChar10", 3);
  bindCsf<std::array<char, 12>>(module, "CSFChar12", 4);
  bindCsf<std::string>(module, "CSFString", 5);

  bindMultisetCsf<uint32_t>(module, "MultisetCSFUint32", 101);
  bindMultisetCsf<uint64_t>(module, "MultisetCSFUint64", 102);
  bindMultisetCsf<std::array<char, 10>>(module, "MultisetCSFChar10", 103);
  bindMultisetCsf<std::array<char, 12>>(module, "MultisetCSFChar12", 104);
  bindMultisetCsf<std::string>(module, "MultisetCSFString", 105);

  bindPermutation<uint32_t>(module, "permute_uint32");
  bindPermutation<uint64_t>(module, "permute_uint64");
  bindPermutation<std::array<char, 10>>(module, "permute_char10");
  bindPermutation<std::array<char, 12>>(module, "permute_char12");
  // permutation is not supported for std::string types

  py::register_exception<CsfDeserializationException>(
      module, "CsfDeserializationException");
}

} // namespace caramel::python