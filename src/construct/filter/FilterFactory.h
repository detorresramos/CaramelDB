#pragma once

#include "BinaryFusePreFilter.h"
#include "BloomPreFilter.h"
#include "FilterConfig.h"
#include "PreFilter.h"
#include "XORPreFilter.h"

namespace caramel::FilterFactory {

// Builds a concrete (non-Auto) filter over the given minority-key hashes.
template <typename T>
PreFilterPtr<T> build(const PreFilterConfigPtr &config, T most_common_value,
                      std::vector<uint64_t> key_hashes, bool verbose) {
  if (auto cfg = std::dynamic_pointer_cast<BloomPreFilterConfig>(config)) {
    return BloomPreFilter<T>::build(cfg->bits_per_element, cfg->num_hashes,
                                    most_common_value, key_hashes, verbose);
  }
  if (auto cfg = std::dynamic_pointer_cast<XORPreFilterConfig>(config)) {
    return XORPreFilter<T>::build(cfg->fingerprint_bits, most_common_value,
                                  std::move(key_hashes), verbose);
  }
  if (auto cfg = std::dynamic_pointer_cast<BinaryFusePreFilterConfig>(config)) {
    return BinaryFusePreFilter<T>::build(cfg->fingerprint_bits,
                                         most_common_value,
                                         std::move(key_hashes), verbose);
  }
  throw std::invalid_argument("Unsupported filter configuration type");
}

} // namespace caramel::FilterFactory
