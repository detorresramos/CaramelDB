#pragma once

#include "Rows.h"
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <src/construct/Hashing.h>
#include <src/construct/filter/PreFilter.h>

namespace caramel::build {

// The built prefilters of a build and which column uses which.
template <typename T> struct Filters {
  std::vector<PreFilterPtr<T>> filters;
  std::vector<int32_t> column_filter; // per column: index into filters, or -1

  bool empty() const { return filters.empty(); }

  int32_t of(uint32_t col) const {
    return col < column_filter.size() ? column_filter[col] : -1;
  }

  const PreFilterPtr<T> &forColumn(uint32_t col) const {
    static const PreFilterPtr<T> none;
    int32_t f = of(col);
    return f < 0 ? none : filters[f];
  }

  // masks[f] bit r is set iff filters[f] passes key r: one probe per key no
  // matter how many filters or columns read the result.
  std::vector<std::vector<uint64_t>> passMasks(const Signatures &sigs) const {
    const size_t n = sigs.size();
    const size_t words = (n + 63) / 64;
    std::vector<std::vector<uint64_t>> masks(filters.size(),
                                             std::vector<uint64_t>(words, 0));
#pragma omp parallel for schedule(static) default(none)                        \
    shared(sigs, masks, n, words)
    for (size_t w = 0; w < words; w++) {
      const size_t end = std::min(n, (w + 1) * 64);
      for (size_t r = w * 64; r < end; r++) {
        uint64_t h = filterHash(sigs[r]);
        for (size_t f = 0; f < filters.size(); f++) {
          if (filters[f]->containsHash(h)) {
            masks[f][w] |= uint64_t(1) << (r % 64);
          }
        }
      }
    }
    return masks;
  }

  template <class Archive> void serialize(Archive &archive) {
    archive(filters, column_filter);
  }
};

inline bool maskBit(const std::vector<uint64_t> &mask, uint64_t r) {
  return (mask[r / 64] >> (r % 64)) & 1;
}

} // namespace caramel::build
