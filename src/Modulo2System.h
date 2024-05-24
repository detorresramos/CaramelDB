#pragma once

#include "BitArray.h"
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace caramel {

class SparseSystem {
public:
  SparseSystem(uint64_t num_equations, uint64_t solution_size)
      : _num_equations(num_equations), _solution_size(solution_size) {
    _equations.reserve(num_equations * 3);
    _constants.reserve(num_equations);
  }

  static std::shared_ptr<SparseSystem> make(uint64_t num_equations,
                                            uint64_t solution_size) {
    return std::make_shared<SparseSystem>(num_equations, solution_size);
  }

  void addEquation(uint64_t *start_var_locations, uint32_t offset, uint32_t bit) {
    _equations.push_back(start_var_locations[0] + offset);
    _equations.push_back(start_var_locations[1] + offset);
    _equations.push_back(start_var_locations[2] + offset);
    _constants.emplace_back(bit);
  }

  std::pair<std::vector<uint64_t>, uint32_t> getEquation(uint64_t equation_id) {
    std::vector<uint64_t> temp{_equations[equation_id * 3],
                               _equations[equation_id * 3 + 1],
                               _equations[equation_id * 3 + 2]};
    return std::make_pair(std::move(temp), _constants[equation_id]);
  }

  std::vector<uint64_t> equationIds() const {
    std::vector<uint64_t> equation_ids(_num_equations);
    std::iota(equation_ids.begin(), equation_ids.end(), 0);
    return equation_ids;
  }

  uint64_t numEquations() const { return _num_equations; }

  uint64_t solutionSize() const { return _solution_size; }

private:
  uint64_t _num_equations;
  uint64_t _solution_size;
  std::vector<uint64_t> _equations;
  std::vector<uint32_t> _constants;
};

using SparseSystemPtr = std::shared_ptr<SparseSystem>;

class DenseSystem {
public:
  DenseSystem(uint32_t solution_size,
              std::optional<uint32_t> expected_num_equations = std::nullopt)
      : _solution_size(solution_size) {
    if (expected_num_equations.has_value()) {
      _equations.reserve(expected_num_equations.value());
    }
  }

  static std::shared_ptr<DenseSystem>
  make(uint32_t solution_size,
       std::optional<uint32_t> expected_num_equations = std::nullopt) {
    return std::make_shared<DenseSystem>(solution_size, expected_num_equations);
  }

  void addEquation(uint32_t equation_id,
                   const std::vector<uint32_t> &participating_variables,
                   uint32_t constant);

  void addEquation(uint32_t equation_id,
                   const std::unordered_set<uint32_t> &participating_variables,
                   uint32_t constant);

  std::pair<BitArrayPtr, uint32_t> getEquation(uint32_t equation_id) const {
    return _equations.find(equation_id)->second;
  }

  void xorEquations(uint32_t equation_to_modify, uint32_t equation_to_xor);

  void swapEquations(uint32_t equation_id_1, uint32_t equation_id_2);

  uint32_t getFirstVar(uint32_t equation_id);

  bool isUnsolvable(uint32_t equation_id) const {
    auto &[equation, constant] = _equations.find(equation_id)->second;
    bool is_empty = !equation;
    return is_empty && constant != 0;
  }

  bool isIdentity(uint32_t equation_id) const {
    auto &[equation, constant] = _equations.find(equation_id)->second;
    bool is_empty = !equation->any();
    return is_empty && constant == 0;
  }

  uint32_t numEquations() const { return _equations.size(); }

  uint32_t solutionSize() const { return _solution_size; }

  std::string str() const;

private:
  std::unordered_map<uint32_t, std::pair<BitArrayPtr, uint32_t>> _equations;
  uint32_t _solution_size;
};

using DenseSystemPtr = std::shared_ptr<DenseSystem>;

class UnsolvableSystemException : public std::logic_error {
public:
  explicit UnsolvableSystemException(const std::string &message)
      : std::logic_error("System not solvable: " + message){};
};

DenseSystemPtr sparseToDense(const SparseSystemPtr &sparse_system);

} // namespace caramel