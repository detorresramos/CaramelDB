#pragma once

#include "src/construct/multiset/ConstructMultiset.h"
#include "src/construct/multiset/RaggedMultisetCsf.h"

namespace caramel {

// Flattens rows into CSR: row r is values[offsets[r] .. offsets[r+1]).
template <typename T>
std::pair<std::vector<T>, std::vector<uint64_t>>
flattenRows(const std::vector<std::vector<T>> &rows) {
  std::vector<uint64_t> offsets(rows.size() + 1, 0);
  for (size_t r = 0; r < rows.size(); r++) {
    offsets[r + 1] = offsets[r] + rows[r].size();
  }
  std::vector<T> flat;
  flat.reserve(offsets.back());
  for (const auto &row : rows) {
    flat.insert(flat.end(), row.begin(), row.end());
  }
  return {std::move(flat), std::move(offsets)};
}

// Ragged rows are permuted in CSR form. Entropy permutation needs rectangular
// input, so it only permutes the columns every row has.
template <typename T>
void permuteRagged(std::vector<T> &flat, const std::vector<uint64_t> &offsets,
                   uint32_t max_length, const PermutationConfigPtr &config) {
  const int64_t n = static_cast<int64_t>(offsets.size()) - 1;
  if (!config || max_length <= 1) {
    return;
  }
  if (auto cfg = std::dynamic_pointer_cast<GlobalSortPermutationConfig>(config)) {
    std::vector<int64_t> row_offsets(offsets.begin(), offsets.end());
    globalSortPermutationRagged<T>(flat.data(), row_offsets.data(), n,
                                   max_length, cfg->refinement_iterations);
  } else if (std::dynamic_pointer_cast<EntropyPermutationConfig>(config)) {
    uint64_t min_length = max_length;
    for (int64_t r = 0; r < n; r++) {
      min_length = std::min(min_length, offsets[r + 1] - offsets[r]);
    }
    if (min_length <= 1) {
      return;
    }
    std::vector<T> prefix(n * min_length);
    for (int64_t r = 0; r < n; r++) {
      std::copy_n(flat.begin() + offsets[r], min_length,
                  prefix.begin() + r * min_length);
    }
    entropyPermutation<T>(prefix.data(), n, min_length);
    for (int64_t r = 0; r < n; r++) {
      std::copy_n(prefix.begin() + r * min_length, min_length,
                  flat.begin() + offsets[r]);
    }
  }
}

template <typename T>
RaggedMultisetCsfPtr<T>
constructRaggedMultisetCsf(const std::vector<std::string> &keys,
                           const std::vector<std::vector<T>> &values,
                           const MultisetConfig &config) {
  std::vector<uint32_t> lengths(values.size());
  uint32_t max_length = 0;
  for (size_t i = 0; i < values.size(); i++) {
    lengths[i] = values[i].size();
    max_length = std::max(max_length, lengths[i]);
  }
  if (max_length == 0) {
    throw std::invalid_argument("All rows are empty.");
  }

  CsfPtr<uint32_t> length_csf =
      constructCsf<uint32_t>(keys, lengths, config.filter_config, config.verbose);

  auto [flat, offsets] = flattenRows(values);
  permuteRagged(flat, offsets, max_length, config.permutation_config);

  build::BuildSpec spec = multisetSpec(config);
  spec.shared_filter = false;
  return std::make_shared<RaggedMultisetCsf<T>>(
      std::move(length_csf),
      build::buildLocal(spec, keys,
                        build::Rows<T>::ragged(flat.data(), offsets.data(),
                                               values.size())));
}

} // namespace caramel
