#pragma once

#include "src/construct/multiset/ConstructRaggedMultiset.h"
#include "src/construct/multiset/PackedCsf.h"

namespace caramel {

template <typename T>
PackedCsfPtr<T> constructPackedCsf(const std::vector<std::string> &keys,
                                   const std::vector<std::vector<T>> &values,
                                   bool verbose = true) {
  if (keys.size() != values.size()) {
    throw std::invalid_argument("Keys and values must have the same length.");
  }
  auto [flat, offsets] = flattenRows(values);
  build::BuildSpec spec;
  spec.encoding = build::Encoding::Packed;
  spec.verbose = verbose;
  return std::make_shared<PackedCsf<T>>(build::buildLocal(
      spec, keys,
      build::Rows<T>::ragged(flat.data(), offsets.data(), values.size())));
}

} // namespace caramel
