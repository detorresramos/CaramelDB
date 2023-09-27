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
  SparseSystem(std::vector<std::vector<uint32_t>> &&equations,
               std::vector<uint32_t> &&constants, uint32_t solution_size)
      : _equations(equations), _constants(constants),
        _solution_size(solution_size) {}

  static std::shared_ptr<SparseSystem>
  make(std::vector<std::vector<uint32_t>> &&equations,
       std::vector<uint32_t> &&constants, uint32_t solution_size) {
    return std::make_shared<SparseSystem>(std::move(equations),
                                          std::move(constants), solution_size);
  }

  std::pair<std::vector<uint32_t>, uint32_t> getEquation(uint32_t equation_id) {
    return std::make_pair(_equations[equation_id], _constants[equation_id]);
  }

  std::vector<uint32_t> equationIds() const {
    std::vector<uint32_t> equation_ids(_equations.size());
    std::iota(equation_ids.begin(), equation_ids.end(), 0);
    return equation_ids;
  }

  uint32_t numEquations() const { return _equations.size(); }

  uint32_t solutionSize() const { return _solution_size; }

private:
  std::vector<std::vector<uint32_t>> _equations;
  std::vector<uint32_t> _constants;
  uint32_t _solution_size;
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