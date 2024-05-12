#pragma once

#include "Construct.h"
#include "MultisetCsf.h"

namespace caramel {

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