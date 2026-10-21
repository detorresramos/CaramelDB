#pragma once

// Byte-string facade over src/build for carameldb.steps: every step result
// crosses into Python as opaque bytes, so it pickles for free.

#include <src/build/Build.h>
#include <src/build/Bytes.h>
#include <src/construct/Csf.h>
#include <src/construct/multiset/MultisetCsf.h>
#include <src/construct/multiset/PackedCsf.h>
#include <src/construct/multiset/RaggedMultisetCsf.h>

namespace caramel::py {

using namespace caramel::build;
using Blobs = std::vector<std::string>;

// Row-major values (offsets == nullptr) or CSR rows.
template <typename T>
Rows<T> view(const T *data, uint64_t rows, uint32_t cols,
             const uint64_t *offsets) {
  return offsets ? Rows<T>::ragged(data, offsets, rows)
                 : Rows<T>::rowMajor(data, rows, cols);
}

template <typename Obj> Obj maybeFromBytes(const std::string &bytes) {
  return bytes.empty() ? Obj() : fromBytes<Obj>(bytes);
}

inline std::string makeSpec(PreFilterConfigPtr filter, bool shared_filter,
                            bool shared_codebook, bool packed, bool verbose) {
  BuildSpec spec;
  spec.filter = FilterType::from(filter);
  spec.shared_filter = shared_filter;
  spec.shared_codebook = shared_codebook;
  spec.encoding = packed ? Encoding::Packed : Encoding::Columns;
  spec.verbose = verbose;
  return toBytes(spec);
}

template <typename T>
std::string countStep(const std::string &spec, const Keys &keys, const T *data,
                     uint64_t rows, uint32_t cols, const uint64_t *offsets,
                     const std::string &filters) {
  auto f = maybeFromBytes<Filters<T>>(filters);
  return toBytes(countValues(fromBytes<BuildSpec>(spec), keys,
                     view(data, rows, cols, offsets),
                     filters.empty() ? nullptr : &f));
}

template <typename T> std::string mergeCounts(const Blobs &parts) {
  Counts<T> merged = fromBytes<Counts<T>>(parts.at(0));
  for (size_t i = 1; i < parts.size(); i++) {
    merged.merge(fromBytes<Counts<T>>(parts[i]));
  }
  return toBytes(merged);
}

template <typename T>
std::string chooseFiltersStep(const std::string &spec, const std::string &counts,
                             uint32_t *num_specs) {
  auto plan = chooseFilters(fromBytes<BuildSpec>(spec), fromBytes<Counts<T>>(counts));
  *num_specs = plan.specs.size();
  return toBytes(plan);
}

template <typename T>
std::string findExceptionsStep(const std::string &choice,
                                   const Keys &keys, const T *data,
                                   uint64_t rows, uint32_t cols,
                                   const uint64_t *offsets) {
  return toBytes(findExceptions(fromBytes<FilterChoice<T>>(choice), keys,
                                   view(data, rows, cols, offsets)));
}

inline std::string mergeExceptions(const Blobs &parts) {
  Exceptions merged;
  for (const auto &p : parts) {
    merged.append(fromBytes<Exceptions>(p));
  }
  return toBytes(merged);
}

template <typename T>
std::string buildFiltersStep(const std::string &choice,
                              const std::string &keys, uint32_t *num_filters) {
  auto filters = buildFilters(fromBytes<FilterChoice<T>>(choice),
                              fromBytes<Exceptions>(keys));
  *num_filters = filters.filters.size();
  return toBytes(filters);
}

template <typename T>
std::string makePlanStep(const std::string &spec, const std::string &counts,
                          const std::string &filters) {
  return toBytes(makePlan(fromBytes<BuildSpec>(spec), fromBytes<Counts<T>>(counts),
                          maybeFromBytes<Filters<T>>(filters)));
}

// Ranges travel as flat (group, begin, end) triples.
template <typename T>
std::vector<uint32_t> planPartitions(const std::string &plan, uint32_t count) {
  std::vector<uint32_t> flat;
  for (const auto &r : fromBytes<Plan<T>>(plan).partitions(count)) {
    flat.insert(flat.end(), {r.group, r.begin, r.end});
  }
  return flat;
}

template <typename T>
Blobs splitStep(const std::string &plan_bytes, const Keys &keys, const T *data,
                 uint64_t rows, uint32_t cols, const uint64_t *offsets,
                 const std::vector<uint32_t> &flat_partitions) {
  auto plan = fromBytes<Plan<T>>(plan_bytes);
  std::vector<Partition> partitions;
  for (size_t i = 0; i + 2 < flat_partitions.size(); i += 3) {
    partitions.push_back({flat_partitions[i], flat_partitions[i + 1], flat_partitions[i + 2]});
  }
  auto source = view(data, rows, cols, offsets);
  Blobs out;
  for (auto &piece : split(plan, keys, source, partitions)) {
    piece.pack(plan);
    out.push_back(toBytes(piece));
  }
  return out;
}

// Merges one partition's pieces (in chunk order) and solves them.
template <typename T>
std::string solveStep(const std::string &plan_bytes, const Blobs &parts) {
  auto plan = fromBytes<Plan<T>>(plan_bytes);
  std::vector<Piece<T>> pieces;
  for (const auto &p : parts) {
    pieces.push_back(fromBytes<Piece<T>>(p));
  }
  return toBytes(solve(plan, Piece<T>::merge(std::move(pieces))));
}

template <typename T>
Index<T> combineIndex(const std::string &plan, const Blobs &solved_bytes) {
  std::vector<SolvedPartition> solved;
  for (const auto &s : solved_bytes) {
    solved.push_back(fromBytes<SolvedPartition>(s));
  }
  return combine(fromBytes<Plan<T>>(plan), std::move(solved));
}

template <typename T>
std::shared_ptr<Csf<T>> combineCsf(const std::string &plan, const Blobs &solved) {
  return std::make_shared<Csf<T>>(combineIndex<T>(plan, solved));
}

template <typename T>
std::shared_ptr<MultisetCsf<T>> combineMultiset(const std::string &plan,
                                                 const Blobs &solved) {
  return std::make_shared<MultisetCsf<T>>(combineIndex<T>(plan, solved));
}

template <typename T>
std::shared_ptr<PackedCsf<T>> combinePacked(const std::string &plan,
                                             const Blobs &solved) {
  return std::make_shared<PackedCsf<T>>(combineIndex<T>(plan, solved));
}

template <typename T>
std::shared_ptr<RaggedMultisetCsf<T>>
combineRagged(std::shared_ptr<Csf<uint32_t>> lengths, const std::string &plan,
               const Blobs &solved) {
  return std::make_shared<RaggedMultisetCsf<T>>(std::move(lengths),
                                                combineIndex<T>(plan, solved));
}

} // namespace caramel::py
