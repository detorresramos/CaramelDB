#include <gtest/gtest.h>
#include <optional>
#include <random>
#include <src/build/Kernel.h>
#include <vector>

namespace caramel::tests {

using build::ribbonStarts;
using build::solveCell;

struct Cell {
  std::vector<__uint128_t> sigs;
  std::vector<std::vector<uint8_t>> bits;
  uint64_t equations = 0;
};

Cell randomCell(uint64_t target_equations, uint32_t min_len, uint32_t max_len,
                uint64_t seed) {
  std::mt19937_64 rng(seed);
  std::uniform_int_distribution<uint32_t> len(min_len, max_len);
  Cell cell;
  while (cell.equations < target_equations) {
    cell.sigs.push_back(static_cast<__uint128_t>(rng()) << 64 | rng());
    std::vector<uint8_t> code(len(rng));
    for (auto &b : code) {
      b = rng() & 1;
    }
    cell.equations += code.size();
    cell.bits.push_back(std::move(code));
  }
  return cell;
}

// Solves with `starts` band starts, growing them until some seed solves, and
// returns the final starts.
uint32_t solveAndCheck(const Cell &cell, uint32_t starts) {
  std::vector<uint64_t> words;
  std::optional<uint8_t> seed;
  for (;; starts = build::grownStarts(starts)) {
    words.assign(cellWords(starts) + 1, 0);
    seed = solveCell(cell.sigs.data(), cell.sigs.size(), starts, words.data(),
                     [&](size_t i, auto &&add) {
                       for (uint32_t j = 0; j < cell.bits[i].size(); j++) {
                         add(j, cell.bits[i][j]);
                       }
                     });
    if (seed) {
      break;
    }
  }
  for (size_t i = 0; i < cell.sigs.size(); i++) {
    RibbonReader reader(words.data(), RibbonHash(cell.sigs[i]), *seed, starts);
    for (uint32_t j = 0; j < cell.bits[i].size(); j++) {
      EXPECT_EQ(reader.next(), cell.bits[i][j]) << "key " << i << " bit " << j;
    }
  }
  return starts;
}

void solveAndCheck(const Cell &cell) {
  const uint32_t starts = ribbonStarts(cell.equations);
  EXPECT_EQ(solveAndCheck(cell, starts), starts) << "needed a grown cell";
}

TEST(RibbonTest, SolvesCellsOfEveryCodeLength) {
  for (auto [lo, hi] : {std::pair{1u, 1u}, {4u, 4u}, {1u, 24u}, {16u, 64u}}) {
    for (uint64_t eq : {1000u, 2000u, 4000u, 10000u}) {
      solveAndCheck(randomCell(eq, lo, hi, eq * 31 + lo));
    }
  }
}

TEST(RibbonTest, SolvesTinyCells) {
  for (uint64_t eq : {0u, 1u, 7u, 100u, 127u, 128u, 129u, 300u}) {
    for (uint64_t s = 0; s < 20; s++) {
      solveAndCheck(randomCell(eq, 1, 8, eq * 1000 + s));
    }
  }
}

TEST(RibbonTest, LongRowsUseManyStrides) {
  // Packed rows: one key's code stream is longer than the cell has starts.
  solveAndCheck(randomCell(2000, 400, 900, 7));
}

TEST(RibbonTest, OverfullCellGrowsUntilItSolves) {
  const Cell cell = randomCell(3000, 1, 8, 11);
  const uint32_t starts = cell.equations * 9 / 10;
  EXPECT_GT(solveAndCheck(cell, starts), starts);
}

TEST(RibbonTest, OverheadIsBelowThreeHashScheme) {
  const uint64_t eq = build::MAX_EQUATIONS_PER_BUCKET;
  const double bits = cellWords(ribbonStarts(eq)) * 64.0;
  EXPECT_LT(bits / eq, 1.089);
}

} // namespace caramel::tests
