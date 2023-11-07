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
      // Call save / load through a lambda to avoid user visibility of type_id.
      .def("save", [type_id](Csf<T> &self, const std::string &filename) {
          return self.save(filename, type_id);
       }, py::arg("filename"))
      .def_static("load", [type_id](const std::string &filename) {
          return Csf<T>::load(filename, type_id);
        }, py::arg("filename"));
}

// We have to wrap a Csf<std::string> because Pybind11 requires one holder type
// per class (so we cannot bind it directly to a shared_ptr<Csf<std::string>>).
class CsfBytes: public Csf<std::string>{ public:
  CsfBytes(Csf<std::string> const & obj) : Csf<std::string>(obj) {}
};

void bindBytesCsf(py::module &module, const char *name, const uint32_t type_id) {
  // Byte keys are fine, since Pybind11 casts from Python bytes to C++ strings.
  // Byte values are not fine, because C++ strings auto-cast to uint-8 Python
  // strings, meaning we must bind the byte values carefully to return bytes.
  py::class_<CsfBytes, std::shared_ptr<CsfBytes>>(module, name)
      .def(py::init([](const std::vector<std::string> &keys,
                       const std::vector<std::string> &values, bool verbose) {
             // This is tricky. If constructCsf can return a unique_ptr, we can
             // probably get this to use an efficient move constructor into the
             // CsfBytes. But as-is, with a shared_ptr, I'm not sure how to
             // cannibalize the Csf<std::string> and avoid a deep mem copy.
             return CsfBytes(*constructCsf<std::string>(keys, values, verbose));
           }),
           py::arg("keys"), py::arg("values"), py::arg("verbose") = true)
      // Query returns std::string, which we must explicitly convert to py::bytes.
      .def("query", [](CsfBytes &self, const std::string& key) {
          std::string s = self.query(key);
          return py::bytes(s);
        }, py::arg("key"))
      .def("save", [type_id](CsfBytes &self, const std::string &filename) {
          return self.save(filename, type_id);
        }, py::arg("filename"))
      .def_static("load", [type_id](const std::string &filename) {
          return CsfBytes(*CsfBytes::load(filename, type_id));
        }, py::arg("filename"));
}

PYBIND11_MODULE(_caramel, module) { // NOLINT
  bindCsf<uint32_t>(module, "CSFUint32", 1);
  bindCsf<std::array<char, 10>>(module, "CSFChar10", 2);
  bindCsf<std::array<char, 12>>(module, "CSFChar12", 3);
  bindCsf<std::string>(module, "CSFString", 4);
  bindBytesCsf(module, "CSFBytes", 5);
  py::register_exception<CsfDeserializationException>(module, "CsfDeserializationException");
}

} // namespace caramel::python