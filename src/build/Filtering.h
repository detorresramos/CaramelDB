#pragma once

#include "Filters.h"
#include "Counts.h"
#include <src/construct/filter/AutoFilterSelection.h>
#include <src/construct/filter/FilterFactory.h>

namespace caramel::build {

// One prefilter to build. `columns` share it: a key is an exception of the
// filter iff any of those columns holds a value other than the MCV. A
// per-column filter is the one-column case; shared_filter only changes how
// columns are assigned to specs.
template <typename T> struct FilterSpec {
  std::vector<uint32_t> columns;
  T most_common_value{};
  uint64_t universe = 0;   // keys the filter decides for
  uint64_t num_unique = 0; // distinct values seen by Auto selection

  template <class Archive> void serialize(Archive &archive) {
    archive(columns, most_common_value, universe, num_unique);
  }
};

template <typename T> struct FilterChoice {
  FilterType choice;
  uint32_t num_cols = 0;
  bool verbose = false;
  std::vector<FilterSpec<T>> specs;

  template <class Archive> void serialize(Archive &archive) {
    archive(choice, num_cols, verbose, specs);
  }
};

// The exception-key hashes of every spec. Mergeable by concatenation; merging
// pieces in input order reproduces a single-piece build exactly.
struct Exceptions {
  std::vector<std::vector<uint64_t>> per_spec;

  void append(Exceptions &&later) {
    if (per_spec.empty()) {
      per_spec = std::move(later.per_spec);
      return;
    }
    for (size_t s = 0; s < per_spec.size(); s++) {
      per_spec[s].insert(per_spec[s].end(), later.per_spec[s].begin(),
                         later.per_spec[s].end());
    }
  }

  template <class Archive> void serialize(Archive &archive) {
    archive(per_spec);
  }
};

template <typename T>
FilterChoice<T> chooseFilters(const BuildSpec &spec, const Counts<T> &counts) {
  FilterChoice<T> plan;
  plan.choice = spec.filter;
  plan.num_cols = counts.num_cols;
  plan.verbose = spec.verbose;
  if (spec.filter.kind == FilterType::None) {
    return plan;
  }
  if (spec.encoding == Encoding::Packed) {
    throw std::invalid_argument("Prefilters are not supported for packed CSFs.");
  }

  if (spec.shared_filter) {
    if (counts.ragged) {
      throw std::invalid_argument(
          "Shared filters are not supported for ragged values.");
    }
    for (uint32_t c = 0; c < counts.num_cols; c++) {
      T mcv = counts.cols[c].mostCommon().first;
      auto it = std::find_if(plan.specs.begin(), plan.specs.end(),
                             [&](const auto &s) { return s.most_common_value == mcv; });
      if (it == plan.specs.end()) {
        // Auto sees the shared filter as a two-valued column: MCV vs. not.
        plan.specs.push_back({{}, mcv, counts.num_rows, 2});
        it = plan.specs.end() - 1;
      }
      it->columns.push_back(c);
    }
    return plan;
  }

  for (uint32_t c = 0; c < counts.num_cols; c++) {
    const auto &col = counts.cols[c];
    plan.specs.push_back(
        {{c}, col.mostCommon().first, col.count, col.freq.size()});
  }
  return plan;
}

template <typename T>
Exceptions findExceptions(const FilterChoice<T> &plan, const Signatures &sigs,
                             const Rows<T> &rows) {
  const uint64_t n = rows.numRows();
  const size_t S = plan.specs.size();
  const size_t blocks = static_cast<size_t>(omp_get_max_threads()) * 4;
  std::vector<Exceptions> partial(blocks);

#pragma omp parallel for schedule(dynamic, 1) default(none)                    \
    shared(plan, sigs, rows, partial, n, S, blocks)
  for (size_t block = 0; block < blocks; block++) {
    auto &out = partial[block].per_spec;
    out.resize(S);
    const uint64_t begin = n * block / blocks;
    const uint64_t end = n * (block + 1) / blocks;
    for (uint64_t r = begin; r < end; r++) {
      uint64_t h = 0;
      bool hashed = false;
      for (size_t s = 0; s < S; s++) {
        const auto &fs = plan.specs[s];
        bool exception = false;
        for (uint32_t c : fs.columns) {
          if (rows.present(r, c) && !(rows.at(r, c) == fs.most_common_value)) {
            exception = true;
            break;
          }
        }
        if (exception) {
          if (!hashed) {
            h = filterHash(sigs[r]);
            hashed = true;
          }
          out[s].push_back(h);
        }
      }
    }
  }

  Exceptions merged;
  merged.per_spec.resize(S);
  for (auto &p : partial) {
    merged.append(std::move(p));
  }
  return merged;
}

// Builds every spec's filter. Auto resolves here, once the exception count is
// known; a spec that resolves to "no filter" leaves its columns unfiltered.
template <typename T>
Filters<T> buildFilters(const FilterChoice<T> &plan, Exceptions keys) {
  Filters<T> out;
  out.column_filter.assign(plan.num_cols, -1);
  for (size_t s = 0; s < plan.specs.size(); s++) {
    const auto &fs = plan.specs[s];
    auto &hashes = keys.per_spec[s];
    PreFilterConfigPtr config = plan.choice.config();
    if (plan.choice.kind == FilterType::Auto) {
      config = selectBestFilter(fs.universe, fs.universe - hashes.size(),
                                fs.num_unique, plan.verbose);
    }
    if (!config) {
      continue;
    }
    out.filters.push_back(FilterFactory::build<T>(
        config, fs.most_common_value, std::move(hashes), plan.verbose));
    for (uint32_t c : fs.columns) {
      out.column_filter[c] = static_cast<int32_t>(out.filters.size() - 1);
    }
  }
  return out;
}

} // namespace caramel::build
