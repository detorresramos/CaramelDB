#pragma once

#include <src/construct/Group.h>

namespace caramel::build {

// The query-ready contents of any index type: what combine() produces and
// every index class is constructed from.
template <typename T> struct Index {
  std::vector<Group<T>> groups;
  std::shared_ptr<CsfCodebook<T>> shared_codebook;
  uint32_t num_cols = 0;
  uint32_t max_pad = 0; // Packed: the widest decode window of any cell
  T stop_symbol{};
};

} // namespace caramel::build
