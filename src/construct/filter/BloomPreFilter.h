#pragma once

#include "BloomFilter.h"
#include "PreFilter.h"
#include <array>
#include <optional>
#include <src/utils/SafeFileIO.h>
#include <string>
#include <vector>

namespace caramel {

template <typename T> class BloomPreFilter;
template <typename T>
using BloomPreFilterPtr = std::shared_ptr<BloomPreFilter<T>>;

template <typename T> class BloomPreFilter final : public PreFilter<T> {
public:
  BloomPreFilter<T>(size_t size, size_t num_hashes)
      : _bloom_filter(nullptr), _most_common_value(std::nullopt), _size(size),
        _num_hashes(num_hashes) {}

  static BloomPreFilterPtr<T> make(size_t size, size_t num_hashes) {
    return std::make_shared<BloomPreFilter<T>>(size, num_hashes);
  }

  bool contains(const std::string &key) override {
    if (_bloom_filter) {
      return _bloom_filter->contains(key);
    }
    return true;
  }

  BloomFilterPtr getBloomFilter() const { return _bloom_filter; }

  std::optional<T> getMostCommonValue() const override {
    return _most_common_value;
  }

  void save(const std::string &filename) const {
    auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(output_stream);
    oarchive(*this);
  }

  static BloomPreFilterPtr<T> load(const std::string &filename) {
    auto filter = std::make_shared<BloomPreFilter<T>>(1, 1); // Placeholder
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
    (void)filter_size; // Bloom uses user-provided _size instead
    _bloom_filter = BloomFilter::makeFixed(_size, _num_hashes);

    _most_common_value = most_common_value;

    if (verbose) {
      std::cout << "BloomPreFilter: size=" << _size
                << ", num_hashes=" << _num_hashes << std::endl;
    }

    // Add all keys to filter that do not correspond to the most common element
    for (size_t i = 0; i < keys.size(); i++) {
      if (values[i] != most_common_value) {
        _bloom_filter->add(keys[i]);
      }
    }
  }

private:
  BloomPreFilter() : _size(0), _num_hashes(0) {} // For cereal

  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(cereal::base_class<PreFilter<T>>(this), _bloom_filter,
       _most_common_value, _size, _num_hashes);
  }

  BloomFilterPtr _bloom_filter;
  std::optional<T> _most_common_value;
  size_t _size;
  size_t _num_hashes;
};

} // namespace caramel

CEREAL_REGISTER_TYPE(caramel::BloomPreFilter<uint32_t>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<uint32_t>,
                                     caramel::BloomPreFilter<uint32_t>)

CEREAL_REGISTER_TYPE(caramel::BloomPreFilter<uint64_t>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<uint64_t>,
                                     caramel::BloomPreFilter<uint64_t>)

using arr10 = std::array<char, 10>;
CEREAL_REGISTER_TYPE(caramel::BloomPreFilter<arr10>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<arr10>,
                                     caramel::BloomPreFilter<arr10>)

CEREAL_REGISTER_TYPE(caramel::BloomPreFilter<std::string>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<std::string>,
                                     caramel::BloomPreFilter<std::string>)

using arr12 = std::array<char, 12>;
CEREAL_REGISTER_TYPE(caramel::BloomPreFilter<std::array<char, 12>>)
CEREAL_REGISTER_POLYMORPHIC_RELATION(caramel::PreFilter<arr12>,
                                     caramel::BloomPreFilter<arr12>)
