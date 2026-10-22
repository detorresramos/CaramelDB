#pragma once

#include "Kernel.h"
#include "Plan.h"
#include "Split.h"
#include <exception>

namespace caramel::build {

// The solved cells of one partition: partition.size() x M cells, bucket-major,
// each starting on a word boundary. Offsets are relative to this part.
struct SolvedPartition {
  Partition partition;
  std::vector<uint32_t> vars; // band start positions of each cell
  std::vector<uint8_t> seeds;
  std::vector<uint64_t> words;
  uint32_t max_row_bits = 0; // Packed: the longest code stream of any row

  template <class Archive> void serialize(Archive &archive) {
    archive(partition, vars, seeds, words, max_row_bits);
  }
};

namespace detail {

// The code stream of piece entry i for group column m: one codeword
// (Columns), or the row's codewords followed by STOP (Packed).
template <typename T> struct EntryCodes {
  const Plan<T> &plan;
  const Piece<T> &piece;

  const CsfCodebook<T> &codebook(uint32_t m) const {
    const auto &group = plan.groups[piece.partition.group];
    return *plan.codebooks[plan.packed() ? 0 : group.columns[m]];
  }

  uint32_t numBits(size_t i, uint32_t m, const CodeDict<T> &dict) const {
    if (!plan.packed()) {
      return dict.at(piece.value(plan, i, m)).length;
    }
    uint32_t bits = dict.at(plan.stop_symbol).length;
    for (uint32_t k = 0, len = piece.length(plan, i); k < len; k++) {
      bits += dict.at(piece.value(plan, i, k)).length;
    }
    return bits;
  }

  template <typename Add>
  void emit(size_t i, uint32_t m, const CodeDict<T> &dict, Add &&add) const {
    auto emitCode = [&](const Code &code, uint32_t offset) {
      for (uint32_t j = 0; j < code.length; j++) {
        add(offset + j, code[j]);
      }
      return offset + code.length;
    };
    if (!plan.packed()) {
      emitCode(dict.at(piece.value(plan, i, m)), 0);
      return;
    }
    uint32_t offset = 0;
    for (uint32_t k = 0, len = piece.length(plan, i); k < len; k++) {
      offset = emitCode(dict.at(piece.value(plan, i, k)), offset);
    }
    emitCode(dict.at(plan.stop_symbol), offset);
  }
};

} // namespace detail

// Solves every cell of a piece: a sizing pass fixes each cell's width (so the
// output is allocated once, at its final size), then each bucket's cells are
// solved straight into place. Buckets are independent; so are pieces.
template <typename T>
SolvedPartition solve(const Plan<T> &plan, const Piece<T> &piece) {
  const auto &group = plan.groups[piece.partition.group];
  const uint32_t M = plan.packed() ? 1 : group.columns.size();
  const uint32_t nb = piece.partition.size();
  const size_t cells = static_cast<size_t>(nb) * M;
  const detail::EntryCodes<T> codes{plan, piece};

  SolvedPartition part;
  part.partition = piece.partition;
  part.vars.resize(cells);
  part.seeds.resize(cells);
  std::vector<uint64_t> word_off(cells);
  std::vector<uint32_t> widest(cells);

  std::exception_ptr error = nullptr;
#pragma omp parallel for schedule(dynamic, 16) default(none)                   \
    shared(plan, piece, codes, part, widest, error, M, nb)
  for (uint32_t b = 0; b < nb; b++) {
    const uint64_t begin = piece.offsets[b];
    const uint64_t end = piece.offsets[b + 1];
    try {
      for (uint32_t m = 0; m < M; m++) {
        const auto &cb = codes.codebook(m);
        uint64_t eq = 0;
        uint32_t row_bits = 0;
        for (uint64_t i = begin; i < end; i++) {
          const uint32_t bits = codes.numBits(i, m, cb.codedict);
          eq += bits;
          row_bits = std::max(row_bits, bits);
        }
        const size_t idx = static_cast<size_t>(b) * M + m;
        widest[idx] = row_bits;
        part.vars[idx] = ribbonStarts(eq, plan.spec.ribbon_overhead);
      }
    } catch (...) {
#pragma omp critical
      error = std::current_exception();
    }
  }
  if (error) {
    std::rethrow_exception(error);
  }

  uint64_t total_words = 0;
  for (size_t idx = 0; idx < cells; idx++) {
    word_off[idx] = total_words;
    total_words += cellWords(part.vars[idx]);
    part.max_row_bits = std::max(part.max_row_bits, widest[idx]);
  }
  // Room for the guard word combine() appends, so a single-part build moves
  // this buffer into the index without reallocating.
  part.words.reserve(total_words + 1);
  part.words.resize(total_words, 0);

  // Solves cell idx in place; false if it needs more band starts.
  auto solveInPlace = [&](size_t idx) {
    const uint32_t b = static_cast<uint32_t>(idx / M);
    const uint32_t m = static_cast<uint32_t>(idx % M);
    const uint64_t begin = piece.offsets[b];
    const auto &dict = codes.codebook(m).codedict;
    const auto seed = solveCell(
        piece.sigs.data() + begin, piece.offsets[b + 1] - begin, part.vars[idx],
        part.words.data() + word_off[idx],
        [&](size_t i, auto &&add) { codes.emit(begin + i, m, dict, add); });
    part.seeds[idx] = seed.value_or(0);
    return seed.has_value();
  };

  std::vector<uint8_t> unsolved(cells, 0);
#pragma omp parallel for schedule(dynamic, 1) default(none)                    \
    shared(solveInPlace, unsolved, error, M, nb)
  for (uint32_t b = 0; b < nb; b++) {
    if (error) {
      continue;
    }
    try {
      for (uint32_t m = 0; m < M; m++) {
        const size_t idx = static_cast<size_t>(b) * M + m;
        unsolved[idx] = !solveInPlace(idx);
      }
    } catch (...) {
#pragma omp critical
      error = std::current_exception();
    }
  }
  if (error) {
    std::rethrow_exception(error);
  }

  // Rare: a cell no seed solves gets more band starts. The layout moves, so
  // solved cells are copied to their new offsets and the rest re-solved.
  while (std::find(unsolved.begin(), unsolved.end(), 1) != unsolved.end()) {
    std::vector<uint64_t> old_words = std::move(part.words);
    const std::vector<uint64_t> old_off = word_off;
    total_words = 0;
    for (size_t idx = 0; idx < cells; idx++) {
      if (unsolved[idx]) {
        part.vars[idx] = grownStarts(part.vars[idx]);
      }
      word_off[idx] = total_words;
      total_words += cellWords(part.vars[idx]);
    }
    part.words.assign(total_words, 0);
    part.words.reserve(total_words + 1);
    for (size_t idx = 0; idx < cells; idx++) {
      if (unsolved[idx]) {
        unsolved[idx] = !solveInPlace(idx);
      } else {
        std::copy_n(old_words.data() + old_off[idx], cellWords(part.vars[idx]),
                    part.words.data() + word_off[idx]);
      }
    }
  }
  return part;
}

} // namespace caramel::build
