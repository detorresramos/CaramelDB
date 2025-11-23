#pragma once

#include "BloomFilter.h"
#include "PreFilter.h"
#include <array>
#include <cmath>
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
  BloomPreFilter<T>(std::optional<float> error_rate = std::nullopt,
                    std::optional<size_t> k = std::nullopt)
      : _bloom_filter(nullptr), _most_common_value(std::nullopt),
        _error_rate(error_rate), _k(k) {}

  static BloomPreFilterPtr<T>
  make(std::optional<float> error_rate = std::nullopt,
       std::optional<size_t> k = std::nullopt) {
    return std::make_shared<BloomPreFilter<T>>(error_rate, k);
  }

  bool contains(const std::string &key) {
    if (_bloom_filter) {
      return _bloom_filter->contains(key);
    }
    return true;
  }

  BloomFilterPtr getBloomFilter() const { return _bloom_filter; }

  std::optional<T> getMostCommonValue() const { return _most_common_value; }

  void save(const std::string &filename) const {
    auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(output_stream);
    oarchive(*this);
  }

  static BloomPreFilterPtr<T> load(const std::string &filename) {
    auto filter = std::make_shared<BloomPreFilter<T>>();
    auto input_stream = SafeFileIO::ifstream(filename, std::ios::binary);
    cereal::BinaryInputArchive iarchive(input_stream);
    iarchive(*filter);
    return filter;
  }

protected:
  float calculateErrorRate(float alpha, float delta) override {
    if (_error_rate.has_value()) {
      return _error_rate.value();
    }
    return (1.44 / (delta * std::log(2.0f))) * ((1.0f - alpha) / alpha);
  }

  void createAndPopulateFilter(size_t filter_size, float error_rate,
                               const std::vector<std::string> &keys,
                               const std::vector<T> &values,
                               T most_common_value, bool verbose) override {
    if (_k) {
      _bloom_filter = BloomFilter::makeAutotunedFixedK(filter_size, error_rate,
                                                       *_k, verbose);
    } else {
      _bloom_filter =
          BloomFilter::makeAutotuned(filter_size, error_rate, verbose);
    }

    _most_common_value = most_common_value;

    // Add all keys to filter that do not correspond to the most common element
    for (size_t i = 0; i < keys.size(); i++) {
      if (values[i] != most_common_value) {
        _bloom_filter->add(keys[i]);
      }
    }
  }

  bool shouldSkipFiltering(float error_rate) const override {
    if (_error_rate.has_value()) {
      return false;
    }
    return error_rate >= 0.5f || error_rate == 0.0f;
  }

private:
  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(cereal::base_class<PreFilter<T>>(this), _bloom_filter,
       _most_common_value, _error_rate);
  }

  BloomFilterPtr _bloom_filter;
  std::optional<T> _most_common_value;
  std::optional<float> _error_rate;
  std::optional<size_t> _k;
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
