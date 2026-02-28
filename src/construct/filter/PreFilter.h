#pragma once

#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/polymorphic.hpp>
#include <optional>
#include <src/construct/CsfStats.h>
#include <src/utils/Timer.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace caramel {

template <typename T> class PreFilter {
public:
  virtual void apply(const std::vector<std::string> &keys,
                     const std::vector<T> &values,
                     std::vector<std::string> &out_keys,
                     std::vector<T> &out_values, float delta, bool verbose) {
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

    out_keys.clear();
    out_keys.reserve(filter_size);
    out_values.clear();
    out_values.reserve(filter_size);

    // Keep only (key, value) pairs that the filter claims are in the CSF
    for (size_t i = 0; i < num_items; i++) {
      if (contains(keys[i])) {
        out_keys.push_back(keys[i]);
        out_values.push_back(values[i]);
      }
    }

    if (verbose) {
      std::cout << " finished in " << timer.seconds() << " seconds."
                << std::endl;
    }
  }

  virtual bool contains(const std::string &key) = 0;

  virtual std::optional<T> getMostCommonValue() const = 0;

  virtual std::optional<FilterStats> getStats() const {
    return std::nullopt;
  }

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
