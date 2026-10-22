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
  std::vector<uint32_t> vars;
  std::vector<uint32_t> seeds;
  std::vector<uint64_t> word_off;
  std::vector<uint64_t> words;
  uint32_t max_pad = 0;

  template <class Archive> void serialize(Archive &archive) {
    archive(partition, vars, seeds, word_off, words, max_pad);
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

inline bool hasDuplicate(const __uint128_t *sigs, size_t n) {
  std::vector<__uint128_t> sorted(sigs, sigs + n);
  std::sort(sorted.begin(), sorted.end());
  return std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end();
}

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
  part.word_off.resize(cells);
  std::vector<uint64_t> equations(cells);
  std::vector<uint32_t> pads(cells);

  std::exception_ptr error = nullptr;
#pragma omp parallel for schedule(dynamic, 16) default(none)                   \
    shared(piece, codes, part, equations, pads, error, M, nb)
  for (uint32_t b = 0; b < nb; b++) {
    const uint64_t begin = piece.offsets[b];
    const uint64_t end = piece.offsets[b + 1];
    try {
      if (detail::hasDuplicate(piece.sigs.data() + begin, end - begin)) {
        throw std::runtime_error("Detected a key collision under 128-bit hash. "
                                 "Likely due to a duplicate key.");
      }
      for (uint32_t m = 0; m < M; m++) {
        const auto &cb = codes.codebook(m);
        uint64_t eq = 0;
        uint32_t widest = 0;
        for (uint64_t i = begin; i < end; i++) {
          const uint32_t bits = codes.numBits(i, m, cb.codedict);
          eq += bits;
          widest = std::max(widest, bits);
        }
        const size_t idx = static_cast<size_t>(b) * M + m;
        equations[idx] = eq;
        // A query reads a max_codelength window starting anywhere in the row's
        // code stream, so the solution extends that far past the variables.
        pads[idx] = cb.max_codelength + (codes.plan.packed() ? widest : 0);
        part.vars[idx] = static_cast<uint32_t>(numVariables(eq));
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
    part.word_off[idx] = total_words;
    total_words += (static_cast<uint64_t>(part.vars[idx]) + pads[idx] + 63) / 64;
    part.max_pad = std::max(part.max_pad, pads[idx]);
  }
  // Room for the guard words combine() appends, so a single-part build moves
  // this buffer into the index without reallocating.
  part.words.reserve(total_words + 2 + part.max_pad / 64);
  part.words.resize(total_words, 0);

#pragma omp parallel for schedule(dynamic, 1) default(none)                    \
    shared(piece, codes, part, equations, pads, error, M, nb)
  for (uint32_t b = 0; b < nb; b++) {
    if (error) {
      continue;
    }
    const uint64_t begin = piece.offsets[b];
    const uint64_t n = piece.offsets[b + 1] - begin;
    try {
      for (uint32_t m = 0; m < M; m++) {
        const size_t idx = static_cast<size_t>(b) * M + m;
        const auto &dict = codes.codebook(m).codedict;
        auto [solution, seed] = solveCell(
            piece.sigs.data() + begin, n, equations[idx], pads[idx],
            [&](size_t i, auto &&add) { codes.emit(begin + i, m, dict, add); });
        part.seeds[idx] = seed;
        const uint64_t *src = solution->backingArrayPtr();
        const uint64_t num_words = (solution->numBits() + 63) / 64;
        std::copy(src, src + num_words, part.words.data() + part.word_off[idx]);
      }
    } catch (...) {
#pragma omp critical
      error = std::current_exception();
    }
  }
  if (error) {
    std::rethrow_exception(error);
  }
  return part;
}

} // namespace caramel::build
