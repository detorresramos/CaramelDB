#pragma once

#include "BinaryFuseFilter.h"
#include "FilterTypes.h"
#include "PreFilter.h"
#include <optional>
#include <src/utils/SafeFileIO.h>
#include <string>
#include <vector>

namespace caramel {

template <typename T> class BinaryFusePreFilter;
template <typename T>
using BinaryFusePreFilterPtr = std::shared_ptr<BinaryFusePreFilter<T>>;

template <typename T> class BinaryFusePreFilter final : public PreFilter<T> {
public:
  explicit BinaryFusePreFilter<T>(int fingerprint_bits)
      : _binary_fuse_filter(nullptr), _most_common_value(std::nullopt),
        _fingerprint_bits(fingerprint_bits) {}

  static BinaryFusePreFilterPtr<T> make(int fingerprint_bits) {
    return std::make_shared<BinaryFusePreFilter<T>>(fingerprint_bits);
  }

  bool contains(const std::string &key) override {
    if (_binary_fuse_filter) {
      return _binary_fuse_filter->contains(key);
    }
    return true;
  }

  bool contains(const char *data, size_t length) override {
    if (_binary_fuse_filter) {
      return _binary_fuse_filter->contains(data, length);
    }
    return true;
  }

  BinaryFuseFilterPtr getBinaryFuseFilter() const {
    return _binary_fuse_filter;
  }

  std::optional<T> getMostCommonValue() const override {
    return _most_common_value;
  }

  std::optional<FilterStats> getStats() const override {
    FilterStats fs;
    fs.type = "binary_fuse";
    if (_binary_fuse_filter) {
      fs.size_bytes = _binary_fuse_filter->size();
      fs.num_elements = _binary_fuse_filter->numElements();
      fs.fingerprint_bits = _binary_fuse_filter->fingerprintWidth();
    } else {
      fs.size_bytes = 0;
      fs.num_elements = 0;
    }
    return fs;
  }

  void save(const std::string &filename) const {
    auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(output_stream);
    oarchive(*this);
  }

  static BinaryFusePreFilterPtr<T> load(const std::string &filename) {
    auto filter = std::make_shared<BinaryFusePreFilter<T>>(8); // Placeholder
    auto input_stream = SafeFileIO::ifstream(filename, std::ios::binary);
    cereal::BinaryInputArchive iarchive(input_stream);
    iarchive(*filter);
    return filter;
  }

protected:
  void createAndPopulateFilter(size_t filter_size,
                               const std::vector<std::string> &keys,
                               const std::vector<T> &values,
                               T most_common_value, bool verbose) override {
    _binary_fuse_filter =
        BinaryFuseFilter::makeFixed(filter_size, _fingerprint_bits, verbose);
    _most_common_value = most_common_value;

    // Add all keys to filter that do not correspond to the most common element
    for (size_t i = 0; i < keys.size(); i++) {
      if (values[i] != most_common_value) {
        _binary_fuse_filter->add(keys[i]);
      }
    }

    _binary_fuse_filter->build();
  }

private:
  BinaryFusePreFilter() : _fingerprint_bits(8) {} // For cereal

  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(cereal::base_class<PreFilter<T>>(this), _binary_fuse_filter,
       _most_common_value, _fingerprint_bits);
  }

  BinaryFuseFilterPtr _binary_fuse_filter;
  std::optional<T> _most_common_value;
  int _fingerprint_bits;
};

} // namespace caramel

CARAMEL_REGISTER_PREFILTER(BinaryFusePreFilter)
