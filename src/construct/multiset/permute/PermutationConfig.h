#pragma once

#include <memory>

namespace caramel {

struct PermutationConfig {
  virtual ~PermutationConfig() = default;
};

using PermutationConfigPtr = std::shared_ptr<PermutationConfig>;

struct EntropyPermutationConfig : public PermutationConfig {};

struct GlobalSortPermutationConfig : public PermutationConfig {
  explicit GlobalSortPermutationConfig(int refinement_iterations = 5)
      : refinement_iterations(refinement_iterations) {}
  int refinement_iterations;
};

} // namespace caramel
