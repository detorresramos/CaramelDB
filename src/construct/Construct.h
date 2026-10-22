#pragma once

#include "Csf.h"
#include <src/build/Build.h>
#include <stdexcept>

namespace caramel {

// Builds a single-column CSF in this process. See src/build/Build.h for the
// steps, which can also be run across machines.
template <typename T>
CsfPtr<T> constructCsf(const build::Signatures &keys,
                       const std::vector<T> &values,
                       PreFilterConfigPtr filter_config = nullptr,
                       bool verbose = true) {
  if (values.empty()) {
    throw std::invalid_argument("Values must be non-empty but found length 0.");
  }
  if (keys.size() != values.size()) {
    throw std::invalid_argument("Keys and values must have the same length.");
  }
  build::BuildSpec spec;
  spec.filter = build::FilterType::from(filter_config);
  spec.verbose = verbose;
  return std::make_shared<Csf<T>>(
      build::buildLocal(spec, keys, build::Rows<T>::single(values)));
}

template <typename T>
CsfPtr<T> constructCsf(const std::vector<std::string> &keys,
                       const std::vector<T> &values,
                       PreFilterConfigPtr filter_config = nullptr,
                       bool verbose = true) {
  return constructCsf<T>(build::hashKeys(keys), values, std::move(filter_config),
                         verbose);
}

} // namespace caramel
