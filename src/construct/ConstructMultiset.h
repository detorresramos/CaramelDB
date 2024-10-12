#pragma once

#include "Construct.h"
#include "MultisetCsf.h"

namespace caramel {

// We can calculate the bucketed hash store once here and use it for every csf
// The bloom filter stuff is by column, if we determine that a column needs a
// filter, We have to apply the filtering and build a custom bucketed hash
// store for that column We can build a separate codedict for each column and
// store it in every csf Or optionally we can build one big codedict and share
// it across the multisetcsf In the case where we share it, we can save it
// once in the multisetcsf but we have to figure out a nice way to do that
// because right now its required in every csf
//
// Lastly, we should think about supporting generic filters and what the logic
// would be for that

template <typename T>
MultisetCsfPtr<T>
constructMultisetCsf(const std::vector<std::string> &keys,
                     const std::vector<std::vector<T>> &values,
                     bool use_bloom_filter = true, bool verbose = true) {
  size_t num_csfs = values.size();

  std::vector<CsfPtr<T>> csfs(num_csfs);

  // TODO(david) can/should we put a pragma here?
  for (size_t i = 0; i < num_csfs; i++) {
    csfs[i] = constructCsf<T>(keys, values[i], use_bloom_filter, verbose);
  }

  return std::make_shared<MultisetCsf<T>>(csfs);
}

} // namespace caramel