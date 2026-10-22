#pragma once

#include "Plan.h"
#include "Solve.h"
#include "Index.h"

namespace caramel::build {

namespace detail {

inline BucketArena concatSolved(std::vector<SolvedPartition> solved, uint32_t num_cols,
                        uint32_t num_buckets) {
  std::sort(solved.begin(), solved.end(), [](const SolvedPartition &a, const SolvedPartition &b) {
    return a.partition.begin < b.partition.begin;
  });
  uint32_t expect = 0;
  for (const auto &s : solved) {
    if (s.partition.begin != expect) {
      throw std::invalid_argument("Solved partitions must tile each group's buckets.");
    }
    expect = s.partition.end;
  }
  if (expect != num_buckets) {
    throw std::invalid_argument("Solved partitions must tile each group's buckets.");
  }

  BucketArena arena;
  arena.num_cols = num_cols;
  arena.num_buckets = num_buckets;
  if (solved.size() == 1) {
    arena.solution_bits = std::move(solved[0].words);
    arena.per_col_vars = std::move(solved[0].vars);
    arena.per_col_seeds = std::move(solved[0].seeds);
  } else {
    for (auto &s : solved) {
      arena.per_col_vars.insert(arena.per_col_vars.end(), s.vars.begin(),
                                s.vars.end());
      arena.per_col_seeds.insert(arena.per_col_seeds.end(), s.seeds.begin(),
                                 s.seeds.end());
      arena.solution_bits.insert(arena.solution_bits.end(), s.words.begin(),
                                 s.words.end());
      s = SolvedPartition();
    }
  }
  // ribbonWindow reads one word past the last cell.
  arena.solution_bits.push_back(0);
  return arena;
}

} // namespace detail

// Concatenates the solved partitions (any order) into the final groups. Consumes
// the plan: the construction-only codedicts are dropped here.
template <typename T> Index<T> combine(Plan<T> plan, std::vector<SolvedPartition> solved) {
  Index<T> index;
  index.num_cols = plan.packed() ? 1 : plan.num_cols;
  index.stop_symbol = plan.stop_symbol;
  for (const auto &s : solved) {
    index.max_row_bits = std::max(index.max_row_bits, s.max_row_bits);
  }

  std::vector<std::vector<SolvedPartition>> by_group(plan.groups.size());
  for (auto &s : solved) {
    by_group.at(s.partition.group).push_back(std::move(s));
  }

  for (uint32_t g = 0; g < plan.groups.size(); g++) {
    const auto &gp = plan.groups[g];
    const uint32_t M = plan.packed() ? 1 : gp.columns.size();
    Group<T> group;
    if (gp.num_buckets > 0) {
      group.arena = detail::concatSolved(std::move(by_group[g]), M, gp.num_buckets);
    } else {
      group.arena.num_cols = M;
    }
    for (uint32_t m = 0; m < M; m++) {
      const uint32_t col = gp.columns[m];
      GroupColumn<T> gc;
      gc.output_index = col;
      gc.codebook = plan.codebooks[col];
      if (gp.filter >= 0) {
        gc.filter = plan.filters.filters[gp.filter];
        gc.most_common_value = gc.filter->getMostCommonValue();
      }
      gc.uses_shared_codebook = plan.spec.shared_codebook;
      gc.max_codelength = gc.codebook ? gc.codebook->max_codelength : 0;
      group.columns.push_back(std::move(gc));
    }
    index.groups.push_back(std::move(group));
  }

  for (auto &cb : plan.codebooks) {
    if (cb) {
      cb->codedict = CodeDict<T>();
      if (plan.spec.shared_codebook) {
        index.shared_codebook = cb;
      }
    }
  }
  return index;
}

} // namespace caramel::build
