#pragma once

#include "src/construct/Construct.h"
#include "src/construct/multiset/MultisetConfig.h"
#include "src/construct/multiset/MultisetCsf.h"
#include "src/construct/multiset/permute/EntropyPermutation.h"
#include "src/construct/multiset/permute/GlobalSortPermutation.h"
#include "src/utils/Timer.h"

namespace caramel {

inline build::BuildSpec multisetSpec(const MultisetConfig &config) {
  build::BuildSpec spec;
  spec.filter = build::FilterType::from(config.filter_config);
  spec.shared_filter = config.shared_filter;
  spec.codebook = config.codebook;
  spec.verbose = config.verbose;
  return spec;
}

// Applies the configured permutation to a row-major num_rows x num_cols buffer.
template <typename T>
void permuteRowMajor(T *data, int64_t num_rows, int64_t num_cols,
                     const PermutationConfigPtr &config) {
  if (!config || num_cols <= 1) {
    return;
  }
  if (std::dynamic_pointer_cast<EntropyPermutationConfig>(config)) {
    entropyPermutation<T>(data, num_rows, num_cols);
  } else if (auto cfg =
                 std::dynamic_pointer_cast<GlobalSortPermutationConfig>(config)) {
    globalSortPermutation<T>(data, num_rows, num_cols, cfg->refinement_iterations);
  }
}

template <typename T>
MultisetCsfPtr<T> constructMultisetCsf(const build::Signatures &keys,
                                       std::vector<std::vector<T>> values,
                                       const MultisetConfig &config) {
  if (values.empty() || values[0].empty()) {
    throw std::invalid_argument("Values must be non-empty.");
  }
  if (config.permutation_config && values.size() > 1) {
    const size_t num_cols = values.size();
    const size_t num_rows = values[0].size();
    std::vector<T> buf(num_rows * num_cols);
    for (size_t c = 0; c < num_cols; c++) {
      for (size_t r = 0; r < num_rows; r++) {
        buf[r * num_cols + c] = values[c][r];
      }
    }
    permuteRowMajor<T>(buf.data(), num_rows, num_cols, config.permutation_config);
    return std::make_shared<MultisetCsf<T>>(build::buildLocal(
        multisetSpec(config), keys,
        build::Rows<T>::rowMajor(buf.data(), num_rows, num_cols)));
  }
  return std::make_shared<MultisetCsf<T>>(build::buildLocal(
      multisetSpec(config), keys, build::Rows<T>::columns(values)));
}

template <typename T>
MultisetCsfPtr<T> constructMultisetCsf(const std::vector<std::string> &keys,
                                       std::vector<std::vector<T>> values,
                                       const MultisetConfig &config) {
  return constructMultisetCsf<T>(build::hashKeys(keys), std::move(values), config);
}

template <typename T> struct MultisetConstructionResult {
  MultisetCsfPtr<T> csf;
  double permutation_seconds = 0.0;
  double build_seconds = 0.0;
};

// Builds from a row-major num_rows x num_cols buffer without copying it. The
// permutation, if any, is applied to `data` in place.
template <typename T>
MultisetConstructionResult<T>
constructMultisetCsfRowMajor(const build::Signatures &keys, T *data,
                             int num_rows, int num_cols,
                             const MultisetConfig &config) {
  MultisetConstructionResult<T> result;
  Timer timer;
  permuteRowMajor<T>(data, num_rows, num_cols, config.permutation_config);
  result.permutation_seconds = timer.seconds();
  result.csf = std::make_shared<MultisetCsf<T>>(build::buildLocal(
      multisetSpec(config), keys,
      build::Rows<T>::rowMajor(data, num_rows, num_cols)));
  result.build_seconds = timer.seconds();
  return result;
}

template <typename T>
MultisetConstructionResult<T>
constructMultisetCsfRowMajor(const std::vector<std::string> &keys, T *data,
                             int num_rows, int num_cols,
                             const MultisetConfig &config) {
  return constructMultisetCsfRowMajor<T>(build::hashKeys(keys), data, num_rows,
                                         num_cols, config);
}

} // namespace caramel
