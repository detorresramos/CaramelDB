#include <memory.h>
#include <src/construct/Construct.h>
#include <src/construct/Csf.h>

// Pybind11 library
#include <pybind11/cast.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace caramel::python {

template <typename T> void bindCsf(py::module &module, const char *name, const uint32_t type_id) {
  py::class_<Csf<T>, std::shared_ptr<Csf<T>>>(module, name)
      .def(py::init([](const std::vector<std::string> &keys,
                       const std::vector<T> &values, bool verbose) {
             return constructCsf<T>(keys, values, verbose);
           }),
           py::arg("keys"), py::arg("values"), py::arg("verbose") = true)
      .def("query", &Csf<T>::query, py::arg("key"))
      .def("save", &Csf<T>::save, py::arg("filename"), py::arg("type_id") = type_id)
      .def_static("load", &Csf<T>::load, py::arg("filename"), py::arg("type_id") = type_id);
}

PYBIND11_MODULE(caramel, module) { // NOLINT
  bindCsf<uint32_t>(module, "CSFUint32", 1);
  bindCsf<std::array<char, 10>>(module, "CSFChar10", 2);
  bindCsf<std::array<char, 12>>(module, "CSFChar12", 3);
  bindCsf<std::string>(module, "CSFString", 4);
  py::register_exception<CsfDeserializationException>(module, "CsfDeserializationException");
}

} // namespace caramel::python