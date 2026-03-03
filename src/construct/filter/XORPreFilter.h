#pragma once

#include "FilterTypes.h"
#include "PreFilter.h"
#include "XorFilter.h"
#include <optional>
#include <src/utils/SafeFileIO.h>
#include <string>
#include <vector>

namespace caramel {

template <typename T> class XORPreFilter;
template <typename T> using XORPreFilterPtr = std::shared_ptr<XORPreFilter<T>>;
template <typename T> class XORPreFilter final : public PreFilter<T> {
public:
  explicit XORPreFilter<T>(int fingerprint_bits)
      : _xor_filter(nullptr), _most_common_value(std::nullopt),
        _fingerprint_bits(fingerprint_bits) {}

  static XORPreFilterPtr<T> make(int fingerprint_bits) {
    return std::make_shared<XORPreFilter<T>>(fingerprint_bits);
  }

  bool contains(const std::string &key) override {
    if (_xor_filter) {
      return _xor_filter->contains(key);
    }
    return true;
  }

  bool contains(const char *data, size_t length) override {
    if (_xor_filter) {
      return _xor_filter->contains(data, length);
    }
    return true;
  }

  XorFilterPtr getXorFilter() const { return _xor_filter; }

  std::optional<T> getMostCommonValue() const override {
    return _most_common_value;
  }

  std::optional<FilterStats> getStats() const override {
    FilterStats fs;
    fs.type = "xor";
    if (_xor_filter) {
      fs.size_bytes = _xor_filter->size();
      fs.num_elements = _xor_filter->numElements();
      fs.fingerprint_bits = _xor_filter->fingerprintWidth();
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

  static XORPreFilterPtr<T> load(const std::string &filename) {
    auto filter = std::make_shared<XORPreFilter<T>>(8); // Placeholder
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
    _xor_filter = XorFilter::makeFixed(filter_size, _fingerprint_bits, verbose);
    _most_common_value = most_common_value;

    // Add all minority keys to the XOR filter
    for (size_t i = 0; i < keys.size(); i++) {
      if (values[i] != most_common_value) {
        _xor_filter->add(keys[i]);
      }
    }

    _xor_filter->build();
  }

private:
  XORPreFilter() : _fingerprint_bits(8) {} // For cereal

  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(cereal::base_class<PreFilter<T>>(this), _xor_filter, _most_common_value,
       _fingerprint_bits);
  }

  XorFilterPtr _xor_filter;
  std::optional<T> _most_common_value;
  int _fingerprint_bits;
};

} // namespace caramel

CARAMEL_REGISTER_PREFILTER(XORPreFilter)
