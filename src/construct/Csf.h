#pragma once
#include <cereal/access.hpp>
#include <memory>
#include <src/BitArray.h>
#include <vector>

namespace caramel {

template <typename T> class Csf;

template <typename T> using CsfPtr = std::shared_ptr<Csf<T>>;

using SubsystemSolutionSeedPair = std::pair<BitArrayPtr, uint32_t>;

template <typename T> class Csf {
public:
  Csf(const std::vector<SubsystemSolutionSeedPair> &solutions_and_seeds,
      const std::vector<uint32_t> &code_length_counts,
      const std::vector<T> &ordered_symbols, uint32_t hash_store_seed)
      : _solutions_and_seeds(solutions_and_seeds),
        _code_length_counts(code_length_counts),
        _ordered_symbols(ordered_symbols), _hash_store_seed(hash_store_seed) {}

  static CsfPtr<T>
  make(const std::vector<SubsystemSolutionSeedPair> &solutions_and_seeds,
       const std::vector<uint32_t> &code_length_counts,
       const std::vector<T> &ordered_symbols, uint32_t hash_store_seed) {
    return std::make_shared<Csf<T>>(solutions_and_seeds, code_length_counts,
                                    ordered_symbols, hash_store_seed);
  }

  T query(const std::string &key) const;

  void save(const std::string &filename) const;

  static CsfPtr<T> load(const std::string &filename);

  uint32_t size() const;

private:
  // Private constructor for cereal
  Csf() {}

  friend class cereal::access;
  template <class Archive> void serialize(Archive &archive);

  std::vector<SubsystemSolutionSeedPair> _solutions_and_seeds;
  std::vector<uint32_t> _code_length_counts;
  std::vector<T> _ordered_symbols;
  uint32_t _hash_store_seed;
};

} // namespace caramel