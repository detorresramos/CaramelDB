#include <memory.h>
#include <src/Dummy.h>

// Pybind11 library
#include <pybind11/cast.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace caramel::python {

void defineCaramelModule(py::module_ &module) {
  py::class_<Dummy, std::shared_ptr<Dummy>>(module, "Dummy")
      .def(py::init<>())
      .def("hello", &Dummy::helloWorld);
}

PYBIND11_MODULE(_caramel, module) { // NOLINT

  defineCaramelModule(module);
}

} // namespace caramel::python