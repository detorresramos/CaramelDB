#pragma once

#include "FilterConfig.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace caramel {

static constexpr double AUTO_DELTA = 1.089;
static constexpr double XOR_C = 1.23;
static constexpr double BINARY_FUSE_C_ASYMPTOTIC = 1.075;

struct DiscreteFilterResult {
  int param;
  double lb;
};

struct DiscreteBloomResult {
  int bpe;
  int k;
  double lb;
};

inline double binaryFuseC(size_t n_filter) {
  if (n_filter <= 1) {
    return BINARY_FUSE_C_ASYMPTOTIC;
  }
  return std::max(
      1.075,
      0.77 + 0.305 * std::log(600000.0) /
                 std::log(static_cast<double>(n_filter)));
}

inline double autoLowerBound(double alpha, double eps, double b_eps,
                             double n_over_N) {
  return AUTO_DELTA * alpha * (1.0 - eps) - n_over_N - b_eps * (1.0 - alpha);
}

inline DiscreteFilterResult bestDiscreteXor(double alpha, double n_over_N,
                                            int max_bits = 8) {
  DiscreteFilterResult best{1, -1e308};
  for (int bits = 1; bits <= max_bits; bits++) {
    double b_eps = XOR_C * bits;
    double eps = std::pow(2.0, -bits);
    double lb = autoLowerBound(alpha, eps, b_eps, n_over_N);
    if (lb > best.lb) {
      best = {bits, lb};
    }
  }
  return best;
}

inline DiscreteFilterResult
bestDiscreteBinaryFuse(double alpha, double n_over_N, size_t n_filter,
                       int max_bits = 8) {
  double C = binaryFuseC(n_filter);
  DiscreteFilterResult best{1, -1e308};
  for (int bits = 1; bits <= max_bits; bits++) {
    double b_eps = C * bits;
    double eps = std::pow(2.0, -bits);
    double lb = autoLowerBound(alpha, eps, b_eps, n_over_N);
    if (lb > best.lb) {
      best = {bits, lb};
    }
  }
  return best;
}

inline DiscreteBloomResult bestDiscreteBloomAllK(double alpha, double n_over_N,
                                                 int max_bpe = 8,
                                                 int max_k = 8) {
  DiscreteBloomResult best{1, 1, -1e308};
  for (int k = 1; k <= max_k; k++) {
    for (int bpe = 1; bpe <= max_bpe; bpe++) {
      double eps = std::pow(1.0 - std::exp(-static_cast<double>(k) / bpe), k);
      double b_eps_val;
      if (eps <= 0.0 || eps >= 1.0) {
        b_eps_val = 1e308;
      } else {
        double root = std::pow(eps, 1.0 / k);
        if (root >= 1.0) {
          b_eps_val = 1e308;
        } else {
          b_eps_val = -static_cast<double>(k) / std::log(1.0 - root);
        }
      }
      double lb = autoLowerBound(alpha, eps, b_eps_val, n_over_N);
      if (lb > best.lb) {
        best = {bpe, k, lb};
      }
    }
  }
  return best;
}

template <typename T>
PreFilterConfigPtr selectBestFilter(const std::vector<T> &values,
                                    bool verbose = false) {
  const size_t N = values.size();
  if (N == 0) {
    return nullptr;
  }

  std::unordered_map<T, size_t> frequencies;
  for (const auto &v : values) {
    frequencies[v]++;
  }

  size_t highest_freq = 0;
  for (const auto &[value, freq] : frequencies) {
    if (freq > highest_freq) {
      highest_freq = freq;
    }
  }

  double alpha = static_cast<double>(highest_freq) / N;
  if (alpha >= 1.0) {
    return nullptr;
  }

  size_t n_unique = frequencies.size();
  double n_over_N = static_cast<double>(n_unique) / N;
  size_t n_filter = N - highest_freq;

  if (n_filter <= 10) {
    return nullptr;
  }

  struct Candidate {
    PreFilterConfigPtr config;
    double lb;
    const char *name;
  };

  auto xor_result = bestDiscreteXor(alpha, n_over_N);
  auto bf_result = bestDiscreteBinaryFuse(alpha, n_over_N, n_filter);
  auto bloom_result = bestDiscreteBloomAllK(alpha, n_over_N);

  Candidate candidates[] = {
      {std::make_shared<XORPreFilterConfig>(xor_result.param), xor_result.lb,
       "XOR"},
      {std::make_shared<BinaryFusePreFilterConfig>(bf_result.param),
       bf_result.lb, "BinaryFuse"},
      {std::make_shared<BloomPreFilterConfig>(bloom_result.bpe, bloom_result.k),
       bloom_result.lb, "Bloom"},
  };

  auto *best =
      std::max_element(std::begin(candidates), std::end(candidates),
                       [](const Candidate &a, const Candidate &b) {
                         return a.lb < b.lb;
                       });

  if (best->lb <= 0.0) {
    if (verbose) {
      std::cout << "AutoFilter: no filter beneficial (alpha=" << alpha << ")"
                << std::endl;
    }
    return nullptr;
  }

  if (verbose) {
    std::cout << "AutoFilter: selected " << best->name
              << " (alpha=" << alpha << ", lb=" << best->lb << ")" << std::endl;
  }

  return best->config;
}

} // namespace caramel
