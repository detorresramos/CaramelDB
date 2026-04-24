#pragma once

#include "src/construct/Construct.h"
#include "src/construct/multiset/ConstructMultiset.h"
#include "src/construct/multiset/RaggedMultisetCsf.h"

namespace caramel {

template <typename T>
RaggedMultisetCsfPtr<T>
constructRaggedMultisetCsf(const std::vector<std::string> &keys,
                           const std::vector<std::vector<T>> &values,
                           const MultisetConfig &config) {
  size_t num_keys = keys.size();

  // Compute lengths and max_length
  std::vector<uint32_t> lengths(num_keys);
  uint32_t max_length = 0;
  uint32_t min_length = std::numeric_limits<uint32_t>::max();
  for (size_t i = 0; i < num_keys; i++) {
    lengths[i] = values[i].size();
    max_length = std::max(max_length, lengths[i]);
    min_length = std::min(min_length, lengths[i]);
  }

  if (max_length == 0) {
    throw std::invalid_argument("All rows are empty.");
  }

  // Build length CSF
  CsfPtr<uint32_t> length_csf =
      constructCsf<uint32_t>(keys, lengths, config.filter_config, config.verbose);

  // Convert row-major ragged to column-major with per-column key subsets
  // Column c includes only keys where lengths[key] > c
  std::vector<std::vector<std::string>> col_keys(max_length);
  std::vector<std::vector<T>> col_values(max_length);

  for (size_t c = 0; c < max_length; c++) {
    for (size_t i = 0; i < num_keys; i++) {
      if (lengths[i] > c) {
        col_keys[c].push_back(keys[i]);
        col_values[c].push_back(values[i][c]);
      }
    }
  }

  // Optional permutation across all columns (ragged-aware)
  if (config.permutation_config && max_length > 1) {
    if (auto cfg = std::dynamic_pointer_cast<GlobalSortPermutationConfig>(
            config.permutation_config)) {
      int iters = cfg->refinement_iterations;

      // Build CSR-style flat buffer from ragged row-major data
      std::vector<int64_t> row_offsets(num_keys + 1, 0);
      for (size_t i = 0; i < num_keys; i++) {
        row_offsets[i + 1] = row_offsets[i] + lengths[i];
      }
      int64_t total = row_offsets[num_keys];
      std::vector<T> buf(total);

      // Column-major (col_values) → row-major flat buffer
      // col_values[c] only has entries for keys with length > c, so we need
      // to track per-column position.
      std::vector<size_t> col_pos(max_length, 0);
      for (size_t i = 0; i < num_keys; i++) {
        for (size_t c = 0; c < lengths[i]; c++) {
          buf[row_offsets[i] + c] = col_values[c][col_pos[c]];
          col_pos[c]++;
        }
      }

      globalSortPermutationRagged<T>(buf.data(), row_offsets.data(), num_keys,
                                     max_length, iters);

      // Write back: flat row-major buffer → column-major col_values
      std::fill(col_pos.begin(), col_pos.end(), 0);
      for (size_t i = 0; i < num_keys; i++) {
        for (size_t c = 0; c < lengths[i]; c++) {
          col_values[c][col_pos[c]] = buf[row_offsets[i] + c];
          col_pos[c]++;
        }
      }
    } else if (std::dynamic_pointer_cast<EntropyPermutationConfig>(
                   config.permutation_config)) {
      // Entropy permutation: fall back to min_length prefix (rectangular)
      if (min_length > 1) {
        std::vector<std::vector<T>> prefix_cols(col_values.begin(),
                                                col_values.begin() + min_length);
        applyPermutation(prefix_cols, entropyPermutation<T>);
        for (size_t c = 0; c < min_length; c++) {
          col_values[c] = std::move(prefix_cols[c]);
        }
      }
    }
  }

  // Optional shared codebook: pool all column values
  std::shared_ptr<CsfCodebook<T>> shared_cb;
  if (config.shared_codebook) {
    std::vector<T> pooled;
    for (size_t c = 0; c < max_length; c++) {
      pooled.insert(pooled.end(), col_values[c].begin(), col_values[c].end());
    }
    shared_cb = std::make_shared<CsfCodebook<T>>(canonicalHuffman<T>(pooled));
  }

  using Group = typename RaggedMultisetCsf<T>::Group;
  using GroupColumn = typename MultisetCsf<T>::GroupColumn;
  std::vector<Group> groups(max_length);

  for (size_t c = 0; c < max_length; c++) {
    const auto &active_keys = col_keys[c];
    const auto &active_values = col_values[c];

    // Per-column filter (not using shared filter for ragged — key sets differ)
    PreFilterPtr<T> col_filter;
    std::vector<std::string> filtered_keys;
    std::vector<T> filtered_values;
    std::optional<T> mcv;

    if (config.filter_config) {
      auto actual_config = config.filter_config;
      if (std::dynamic_pointer_cast<AutoPreFilterConfig>(config.filter_config)) {
        actual_config = selectBestFilter<T>(active_values);
      }
      if (actual_config) {
        col_filter = FilterFactory::makeFilter<T>(actual_config);
        col_filter->apply(active_keys, active_values, filtered_keys,
                          filtered_values, DELTA, config.verbose);
        mcv = col_filter->getMostCommonValue();
      }
    }

    bool using_filter = (col_filter != nullptr);
    const auto &build_keys = using_filter ? filtered_keys : active_keys;
    const auto &build_values = using_filter ? filtered_values : active_values;

    // Per-column codebook (or shared) + single-column group assembly.
    auto col_codebook = config.shared_codebook
        ? shared_cb
        : std::make_shared<CsfCodebook<T>>(
              build_values.empty()
                  ? CsfCodebook<T>{}
                  : canonicalHuffman<T>(build_values));

    Group group;
    GroupColumn gc;
    gc.output_index = static_cast<uint32_t>(c);
    gc.codebook = col_codebook;
    gc.filter = col_filter;
    gc.most_common_value = mcv;
    gc.uses_shared_codebook = config.shared_codebook;
    gc.max_codelength = col_codebook ? col_codebook->max_codelength : 0;

    if (build_keys.empty()) {
      group.hash_store_seed = 0;
      group.num_buckets = 0;
      group.arena.num_cols = 1;
      group.arena.num_buckets = 0;
      group.columns.push_back(std::move(gc));
      groups[c] = std::move(group);
      continue;
    }

    const CodeDict<T> &codedict = col_codebook->codedict;
    uint32_t max_cl = col_codebook->max_codelength;

    uint64_t num_buckets = targetBucketCount(build_values, codedict);

    BucketedHashStore<T> hash_store =
        partitionToBuckets<T>(build_keys, build_values, num_buckets);

    std::exception_ptr exception = nullptr;
    std::vector<SubsystemSolutionSeedPair> solutions_and_seeds(
        hash_store.num_buckets);

#pragma omp parallel for default(none)                                         \
    shared(hash_store, solutions_and_seeds, exception, codedict, max_cl, DELTA)
    for (uint32_t j = 0; j < hash_store.num_buckets; j++) {
      if (exception) {
        continue;
      }
      try {
        solutions_and_seeds[j] = constructAndSolveSubsystem<T>(
            hash_store.key_buckets[j], hash_store.value_buckets[j], codedict,
            max_cl, DELTA);
      } catch (std::exception &) {
#pragma omp critical
        { exception = std::current_exception(); }
      }
    }

    if (exception) {
      std::rethrow_exception(exception);
    }

    group.hash_store_seed = static_cast<uint32_t>(hash_store.seed);
    group.num_buckets = static_cast<uint32_t>(hash_store.num_buckets);

    std::vector<std::vector<SubsystemSolutionSeedPair>> per_col_solutions(1);
    per_col_solutions[0] = std::move(solutions_and_seeds);
    packArena<T>(group.arena, per_col_solutions,
                 static_cast<uint32_t>(hash_store.num_buckets));

    group.columns.push_back(std::move(gc));
    groups[c] = std::move(group);
  }

  return std::make_shared<RaggedMultisetCsf<T>>(std::move(length_csf),
                                                 std::move(groups));
}

} // namespace caramel
