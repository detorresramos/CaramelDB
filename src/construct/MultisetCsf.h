#pragma once

#include "Csf.h"

namespace caramel {

template <typename T> class MultisetCsf;
template <typename T> using MultisetCsfPtr = std::shared_ptr<MultisetCsf<T>>;

template <typename T> class MultisetCsf {
public:
  MultisetCsf(const std::vector<CsfPtr<T>> &csfs) : _csfs(csfs) {}

  std::vector<T> query(const std::string &key) const {
    std::vector<T> outputs(_csfs.size());

#pragma omp parallel for default(none) shared(key, _csfs, outputs)
    for (size_t i = 0; i < _csfs.size(); i++) {
      outputs[i] = _csfs[i]->query(key);
    }

    return outputs;
  }

  void save(const std::string &filename, const uint32_t type_id = 0) const {
    auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
    output_stream.write(reinterpret_cast<const char *>(&type_id),
                        sizeof(uint32_t));
    cereal::BinaryOutputArchive oarchive(output_stream);
    oarchive(*this);
  }

  static MultisetCsfPtr<T> load(const std::string &filename,
                                const uint32_t type_id = 0) {
    auto input_stream = SafeFileIO::ifstream(filename, std::ios::binary);
    // Check the type_id before deserializing a (potentially large) CSF.
    uint32_t type_id_found = 0;
    input_stream.read(reinterpret_cast<char *>(&type_id_found),
                      sizeof(uint32_t));
    if (type_id != type_id_found) {
      throw CsfDeserializationException(
          "Expected type_id to be " + std::to_string(type_id) +
          " but found type_id = " + std::to_string(type_id_found) +
          " when deserializing " + filename);
    }
    cereal::BinaryInputArchive iarchive(input_stream);
    MultisetCsfPtr<T> deserialize_into(new MultisetCsf<T>());
    iarchive(*deserialize_into);

    return deserialize_into;
  }

private:
  // Private constructor for cereal
  MultisetCsf() {}

  friend class cereal::access;
  template <class Archive> void serialize(Archive &archive) { archive(_csfs); }

  std::vector<CsfPtr<T>> _csfs;
};

} // namespace caramel