#pragma once

#include <algorithm>
#include <cstddef>
#include <omp.h>
#include <vector>

namespace caramel {

// std::sort of [first, last) on every thread: sort equal slices, then merge
// neighbouring runs pairwise. Small ranges sort serially.
template <typename It, typename Less>
void parallelSort(It first, It last, Less less) {
  const size_t n = static_cast<size_t>(last - first);
  const size_t parts = static_cast<size_t>(omp_get_max_threads());
  if (n < (1u << 16) || parts < 2) {
    std::sort(first, last, less);
    return;
  }
  std::vector<size_t> bounds(parts + 1);
  for (size_t i = 0; i <= parts; i++) {
    bounds[i] = n * i / parts;
  }
#pragma omp parallel for schedule(static, 1) default(none) shared(first, bounds, less, parts)
  for (size_t i = 0; i < parts; i++) {
    std::sort(first + bounds[i], first + bounds[i + 1], less);
  }
  for (size_t width = 1; width < parts; width *= 2) {
    const size_t pairs = (parts + 2 * width - 1) / (2 * width);
#pragma omp parallel for schedule(dynamic, 1) default(none)                    \
    shared(first, bounds, less, parts, width, pairs)
    for (size_t k = 0; k < pairs; k++) {
      const size_t lo = 2 * width * k;
      const size_t mid = std::min(lo + width, parts);
      const size_t hi = std::min(lo + 2 * width, parts);
      if (mid < hi) {
        std::inplace_merge(first + bounds[lo], first + bounds[mid],
                           first + bounds[hi], less);
      }
    }
  }
}

} // namespace caramel
