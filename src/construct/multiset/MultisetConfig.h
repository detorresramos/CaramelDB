#pragma once

#include "src/construct/filter/FilterConfig.h"
#include "src/construct/multiset/permute/PermutationConfig.h"

namespace caramel {

struct MultisetConfig {
  PermutationConfigPtr permutation_config = nullptr;
  PreFilterConfigPtr filter_config = nullptr;
  bool shared_codebook = false;
  bool shared_filter = false;
  bool verbose = true;
  // If false, skip building Huffman lookup tables at reload time.
  // Queries fall back to O(max_codelength) canonical decode.
  // Saves ~2^max_codelength * sizeof(T) bytes per column.
  bool build_lookup_table = true;
};

} // namespace caramel
