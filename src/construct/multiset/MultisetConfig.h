#pragma once

#include "src/construct/filter/FilterConfig.h"

namespace caramel {

enum class PermutationStrategy { None, Entropy };

struct MultisetConfig {
  PermutationStrategy permutation = PermutationStrategy::None;
  PreFilterConfigPtr filter_config = nullptr;
  bool shared_codebook = false;
  bool shared_filter = false;
  bool verbose = true;
};

} // namespace caramel
