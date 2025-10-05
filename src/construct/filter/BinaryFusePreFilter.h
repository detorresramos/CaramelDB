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
  BinaryFusePreFilter<T>() : _binary_fuse_filter(nullptr), _most_common_value(std::nullopt) {}

  static BinaryFusePreFilterPtr<T> make() {
    return std::make_shared<BinaryFusePreFilter<T>>();
  }

  void apply(std::vector<std::string> &keys, std::vector<T> &values,
             float delta, bool verbose) {
    (void)delta; // Binary fuse filter is always applied for now
    Timer timer;

    size_t num_items = keys.size();

    auto [highest_frequency, most_common_value] = highestFrequency(values);

    if (verbose) {
      std::cout << "Applying binary fuse pre-filtering...";
    }

    size_t bf_size = num_items - highest_frequency;

    // Only create binary fuse filter if bf_size > 0
    if (bf_size == 0) {
      return;
    }

    _binary_fuse_filter = BinaryFuseFilter::make(bf_size, verbose);
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

  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(cereal::base_class<PreFilter<T>>(this), _binary_fuse_filter,
       _most_common_value);
  }

  BinaryFuseFilterPtr _binary_fuse_filter;
  std::optional<T> _most_common_value;
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
