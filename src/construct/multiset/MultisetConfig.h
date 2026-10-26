#pragma once

#include "src/build/Spec.h"
#include "src/construct/filter/FilterConfig.h"
#include "src/construct/multiset/permute/PermutationConfig.h"

namespace caramel {

struct MultisetConfig {
  PermutationConfigPtr permutation_config = nullptr;
  PreFilterConfigPtr filter_config = nullptr;
  build::CodebookMode codebook = build::CodebookMode::Auto;
  bool shared_filter = false;
  bool verbose = true;
};

} // namespace caramel
