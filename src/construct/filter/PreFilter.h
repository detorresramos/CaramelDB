#pragma once

#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/polymorphic.hpp>
#include <optional>
#include <src/utils/Timer.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace caramel {

template <typename T> class PreFilter {
public:
  virtual void apply(std::vector<std::string> &keys, std::vector<T> &values,
                     float delta, bool verbose) {
    Timer timer;
    (void)delta; // No longer used for autotuning

    const size_t num_items = keys.size();
    if (num_items == 0) {
      return;
    }

    auto [highest_frequency, most_common_value] = highestFrequency(values);
    const size_t filter_size = num_items - highest_frequency;

    // Always create filter (user explicitly requested it via config)
    createAndPopulateFilter(filter_size, keys, values, most_common_value,
                            verbose);

    std::vector<std::string> filtered_keys;
    filtered_keys.reserve(num_items);
    std::vector<T> filtered_values;
    filtered_values.reserve(num_items);

    // Keep only (key, value) pairs that the filter claims are in the CSF
    for (size_t i = 0; i < num_items; i++) {
      if (contains(keys[i])) {
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

  virtual bool contains(const std::string &key) = 0;

  virtual std::optional<T> getMostCommonValue() const = 0;

  virtual ~PreFilter() = default;

protected:
  virtual void createAndPopulateFilter(size_t filter_size,
                                       const std::vector<std::string> &keys,
                                       const std::vector<T> &values,
                                       T most_common_value, bool verbose) = 0;

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

private:
  friend class cereal::access;
  template <typename Archive> void serialize(Archive &archive) {
    (void)archive;
  }
};

template <typename T> using PreFilterPtr = std::shared_ptr<PreFilter<T>>;

} // namespace caramel
