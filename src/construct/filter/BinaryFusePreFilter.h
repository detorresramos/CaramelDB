#pragma once

#include "BinaryFuseFilter.h"
#include "PreFilter.h"
#include <array>
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

  BinaryFuseFilterPtr getBinaryFuseFilter() const {
    return _binary_fuse_filter;
  }

  std::optional<T> getMostCommonValue() const override {
    return _most_common_value;
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

CEREAL_REGISTER_TYPE(caramel::BinaryFusePreFilter<uint32_t>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<uint32_t>,
                                     caramel::BinaryFusePreFilter<uint32_t>)

CEREAL_REGISTER_TYPE(caramel::BinaryFusePreFilter<uint64_t>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<uint64_t>,
                                     caramel::BinaryFusePreFilter<uint64_t>)

using arr10 = std::array<char, 10>;
CEREAL_REGISTER_TYPE(caramel::BinaryFusePreFilter<arr10>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<arr10>,
                                     caramel::BinaryFusePreFilter<arr10>)

CEREAL_REGISTER_TYPE(caramel::BinaryFusePreFilter<std::string>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<std::string>,
                                     caramel::BinaryFusePreFilter<std::string>)

using arr12 = std::array<char, 12>;
CEREAL_REGISTER_TYPE(caramel::BinaryFusePreFilter<std::array<char, 12>>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<arr12>,
                                     caramel::BinaryFusePreFilter<arr12>)
