#pragma once

#include "BitArray.h"
#include <cassert>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace caramel {

class SparseSystem {
public:
  static constexpr size_t EQUATION_SIZE = 4;

  SparseSystem(uint64_t num_equations, uint64_t solution_size)
      : _num_equations(num_equations), _solution_size(solution_size) {
    _equations.reserve(num_equations * EQUATION_SIZE);
  }

  static std::shared_ptr<SparseSystem> make(uint64_t num_equations,
                                            uint64_t solution_size) {
    return std::make_shared<SparseSystem>(num_equations, solution_size);
  }

  void addEquation(uint64_t *start_var_locations, uint32_t offset,
                   uint64_t bit) {
    _equations.push_back(start_var_locations[0] + offset);
    _equations.push_back(start_var_locations[1] + offset);
    _equations.push_back(start_var_locations[2] + offset);
    _equations.push_back(bit);
  }

  void addTestEquation(const std::vector<uint64_t> &equation, uint64_t bit) {
    _equations.push_back(equation[0]);
    _equations.push_back(equation[1]);
    _equations.push_back(equation[2]);
    _equations.push_back(bit);
  }

  const uint64_t *getEquation(uint64_t equation_id) const {
    return &_equations[equation_id * EQUATION_SIZE];
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
};

using SparseSystemPtr = std::shared_ptr<SparseSystem>;

class DenseSystem {
public:
  DenseSystem(uint64_t solution_size, uint64_t num_equations)
      : _solution_size(solution_size) {
    _equations.resize(num_equations);
  }

  static std::shared_ptr<DenseSystem> make(uint64_t solution_size,
                                           uint64_t num_equations) {
    return std::make_shared<DenseSystem>(solution_size, num_equations);
  }

  void addEquation(uint64_t equation_id,
                   const std::vector<uint64_t> &participating_variables,
                   uint32_t constant);

  void addEquation(uint64_t equation_id,
                   const std::unordered_set<uint64_t> &participating_variables,
                   uint32_t constant);

  void addEquation(uint64_t equation_id, const uint64_t *equation_ptr,
                   uint32_t constant);

  std::tuple<BitArrayPtr, uint32_t, uint64_t> &
  getEquation(uint64_t equation_id) {
    return _equations[equation_id];
  }

  void xorEquations(uint64_t equation_to_modify, uint64_t equation_to_xor);

  void swapEquations(uint64_t equation_id_1, uint64_t equation_id_2);

  void updateFirstVar(uint64_t equation_id);

  bool isUnsolvable(uint64_t equation_id) const {
    auto &[equation, constant, _] = _equations[equation_id];
    bool is_empty = !equation;
    return is_empty && constant != 0;
  }

  bool isIdentity(uint64_t equation_id) const {
    auto &[equation, constant, _] = _equations[equation_id];
    bool is_empty = !equation->any();
    return is_empty && constant == 0;
  }

  uint64_t numEquations() const { return _equations.size(); }

  uint64_t solutionSize() const { return _solution_size; }

  std::string str() const;

private:
  std::vector<std::tuple<BitArrayPtr, uint32_t, uint64_t>> _equations;
  uint64_t _solution_size;
};

using DenseSystemPtr = std::shared_ptr<DenseSystem>;

class UnsolvableSystemException : public std::logic_error {
public:
  explicit UnsolvableSystemException(const std::string &message)
      : std::logic_error("System not solvable: " + message){};
};

DenseSystemPtr sparseToDense(const SparseSystemPtr &sparse_system);

} // namespace caramel