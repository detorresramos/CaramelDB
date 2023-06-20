#pragma once

#include "BitArray.h"
#include <string>
#include <unordered_map>
#include <utility>

namespace caramel {

class SparseSystem {
public:
  SparseSystem(uint32_t solution_size) : _solution_size(solution_size) {}

private:
  uint32_t _solution_size;
};

class DenseSystem {
public:
  DenseSystem(uint32_t solution_size) : _solution_size(solution_size) {}

  void addEquation(uint32_t equation_id,
                   std::vector<uint32_t> participating_variables,
                   uint32_t constant);

  std::pair<BitArray, uint32_t> getEquation(uint32_t equation_id);

  void xorEquations(uint32_t equation_to_modify, uint32_t equation_to_xor);

  void swapEquations(uint32_t equation_id_1, uint32_t equation_id_2);

  uint32_t getFirstVar(uint32_t equation_id);

  bool isUnsolvable(uint32_t equation_id);

  bool isIdentity(uint32_t equation_id);

  uint32_t numEquations() const { return _equations.size(); }

  uint32_t solutionSize() const { return _solution_size; }

private:
  std::unordered_map<uint32_t, BitArray> _equations;
  std::unordered_map<uint32_t, uint32_t> _constants;
  uint32_t _solution_size;
};

class UnsolvableSystemException : public std::logic_error {
public:
  explicit UnsolvableSystemException(const std::string &message)
      : std::logic_error("System not solvable: " + message){};
};

} // namespace caramel