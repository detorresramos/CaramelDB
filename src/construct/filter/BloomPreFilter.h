#pragma once

#include "BloomFilter.h"
#include "PreFilter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <src/utils/Timer.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace caramel {

template <typename T> class BloomPreFilter;
template <typename T>
using BloomPreFilterPtr = std::shared_ptr<BloomPreFilter<T>>;

template <typename T> class BloomPreFilter final : public PreFilter<T> {
public:
  BloomPreFilter<T>(std::optional<float> error_rate = std::nullopt)
      : _bloom_filter(nullptr), _most_common_value(std::nullopt), _error_rate(error_rate) {}

  static BloomPreFilterPtr<T> make(std::optional<float> error_rate = std::nullopt) {
    return std::make_shared<BloomPreFilter<T>>(error_rate);
  }

  void apply(std::vector<std::string> &keys, std::vector<T> &values,
             float delta, bool verbose) {
    Timer timer;

    size_t num_items = keys.size();

    auto [highest_frequency, most_common_value] = highestFrequency(values);

    float highest_normalized_frequency =
        static_cast<float>(highest_frequency) / static_cast<float>(num_items);

    float error_rate;
    if (_error_rate.has_value()) {
      error_rate = _error_rate.value();
    } else {
      error_rate = calculateErrorRate(
          /* alpha= */ highest_normalized_frequency, /* delta= */ delta);
      
      if (error_rate >= 0.5 || error_rate == 0) {
        return;
      }
    }

    if (verbose) {
      std::cout << "Applying bloom pre-filtering...";
    }

    size_t bf_size = num_items - highest_frequency;
    
    // Only create bloom filter if bf_size > 0
    if (bf_size == 0) {
      return;
    }
    
    _bloom_filter = BloomFilter::makeAutotuned(bf_size, error_rate);
    _most_common_value = most_common_value;

    // add all keys to bf that do not correspond to the most common element
    for (size_t i = 0; i < num_items; i++) {
      if (values[i] != most_common_value) {
        _bloom_filter->add(keys[i]);
      }
    }

    std::vector<std::string> filtered_keys;
    filtered_keys.reserve(num_items);
    std::vector<T> filtered_values;
    filtered_values.reserve(num_items);

    // write all (key, value) pairs that the bf claims are in the csf
    for (size_t i = 0; i < num_items; i++) {
      if (_bloom_filter->contains(keys[i])) {
        filtered_keys.push_back(keys[i]);
        filtered_values.push_back(values[i]);
      }
    }

    keys = std::move(filtered_keys);
    values = std::move(filtered_values);

    if (verbose) {
      std::cout << " finished in " << timer.seconds() << " seconds."
                << std::endl;
    }
  }

  bool contains(const std::string &key) {
    if (_bloom_filter) {
      return _bloom_filter->contains(key);
    }
    return true;
  }

  BloomFilterPtr getBloomFilter() const { return _bloom_filter; }

  std::optional<T> getMostCommonValue() const { return _most_common_value; }

private:
  std::pair<size_t, T> highestFrequency(const std::vector<T> &values) {
    std::unordered_map<T, size_t> frequencies;
    for (const T &value : values) {
      frequencies[value]++;
    }

    size_t highest_freq = 0;
    T most_common_value = values[0];
    for (auto [value, frequency] : frequencies) {
      highest_freq = std::max(highest_freq, frequency);
      if (highest_freq == frequency) {
        most_common_value = value;
      }
    }

    return {highest_freq, most_common_value};
  }

  inline float calculateErrorRate(float alpha, float delta) {
    return (1.44 / (delta * log(2))) * ((1 - alpha) / alpha);
  }

  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(cereal::base_class<PreFilter<T>>(this), _bloom_filter,
       _most_common_value, _error_rate);
  }

  BloomFilterPtr _bloom_filter;
  std::optional<T> _most_common_value;
  std::optional<float> _error_rate;
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
