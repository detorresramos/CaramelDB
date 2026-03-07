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
};

} // namespace caramel
