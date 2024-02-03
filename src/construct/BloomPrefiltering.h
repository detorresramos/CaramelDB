#pragma once

#include "BloomFilter.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace caramel {

template <typename T>
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

template <typename T>
std::tuple<std::vector<std::string>, std::vector<T>, BloomFilterPtr,
           std::optional<T>>
bloomPrefiltering(const std::vector<std::string> &keys,
                  const std::vector<T> &values, float delta) {
  auto [highest_frequency, most_common_value] = highestFrequency(values);
  float highest_normalized_frequency =
      static_cast<float>(highest_frequency) / static_cast<float>(values.size());

  float error_rate = calculateErrorRate(
      /* alpha= */ highest_normalized_frequency, /* delta= */ delta);

  if (error_rate > 1) {
    return {std::move(keys), std::move(values), nullptr, std::nullopt};
  }

  auto bloom_filter =
      BloomFilter::make(values.size() - highest_frequency, error_rate);

  // add all keys to bf that do not correspond to the most common element
  for (size_t i = 0; i < keys.size(); i++) {
    if (values[i] != most_common_value) {
      bloom_filter->add(keys[i]);
    }
  }

  std::vector<std::string> filtered_keys;
  std::vector<T> filtered_values;

  // write all (key, value) pairs that the bf claims are in the csf
  for (size_t i = 0; i < keys.size(); i++) {
    if (bloom_filter->contains(keys[i])) {
      filtered_keys.push_back(keys[i]);
      filtered_values.push_back(values[i]);
    }
  }

  return {std::move(filtered_keys), std::move(filtered_values), bloom_filter,
          most_common_value};
}

} // namespace caramel