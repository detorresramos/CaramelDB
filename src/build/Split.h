#pragma once

#include "Plan.h"
#include "Rows.h"
#include <cereal/types/vector.hpp>
#include <src/construct/Hashing.h>

namespace caramel::build {

// The keys of one partition, grouped by bucket (CSR) and in input order
// within each bucket. Locally a piece only indexes the caller's rows; to cross
// a process boundary, pack() copies the values it needs into the piece.
template <typename T> struct Piece {
  Partition partition;
  std::vector<uint64_t> offsets; // partition.size() + 1; bucket b is [offsets[b], offsets[b+1])
  std::vector<__uint128_t> sigs;
  std::vector<uint32_t> rows;    // source row of each entry (until packed)
  const Rows<T> *source = nullptr;

  // Packed form: entry i's values, for the group's columns (Columns) or the
  // whole row (Packed encoding), at values[value_offsets[i] ..].
  std::vector<T> values;
  std::vector<uint64_t> value_offsets;

  bool isPacked() const { return source == nullptr; }
  size_t size() const { return sigs.size(); }

  uint32_t length(const Plan<T> &plan, size_t i) const {
    if (isPacked()) {
      return static_cast<uint32_t>(value_offsets[i + 1] - value_offsets[i]);
    }
    return plan.packed() ? source->length(rows[i])
                         : static_cast<uint32_t>(
                               plan.groups[partition.group].columns.size());
  }

  // Value k of entry i: the k-th group column, or the k-th value of the row.
  const T &value(const Plan<T> &plan, size_t i, uint32_t k) const {
    if (isPacked()) {
      return values[value_offsets[i] + k];
    }
    return plan.packed()
               ? source->at(rows[i], k)
               : source->at(rows[i], plan.groups[partition.group].columns[k]);
  }

  void pack(const Plan<T> &plan) {
    if (isPacked()) {
      return;
    }
    value_offsets.assign(1, 0);
    value_offsets.reserve(size() + 1);
    for (size_t i = 0; i < size(); i++) {
      const uint32_t len = length(plan, i);
      for (uint32_t k = 0; k < len; k++) {
        values.push_back(value(plan, i, k));
      }
      value_offsets.push_back(values.size());
    }
    rows = {};
    source = nullptr;
  }

  // Concatenates packed pieces of the same partition, bucket by bucket, in the
  // given order (pass pieces in input-piece order for a deterministic build).
  static Piece merge(std::vector<Piece> parts) {
    if (parts.empty()) {
      throw std::invalid_argument("Nothing to merge.");
    }
    Piece out;
    out.partition = parts[0].partition;
    const uint32_t nb = out.partition.size();
    out.offsets.assign(nb + 1, 0);
    out.value_offsets.assign(1, 0);
    for (const auto &p : parts) {
      if (!p.isPacked() || p.partition.group != out.partition.group ||
          p.partition.begin != out.partition.begin || p.partition.end != out.partition.end) {
        throw std::invalid_argument("Can only merge packed pieces of one partition.");
      }
    }
    for (uint32_t b = 0; b < nb; b++) {
      for (const auto &p : parts) {
        for (uint64_t i = p.offsets[b]; i < p.offsets[b + 1]; i++) {
          out.sigs.push_back(p.sigs[i]);
          out.values.insert(out.values.end(),
                            p.values.begin() + p.value_offsets[i],
                            p.values.begin() + p.value_offsets[i + 1]);
          out.value_offsets.push_back(out.values.size());
        }
      }
      out.offsets[b + 1] = out.sigs.size();
    }
    return out;
  }

  template <class Archive> void save(Archive &archive) const {
    if (!isPacked()) {
      throw std::logic_error("pack() a piece before serializing it.");
    }
    archive(partition, offsets, cereal::binary_data(sigs.data(), sigs.size() * 16),
            values, value_offsets);
  }

  template <class Archive> void load(Archive &archive) {
    archive(partition, offsets);
    sigs.resize(offsets.back());
    archive(cereal::binary_data(sigs.data(), sigs.size() * 16), values,
            value_offsets);
  }
};

// Groups each requested partition's keys by bucket: keep the keys its group
// encodes (row present, filter passes), and counting-sort them
// into their bucket's slot. Ranges may come from any groups; a local build asks
// for one whole group at a time, a worker asks for every partition at once.
template <typename T>
std::vector<Piece<T>> split(const Plan<T> &plan, const Signatures &sigs,
                                  const Rows<T> &rows,
                                  const std::vector<Partition> &partitions) {
  const uint64_t n = rows.numRows();
  if (sigs.size() != n) {
    throw std::invalid_argument("Keys and values must have the same length.");
  }
  if (n > std::numeric_limits<uint32_t>::max()) {
    throw std::invalid_argument("A piece may hold at most 2^32 - 1 rows.");
  }

  // Slot = position of (partition, bucket) in the concatenation of all partitions.
  // slot_of[g][b] is the slot of bucket b of group g, or -1 if unrequested.
  std::vector<std::vector<int64_t>> slot_of(plan.groups.size());
  std::vector<uint32_t> groups_used;
  uint64_t num_slots = 0;
  for (const auto &partition : partitions) {
    auto &slots = slot_of[partition.group];
    if (slots.empty()) {
      slots.assign(plan.groups[partition.group].num_buckets, -1);
      groups_used.push_back(partition.group);
    }
    for (uint32_t b = partition.begin; b < partition.end; b++) {
      slots[b] = static_cast<int64_t>(num_slots++);
    }
  }

  // Calls visit(row, slot, sig) for every kept key of rows [begin, end).
  auto forEachKept = [&](uint64_t begin, uint64_t end, auto &&visit) {
    for (uint64_t r = begin; r < end; r++) {
      const __uint128_t sig = sigs[r];
      const uint64_t fh = filterHash(sig);
      for (uint32_t g : groups_used) {
        const auto &group = plan.groups[g];
        if (!plan.packed() && !rows.present(r, group.columns[0])) {
          continue;
        }
        if (group.filter >= 0 &&
            !plan.filters.filters[group.filter]->containsHash(fh)) {
          continue;
        }
        const int64_t slot = slot_of[g][getBucketID(sig, group.num_buckets)];
        if (slot >= 0) {
          visit(r, static_cast<uint64_t>(slot), sig);
        }
      }
    }
  };

  // Two passes over fixed row blocks (count, then place) keep every bucket in
  // input order with no per-row buffer.
  const size_t blocks = static_cast<size_t>(omp_get_max_threads());
  std::vector<std::vector<uint64_t>> counts(blocks,
                                            std::vector<uint64_t>(num_slots, 0));
#pragma omp parallel for schedule(static, 1) default(none)                     \
    shared(counts, forEachKept, n, blocks)
  for (size_t block = 0; block < blocks; block++) {
    auto &local = counts[block];
    forEachKept(n * block / blocks, n * (block + 1) / blocks,
                  [&](uint64_t, uint64_t slot, const __uint128_t &) {
                    local[slot]++;
                  });
  }

  // slot_start[s] = first position of slot s; counts become each block's
  // write cursor within its slot.
  std::vector<uint64_t> slot_start(num_slots + 1, 0);
  for (uint64_t s = 0; s < num_slots; s++) {
    uint64_t pos = slot_start[s];
    for (size_t block = 0; block < blocks; block++) {
      uint64_t c = counts[block][s];
      counts[block][s] = pos;
      pos += c;
    }
    slot_start[s + 1] = pos;
  }

  std::vector<__uint128_t> sorted(slot_start[num_slots]);
  std::vector<uint32_t> row_ids(slot_start[num_slots]);
#pragma omp parallel for schedule(static, 1) default(none)                     \
    shared(counts, forEachKept, sorted, row_ids, n, blocks)
  for (size_t block = 0; block < blocks; block++) {
    auto &cursor = counts[block];
    forEachKept(n * block / blocks, n * (block + 1) / blocks,
                  [&](uint64_t r, uint64_t slot, const __uint128_t &sig) {
                    const uint64_t pos = cursor[slot]++;
                    sorted[pos] = sig;
                    row_ids[pos] = static_cast<uint32_t>(r);
                  });
  }
  counts = {};

  std::vector<Piece<T>> pieces(partitions.size());
  uint64_t slot = 0;
  for (size_t k = 0; k < partitions.size(); k++) {
    auto &piece = pieces[k];
    piece.partition = partitions[k];
    piece.source = &rows;
    const uint64_t first = slot_start[slot];
    const uint64_t last = slot_start[slot + partitions[k].size()];
    piece.offsets.resize(partitions[k].size() + 1);
    for (uint32_t b = 0; b <= partitions[k].size(); b++) {
      piece.offsets[b] = slot_start[slot + b] - first;
    }
    if (partitions.size() == 1) {
      piece.sigs = std::move(sorted);
      piece.rows = std::move(row_ids);
    } else {
      piece.sigs.assign(sorted.begin() + first, sorted.begin() + last);
      piece.rows.assign(row_ids.begin() + first, row_ids.begin() + last);
    }
    slot += partitions[k].size();
  }
  return pieces;
}

} // namespace caramel::build
