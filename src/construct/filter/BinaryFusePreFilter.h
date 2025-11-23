#pragma once

#include "PreFilter.h"
#include "BinaryFuseFilter.h"
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

template <typename T> class BinaryFusePreFilter;
template <typename T>
using BinaryFusePreFilterPtr = std::shared_ptr<BinaryFusePreFilter<T>>;

template <typename T> class BinaryFusePreFilter final : public PreFilter<T> {
public:
  // Allow optional override of error rate
  explicit BinaryFusePreFilter<T>(std::optional<float> error_rate = std::nullopt)
      : _binary_fuse_filter(nullptr), _most_common_value(std::nullopt),
        _error_rate(error_rate) {}

  static BinaryFusePreFilterPtr<T>
  make(std::optional<float> error_rate = std::nullopt) {
    return std::make_shared<BinaryFusePreFilter<T>>(error_rate);
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
      error_rate = calculateErrorRateBinaryFuse(alpha, delta);
      // If the "optimal" ε is degenerate, theory says "no prefilter"
      if (error_rate >= 0.5f || error_rate <= 0.0f) {
        if (verbose) {
          std::cout << "Skipping Binary Fuse pre-filtering (epsilon="
                    << error_rate << ")" << std::endl;
        }
        return;
      }
    }

    if (verbose) {
      std::cout << "Applying binary fuse pre-filtering with target ε≈"
                << error_rate << "...";
    }

    const size_t bf_size = num_items - highest_frequency;

    // Only create binary fuse filter if bf_size > 0
    if (bf_size == 0) {
      if (verbose) {
        std::cout << " nothing to filter (bf_size=0)." << std::endl;
      }
      return;
    }

    // Create Binary Fuse filter with calculated optimal error rate
    _binary_fuse_filter = BinaryFuseFilter::make(bf_size, error_rate, verbose);
    _most_common_value = most_common_value;

    // Add all keys to binary fuse filter that do not correspond to the most common element
    for (size_t i = 0; i < num_items; i++) {
      if (values[i] != most_common_value) {
        _binary_fuse_filter->add(keys[i]);
      }
    }

    // Build the binary fuse filter
    _binary_fuse_filter->build();

    std::vector<std::string> filtered_keys;
    filtered_keys.reserve(num_items);
    std::vector<T> filtered_values;
    filtered_values.reserve(num_items);

    // Write all (key, value) pairs that the binary fuse filter claims are in the csf
    for (size_t i = 0; i < num_items; i++) {
      if (_binary_fuse_filter->contains(keys[i])) {
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
    if (_binary_fuse_filter) {
      return _binary_fuse_filter->contains(key);
    }
    return true;
  }

  BinaryFuseFilterPtr getBinaryFuseFilter() const { return _binary_fuse_filter; }

  std::optional<T> getMostCommonValue() const { return _most_common_value; }

  void save(const std::string &filename) const {
    auto output_stream = SafeFileIO::ofstream(filename, std::ios::binary);
    cereal::BinaryOutputArchive oarchive(output_stream);
    oarchive(*this);
  }

  static BinaryFusePreFilterPtr<T> load(const std::string &filename) {
    auto filter = std::make_shared<BinaryFusePreFilter<T>>();
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
      highest_freq = std::max(highest_freq, frequency);
      if (highest_freq == frequency) {
        most_common_value = value;
      }
    }

    return {highest_freq, most_common_value};
  }

  // Binary Fuse filter bit cost: b(ε) ≈ 1.075 * 8 bits (for 4-wise)
  // Binary Fuse has 1.075x space overhead (better than XOR's 1.23x)
  // Using the correct overhead constant for optimal error rate calculation.
  inline float calculateErrorRateBinaryFuse(float alpha, float delta) {
    constexpr float c_binary_fuse = 1.075f;
    return (c_binary_fuse / (delta * std::log(2.0f))) * ((1.0f - alpha) / alpha);
  }

  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(cereal::base_class<PreFilter<T>>(this), _binary_fuse_filter,
       _most_common_value, _error_rate);
  }

  BinaryFuseFilterPtr _binary_fuse_filter;
  std::optional<T> _most_common_value;
  std::optional<float> _error_rate;
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
