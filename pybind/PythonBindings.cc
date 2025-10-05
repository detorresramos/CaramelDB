#include <memory.h>
#include <src/construct/Construct.h>
#include <src/construct/ConstructMultiset.h>
#include <src/construct/Csf.h>
#include <src/construct/EntropyPermutation.h>
#include <src/construct/MultisetCsf.h>
#include <src/construct/filter/BloomPreFilter.h>
#include <src/construct/filter/XORPreFilter.h>
#include <src/construct/filter/BinaryFusePreFilter.h>
#include <src/construct/filter/FilterConfig.h>
#include <src/construct/filter/PreFilter.h>

// Pybind11 library
#include <pybind11/cast.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace caramel::python {

void bindBloomFilter(py::module &module) {
  py::class_<BloomFilter, BloomFilterPtr>(module, "BloomFilter")
      .def_static("autotuned", &BloomFilter::makeAutotuned,
                  py::arg("num_elements"), py::arg("error_rate"),
                  py::arg("verbose") = false)
      .def_static("fixed", &BloomFilter::makeFixed, py::arg("bitarray_size"),
                  py::arg("num_hashes"))
      .def("add", &BloomFilter::add, py::arg("key"))
      .def("size", &BloomFilter::size)
      .def("num_hashes", &BloomFilter::numHashes)
      .def("contains", &BloomFilter::contains, py::arg("key"));
}

template <typename T> void bindPreFilter(py::module &module, const char *name) {
  py::class_<PreFilter<T>, PreFilterPtr<T>>(module, name);
}

template <typename T>
void bindBloomPreFilter(py::module &module, const char *bloom_name) {
  py::class_<BloomPreFilter<T>, PreFilter<T>, BloomPreFilterPtr<T>>(module,
                                                                    bloom_name)
      .def("save", &BloomPreFilter<T>::save, py::arg("filename"))
      .def("get_bloom_filter", &BloomPreFilter<T>::getBloomFilter,
           py::return_value_policy::reference)
      .def("get_most_common_value", &BloomPreFilter<T>::getMostCommonValue);
}

template <typename T>
void bindXORPreFilter(py::module &module, const char *xor_name) {
  py::class_<XORPreFilter<T>, PreFilter<T>, XORPreFilterPtr<T>>(module,
                                                                xor_name)
      .def("save", &XORPreFilter<T>::save, py::arg("filename"))
      .def("get_xor_filter", &XORPreFilter<T>::getXorFilter,
           py::return_value_policy::reference)
      .def("get_most_common_value", &XORPreFilter<T>::getMostCommonValue);
}

template <typename T>
void bindBinaryFusePreFilter(py::module &module, const char *bf_name) {
  py::class_<BinaryFusePreFilter<T>, PreFilter<T>, BinaryFusePreFilterPtr<T>>(module,
                                                                              bf_name)
      .def("save", &BinaryFusePreFilter<T>::save, py::arg("filename"))
      .def("get_binary_fuse_filter", &BinaryFusePreFilter<T>::getBinaryFuseFilter,
           py::return_value_policy::reference)
      .def("get_most_common_value", &BinaryFusePreFilter<T>::getMostCommonValue);
}

void bindPreFilterConfig(py::module &module) {
  py::class_<PreFilterConfig, std::shared_ptr<PreFilterConfig>>( // NOLINT
      module, "PreFilterConfig");

  py::class_<BloomPreFilterConfig, PreFilterConfig,
             std::shared_ptr<BloomPreFilterConfig>>(module, "BloomFilterConfig")
      .def(py::init<std::optional<float>, std::optional<size_t>>(),
           py::arg("error_rate") = std::nullopt, py::arg("k") = std::nullopt)
      .def_readwrite("error_rate", &BloomPreFilterConfig::error_rate);

  py::class_<XORPreFilterConfig, PreFilterConfig,
             std::shared_ptr<XORPreFilterConfig>>(module, "XORFilterConfig")
      .def(py::init<>());

  py::class_<BinaryFusePreFilterConfig, PreFilterConfig,
             std::shared_ptr<BinaryFusePreFilterConfig>>(module, "BinaryFuseFilterConfig")
      .def(py::init<>());
}

template <typename T>
void bindCsf(py::module &module, const char *name, const uint32_t type_id) {
  py::class_<Csf<T>, std::shared_ptr<Csf<T>>>(module, name)
      .def(py::init([](const std::vector<std::string> &keys,
                       const std::vector<T> &values,
                       std::shared_ptr<PreFilterConfig> filter_config,
                       bool verbose) {
             return constructCsf<T>(keys, values, filter_config, verbose);
           }),
           py::arg("keys"), py::arg("values"), py::arg("prefilter") = nullptr,
           py::arg("verbose") = true)
      .def("query", &Csf<T>::query, py::arg("key"))
      .def("get_filter", &Csf<T>::getFilter, py::return_value_policy::reference)
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
                       std::shared_ptr<PreFilterConfig> filter_config,
                       bool verbose) {
             return constructMultisetCsf<T>(keys, values, filter_config,
                                            verbose);
           }),
           py::arg("keys"), py::arg("values"), py::arg("prefilter") = nullptr,
           py::arg("verbose") = true)
      .def("query", &MultisetCsf<T>::query, py::arg("key"),
           py::arg("parallel") = true)
      // Call save / load through a lambda to avoid user visibility of type_id.
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
  bindBloomFilter(module);
  bindPreFilterConfig(module);

  bindPreFilter<uint32_t>(module, "PreFilterUint32");
  bindPreFilter<uint64_t>(module, "PreFilterUint64");
  bindPreFilter<std::array<char, 10>>(module, "PreFilterChar10");
  bindPreFilter<std::array<char, 12>>(module, "PreFilterChar12");
  bindPreFilter<std::string>(module, "PreFilterString");

  bindBloomPreFilter<uint32_t>(module, "BloomPreFilterUint32");
  bindBloomPreFilter<uint64_t>(module, "BloomPreFilterUint64");
  bindBloomPreFilter<std::array<char, 10>>(module, "BloomPreFilterChar10");
  bindBloomPreFilter<std::array<char, 12>>(module, "BloomPreFilterChar12");
  bindBloomPreFilter<std::string>(module, "BloomPreFilterString");

  bindXORPreFilter<uint32_t>(module, "XORPreFilterUint32");
  bindXORPreFilter<uint64_t>(module, "XORPreFilterUint64");
  bindXORPreFilter<std::array<char, 10>>(module, "XORPreFilterChar10");
  bindXORPreFilter<std::array<char, 12>>(module, "XORPreFilterChar12");
  bindXORPreFilter<std::string>(module, "XORPreFilterString");

  bindBinaryFusePreFilter<uint32_t>(module, "BinaryFusePreFilterUint32");
  bindBinaryFusePreFilter<uint64_t>(module, "BinaryFusePreFilterUint64");
  bindBinaryFusePreFilter<std::array<char, 10>>(module, "BinaryFusePreFilterChar10");
  bindBinaryFusePreFilter<std::array<char, 12>>(module, "BinaryFusePreFilterChar12");
  bindBinaryFusePreFilter<std::string>(module, "BinaryFusePreFilterString");

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