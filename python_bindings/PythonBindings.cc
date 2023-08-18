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

void defineCaramelModule(py::module_ &module) {
  py::class_<Csf, CsfPtr>(module, "CSF")
      .def(py::init([](const std::vector<std::string> &keys,
                       const std::vector<uint32_t> &values) {
        return constructCsf(keys, values);
      }))
      .def("query", &Csf::query, py::arg("key"));
}

PYBIND11_MODULE(caramel, module) { // NOLINT

  defineCaramelModule(module);
}

} // namespace caramel::python