#pragma once

#include <cereal/access.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/polymorphic.hpp>
#include <optional>
#include <src/construct/CsfStats.h>
#include <src/construct/Hashing.h>
#include <string>
#include <vector>

namespace caramel {

// A prefilter answers "might this key's value differ from the most common
// value?". Keys it rejects decode to the MCV without touching the CSF. Every
// filter is built from the filterHash() of the minority keys only, so a build
// can collect those hashes on many machines and construct the filter once.
template <typename T> class PreFilter {
public:
  virtual ~PreFilter() = default;

  virtual bool containsHash(uint64_t key_hash) const = 0;

  bool contains(const char *data, size_t length) const {
    return containsHash(filterHash(data, length));
  }

  bool contains(const std::string &key) const {
    return contains(key.data(), key.size());
  }

  std::optional<T> getMostCommonValue() const { return _most_common_value; }

  virtual std::optional<FilterStats> getStats() const { return std::nullopt; }

protected:
  std::optional<T> _most_common_value;

private:
  friend class cereal::access;
  template <typename Archive> void serialize(Archive &archive) {
    archive(_most_common_value);
  }
};

template <typename T> using PreFilterPtr = std::shared_ptr<PreFilter<T>>;

} // namespace caramel
