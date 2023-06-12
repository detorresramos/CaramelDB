#include <memory.h>
#include <src/CSF.h>

// Pybind11 library
#include <pybind11/cast.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace caramel::python {

void defineCaramelModule(py::module_ &module) {
  py::class_<CSF, std::shared_ptr<CSF>>(module, "CSF")
      .def(py::init<>())
      .def("hello", &CSF::helloWorld);
}

PYBIND11_MODULE(caramel, module) { // NOLINT

  defineCaramelModule(module);
}

} // namespace caramel::python