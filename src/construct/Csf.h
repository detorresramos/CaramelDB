#pragma once
#include <memory>
#include <src/BitArray.h>
#include <vector>

namespace caramel {

class Csf;
using CsfPtr = std::shared_ptr<Csf>;

using SubsystemSolutionSeedPair = std::pair<BitArrayPtr, uint32_t>;

class Csf {
public:
  Csf(const std::vector<SubsystemSolutionSeedPair> &solutions_and_seeds,
      const std::vector<uint32_t> &code_length_counts,
      const std::vector<uint32_t> &ordered_symbols, uint32_t hash_store_seed)
      : _solutions_and_seeds(solutions_and_seeds),
        _code_length_counts(code_length_counts),
        _ordered_symbols(ordered_symbols), _hash_store_seed(hash_store_seed) {}

  static CsfPtr
  make(const std::vector<SubsystemSolutionSeedPair> &solutions_and_seeds,
       const std::vector<uint32_t> &code_length_counts,
       const std::vector<uint32_t> &ordered_symbols, uint32_t hash_store_seed) {
    return std::make_shared<Csf>(solutions_and_seeds, code_length_counts,
                                 ordered_symbols, hash_store_seed);
  }

  uint32_t query(const std::string &key) const;

  uint32_t size() const;

private:
  std::vector<SubsystemSolutionSeedPair> _solutions_and_seeds;
  std::vector<uint32_t> _code_length_counts;
  std::vector<uint32_t> _ordered_symbols;
  uint32_t _hash_store_seed;
};

} // namespace caramel