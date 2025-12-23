#pragma once

#include "BinaryFusePreFilter.h"
#include "BloomPreFilter.h"
#include "FilterConfig.h"
#include "PreFilter.h"
#include "XORPreFilter.h"

namespace caramel::FilterFactory {

template <typename T>
PreFilterPtr<T> makeFilter(std::shared_ptr<PreFilterConfig> filter_config) {
  if (auto cfg =
          std::dynamic_pointer_cast<BloomPreFilterConfig>(filter_config)) {
    return BloomPreFilter<T>::make(cfg->bits_per_element, cfg->num_hashes);
  }

  if (auto cfg =
          std::dynamic_pointer_cast<XORPreFilterConfig>(filter_config)) {
    return XORPreFilter<T>::make(cfg->fingerprint_bits);
  }

  if (auto cfg =
          std::dynamic_pointer_cast<BinaryFusePreFilterConfig>(filter_config)) {
    return BinaryFusePreFilter<T>::make(cfg->fingerprint_bits);
  }

  throw std::invalid_argument("Unsupported filter configuration type");
}

} // namespace caramel::FilterFactory
