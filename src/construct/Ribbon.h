#pragma once

#include <cstdint>

namespace caramel {

// Each cell stores one solution vector Z of starts + RIBBON_WIDTH - 1 bits.
// Code bit j of a key is the parity of a 64-bit band of Z: the band starts at
// (start + j * stride) mod starts and has the key's j-th coefficient. Spreading
// a key's bits across the cell (rather than at start, start + 1, ...) keeps
// the load of every band window even, which is what makes the banded system
// solvable at a few percent overhead.
static constexpr uint32_t RIBBON_WIDTH = 64;

// Solution bits per equation. Measured on cells of 1k-6k equations: solvable
// on the first seed in >= 98% of cells for every code length distribution.
static constexpr double RIBBON_OVERHEAD = 1.05;

inline uint64_t ribbonMix(uint64_t k) {
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccdULL;
  k ^= k >> 33;
  k *= 0xc4ceb9fe1a85ec53ULL;
  k ^= k >> 33;
  return k;
}

// The part of a key's equations that is the same in every cell: computed
// once per query, while the cells load.
struct RibbonHash {
  uint64_t x, y;

  // Mixed because the bucket id fixes the top bits of lo, and the filter
  // probe uses hi.
  explicit RibbonHash(const __uint128_t &signature)
      : x(ribbonMix(static_cast<uint64_t>(signature))),
        y(ribbonMix(static_cast<uint64_t>(signature >> 64))) {}
};

// One key's equations in one cell.
struct RibbonKey {
  uint32_t start;
  uint32_t stride;
  uint32_t starts;
  uint64_t a;

  RibbonKey(const RibbonHash &h, uint32_t seed, uint32_t num_starts)
      : starts(num_starts) {
    const uint64_t s = (seed + 1) * 0x9E3779B97F4A7C15ULL;
    start = static_cast<uint32_t>(
        (static_cast<__uint128_t>((h.x + h.y) ^ s) * starts) >> 64);
    stride = static_cast<uint32_t>((uint64_t{starts} * 0x9E3779B9ULL) >> 32);
    a = h.x ^ s;
  }

  // Bit 0 is always set: it is the band's leading (pivot) position.
  uint64_t coefficient(uint32_t j) const { return a * (2 * uint64_t{j} + 1) | 1; }

  uint32_t position(uint32_t j) const {
    return static_cast<uint32_t>((start + uint64_t{j} * stride) % starts);
  }

  uint32_t next(uint32_t pos) const {
    pos += stride;
    return pos >= starts ? pos - starts : pos;
  }
};

// Words of a cell's solution.
inline uint64_t cellWords(uint32_t starts) {
  return (uint64_t{starts} + RIBBON_WIDTH - 1 + 63) / 64;
}

// Bits [pos, pos + 64) of Z, bit i of Z being bit i % 64 of word i / 64.
// Reads words[pos / 64 + 1], so the solution needs one word past its end.
inline uint64_t __attribute__((always_inline))
ribbonWindow(const uint64_t *words, uint32_t pos) {
  const uint64_t *w = words + pos / 64;
  const unsigned s = pos % 64;
  return (w[0] >> s) | ((w[1] << 1) << (63 - s));
}

// Reads a key's code bits in order.
struct RibbonReader {
  const uint64_t *words;
  RibbonKey key;
  uint32_t pos;
  uint64_t c; // the next bit's coefficient, without the | 1

  RibbonReader(const uint64_t *data, const RibbonHash &hash, uint32_t seed,
               uint32_t starts)
      : words(data), key(hash, seed, starts), pos(key.start), c(key.a) {}

  uint32_t __attribute__((always_inline)) next() {
    const uint32_t bit = __builtin_parityll(ribbonWindow(words, pos) & (c | 1));
    c += 2 * key.a;
    pos = key.next(pos);
    return bit;
  }
};

} // namespace caramel
