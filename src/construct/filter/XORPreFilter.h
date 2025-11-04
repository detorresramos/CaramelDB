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
  XORPreFilter<T>() : _xor_filter(nullptr), _most_common_value(std::nullopt) {}

  static XORPreFilterPtr<T> make() {
    return std::make_shared<XORPreFilter<T>>();
  }

  void apply(std::vector<std::string> &keys, std::vector<T> &values,
             float delta, bool verbose) {
    (void)delta; // XOR filter is always applied for now
    Timer timer;

    size_t num_items = keys.size();

    auto [highest_frequency, most_common_value] = highestFrequency(values);

    if (verbose) {
      std::cout << "Applying XOR pre-filtering...";
    }

    size_t xf_size = num_items - highest_frequency;

    // Only create xor filter if xf_size > 0
    if (xf_size == 0) {
      return;
    }

    _xor_filter = XorFilter::make(xf_size, verbose);
    _most_common_value = most_common_value;

    // Add all keys to xor filter that do not correspond to the most common
    // element
    for (size_t i = 0; i < num_items; i++) {
      if (values[i] != most_common_value) {
        _xor_filter->add(keys[i]);
      }
    }

    // Build the xor filter
    _xor_filter->build();

    std::vector<std::string> filtered_keys;
    filtered_keys.reserve(num_items);
    std::vector<T> filtered_values;
    filtered_values.reserve(num_items);

    // Write all (key, value) pairs that the xor filter claims are in the csf
    for (size_t i = 0; i < num_items; i++) {
      if (_xor_filter->contains(keys[i])) {
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
    if (_xor_filter) {
      return _xor_filter->contains(key);
    }
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
      highest_freq = std::max(highest_freq, frequency);
      if (highest_freq == frequency) {
        most_common_value = value;
      }
    }

    return {highest_freq, most_common_value};
  }

  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(cereal::base_class<PreFilter<T>>(this), _xor_filter, _most_common_value);
  }

  XorFilterPtr _xor_filter;
  std::optional<T> _most_common_value;
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
