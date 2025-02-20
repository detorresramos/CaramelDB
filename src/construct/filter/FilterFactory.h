#pragma once

#include "BloomPreFilter.h"
#include "FilterConfig.h"
#include "PreFilter.h"

namespace caramel::FilterFactory {

template <typename T>
PreFilterPtr<T> makeFilter(std::shared_ptr<PreFilterConfig> filter_config) {
  if (auto specific_config =
          std::dynamic_pointer_cast<BloomPreFilterConfig>(filter_config)) {
    return BloomPreFilter<T>::make();
  }

//   if (auto specific_config =
//           std::dynamic_pointer_cast<XORPreFilterConfig>(filter_config)) {
//     return BloomPreFilter<T>::make();
//   }

  throw std::invalid_argument("Unsupported filter configuration type");
}

} // namespace caramel::FilterFactory