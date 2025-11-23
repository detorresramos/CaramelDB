#pragma once

#include "PreFilter.h"
#include "XorFilter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <src/utils/SafeFileIO.h>
#include <src/utils/Timer.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace caramel {

template <typename T> class XORPreFilter;
template <typename T> using XORPreFilterPtr = std::shared_ptr<XORPreFilter<T>>;
template <typename T> class XORPreFilter final : public PreFilter<T> {
public:
  // Allow optional override of error rate, analogous to BloomPreFilter
  explicit XORPreFilter<T>(std::optional<float> error_rate = std::nullopt)
      : _xor_filter(nullptr), _most_common_value(std::nullopt),
        _error_rate(error_rate) {}

  static XORPreFilterPtr<T>
  make(std::optional<float> error_rate = std::nullopt) {
    return std::make_shared<XORPreFilter<T>>(error_rate);
  }

  void apply(std::vector<std::string> &keys, std::vector<T> &values,
             float delta, bool verbose) {
    Timer timer;

    const size_t num_items = keys.size();
    if (num_items == 0) {
      return;
    }

    auto [highest_frequency, most_common_value] = highestFrequency(values);
    const float alpha =
        static_cast<float>(highest_frequency) / static_cast<float>(num_items);

    // Decide epsilon (false positive rate) using theory, unless fixed
    // externally
    float error_rate;
    if (_error_rate.has_value()) {
      error_rate = _error_rate.value();
    } else {
      error_rate = calculateErrorRateXor(alpha, delta);
      // If the "optimal" ε is degenerate, theory says "no prefilter"
      if (error_rate >= 0.5f || error_rate <= 0.0f) {
        if (verbose) {
          std::cout << "Skipping XOR pre-filtering (epsilon=" << error_rate
                    << ")" << std::endl;
        }
        return;
      }
    }

    if (verbose) {
      std::cout << "Applying XOR pre-filtering with target ε≈" << error_rate
                << "...";
    }

    const size_t xf_size = num_items - highest_frequency;

    // Only create xor filter if xf_size > 0
    if (xf_size == 0) {
      if (verbose) {
        std::cout << " nothing to filter (xf_size=0)." << std::endl;
      }
      return;
    }

    // Create XOR filter with calculated optimal error rate
    _xor_filter = XorFilter::make(xf_size, error_rate, verbose);
    _most_common_value = most_common_value;

    // Add all minority keys to the XOR filter
    for (size_t i = 0; i < num_items; i++) {
      if (values[i] != most_common_value) {
        _xor_filter->add(keys[i]);
      }
    }

    _xor_filter->build();

    std::vector<std::string> filtered_keys;
    filtered_keys.reserve(num_items);
    std::vector<T> filtered_values;
    filtered_values.reserve(num_items);

    // Keep only (key, value) pairs that the XOR filter claims are in the CSF
    for (size_t i = 0; i < num_items; i++) {
      if (_xor_filter->contains(keys[i])) {
        filtered_keys.push_back(std::move(keys[i]));
        filtered_values.push_back(std::move(values[i]));
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
    if (_xor_filter) {
      return _xor_filter->contains(key);
    }
    // If we never built a filter, behave as "no filter": always forward
    return true;
  }

  XorFilterPtr getXorFilter() const { return _xor_filter; }

  std::optional<T> getMostCommonValue() const { return _most_common_value; }

  void save(const std::string &filename) const {
    auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(output_stream);
    oarchive(*this);
  }

  static XORPreFilterPtr<T> load(const std::string &filename) {
    auto filter = std::make_shared<XORPreFilter<T>>();
    auto input_stream = SafeFileIO::ifstream(filename, std::ios::binary);
    cereal::BinaryInputArchive iarchive(input_stream);
    iarchive(*filter);
    return filter;
  }

private:
  std::pair<size_t, T> highestFrequency(const std::vector<T> &values) {
    std::unordered_map<T, size_t> frequencies;
    for (const T &value : values) {
      frequencies[value]++;
    }

    size_t highest_freq = 0;
    T most_common_value = values[0];
    for (auto [value, frequency] : frequencies) {
      if (frequency >= highest_freq) {
        highest_freq = frequency;
        most_common_value = value;
      }
    }

    return {highest_freq, most_common_value};
  }

  // XOR filter bit cost: b(ε) ≈ 1.23 log2(1/ε)
  // A first-order optimization of Δ(ε) with this b(ε) yields the same
  // closed form as the Bloom prefilter but with 1.23 instead of 1.44.
  inline float calculateErrorRateXor(float alpha, float delta) {
    constexpr float c_xor = 1.23f;
    return (c_xor / (delta * std::log(2.0f))) * ((1.0f - alpha) / alpha);
  }

  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(cereal::base_class<PreFilter<T>>(this), _xor_filter, _most_common_value,
       _error_rate);
  }

  XorFilterPtr _xor_filter;
  std::optional<T> _most_common_value;
  std::optional<float> _error_rate;
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
