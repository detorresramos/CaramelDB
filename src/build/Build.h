#pragma once

// The steps of a build. Every index type is built by the same steps, each a
// pure function of serializable inputs, so a driver can run them in one
// process (buildLocal below) or spread them over machines. Vocabulary:
//   chunk      a slice of the input rows, loaded by one worker
//   partition  a run of one group's buckets, solved as one unit of work
//   piece      the rows of one chunk that belong to one partition
//
//   countValues          chunk               -> Counts        each chunk; Counts::merge
//   chooseFilters        Counts              -> FilterChoice  once (with a prefilter)
//   findExceptions       chunk               -> Exceptions    each chunk; Exceptions::append
//   buildFilters         Exceptions          -> Filters       once
//   countValues(filters) chunk               -> Counts        each chunk (if filtered)
//   makePlan             Counts, Filters     -> Plan          once; sent to every worker
//   split                chunk, partitions   -> Piece[]       each chunk
//   solve                Piece (merged)      -> SolvedPartition  each partition
//   combine              Plan, solved        -> Index         once

#include "Combine.h"
#include "Counts.h"
#include "Filtering.h"
#include "Plan.h"
#include "Solve.h"
#include "Split.h"
#include <iostream>
#include <src/utils/Timer.h>

namespace caramel::build {

// The plan for rows held in this process: count, then (when filtering) build
// the filters and recount the rows they pass.
template <typename T>
Plan<T> planLocal(const BuildSpec &spec, const Keys &keys, const Rows<T> &rows) {
  Counts<T> counts = countValues(spec, keys, rows);
  Filters<T> filters;
  if (spec.filter.kind != FilterType::None) {
    FilterChoice<T> choice = chooseFilters(spec, counts);
    filters = buildFilters(choice, findExceptions(choice, keys, rows));
    if (!filters.empty()) {
      counts = countValues(spec, keys, rows, &filters);
    }
  }
  return makePlan(spec, counts, std::move(filters));
}

// Builds in this process. Groups are split and solved one at a time so only
// one group's piece and codedicts are alive at once.
template <typename T>
Index<T> buildLocal(const BuildSpec &spec, const Keys &keys, const Rows<T> &rows) {
  Timer timer;
  Plan<T> plan = planLocal(spec, keys, rows);
  if (spec.verbose) {
    std::cout << "Planned " << plan.groups.size() << " groups in "
              << timer.seconds() << "s" << std::endl;
  }
  std::vector<SolvedPartition> solved;
  for (uint32_t g = 0; g < plan.groups.size(); g++) {
    const uint32_t nb = plan.groups[g].num_buckets;
    if (nb == 0) {
      continue;
    }
    auto pieces = split(plan, keys, rows, {{g, 0, nb}});
    solved.push_back(solve(plan, pieces[0]));
    plan.releaseCodedicts(g);
  }
  if (spec.verbose) {
    std::cout << "Solved in " << timer.seconds() << "s" << std::endl;
  }
  return combine(std::move(plan), std::move(solved));
}

} // namespace caramel::build
