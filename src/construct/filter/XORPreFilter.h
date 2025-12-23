#pragma once

#include "PreFilter.h"
#include "XorFilter.h"
#include <array>
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
    // If we never built a filter, behave as "no filter": always forward
    return true;
  }

  XorFilterPtr getXorFilter() const { return _xor_filter; }

  std::optional<T> getMostCommonValue() const override {
    return _most_common_value;
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

CEREAL_REGISTER_TYPE(caramel::XORPreFilter<uint32_t>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<uint32_t>,
                                     caramel::XORPreFilter<uint32_t>)

CEREAL_REGISTER_TYPE(caramel::XORPreFilter<uint64_t>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<uint64_t>,
                                     caramel::XORPreFilter<uint64_t>)

using arr10 = std::array<char, 10>;
CEREAL_REGISTER_TYPE(caramel::XORPreFilter<arr10>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<arr10>,
                                     caramel::XORPreFilter<arr10>)

CEREAL_REGISTER_TYPE(caramel::XORPreFilter<std::string>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<std::string>,
                                     caramel::XORPreFilter<std::string>)

using arr12 = std::array<char, 12>;
CEREAL_REGISTER_TYPE(caramel::XORPreFilter<std::array<char, 12>>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<arr12>,
                                     caramel::XORPreFilter<arr12>)
